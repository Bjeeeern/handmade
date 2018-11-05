#include "platform.h"
#include "memory.h"
#include "intrinsics.h"
#include "world_map.h"
#include "random.h"
#include "renderer.h"
#include "resource.h"

//
// NOTE(bjorn): These structs only have a meaning within the game.
// Pi32

struct hero_bitmaps
{
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;

	v2s Alignment;
};

enum entity_type
{
	EntityType_Player,
	EntityType_Wall,
	EntityType_Stair,
};

struct high_entity
{
	v3 P;
	v3 dP;

	b32 IsOnStairs;

	u32 FacingDirection;

	u32 LowEntityIndex;
};

struct low_entity
{
	entity_type Type;

	world_map_position WorldP;
	v3 Dim;

	b32 Collides;

	f32 dZ;

	u32 HighEntityIndex;
};

struct entity
{
	high_entity* High;
	low_entity* Low;
};

struct game_state
{
	memory_arena WorldArena;
	world_map* WorldMap;

	s32 RoomWidthInTiles;
	s32 RoomHeightInTiles;
	v3s RoomOrigin;

	//TODO(bjorn): Should we allow split-screen?
	u32 CameraFollowingPlayerIndex;
	world_map_position CameraPosition;

	u32 PlayerIndexForController[ArrayCount(((game_input*)0)->Keyboards)];
	u32 PlayerIndexForKeyboard[ArrayCount(((game_input*)0)->Controllers)];

	u32 HighEntityCount;
	u32 LowEntityCount;
	high_entity HighEntities[256];
	low_entity LowEntities[100000];

	loaded_bitmap Backdrop;

	hero_bitmaps HeroBitmaps[4];
};

internal_function void
MapEntityIntoHigh(game_state* GameState, u32 LowIndex)
{
	low_entity* Low = GameState->LowEntities + LowIndex;
	if(!Low->HighEntityIndex)
	{
		if(GameState->HighEntityCount < (ArrayCount(GameState->HighEntities) - 1))
		{
			u32 HighIndex = ++GameState->HighEntityCount;
			Low->HighEntityIndex = HighIndex;

			high_entity* High = GameState->HighEntities + HighIndex;

			*High = {};
			High->LowEntityIndex = LowIndex;
			High->P = GetWorldMapPosDifference(GameState->WorldMap, Low->WorldP, 
																				GameState->CameraPosition);
		}
		else
		{
			InvalidCodePath;
		}
	}
}

	internal_function void
MapEntityIntoLow(game_state* GameState, u32 LowIndex)
{
	low_entity* Low = GameState->LowEntities + LowIndex;
	u32 HighIndex = Low->HighEntityIndex;
	if(HighIndex)
	{
		high_entity* High = GameState->HighEntities + HighIndex;
		Low->WorldP = Offset(GameState->WorldMap, GameState->CameraPosition, High->P);
		Low->HighEntityIndex = 0;

		if(HighIndex != GameState->HighEntityCount)
		{
			high_entity* HighAtEnd = GameState->HighEntities + GameState->HighEntityCount;
			*High = *HighAtEnd;

			low_entity* AffectedLow = GameState->LowEntities + HighAtEnd->LowEntityIndex;
			AffectedLow->HighEntityIndex = HighIndex;
		}
		GameState->HighEntityCount -= 1;
	}
}

	inline entity
GetEntityByLowIndex(game_state *GameState, u32 LowEntityIndex)
{
	entity Result = {};

	if(0 < LowEntityIndex && LowEntityIndex <= GameState->LowEntityCount)
	{
		Result.Low = GameState->LowEntities + LowEntityIndex;
		if(Result.Low->HighEntityIndex)
		{
			Result.High = GameState->HighEntities + Result.Low->HighEntityIndex;
		}
	}
	else
	{
		// NOTE(bjorn): I often use this to check whether an index is valid or not.
		//InvalidCodePath;
	}

	return Result;
}

	inline entity
GetEntityByHighIndex(game_state *GameState, u32 HighEntityIndex)
{
	entity Result = {};

	if(0 < HighEntityIndex && HighEntityIndex <= GameState->HighEntityCount)
	{
		Result.High = GameState->HighEntities + HighEntityIndex;

		Assert(Result.High->LowEntityIndex);
		Result.Low = GameState->LowEntities + Result.High->LowEntityIndex;
	}
	else
	{
		InvalidCodePath;
	}

	return Result;
}

	internal_function u32
AddEntity(game_state* GameState)
{
	u32 LowIndex = {};

	GameState->LowEntityCount++;
	Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));

	LowIndex = GameState->LowEntityCount;
	GameState->LowEntities[LowIndex] = {};

	return LowIndex;
}

	internal_function u32
AddPlayer(game_state* GameState)
{
	u32 LowIndex = AddEntity(GameState);
	low_entity* Low = GameState->LowEntities + LowIndex;

	Low->Type = EntityType_Player;

	Low->WorldP = GameState->CameraPosition;
	Low->WorldP = Offset(GameState->WorldMap, Low->WorldP, 
											 GetChunkPosFromAbsTile(GameState->WorldMap, v3s{-2, 1, 0}).Offset_);

	Low->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Low->Collides = true;

	ChangeEntityLocation(&GameState->WorldArena, GameState->WorldMap, LowIndex, 0, &Low->WorldP);

	entity Test = GetEntityByLowIndex(GameState, GameState->CameraFollowingPlayerIndex);
	if(!Test.Low)
	{
		GameState->CameraFollowingPlayerIndex = LowIndex;
	}

	return LowIndex;
}

	internal_function u32
AddWall(game_state* GameState, world_map_position WorldPos)
{
	u32 LowIndex = AddEntity(GameState);
	low_entity* Low = GameState->LowEntities + LowIndex;

	Low->Type = EntityType_Wall;

	Low->WorldP = WorldPos;

	Low->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Low->Collides = true;

	ChangeEntityLocation(&GameState->WorldArena, GameState->WorldMap, LowIndex, 0, &WorldPos);

	return LowIndex;
}

	internal_function u32
AddStair(game_state* GameState, world_map_position WorldPos, f32 dZ)
{
	u32 LowIndex = AddEntity(GameState);
	low_entity* Low = GameState->LowEntities + LowIndex;

	Low->Type = EntityType_Stair;

	Low->WorldP = WorldPos;

	Low->Dim = v3{1, 1, 1} * GameState->WorldMap->TileSideInMeters;
	Low->Collides = true;
	Low->dZ = dZ;

	ChangeEntityLocation(&GameState->WorldArena, GameState->WorldMap, LowIndex, 0, &WorldPos);

	return LowIndex;
}

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState)
{
	GameState->Backdrop = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test/test_background.bmp");

	hero_bitmaps Hero = {};
	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_front_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[0] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_left_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[1] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_back_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[2] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_right_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[3] = Hero;

	InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
									(u8*)Memory->PermanentStorage + sizeof(game_state));

	GameState->WorldMap = PushStruct(&GameState->WorldArena, world_map);

	world_map *WorldMap = GameState->WorldMap;
	WorldMap->ChunkSafetyMargin = 256;
	WorldMap->TileSideInMeters = 1.4f;
	WorldMap->ChunkSideInMeters = WorldMap->TileSideInMeters * TILES_PER_CHUNK;

	u32 RandomNumberIndex = 0;

	GameState->RoomWidthInTiles = 17;
	GameState->RoomHeightInTiles = 9;

	u32 TilesPerWidth = GameState->RoomWidthInTiles;
	u32 TilesPerHeight = GameState->RoomHeightInTiles;

	GameState->RoomOrigin = (v3s)RoundV2ToV2S((v2)v2u{TilesPerWidth, TilesPerHeight} / 2.0f);

	GameState->CameraPosition = GetChunkPosFromAbsTile(WorldMap, GameState->RoomOrigin);
	{
		u32 EntityIndex = AddPlayer(GameState);
		GameState->PlayerIndexForKeyboard[0] = EntityIndex;
	}

	u32 ScreenX = 0;
	u32 ScreenY = 0;
	u32 ScreenZ = 0;

	b32 DoorLeft = false;
	b32 DoorRight = false;
	b32 DoorUp = false;
	b32 DoorDown = false;

	b32 BuildStair = false;
	b32 BuildPause = false;

	s32 BuildDirection = 0;
	s32 StairToBuild = 1;


	for(u32 ScreenIndex = 0;
			ScreenIndex < 50;
			++ScreenIndex)
	{
		//TODO(bjorn): Implement a random number generator!
		Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
		u32 RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;

		if(RandomChoice == 2)
		{
			if(BuildPause)
			{
				//NOTE(bjorn): Do nothing and re-roll.

				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
			}
			else
			{
				BuildStair = true;
				if(ScreenZ == 0)
				{
					StairToBuild = 3;
				}
				else if(ScreenZ == 1)
				{
					StairToBuild = 4;
				}
			}
		}
		if(RandomChoice == 1)
		{
			DoorUp = true;
		}
		if(RandomChoice == 0)
		{
			DoorRight = true;
		}

		for(u32 TileY = 0;
				TileY < TilesPerHeight;
				++TileY)
		{
			for(u32 TileX = 0;
					TileX < TilesPerWidth;
					++TileX)
			{
				v3u AbsTile = {
					ScreenX * TilesPerWidth + TileX, 
					ScreenY * TilesPerHeight + TileY, 
					ScreenZ};

				u32 TileValue = 1;
				if((TileX == 0))
				{
					if(TileY == (TilesPerHeight/2) &&
						 DoorLeft)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileX == (TilesPerWidth-1)))
				{
					if(TileY == (TilesPerHeight/2) &&
						 DoorRight)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileY == 0))
				{
					if(TileX == (TilesPerWidth/2) &&
						 DoorDown)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileY == (TilesPerHeight-1)))
				{
					if(TileX == (TilesPerWidth/2) &&
						 DoorUp)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileX == (TilesPerWidth/2)) &&
					 (TileY == (TilesPerHeight/2)))
				{
					TileValue = StairToBuild;
				}

				world_map_position WorldPos = GetChunkPosFromAbsTile(WorldMap, AbsTile);
				if(TileValue == 3)
				{
					AddStair(GameState, WorldPos, 1);
				}
				if(TileValue == 4)
				{
					AddStair(GameState, WorldPos, -1);
				}
				if(TileValue ==  2)
				{
					AddWall(GameState, WorldPos);
				}
			}
		}

		DoorLeft = DoorRight;
		DoorDown = DoorUp;

		DoorRight = false;
		DoorUp = false;

		if(BuildPause)
		{
			BuildPause = false;
			BuildStair = false;
			StairToBuild = 1;
		}

		if(RandomChoice == 2)
		{
			if(!BuildPause)
			{
				if(ScreenZ == 0)
				{
					ScreenZ = 1;
				}
				else if(ScreenZ == 1)
				{
					ScreenZ = 0;
				}

				BuildPause = true;

				if(ScreenZ == 0)
				{
					StairToBuild = 3;
				}
				else if(ScreenZ == 1)
				{
					StairToBuild = 4;
				}
			}
		}
		else if(RandomChoice == 1)
		{
			++ScreenY;
		}
		else
		{
			++ScreenX;
		} 
	}
}

internal_function b32
TestWall(v2 Axis, v2 P0, v2 D0, f32 Wall, v2 MinEdge, v2 MaxEdge, f32* BestTime)
{
	b32 Hit = false;
	v2 Coaxis = {Axis.Y, Axis.X};

	if(Dot(D0, Axis) != 0.0f)
	{
		f32 t = (Wall - Dot(P0, Axis)) / Dot(D0, Axis);

		if(0 < t && t < *BestTime)
		{
			v2 P1 = P0 + t * D0;

			if(Dot(MinEdge, Coaxis) < Dot(P1, Coaxis) && Dot(P1, Coaxis) < Dot(MaxEdge, Coaxis))
			{
				f32 tEpsilon = 0.0001f;
				*BestTime = Max(0.0f, t - tEpsilon);
				Hit = true;
			}
		}
	}

	return Hit;
}

internal_function world_map_position
MapCameraPosToWorldPos(world_map_position CameraP, world_map* WorldMap, v3 EntityP)
{
	world_map_position Result = CameraP;
	Result.Offset_ += EntityP;
	return RecanonilizePosition(WorldMap, Result);
}

	internal_function void
MoveEntity(game_state* GameState, entity Entity, v2 InputDirection, f32 SecondsToUpdate, 
					 world_map* WorldMap, world_map_position CameraP)
{
	v2 OldPos = Entity.High->P.XY;

	v2 ddP = InputDirection * 85.0f;
	//TODO(casey): ODE here!
	ddP += -8.5f * Entity.High->dP.XY;

	v2 D0 = (0.5f * ddP * Square(SecondsToUpdate) + (Entity.High->dP.XY * SecondsToUpdate));

	//
	// NOTE(bjorn): Collision check after movement
	//
	v2 P0 = OldPos;
	v2 D1 = D0;
	for(s32 Steps = 3;
			Steps > 0;
			Steps--)
	{
		v2 WallNormal = {};
		f32 BestTime = 1.0f;
		entity HitEntity = {};
		b32 HitDetected = false;
		b32 CollidedFromTheInside = false;

		for(u32 CollisionHighEntityIndex = 1;
				CollisionHighEntityIndex <= GameState->HighEntityCount;
				CollisionHighEntityIndex++)
		{
			entity CollisionEntity = GetEntityByHighIndex(GameState, CollisionHighEntityIndex);
			if(CollisionEntity.Low->Collides && 
				 Entity.Low->HighEntityIndex != CollisionHighEntityIndex)
			{

				v2 BottomLeftEdge = CollisionEntity.High->P.XY - 
					(CollisionEntity.Low->Dim.XY + Entity.Low->Dim.XY) * 0.5f;

				v2 TopRightEdge = CollisionEntity.High->P.XY + 
					(CollisionEntity.Low->Dim.XY + Entity.Low->Dim.XY) * 0.5f;

				b32 RightWallTest = TestWall({1,0}, P0, D0, TopRightEdge.X, 
																		 BottomLeftEdge, TopRightEdge, &BestTime);
				b32 LeftWallTest = TestWall({1,0}, P0, D0, BottomLeftEdge.X, 
																		BottomLeftEdge, TopRightEdge, &BestTime);
				b32 TopWallTest = TestWall({0,1}, P0, D0, TopRightEdge.Y, 
																	 BottomLeftEdge, TopRightEdge, &BestTime);
				b32 BottomWallTest = TestWall({0,1}, P0, D0, BottomLeftEdge.Y, 
																			BottomLeftEdge, TopRightEdge, &BestTime);

				if(RightWallTest || LeftWallTest || TopWallTest || BottomWallTest)
				{
					HitDetected = true;
					HitEntity = CollisionEntity;

					CollidedFromTheInside = IsInRectangle(RectMinMax(BottomLeftEdge, TopRightEdge), P0);
				}

				if(RightWallTest)  { WallNormal = { 1, 0}; }
				if(LeftWallTest)   { WallNormal = {-1, 0}; }
				if(TopWallTest)    { WallNormal = { 0, 1}; }
				if(BottomWallTest) { WallNormal = { 0,-1}; }
			}
		}

		if(BestTime >= 1.0f && !HitDetected) 
		{ 
			P0 += D0;
			break; 
		}

		if(HitDetected)
		{
			if(HitEntity.Low->Type == EntityType_Stair)
			{
				P0 += D0;
				if(!CollidedFromTheInside)
				{
					Entity.High->P.Z += HitEntity.Low->dZ;
				}
				break; 
			}
			else
			{
				P0 += BestTime * D0;
				D0 *= (1.0f - BestTime);
				D0              -= Dot(D0,							   WallNormal) * WallNormal;
				Entity.High->dP.XY -= Dot(Entity.High->dP.XY, WallNormal) * WallNormal;
			}
		}
	}

	Entity.High->P.XY = P0;
	Entity.High->dP.XY = (ddP * SecondsToUpdate) + Entity.High->dP.XY;

	if(Entity.High->dP.X != 0.0f && Entity.High->dP.Y != 0.0f)
	{
		if(Absolute(Entity.High->dP.X) > Absolute(Entity.High->dP.Y))
		{
			if(Entity.High->dP.X > 0)
			{
				Entity.High->FacingDirection = 3;
			}
			else
			{
				Entity.High->FacingDirection = 1;
			}
		}
		else
		{
			if(Entity.High->dP.Y > 0)
			{
				Entity.High->FacingDirection = 2;
			}
			else
			{
				Entity.High->FacingDirection = 0;
			}
		}
	}

	world_map_position NewWorldP = MapCameraPosToWorldPos(CameraP, WorldMap, Entity.High->P);
	ChangeEntityLocation(&GameState->WorldArena, GameState->WorldMap, 
											 Entity.High->LowEntityIndex, &Entity.Low->WorldP, &NewWorldP);
  Entity.Low->WorldP = NewWorldP;
}

	internal_function world_map_position 
GetCenterOfRoom(world_map* WorldMap, v3s AbsTileC, world_map_position A, 
								s32 RoomWidthInTiles, s32 RoomHeightInTiles)
{
	world_map_position Result = {};

	v3s AbsTileA = GetAbsTileFromChunkPos(WorldMap, A);
	AbsTileC.Z = AbsTileA.Z;

	v3s Diff = AbsTileA - AbsTileC;
	Diff.X = (Diff.X / RoomWidthInTiles) * RoomWidthInTiles + FloorF32ToS32(RoomWidthInTiles*0.5f);
	Diff.Y = (Diff.Y / RoomHeightInTiles) * RoomHeightInTiles + FloorF32ToS32(RoomHeightInTiles*0.5f);

	Result = GetChunkPosFromAbsTile(WorldMap, AbsTileC + Diff);

	return Result;
}

internal_function void
SetCamera(game_state* GameState, world_map_position NewCameraPosition)
{
	v3 Diff = GetWorldMapPosDifference(GameState->WorldMap, NewCameraPosition, 
																								GameState->CameraPosition);

	GameState->CameraPosition = NewCameraPosition;

	world_map* WorldMap = GameState->WorldMap;

	v2 HighFrequencyUpdateDim = v2{2.0f, 2.0f}*WorldMap->ChunkSideInMeters;

	rectangle2 CameraUpdateBounds = RectCenterDim(v2{0,0}, HighFrequencyUpdateDim);

	world_map_position MinWorldP = MapCameraPosToWorldPos(NewCameraPosition, WorldMap, 
																												CameraUpdateBounds.Min);
	world_map_position MaxWorldP = MapCameraPosToWorldPos(NewCameraPosition, WorldMap, 
																												CameraUpdateBounds.Max);

	rectangle2s CameraUpdateAbsBounds = RectMinMax(MinWorldP.ChunkP.XY, MaxWorldP.ChunkP.XY);

	for(u32 LowEntityIndex = 1;
			LowEntityIndex <= GameState->LowEntityCount;
			LowEntityIndex++)
	{
		low_entity* Low = GameState->LowEntities + LowEntityIndex;

		if(Low->HighEntityIndex)
		{
			high_entity* High = GameState->HighEntities + Low->HighEntityIndex;
			High->P -= Diff;

			if(!(IsInRectangle(CameraUpdateBounds, High->P.XY) && (Absolute(High->P.Z) <= 0.5f)))
			{
				MapEntityIntoLow(GameState, LowEntityIndex);
			}
		}
		else
		{
			if(IsInRectangle(CameraUpdateAbsBounds, Low->WorldP.ChunkP.XY))
			{
				v3 RelCamP = GetWorldMapPosDifference(WorldMap, Low->WorldP, NewCameraPosition);
				if(IsInRectangle(CameraUpdateBounds, RelCamP.XY) && (Absolute(RelCamP.Z) <= 0.5f))
				{
					MapEntityIntoHigh(GameState, LowEntityIndex);
				}
			}
		}
	}
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert( (&GetController(Input, 0)->Struct_Terminator - 
					 &GetController(Input, 0)->Buttons[0]) ==
					ArrayCount(GetController(Input, 0)->Buttons)
				);
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	if(!Memory->IsInitialized)
	{
		InitializeGame(Memory, GameState);
		Memory->IsInitialized = true;
	}

	world_map* WorldMap = GameState->WorldMap;

	//
	// NOTE(bjorn): Movement
	//
	for(s32 ControllerIndex = 0;
			ControllerIndex < ArrayCount(Input->Controllers);
			ControllerIndex++)
	{
		game_controller* Controller = GetController(Input, ControllerIndex);
		if(Controller->IsConnected)
		{
			u32 ControlledEntityIndex = GameState->PlayerIndexForController[ControllerIndex];
			entity ControlledEntity = GetEntityByLowIndex(GameState, ControlledEntityIndex);

			if(ControlledEntity.Low)
			{
				v2 InputDirection = {};

				InputDirection = Controller->LeftStick.End;

				if(ControlledEntity.High)
				{
					MoveEntity(GameState, ControlledEntity, 
										 InputDirection, SecondsToUpdate, 
										 WorldMap, GameState->CameraPosition);
				}

				if(Controller->Start.EndedDown && Controller->Start.HalfTransitionCount)
				{
					//TODO(bjorn) Implement RemoveEntity();
					//GameState->EntityResidencies[ControlledEntityIndex] = EntityResidence_Nonexistent;
					//GameState->PlayerIndexForController[ControllerIndex] = 0;
				}
			}
			else
			{
				if(Controller->Start.EndedDown && Controller->Start.HalfTransitionCount)
				{
					GameState->PlayerIndexForController[ControllerIndex] = AddPlayer(GameState);
				}
			}
		}
	}

	for(s32 KeyboardIndex = 0;
			KeyboardIndex < ArrayCount(Input->Keyboards);
			KeyboardIndex++)
	{
		game_keyboard* Keyboard = GetKeyboard(Input, KeyboardIndex);
		if(Keyboard->IsConnected)
		{
			u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[KeyboardIndex];
			entity ControlledEntity = GetEntityByLowIndex(GameState, ControlledEntityIndex);

			if(ControlledEntity.Low)
			{
				v2 InputDirection = {};

				if(Keyboard->S.EndedDown)
				{
					InputDirection.Y += -1;
				}
				if(Keyboard->A.EndedDown)
				{
					InputDirection.X += -1;
				}
				if(Keyboard->W.EndedDown)
				{
					InputDirection.Y += 1;
				}
				if(Keyboard->D.EndedDown)
				{
					InputDirection.X += 1;
				}

				if(InputDirection.X && InputDirection.Y)
				{
					InputDirection *= invroot2;
				}

				if(ControlledEntity.High)
				{
					MoveEntity(GameState, ControlledEntity, 
										 InputDirection, SecondsToUpdate, 
										 WorldMap, GameState->CameraPosition);
				}
			}
		}
	}


	//
	// NOTE(bjorn): Update camera
	//
	entity CameraTarget = GetEntityByLowIndex(GameState, GameState->CameraFollowingPlayerIndex);
	if(CameraTarget.Low)
	{
#if 0
		world_map_position NewCameraPosition = GetCenterOfRoom(WorldMap, 
																													 GameState->RoomOrigin,
																													 CameraTarget.Low->WorldP,
																													 GameState->RoomWidthInTiles,
																													 GameState->RoomHeightInTiles);
		SetCamera(GameState, NewCameraPosition);
#else
		SetCamera(GameState, CameraTarget.Low->WorldP);
#endif
	}

	//
	// NOTE(bjorn): Rendering
	//
	s32 TileSideInPixels = 60;
	f32 PixelsPerMeter = (f32)TileSideInPixels / WorldMap->TileSideInMeters;

	DrawRectangle(Buffer, 
								RectMinMax(v2{0.0f, 0.0f}, v2{(f32)Buffer->Width, (f32)Buffer->Height}), 
								{1.0f, 0.0f, 1.0f});

#if 1
	DrawBitmap(Buffer, &GameState->Backdrop, {-40.0f, -40.0f}, 
						 {(f32)GameState->Backdrop.Width, (f32)GameState->Backdrop.Height});
#endif

	v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

	for(u32 HighEntityIndex = 1;
			HighEntityIndex <= GameState->HighEntityCount;
			HighEntityIndex++)
	{
		entity Entity = GetEntityByHighIndex(GameState, HighEntityIndex);

		v2 CollisionMarkerPixelDim = 
			Hadamard(Entity.Low->Dim.XY, {PixelsPerMeter, PixelsPerMeter});
		m22 GameSpaceToScreenSpace = 
		{PixelsPerMeter, 0             ,
		 0             ,-PixelsPerMeter};

		v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * Entity.High->P.XY;
		//TODO(bjorn)Add z axis jump.
		v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;

		if(Entity.Low->Type == EntityType_Stair)
		{
			v3 StairColor = {};
			if(Entity.Low->dZ == 1)
			{
				v3 LightGreen = {0.5f, 1, 0.5f};
				StairColor = LightGreen;
			}
			if(Entity.Low->dZ == -1)
			{
				v3 LightRed = {1, 0.5f, 0.5f};
				StairColor = LightRed;
			}
			DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), StairColor);
		}
		if(Entity.Low->Type == EntityType_Wall)
		{
			v3 White = {1, 1, 1};
			DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), White);
		}
		if(Entity.Low->Type == EntityType_Player)
		{
			v2 PlayerPixelDim = Hadamard({0.75f, 1.0f}, 
																	 (v2)v2s{TileSideInPixels, TileSideInPixels});

			if(Entity.Low->WorldP.ChunkP.Z == GameState->CameraPosition.ChunkP.Z)
			{
				v3 Yellow = {1.0f, 1.0f, 0.0f};
				DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

				hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

				DrawBitmap(Buffer, &Hero->Torso, EntityPixelPos - Hero->Alignment, (v2)Hero->Torso.Dim);
				DrawBitmap(Buffer, &Hero->Cape, EntityPixelPos - Hero->Alignment, (v2)Hero->Cape.Dim);
				DrawBitmap(Buffer, &Hero->Head, EntityPixelPos - Hero->Alignment, (v2)Hero->Head.Dim);
			}
		}
	}
}

	internal_function void
OutputSound(game_sound_output_buffer *SoundBuffer, game_state* GameState)
{
	s32 ToneVolume = 1000;
	s16 *SampleOut = SoundBuffer->Samples;
	for(s32 SampleIndex = 0;
			SampleIndex < SoundBuffer->SampleCount;
			++SampleIndex)
	{
		s16 SampleValue = 0;
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
	}
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	OutputSound(SoundBuffer, GameState);
}
