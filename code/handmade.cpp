#include "platform.h"
#include "memory.h"
#include "intrinsics.h"
#include "tile_map.h"
#include "random.h"
#include "renderer.h"
#include "resource.h"

//
// NOTE(bjorn): These structs only have a meaning within the game.
// Pi32
struct world
{
	tile_map *TileMap;
};

struct hero_bitmaps
{
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;

	v2s Alignment;
};

enum entity_residence
{
	EntityResidence_Nonexistent,
	EntityResidence_Dormant,
	EntityResidence_Low,
	EntityResidence_High,
};

enum entity_type
{
	EntityType_Player,
	EntityType_Wall,
};

struct high_entity
{
	v2 P;
	v2 dP;
	b32 IsOnStairs;
	u32 FacingDirection;
};

struct low_entity
{
};

struct dormant_entity
{
	entity_type Type;

	tile_map_position TileP;
	union
	{
		v2 Dim;
		struct
		{
			f32 Width;
			f32 Height;
		};
	};
	b32 Collides;
	s32 dAbsTileZ;
};

struct entity
{
	entity_residence* Residence;
	high_entity* High;
	low_entity* Low;
	dormant_entity* Dormant;
};

struct game_state
{
	memory_arena WorldArena;
	world* World;

	s32 RoomWidthInTiles;
	s32 RoomHeightInTiles;

	//TODO(bjorn): Should we allow split-screen?
	u32 CameraFollowingPlayerIndex;
	tile_map_position CameraPosition;

	u32 PlayerIndexForController[ArrayCount(((game_input*)0)->Keyboards)];
	u32 PlayerIndexForKeyboard[ArrayCount(((game_input*)0)->Controllers)];

	u32 EntityCount;
	entity_residence EntityResidencies[256];
	high_entity HighEntities[256];
	low_entity LowEntities[256];
	dormant_entity DormantEntities[256];

	loaded_bitmap Backdrop;

	hero_bitmaps HeroBitmaps[4];
};

internal_function void
MapEntityIntoHigh(game_state* GameState, u32 EntityIndex)
{
	if(GameState->EntityResidencies[EntityIndex] != EntityResidence_High)
	{
		GameState->EntityResidencies[EntityIndex] = EntityResidence_High;

		high_entity* High = GameState->HighEntities + EntityIndex;
		dormant_entity* Dormant = GameState->DormantEntities + EntityIndex;

		*High = {};
		High->P = GetTileMapPosDifference(GameState->World->TileMap, Dormant->TileP, 
																			GameState->CameraPosition).MeterDiff.XY;
	}
}
internal_function void
MapEntityIntoDormant(game_state* GameState, u32 EntityIndex)
{
	if(GameState->EntityResidencies[EntityIndex] != EntityResidence_Dormant)
	{
		GameState->EntityResidencies[EntityIndex] = EntityResidence_Dormant;

		high_entity* High = GameState->HighEntities + EntityIndex;
		dormant_entity* Dormant = GameState->DormantEntities + EntityIndex;

		Dormant->TileP = Offset(GameState->World->TileMap, GameState->CameraPosition, High->P);
	}
}

	inline entity
GetEntity(game_state *GameState, entity_residence DesiredResidence, u32 EntityIndex)
{
	entity Result = {};

	if(0 < EntityIndex && EntityIndex <= GameState->EntityCount)
	{
		if(GameState->EntityResidencies[EntityIndex] > EntityResidence_Nonexistent)
		{
			if(DesiredResidence == EntityResidence_High && 
				 GameState->EntityResidencies[EntityIndex] < EntityResidence_High)
			{
				MapEntityIntoHigh(GameState, EntityIndex);
			}

			Result.Residence = &GameState->EntityResidencies[EntityIndex];
			Result.High = &GameState->HighEntities[EntityIndex];
			Result.Low = &GameState->LowEntities[EntityIndex];
			Result.Dormant = &GameState->DormantEntities[EntityIndex];
		}
	}

	return Result;
}

	internal_function u32
AddEntity(game_state* GameState)
{
	u32 Result = {};

	GameState->EntityCount++;
	Assert(GameState->EntityCount < ArrayCount(GameState->HighEntities));

	Result = GameState->EntityCount;
	GameState->EntityResidencies[Result] = EntityResidence_Dormant;

	return Result;
}

	internal_function u32
AddPlayer(game_state* GameState)
{
	u32 EntityIndex = AddEntity(GameState);
	entity Entity = GetEntity(GameState, EntityResidence_Dormant, EntityIndex);

	*Entity.High = {};
	*Entity.Low = {};
	*Entity.Dormant = {};

	Entity.Dormant->Type = EntityType_Player;

	Entity.Dormant->TileP.AbsTile = GameState->CameraPosition.AbsTile;
	Entity.Dormant->TileP.AbsTile += v3s{-2, 1, 0};

	Entity.Dormant->Dim = v2{0.5f, 0.3f} * GameState->World->TileMap->TileSideInMeters;
	Entity.Dormant->Collides = true;

	entity Test = GetEntity(GameState, EntityResidence_Dormant, 
									 GameState->CameraFollowingPlayerIndex);
	if(!Test.Residence)
	{
		GameState->CameraFollowingPlayerIndex = EntityIndex;
		Entity.Dormant->TileP.AbsTile = v3u{7, 5, 0};
	}

	return EntityIndex;
}

	internal_function u32
AddWall(game_state* GameState, v3u AbsTilePos)
{
	u32 Result = AddEntity(GameState);
	entity Entity = GetEntity(GameState, EntityResidence_Dormant, Result);

	*Entity.High = {};
	*Entity.Low = {};
	*Entity.Dormant = {};

	Entity.Dormant->Type = EntityType_Wall;

	Entity.Dormant->TileP.AbsTile = AbsTilePos;

	Entity.Dormant->Dim = v2{1, 1} * GameState->World->TileMap->TileSideInMeters;
	Entity.Dormant->Collides = true;

	return Result;
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

	GameState->World = PushStruct(&GameState->WorldArena, world);
	world *World = GameState->World;
	World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

	tile_map *TileMap = World->TileMap;
	TileMap->ChunkShift = 4;
	TileMap->ChunkDim = (1 << TileMap->ChunkShift);
	TileMap->ChunkMask = TileMap->ChunkDim - 1;

	TileMap->TileSideInMeters = 1.4f;

	TileMap->ChunkCount = {128, 128, 2};

	TileMap->TileChunks = PushArray(&GameState->WorldArena, 
																	TileMap->ChunkCount.X*
																	TileMap->ChunkCount.Y*
																	TileMap->ChunkCount.Z, tile_chunk);

	{
		u32 EntityIndex = AddPlayer(GameState);
		GameState->PlayerIndexForKeyboard[0] = EntityIndex;
		MapEntityIntoHigh(GameState, EntityIndex);
	}

	u32 RandomNumberIndex = 0;

	GameState->RoomWidthInTiles = 17;
	GameState->RoomHeightInTiles = 9;
	u32 TilesPerWidth = GameState->RoomWidthInTiles;
	u32 TilesPerHeight = GameState->RoomHeightInTiles;

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
			ScreenIndex < 100;
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

				//SetTileValue(&GameState->WorldArena, World->TileMap, AbsTile, TileValue);
				if(TileValue ==  2 && AbsTile.X < 30 && AbsTile.Y < 30)
				{
					u32 EntityIndex = AddWall(GameState, AbsTile);
					//MapEntityIntoHigh(GameState, EntityIndex);
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

internal_function v2 
ClosestPointInRectangle(v2 MinCorner, v2 MaxCorner, tile_map_diff RelNewPos)
{
	v2 Result = {};
	Result.X = Clamp(RelNewPos.MeterDiff.X, MinCorner.X, MaxCorner.X);
	Result.Y = Clamp(RelNewPos.MeterDiff.Y, MinCorner.Y, MaxCorner.Y);

	return Result;
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

internal_function tile_map_position
MapIntoTileSpace(tile_map_position CameraP, tile_map* TileMap, v2 EntityP)
{
	tile_map_position Result = CameraP;
	Result.Offset_ += EntityP;
	return RecanonilizePosition(TileMap, Result);
}

	internal_function void
MoveEntity(game_state* GameState, u32 EntityIndex, v2 InputDirection, f32 SecondsToUpdate, 
					 tile_map* TileMap, tile_map_position CameraP)
{
	entity Entity = GetEntity(GameState, EntityResidence_High, EntityIndex);

	v2 OldPos = Entity.High->P;

	v2 ddP = InputDirection * 85.0f;
	//TODO(casey): ODE here!
	ddP += -8.5f * Entity.High->dP;

	v2 D0 = (0.5f * ddP * Square(SecondsToUpdate) + (Entity.High->dP * SecondsToUpdate));

	v2 NewPos = OldPos + D0;

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
		u32 HitEntityIndex = 0;

		for(u32 CollisionEntityIndex = 1;
				CollisionEntityIndex <= GameState->EntityCount;
				CollisionEntityIndex++)
		{
			if((EntityIndex != CollisionEntityIndex) &&
				 (GameState->EntityResidencies[CollisionEntityIndex] == EntityResidence_High))
			{
				entity CollisionEntity = GetEntity(GameState, 
																					 EntityResidence_High, CollisionEntityIndex);

				v2 BottomLeftEdge = CollisionEntity.High->P - 
					(CollisionEntity.Dormant->Dim + Entity.Dormant->Dim) * 0.5f;

				v2 TopRightEdge = CollisionEntity.High->P + 
					(CollisionEntity.Dormant->Dim + Entity.Dormant->Dim) * 0.5f;

				if(TestWall({1,0}, P0, D0, TopRightEdge.X, BottomLeftEdge, TopRightEdge, &BestTime))
				{
					WallNormal = {1, 0};
				}
				if(TestWall({1,0}, P0, D0, BottomLeftEdge.X, BottomLeftEdge, TopRightEdge, &BestTime))
				{
					WallNormal = {-1, 0};
				}
				if(TestWall({0,1}, P0, D0, TopRightEdge.Y, BottomLeftEdge, TopRightEdge, &BestTime))
				{
					WallNormal = {0, 1};
				}
				if(TestWall({0,1}, P0, D0, BottomLeftEdge.Y, BottomLeftEdge, TopRightEdge, &BestTime))
				{
					WallNormal = {0, -1};
				}
			}
		}

		P0 += BestTime * D0;

		D0 *= (1.0f - BestTime);
		D0 -= Dot(D0, WallNormal) * WallNormal;
		Entity.High->dP -= Dot(Entity.High->dP, WallNormal) * WallNormal;

		if(BestTime >= 1.0f) { break; }
	}

	Entity.High->P = OldPos + P0 - OldPos;
	Entity.High->dP = (ddP * SecondsToUpdate) + Entity.High->dP;

#if 0
	u32 CurrentTile = GetTileValue(TileMap, Entity->Pos);
	if((CurrentTile == 3) ||
		 (CurrentTile == 4))
	{
		if(!Entity->IsOnStairs)
		{
			if(CurrentTile == 3)
			{
				Entity->Pos.AbsTile.Z += 1;
			}
			else if(CurrentTile == 4)
			{
				Entity->Pos.AbsTile.Z -= 1;
			}
			Entity->IsOnStairs = true;
		}
	}
	else
	{
		Entity->IsOnStairs = false;
	}
#endif
	Entity.Dormant->TileP = MapIntoTileSpace(CameraP, TileMap, Entity.High->P);

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
}

	internal_function tile_map_position 
GetCenterOfRoom(tile_map_position A, s32 RoomWidthInTiles, s32 RoomHeightInTiles)
{
	tile_map_position Result = {};

	Result.AbsTile.X = 
		(A.AbsTile.X / RoomWidthInTiles) * RoomWidthInTiles + FloorF32ToS32(RoomWidthInTiles*0.5f);
	Result.AbsTile.Y = 
		(A.AbsTile.Y / RoomHeightInTiles) * RoomHeightInTiles + FloorF32ToS32(RoomHeightInTiles*0.5f);
	Result.AbsTile.Z = A.AbsTile.Z;

	return Result;
}

internal_function v2
SetCamera(game_state* GameState, tile_map_position NewCameraPosition)
{
	v2 Result = {};

	tile_map_diff Diff = GetTileMapPosDifference(GameState->World->TileMap, 
																							 NewCameraPosition, 
																							 GameState->CameraPosition);

	GameState->CameraPosition = NewCameraPosition;
	Result = Diff.MeterDiff.XY;

	tile_map* TileMap = GameState->World->TileMap;

	u32 AbsTileWidth = 17 * 3;
	u32 AbsTileHeight = 9 * 3;
	rectangle2 CameraUpdateBounds = 
		RectCenterDim({0,0}, (v2)v2u{AbsTileWidth, AbsTileHeight} * TileMap->TileSideInMeters);
	rectangle2u CameraUpdateAbsBounds = RectCenterDim(v2u{0,0}, v2u{AbsTileWidth, AbsTileHeight});

	//TODO(bjorn): This should not be a problem when we are in the middle of the
	//as good as infinite map.
	if(CameraUpdateAbsBounds.Min.X > max_u32 / 2) CameraUpdateAbsBounds.Min.X = 0;
	if(CameraUpdateAbsBounds.Min.Y > max_u32 / 2) CameraUpdateAbsBounds.Min.Y = 0;

	for(u32 EntityIndex = 1;
			EntityIndex <= GameState->EntityCount;
			EntityIndex++)
	{
		high_entity* High = GameState->HighEntities + EntityIndex;
		dormant_entity* Dormant = GameState->DormantEntities + EntityIndex;

		if(GameState->EntityResidencies[EntityIndex] == EntityResidence_High)
		{
			High->P -= Result;

			if(!IsInRectangle(CameraUpdateBounds, High->P))
			{
				MapEntityIntoDormant(GameState, EntityIndex);
			}
		}
		if(GameState->EntityResidencies[EntityIndex] == EntityResidence_Dormant)
		{
			if(Dormant->TileP.AbsTile.Z == NewCameraPosition.AbsTile.Z &&
				 IsInRectangle(CameraUpdateAbsBounds, Dormant->TileP.AbsTile.XY))
			{
				MapEntityIntoHigh(GameState, EntityIndex);
			}
		}
	}

	return Result;
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

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	f32 PlayerWidth = (f32)TileMap->TileSideInMeters * 0.75f;

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

			if(GameState->EntityResidencies[ControlledEntityIndex] == EntityResidence_High)
			{
				v2 InputDirection = {};

				InputDirection = Controller->LeftStick.End;

				MoveEntity(GameState, ControlledEntityIndex, 
									 InputDirection, SecondsToUpdate, 
									 TileMap, GameState->CameraPosition);

				if(Controller->Start.EndedDown && Controller->Start.HalfTransitionCount)
				{
					GameState->EntityResidencies[ControlledEntityIndex] = EntityResidence_Nonexistent;
					GameState->PlayerIndexForController[ControllerIndex] = 0;
				}
			}
			else
			{
				if(Controller->Start.EndedDown && Controller->Start.HalfTransitionCount)
				{
					GameState->PlayerIndexForController[ControllerIndex] = AddPlayer(GameState);
					//MapEntityIntoHigh(GameState, GameState->PlayerIndexForController[ControllerIndex]);
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
			if(GameState->EntityResidencies[ControlledEntityIndex] == EntityResidence_High)
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

				MoveEntity(GameState, ControlledEntityIndex, 
									 InputDirection, SecondsToUpdate, 
									 TileMap, GameState->CameraPosition);
			}
		}
	}


	//
	// NOTE(bjorn): Update camera/Z-position
	//
	v2 CameraMovedOffset = {};
	entity CameraFollowingEntity = GetEntity(GameState, EntityResidence_Dormant, 
																					 GameState->CameraFollowingPlayerIndex);
	if(*CameraFollowingEntity.Residence >= EntityResidence_Dormant)
	{
		tile_map_position NewCameraPosition = GetCenterOfRoom(CameraFollowingEntity.Dormant->TileP,
																													GameState->RoomWidthInTiles,
																													GameState->RoomHeightInTiles);
		CameraMovedOffset = SetCamera(GameState, NewCameraPosition);
	}

	//
	// NOTE(bjorn): Rendering
	//
	s32 TileSideInPixels = 60;
	f32 PixelsPerMeter = (f32)TileSideInPixels / TileMap->TileSideInMeters;

	DrawRectangle(Buffer, RectMinMax({0.0f, 0.0f}, {(f32)Buffer->Width, (f32)Buffer->Height}), 
								{1.0f, 0.0f, 1.0f});

#if 1
	DrawBitmap(Buffer, &GameState->Backdrop, {-40.0f, -40.0f}, 
						 {(f32)GameState->Backdrop.Width, (f32)GameState->Backdrop.Height});
#endif

	v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

	for(s32 RelRow = -20;
			RelRow < 40;
			++RelRow)
	{
		for(s32 RelColumn = -30;
				RelColumn < 60;
				++RelColumn)
		{
			u32 Column = GameState->CameraPosition.AbsTile.X + RelColumn;
			u32 Row = GameState->CameraPosition.AbsTile.Y + RelRow;

			u32 TileID = GetTileValue(TileMap, 
																v3u{Column, Row, GameState->CameraPosition.AbsTile.Z});
			v3 TileRGB = {};

			if(TileID == 0)
			{
				TileRGB = {1.0f, 0.0f, 0.0f};
			}
			if(TileID == 1)
			{
				f32 Gray = 0.5f;
				TileRGB = {Gray, Gray, Gray};
			}
			if(TileID == 2)
			{
				f32 White = 1.0f;
				TileRGB = {White, White, White};
			}
			if(TileID == 3)
			{
				TileRGB = {0.0f, 0.5f, 1.0f};
			}
			if(TileID == 4)
			{
				TileRGB = {0.0f, 1.0f, 0.5f};
			}

			m22 InvertY = {1, 0,
										 0,-1};

			v2 Min = ScreenCenter 
				+ ((v2)v2s{RelColumn, -RelRow} - v2{0.5f, -0.5f}) * TileSideInPixels
				- InvertY * GameState->CameraPosition.Offset_;

			v2 Max = Min + v2{1.0f, -1.0f} * TileSideInPixels;

			if(TileID != 1)
			{
				DrawRectangle(Buffer, RectMinMax(Min, Max), TileRGB);
			}
		}
	}

	for(u32 EntityIndex = 1;
			EntityIndex <= GameState->EntityCount;
			EntityIndex++)
	{
		if(GameState->EntityResidencies[EntityIndex] == EntityResidence_High)
		{
			entity Entity = GetEntity(GameState, EntityResidence_High, EntityIndex);

			v2 CollisionMarkerPixelDim = 
				Hadamard(Entity.Dormant->Dim, {PixelsPerMeter, PixelsPerMeter});
			m22 GameSpaceToScreenSpace = 
			{PixelsPerMeter, 0             ,
				0             ,-PixelsPerMeter};

			v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * Entity.High->P;
			v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;

			if(Entity.Dormant->Type == EntityType_Wall)
			{
				v3 White = {1, 1, 1};
				DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), White);
			}
			if(Entity.Dormant->Type == EntityType_Player)
			{
				v2 PlayerPixelDim = Hadamard({0.75f, 1.0f}, 
																		 (v2)v2s{TileSideInPixels, TileSideInPixels});


				if(Entity.Dormant->TileP.AbsTile.Z == GameState->CameraPosition.AbsTile.Z)
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
