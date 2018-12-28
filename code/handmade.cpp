#include "platform.h"
#include "memory.h"
#include "intrinsics.h"
#include "world_map.h"
#include "random.h"
#include "renderer.h"
#include "resource.h"
#include "entity.h"

// QUICK TODO
//
// generate world as you drive
// car engine that is settable by mouse click and drag
// collide with rocks
// ai cars
//


struct hero_bitmaps
{
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;
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
	world_map_position CameraP;

	u32 PlayerIndexForController[ArrayCount(((game_input*)0)->Keyboards)];
	u32 PlayerIndexForKeyboard[ArrayCount(((game_input*)0)->Controllers)];

	entities Entities;

	loaded_bitmap Backdrop;
	loaded_bitmap Rock;
	loaded_bitmap Dirt;

	hero_bitmaps HeroBitmaps[4];
};

	internal_function entity
AddPlayer(game_state* GameState)
{
	world_map_position InitP = OffsetWorldPos(GameState->WorldMap, GameState->CameraP, 
																						GetChunkPosFromAbsTile(GameState->WorldMap, 
																													 v3s{-2, 1, 0}).Offset_);

	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Player, InitP);

	Entity.Low->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;
	Entity.Low->DecelerationFactor = -8.5f;
	Entity.Low->AccelerationFactor = 85.0f;

	entity Test = GetEntityByLowIndex(&GameState->Entities, GameState->CameraFollowingPlayerIndex);
	if(!Test.Low)
	{
		GameState->CameraFollowingPlayerIndex = Entity.LowEntityIndex;
	}

	return Entity;
}

	internal_function entity
AddCarFrame(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityType_CarFrame, WorldPos);

	Entity.Low->Dim = v2{4, 6};
	Entity.Low->Collides = true;

	return Entity;
}

	internal_function entity
AddEngine(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 	EntityType_Engine, WorldPos);

	Entity.Low->Dim = v2{3, 2};

	return Entity;
}

	internal_function entity
AddWheel(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 	EntityType_Wheel, WorldPos);

	Entity.Low->Dim = v2{0.6f, 1.5f};

	return Entity;
}

internal_function void
MoveCarPartsToStartingPosition(memory_arena* WorldArena, world_map* WorldMap, entities* Entities,
															 u32 CarFrameLowIndex)
{
	Assert(CarFrameLowIndex);
	entity CarFrameEntity = GetEntityByLowIndex(Entities, CarFrameLowIndex);
	Assert(CarFrameEntity.Low);
	if(CarFrameEntity.Low)
	{
		entity EngineEntity = GetEntityByLowIndex(Entities, CarFrameEntity.Low->CarFrame.Engine);
		if(EngineEntity.Low)
		{
			OffsetAndChangeEntityLocation(WorldArena, WorldMap, &EngineEntity, 
																		CarFrameEntity.Low->WorldP, v3{0, 1.5f, 0});
		}

		for(s32 WheelIndex = 0;
				WheelIndex < 4;
				WheelIndex++)
		{
			entity WheelEntity = 
				GetEntityByLowIndex(Entities, CarFrameEntity.Low->CarFrame.Wheels[WheelIndex]);
			if(WheelEntity.Low)
			{
				f32 dX = ((WheelIndex % 2) - 0.5f) * 2.0f;
				f32 dY = ((WheelIndex / 2) - 0.5f) * 2.0f;
				v2 D = v2{dX, dY};

				v2 O = (Hadamard(D, CarFrameEntity.Low->Dim.XY * 0.5f) - 
								Hadamard(D, WheelEntity.Low->Dim.XY * 0.5f));

				OffsetAndChangeEntityLocation(WorldArena, WorldMap, &WheelEntity, 
																			CarFrameEntity.Low->WorldP, (v3)O);
			}
		}
	}
}

internal_function void
DismountEntityFromCar(memory_arena* WorldArena, world_map* WorldMap, entity* Entity, entity* Car)
{
	Assert(Car->Low->Type == EntityType_CarFrame);

	Car->Low->CarFrame.DriverSeat = 0;
	Entity->Low->Player.RidingVehicle = 0;

	v3 Offset = {4.0f, 0, 0};
	if(Entity->High && Car->High)
	{
		Entity->High->P = Car->High->P + Offset;
	}
	OffsetAndChangeEntityLocation(WorldArena, WorldMap, Entity, Car->Low->WorldP, Offset);
}
internal_function void
MountEntityOnCar(memory_arena* WorldArena, world_map* WorldMap, entity* Entity, entity* Car)
{
	Assert(Car->Low->Type == EntityType_CarFrame);

	Car->Low->CarFrame.DriverSeat = Entity->LowEntityIndex;
	Entity->Low->Player.RidingVehicle = Car->LowEntityIndex;

	//TODO(bjorn) What happens if you mount a car outside of the high entity zone.
	if(Entity->High && Car->High)
	{
		Entity->High->P = Car->High->P;
	}
	OffsetAndChangeEntityLocation(WorldArena, WorldMap, Entity, Car->Low->WorldP, {});
}

	internal_function entity
AddCar(game_state* GameState, world_map_position WorldPos)
{
	entity CarFrameEntity = AddCarFrame(GameState, WorldPos);
	CarFrameEntity.Low->DecelerationFactor = -0.5f;

	entity EngineEntity = AddEngine(GameState, WorldPos);
	CarFrameEntity.Low->CarFrame.Engine = EngineEntity.LowEntityIndex;
	EngineEntity.Low->Engine.Vehicle = CarFrameEntity.LowEntityIndex;

	for(s32 WheelIndex = 0;
			WheelIndex < 4;
			WheelIndex++)
	{
    entity WheelEntity = AddWheel(GameState, WorldPos);
		CarFrameEntity.Low->CarFrame.Wheels[WheelIndex] = WheelEntity.LowEntityIndex;
		WheelEntity.Low->Wheel.Vehicle = CarFrameEntity.LowEntityIndex;
	}

	MoveCarPartsToStartingPosition(&GameState->WorldArena, GameState->WorldMap, 
																 &GameState->Entities, CarFrameEntity.LowEntityIndex);

	return CarFrameEntity;
}

	internal_function entity
AddGround(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Ground, WorldPos);

	Entity.Low->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = false;

	return Entity;
}

	internal_function entity
AddWall(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Wall, WorldPos);

	Entity.Low->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;

	return Entity;
}

	internal_function entity
AddStair(game_state* GameState, world_map_position WorldPos, f32 dZ)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 	EntityType_Stair, WorldPos);

	Entity.Low->Dim = v3{1, 1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;
	Entity.Low->Stair.dZ = dZ;

	return Entity;
}

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState)
{
	GameState->Backdrop = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test/test_background.bmp");

	GameState->Rock = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test2/rock00.bmp");
	GameState->Rock.Alignment = {35, 41};

	GameState->Dirt = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test2/ground00.bmp");
	GameState->Dirt.Alignment = {133, 56};

	hero_bitmaps Hero = {};
	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_front_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[0] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_left_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[1] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_back_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[2] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_right_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
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

	GameState->CameraP = GetChunkPosFromAbsTile(WorldMap, GameState->RoomOrigin);
	{
		entity Player = AddPlayer(GameState);
		GameState->PlayerIndexForKeyboard[0] = Player.LowEntityIndex;
		AddCar(GameState, OffsetWorldPos(GameState->WorldMap, Player.Low->WorldP, {3.0f, 0, 0}));
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
			ScreenIndex < 500;
			++ScreenIndex)
	{
		//TODO(bjorn): Implement a random number generator!
		Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
		u32 RandomChoice = RandomNumberTable[RandomNumberIndex++] % 1;

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
				if((TileValue ==  1) && !(TileX % 5) && !(TileY % 3))
				{
					AddGround(GameState, WorldPos);
				}
				if(TileValue ==  2)
				{
					if(!ScreenIndex)
					{
						AddWall(GameState, WorldPos);
					}
				}
				if(TileValue == 3)
				{
					//AddStair(GameState, WorldPos, 1);
				}
				if(TileValue == 4)
				{
					//AddStair(GameState, WorldPos, -1);
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

//NOTE(bjorn): Lazy fix. https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
// I dont even understand the maths behind it.
struct intersection_result
{
	b32 Valid;
	f32 t;
};
internal_function intersection_result
GetTimeOfIntersectionWithLineSegment(v2 P, v2 Delta, v2 A, v2 B)
{
	intersection_result Result = {};
	v2 PD = P+Delta;
	f32 denominator = (P.X-PD.X) * (A.Y-B.Y) - (P.Y-PD.Y) * (A.X-B.X);
	if(denominator != 0)
	{
		f32 u = -(((P.X-PD.X) * (P.Y-A.Y) - (P.Y-PD.Y) * (P.X-A.X)) / denominator);
		if((u >= 0.0f) && (u <= 1.0f))
		{
			Result.Valid = true;
			Result.t = ((P.X-A.X) * (A.Y-B.Y) - (P.Y-A.Y) * (A.X-B.X)) / denominator;
		}
	}

	return Result;
}

struct collision_result
{
	b32 Hit;
  f32 TimeOfImpact;
	v2 Normal;
};

internal_function collision_result 
MinkowskiSumCollision(entity* CollisionEntity, entity* Entity, v2 DeltaP)
{
	collision_result Result = {};

	v2 EntD    = Entity->High->D.XY;
	v2 ColEntD = CollisionEntity->High->D.XY;

	v2 EntP    = Entity->High->P.XY;
	v2 ColEntP = CollisionEntity->High->P.XY;

	v2 ps[8] = {};
	v2 dir_ord[4] = {{-1, 1}, {1, 1}, {1, -1}, {-1, -1}};
	m22 cw = { 0,-1,
						 1, 0};
	for(int i = 0; i < 4; i++)
	{
		f32 x = dir_ord[i%4].X;
		f32 y = dir_ord[i%4].Y;

		v2 ToCorner = Hadamard(v2{x, y}, CollisionEntity->Low->Dim.XY) * 0.5f;
		ps[i] = cw * ColEntD * ToCorner.X + 
			           ColEntD * ToCorner.Y;
	}
	for(int i = 4; i < 8; i++)
	{
		f32 x = dir_ord[i%4].X;
		f32 y = dir_ord[i%4].Y;

		v2 ToCorner = Hadamard(v2{x, y}, Entity->Low->Dim.XY) * 0.5f;
		ps[i] = cw * EntD * ToCorner.X + 
			           EntD * ToCorner.Y;
	}

	s32 SecondEdgeIndex;
	if(Dot(ColEntD, EntD) > 0)
	{
		if(Cross((v3)ColEntD, (v3)EntD).Z > 0)
		{
			SecondEdgeIndex = 4;
		}
		else
		{
			SecondEdgeIndex = 5;
		}
	}
	else
	{
		if(Cross((v3)ColEntD, (v3)EntD).Z > 0)
		{
			SecondEdgeIndex = 7;
		}
		else
		{
			SecondEdgeIndex = 6;
		}
	}

	v2 EdgeStart = ColEntP + ps[0] + ps[SecondEdgeIndex];
	for(int i = 0; i < 4; i++)
	{
		{
			s32 j = 0 + i;
			v2 diff = (ps[(j+1)%4 + 0] - 
								 ps[(j+0)%4 + 0]);

			v2 p0 = EdgeStart;
			v2 p1 = EdgeStart + diff;

			intersection_result Test = GetTimeOfIntersectionWithLineSegment(EntP, DeltaP, p0, p1);
			if(Test.Valid && (Test.t >= 0.0f) && (Test.t <= 1.0f))
			{
				Result.Hit = true;
				Result.TimeOfImpact = Test.t;
				v2 n = cw * diff;
				if(Dot(DeltaP, n) > 0) { n *= -1; } 
				Result.Normal = Normalize(n);
				break;
			}

			EdgeStart = p1;
		}
		{
			s32 j = SecondEdgeIndex + i;
			v2 diff = (ps[(j+1)%4 + 4] - 
								 ps[(j+0)%4 + 4]);

			v2 p0 = EdgeStart;
			v2 p1 = EdgeStart + diff;

			intersection_result Test = GetTimeOfIntersectionWithLineSegment(EntP, DeltaP, p0, p1);
			if(Test.Valid && (0.0f <= Test.t ) && (Test.t <= 1.0f))
			{
				Result.Hit = true;
				Result.TimeOfImpact = Test.t;
				v2 n = cw * diff;
				if(Dot(DeltaP, n) > 0) { n *= -1; } 
				Result.Normal = Normalize(n);
				break;
			}

			EdgeStart = p1;
		}
	}

	f32 tEpsilon = 0.1f;             
	Result.TimeOfImpact = Max(0.0f, Result.TimeOfImpact - tEpsilon);
	return Result;
}

#if HANDMADE_INTERNAL
internal_function void
DEBUGMinkowskiSum(game_offscreen_buffer* Buffer, entity* A, entity* B, 
									m22 GameSpaceToScreenSpace, v2 ScreenCenter)
{
	v2 ad = A->High->D.XY;
	v2 bd = B->High->D.XY;

	v2 adim = A->Low->Dim.XY;
	v2 bdim = B->Low->Dim.XY;

	v2 ps[8] = {};
	v2 dir_ord[4] = {{-1, 1}, {1, 1}, {1, -1}, {-1, -1}};
	m22 cw = { 0,-1,
						 1, 0};
	for(int i = 0; i < 4; i++)
	{
		f32 x = dir_ord[i%4].X;
		f32 y = dir_ord[i%4].Y;

		v2 ToCorner = Hadamard(v2{x, y}, adim) * 0.5f;
		ps[i] = cw * ad * ToCorner.X + 
			           ad * ToCorner.Y;
	}
	for(int i = 4; i < 8; i++)
	{
		f32 x = dir_ord[i%4].X;
		f32 y = dir_ord[i%4].Y;

		v2 ToCorner = Hadamard(v2{x, y}, bdim) * 0.5f;
		ps[i] = cw * bd * ToCorner.X + 
			           bd * ToCorner.Y;
	}

	s32 asi = 0;
	s32 bsi;
	if(Dot(ad, bd) > 0)
	{
		if(Cross((v3)ad, (v3)bd).Z > 0)
		{
			bsi = 4;
		}
		else
		{
			bsi = 5;
		}
	}
	else
	{
		if(Cross((v3)ad, (v3)bd).Z > 0)
		{
			bsi = 7;
		}
		else
		{
			bsi = 6;
		}
	}

	{
		v2 EdgeStart = A->High->P.XY + ps[asi] + ps[bsi];
		for(int i = 0; i < 4; i++)
		{
			{
				s32 j = 0 + i;
				v2 diff = (ps[(j+1)%4 + 0] - 
									 ps[(j+0)%4 + 0]);

				v2 p0 = EdgeStart;
				v2 p1 = EdgeStart + diff;

				DrawLine(Buffer, 
								 ScreenCenter + GameSpaceToScreenSpace * p0, 
								 ScreenCenter + GameSpaceToScreenSpace * p1, {0, 0, 1});

				EdgeStart = p1;
			}
			{
				s32 j = bsi + i;
				v2 diff = (ps[(j+1)%4 + 4] - 
									 ps[(j+0)%4 + 4]);

				v2 p0 = EdgeStart;
				v2 p1 = EdgeStart + diff;

				DrawLine(Buffer, 
								 ScreenCenter + GameSpaceToScreenSpace * p0, 
								 ScreenCenter + GameSpaceToScreenSpace * p1, {0, 0, 1});

				EdgeStart = p1;
			}
		}
	}
}
#endif

	internal_function void
MoveEntity(game_state* GameState, entity Entity, f32 SecondsToUpdate, 
					 world_map* WorldMap, world_map_position CameraP)
{
	entities* Entities = &GameState->Entities;

	v2 ddP = Entity.High->ddP.XY;
	//TODO(casey): ODE here!
	ddP += Entity.Low->DecelerationFactor * Entity.High->dP.XY;

	v2 P = Entity.High->P.XY;
	v2 Delta = (0.5f * ddP * Square(SecondsToUpdate) + (Entity.High->dP.XY * SecondsToUpdate));

	//
	// NOTE(bjorn): Collision check after movement
	//
	for(s32 Steps = 3;
			Steps > 0;
			Steps--)
	{
		v2 WallNormal = {};
		f32 BestTime = 1.0f;
		entity HitEntity = {};
		b32 HitDetected = false;
		b32 CollidedFromTheInside = false;

		for(LoopOverHighEntitiesNamed(CollisionEntity))
		{
			if(CollisionEntity.Low->Collides && 
				 Entity.Low->HighEntityIndex != CollisionEntity.Low->HighEntityIndex)
			{
				//TODO(bjorn): Funky stuff will happen if I don't deal with inside
				//outside stuff. For example; if the object is small enough and if the
				//player travel fast enough he might hit the inner wall instead of the
				//outer wall.
				collision_result Test = MinkowskiSumCollision(&CollisionEntity, &Entity, Delta);

				if(Test.Hit && Test.TimeOfImpact < BestTime)
				{
					HitDetected = Test.Hit;

					HitEntity = CollisionEntity;

					CollidedFromTheInside = false;
					BestTime = Test.TimeOfImpact;
					WallNormal = Test.Normal;
				}
#if 0
				v2 BottomLeftEdge = CollisionEntity.High->P.XY - 
					(CollisionEntity.Low->Dim.XY + Entity.Low->Dim.XY) * 0.5f;

				v2 TopRightEdge = CollisionEntity.High->P.XY + 
					(CollisionEntity.Low->Dim.XY + Entity.Low->Dim.XY) * 0.5f;

				b32 RightWallTest  = TestWall({1,0}, P, Delta, TopRightEdge.X, 
																			BottomLeftEdge, TopRightEdge, &BestTime);
				b32 LeftWallTest   = TestWall({1,0}, P, Delta, BottomLeftEdge.X, 
																			BottomLeftEdge, TopRightEdge, &BestTime);
				b32 TopWallTest    = TestWall({0,1}, P, Delta, TopRightEdge.Y, 
																			BottomLeftEdge, TopRightEdge, &BestTime);
				b32 BottomWallTest = TestWall({0,1}, P, Delta, BottomLeftEdge.Y, 
																			BottomLeftEdge, TopRightEdge, &BestTime);

				if(RightWallTest || LeftWallTest || TopWallTest || BottomWallTest)
				{
					HitDetected = true;
					HitEntity = CollisionEntity;

					CollidedFromTheInside = IsInRectangle(RectMinMax(BottomLeftEdge, TopRightEdge), P);
				}

				if(RightWallTest)  { WallNormal = { 1, 0}; }
				if(LeftWallTest)   { WallNormal = {-1, 0}; }
				if(TopWallTest)    { WallNormal = { 0, 1}; }
				if(BottomWallTest) { WallNormal = { 0,-1}; }
#endif
			}
		}

		if(BestTime >= 1.0f && !HitDetected) 
		{ 
			P += Delta;
			break; 
		}

		if(HitDetected)
		{
			if(HitEntity.Low->Type == EntityType_Stair)
			{
				P += Delta;
				if(!CollidedFromTheInside)
				{
					Entity.High->P.Z += HitEntity.Low->Stair.dZ;
				}
				break; 
			}
			else
			{
				P += BestTime * Delta;
				Delta *= (1.0f - BestTime);
				Delta              -= Dot(Delta,					    WallNormal) * WallNormal;
				Entity.High->dP.XY -= Dot(Entity.High->dP.XY, WallNormal) * WallNormal;
			}
		}
	}

	Entity.High->P.XY = P;
	Entity.High->dP.XY = (ddP * SecondsToUpdate) + Entity.High->dP.XY;

	if(Entity.High->dP.X != 0.0f && Entity.High->dP.Y != 0.0f)
	{
		if(Absolute(Entity.High->dP.X) > Absolute(Entity.High->dP.Y))
		{
			if(Entity.High->dP.X > 0)
			{
				Entity.High->Player.FacingDirection = 3;
			}
			else
			{
				Entity.High->Player.FacingDirection = 1;
			}
		}
		else
		{
			if(Entity.High->dP.Y > 0)
			{
				Entity.High->Player.FacingDirection = 2;
			}
			else
			{
				Entity.High->Player.FacingDirection = 0;
			}
		}
	}

	OffsetAndChangeEntityLocation(&GameState->WorldArena, GameState->WorldMap, &Entity, 
																CameraP, Entity.High->P);
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
SetCamera(world_map* WorldMap, entities* Entities, 
					world_map_position* CameraP, world_map_position NewCameraP)
{
	Assert(ValidateEntityPairs(Entities));

	*CameraP = NewCameraP;

	//TODO(bjorn): Think more about render distances.
	v3 HighFrequencyUpdateDim = v3{2.0f, 2.0f, 1.0f/TILES_PER_CHUNK}*WorldMap->ChunkSideInMeters;

	rectangle3 CameraUpdateBounds = RectCenterDim(v3{0,0,0}, HighFrequencyUpdateDim);

	world_map_position MinWorldP = OffsetWorldPos(WorldMap, NewCameraP, CameraUpdateBounds.Min);
	world_map_position MaxWorldP = OffsetWorldPos(WorldMap, NewCameraP, CameraUpdateBounds.Max);

	rectangle3s CameraUpdateAbsBounds = RectMinMax(MinWorldP.ChunkP, MaxWorldP.ChunkP);

	for(s32 Z = MinWorldP.ChunkP.Z; 
			Z <= MaxWorldP.ChunkP.Z; 
			Z++)
	{
		for(s32 Y = MinWorldP.ChunkP.Y; 
				Y <= MaxWorldP.ChunkP.Y; 
				Y++)
		{
			for(s32 X = MinWorldP.ChunkP.X; 
					X <= MaxWorldP.ChunkP.X; 
					X++)
			{
				world_chunk* Chunk = GetWorldChunk(WorldMap, v3s{X,Y,Z});
				if(Chunk)
				{
					for(entity_block* Block = Chunk->Block;
							Block;
							Block = Block->Next)
					{
						for(u32 Index = 0;
								Index < Block->EntityIndexCount;
								Index++)
						{
							entity Entity = GetEntityByLowIndex(Entities, Block->EntityIndexes[Index]);
							Assert(Entity.Low);

							v3 RelCamP = GetWorldMapPosDifference(WorldMap, Entity.Low->WorldP, NewCameraP);

							if(Entity.High)
							{
								Entity.High->P = RelCamP;

								if(!IsInRectangle(CameraUpdateBounds, Entity.High->P))
								{
									world_map_position WorldP = 
										OffsetWorldPos(WorldMap, NewCameraP, Entity.High->P);
									MapEntityIntoLow(Entities, Entity.LowEntityIndex, WorldP);
								}
							}
							else
							{
								if(IsInRectangle(CameraUpdateBounds, RelCamP))
								{
									MapEntityIntoHigh(Entities, Entity.LowEntityIndex, RelCamP);
								}
							}
						}
					}
				}
			}
		}
	}

	Assert(ValidateEntityPairs(Entities));
}

internal_function void
AlignWheelsForward(entities* Entities, entity* CarFrameEntity, f32 SecondsToUpdate)
{
		entity LeftFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->CarFrame.Wheels[2]);
		entity RightFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->CarFrame.Wheels[3]);

		Assert(LeftFrontWheel.High);
		Assert(RightFrontWheel.High);

		f32 TurnRate = Lenght(CarFrameEntity->High->dP) * 0.05f;
		v3 CarDir = CarFrameEntity->High->D;
		v3 WheelDir = LeftFrontWheel.High->D;

		f32 S = Distance(CarDir, WheelDir);
		f32 M = SecondsToUpdate * TurnRate;

		v3 NewDir; 
		if(S <= M)
		{
			NewDir = CarDir;
		}
		else
		{
			NewDir = WheelDir + Normalize(CarDir - WheelDir) * M;
		}

		LeftFrontWheel.High->D = NewDir;
		RightFrontWheel.High->D = NewDir;
}

internal_function void
TurnWheels(entities* Entities, entity* CarFrameEntity, v2 InputDirection, f32 SecondsToUpdate)
{
	if(InputDirection.X == 0.0f) return;

	Assert(CarFrameEntity->High);
	if(CarFrameEntity->High)
	{
		f32 TurnRate = Lenght(CarFrameEntity->High->dP) * 0.15f;
		TurnRate = Clamp(TurnRate, 0.5f, 3.0f); 

		f32 Deg90 = tau32 * 0.25f;
	  m22 Rot90CW = { 0,-1,
		                1, 0};

		entity LeftFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->CarFrame.Wheels[2]);
		entity RightFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->CarFrame.Wheels[3]);

		Assert(LeftFrontWheel.High);
		Assert(RightFrontWheel.High);
		
		v2 CarD = CarFrameEntity->High->D.XY;
		v2 TD = Rot90CW * CarD;

		v2 NewD = Normalize(LeftFrontWheel.High->D.XY + 
												TD * -InputDirection.X * TurnRate * SecondsToUpdate);

		f32 MaxDeg = pi32 * (0.25f - Lenght(CarFrameEntity->High->dP) * 0.01f);
		MaxDeg = Clamp(MaxDeg, pi32 * 0.10f, pi32 * 0.25f); 

		b32 RotatedMoreThanXDegrees = Dot(NewD, CarD) < Cos(MaxDeg);
		if(RotatedMoreThanXDegrees)
		{
			if(InputDirection.X < 0.0f)
			{
			  m22 RotCCW  = { Cos(MaxDeg),-Sin(MaxDeg),
			  	              Sin(MaxDeg), Cos(MaxDeg)};
				NewD = RotCCW * CarD;
			}
			else
			{
			  m22 RotCW = { Cos(MaxDeg), Sin(MaxDeg),
			  	           -Sin(MaxDeg), Cos(MaxDeg)};
				NewD = RotCW * CarD;
			}
			NewD = Normalize(NewD);
		}

		Assert(LenghtSquared(NewD) <= 1.001f);
		Assert(LenghtSquared(NewD) >= 0.999f);

		LeftFrontWheel.High->D = NewD;
		RightFrontWheel.High->D = NewD;
	}
}

internal_function v2
GetCoordinatesRelativeTransform(v2 Origo, v2 Y, v2 A)
{
	v2 Result;

	m22 Rot90CW = {0,-1,
								 1, 0};
	v2 X = Rot90CW * Y;

	Result = v2{Dot(A - Origo, X), Dot(A - Origo, Y)};

	return Result;
}

internal_function v2
GetGlobalPosFromRelativeCoordinates(v2 Origo, v2 Y, v2 A)
{
	v2 Result;

	m22 Rot90CW = {0,-1,
								 1, 0};
	v2 X = Rot90CW * Y;

	Result = Origo + X * A.X + Y * A.Y;

	return Result;
}

	internal_function void
UpdateCarPos(memory_arena* WorldArena, world_map* WorldMap, entities* Entities, 
						 entity* CarFrame, world_map_position* CameraP, v2 NewP, v2 NewD)
{
	v2 OldP = CarFrame->High->P.XY;
	v2 OldD = CarFrame->High->D.XY;

	CarFrame->High->D = NewD;
	CarFrame->High->P = NewP;

	OffsetAndChangeEntityLocation(WorldArena, WorldMap, CarFrame, *CameraP, (v2)NewP);

	entity EngineEntity = GetEntityByLowIndex(Entities, CarFrame->Low->CarFrame.Engine);
	if(EngineEntity.High)
	{
		v2 RelPos = GetCoordinatesRelativeTransform(OldP, OldD, EngineEntity.High->P.XY);
		v2 RelDir = GetCoordinatesRelativeTransform(OldP, OldD, EngineEntity.High->P.XY + 
																								EngineEntity.High->D.XY);
		v2 NewPos = GetGlobalPosFromRelativeCoordinates(NewP, NewD, RelPos);
		v2 NewDir = GetGlobalPosFromRelativeCoordinates(NewP, NewD, RelDir) - NewPos;

		EngineEntity.High->P = NewPos;
		EngineEntity.High->D = Normalize(NewDir);
		OffsetAndChangeEntityLocation(WorldArena, WorldMap, &EngineEntity, *CameraP, (v3)NewPos);
	}

	entity DriverEntity = GetEntityByLowIndex(Entities, CarFrame->Low->CarFrame.DriverSeat);
	if(DriverEntity.High)
	{
		v2 RelPos = GetCoordinatesRelativeTransform(OldP, OldD, DriverEntity.High->P.XY);
		v2 RelDir = GetCoordinatesRelativeTransform(OldP, OldD, DriverEntity.High->P.XY + 
																								DriverEntity.High->D.XY);
		v2 NewPos = GetGlobalPosFromRelativeCoordinates(NewP, NewD, RelPos);
		v2 NewDir = GetGlobalPosFromRelativeCoordinates(NewP, NewD, RelDir) - NewPos;

		DriverEntity.High->P = NewPos;
		//DriverEntity.High->D = Normalize(NewDir);
		OffsetAndChangeEntityLocation(WorldArena, WorldMap, &DriverEntity, *CameraP, (v3)NewPos);
	}

	for(s32 WheelIndex = 0;
			WheelIndex < 4;
			WheelIndex++)
	{
		entity WheelEntity = GetEntityByLowIndex(Entities, CarFrame->Low->CarFrame.Wheels[WheelIndex]);
		if(WheelEntity.High)
		{
			v2 RelPos = GetCoordinatesRelativeTransform(OldP, OldD, WheelEntity.High->P.XY);
			v2 RelDir = GetCoordinatesRelativeTransform(OldP, OldD, WheelEntity.High->P.XY + 
																									WheelEntity.High->D.XY);
			v2 NewPos = GetGlobalPosFromRelativeCoordinates(NewP, NewD, RelPos);
			v2 NewDir = GetGlobalPosFromRelativeCoordinates(NewP, NewD, RelDir) - NewPos;

			WheelEntity.High->P = NewPos;
			WheelEntity.High->D = Normalize(NewDir);
			OffsetAndChangeEntityLocation(WorldArena, WorldMap, &WheelEntity, *CameraP, (v3)NewPos);
		}
	}
}

	internal_function void
PropelCar(memory_arena* WorldArena, world_map* WorldMap, entities* Entities, 
					entity* CarFrame, world_map_position* CameraP, f32 SecondsToUpdate)
{
	Assert(CarFrame->High);
	if(CarFrame->High)
	{
		entity LeftFrontWheel = GetEntityByLowIndex(Entities, CarFrame->Low->CarFrame.Wheels[2]);
		entity RightFrontWheel = GetEntityByLowIndex(Entities, CarFrame->Low->CarFrame.Wheels[3]);

		v3 ddP = CarFrame->High->ddP;
		ddP += CarFrame->Low->DecelerationFactor * CarFrame->High->dP;

		v3 DeltaP = (0.5f * ddP * Square(SecondsToUpdate) + 
								 (CarFrame->High->dP * SecondsToUpdate));
		CarFrame->High->dP += (ddP * SecondsToUpdate);

		v3 OldFrontP = CarFrame->High->P + CarFrame->High->D * CarFrame->Low->Dim.Y * 0.5f;
		v3 NewFrontP;
		if(LeftFrontWheel.High)
		{
			NewFrontP = OldFrontP + LeftFrontWheel.High->D * Lenght(DeltaP);
		}
		else if(LeftFrontWheel.High)
		{
			NewFrontP = OldFrontP + LeftFrontWheel.High->D * Lenght(DeltaP);
		}
		else
		{
			NewFrontP = OldFrontP + CarFrame->High->D * Lenght(DeltaP);
		}

		v3 OldP = CarFrame->High->P;
		v3 NewD = Normalize(NewFrontP - OldP);
		v3 NewP = NewFrontP - NewD * Distance(OldP, OldFrontP);

		CarFrame->High->dP = NewD * Lenght(CarFrame->High->dP);
		CarFrame->High->ddP = NewD * Lenght(CarFrame->High->ddP);

		if(Lenght(CarFrame->High->ddP) < 0.1f && Lenght(CarFrame->High->dP) < 0.4f)
		{
			CarFrame->High->dP = {};
		}

		UpdateCarPos(WorldArena, WorldMap, Entities, CarFrame, CameraP, NewP.XY, NewD.XY);
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

	memory_arena* WorldArena = &GameState->WorldArena;
	world_map* WorldMap = GameState->WorldMap;
	entities* Entities = &GameState->Entities;

	for(s32 ControllerIndex = 0;
			ControllerIndex < ArrayCount(Input->Controllers);
			ControllerIndex++)
	{
		game_controller* Controller = GetController(Input, ControllerIndex);
		if(Controller->IsConnected)
		{
			u32 ControlledEntityIndex = GameState->PlayerIndexForController[ControllerIndex];
			entity ControlledEntity = GetEntityByLowIndex(Entities, ControlledEntityIndex);

			if(ControlledEntity.Low)
			{
				v2 InputDirection = {};

				InputDirection = Controller->LeftStick.End;

				if(ControlledEntity.High)
				{
					ControlledEntity.High->ddP = InputDirection * ControlledEntity.Low->AccelerationFactor;
				}

				if(Clicked(Controller, Start))
				{
					//TODO(bjorn) Implement RemoveEntity();
					//GameState->EntityResidencies[ControlledEntityIndex] = EntityResidence_Nonexistent;
					//GameState->PlayerIndexForController[ControllerIndex] = 0;
				}
			}
			else
			{
				if(Clicked(Controller, Start))
				{
					GameState->PlayerIndexForController[ControllerIndex] = 
						AddPlayer(GameState).LowEntityIndex;
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
			entity ControlledEntity = GetEntityByLowIndex(Entities, ControlledEntityIndex);

			if(ControlledEntity.Low)
			{
				v2 InputDirection = {};

				if(Held(Keyboard, S))
				{
					InputDirection.Y += -1;
				}
				if(Held(Keyboard, A))
				{
					InputDirection.X += -1;
				}
				if(Held(Keyboard, W))
				{
					InputDirection.Y += 1;
				}
				if(Held(Keyboard, D))
				{
					InputDirection.X += 1;
				}

				if(InputDirection.X && InputDirection.Y)
				{
					InputDirection *= invroot2;
				}

				if(ControlledEntity.High)
				{
					if(ControlledEntity.Low->Type == EntityType_Player)
					{
						if(Clicked(Keyboard, E))
						{
							if(ControlledEntity.Low->Player.RidingVehicle)
							{
								entity Vehicle = GetEntityByLowIndex(Entities, 
																										 ControlledEntity.Low->Player.RidingVehicle);
								DismountEntityFromCar(WorldArena, WorldMap, &ControlledEntity, &Vehicle);
							}
							else
							{
								for(LoopOverHighEntities)
								{
									if(Entity.Low->Type == EntityType_CarFrame && 
										 Distance(Entity.High->P, ControlledEntity.High->P) < 4.0f)
									{
										MountEntityOnCar(WorldArena, WorldMap, &ControlledEntity, &Entity);
										break;
									}
								}
							}
						}
						if(ControlledEntity.Low->Player.RidingVehicle)
						{
							entity CarFrame = 
								GetEntityByLowIndex(Entities, ControlledEntity.Low->Player.RidingVehicle);
							Assert(CarFrame.Low->Type == EntityType_CarFrame);

							if(InputDirection.X)
							{ 
								TurnWheels(Entities, &CarFrame, InputDirection, SecondsToUpdate); 
							}
							else
							{ 
								AlignWheelsForward(Entities, &CarFrame, SecondsToUpdate); 
							}

							if(Clicked(Keyboard, Space))
							{ 
								CarFrame.High->ddP = CarFrame.High->D * (Lenght(CarFrame.High->ddP) + 3.0f);
								if(Lenght(CarFrame.High->ddP) > 11.0f)
								{
									CarFrame.High->ddP = {}; 
								}
							}
						}
						else
						{
							ControlledEntity.High->ddP = 
								InputDirection * ControlledEntity.Low->AccelerationFactor;
						}
					}
				}
			}
		}
	}

	//
	// NOTE(bjorn): Movement
	//
	for(LoopOverHighEntities)
	{
		if(Entity.Low->Type == EntityType_CarFrame)
		{
			PropelCar(WorldArena, WorldMap, Entities, &Entity, &GameState->CameraP, SecondsToUpdate);
		}
		else if(Entity.Low->Type == EntityType_Player && !Entity.Low->Player.RidingVehicle)
		{
			MoveEntity(GameState, Entity, SecondsToUpdate, WorldMap, GameState->CameraP);
		}
	}

	//
	// NOTE(bjorn): Update camera
	//
	entity CameraTarget = GetEntityByLowIndex(Entities, GameState->CameraFollowingPlayerIndex);
	if(CameraTarget.Low)
	{
#if 0
		world_map_position NewCameraP = GetCenterOfRoom(WorldMap, 
																										GameState->RoomOrigin,
																										CameraTarget.Low->WorldP,
																										GameState->RoomWidthInTiles,
																										GameState->RoomHeightInTiles);
		SetCamera(GameState, NewCameraP);
#else
		SetCamera(WorldMap, Entities, &GameState->CameraP, CameraTarget.Low->WorldP);
#endif
	}

	//
	// NOTE(bjorn): Rendering
	//
	s32 TileSideInPixels = 60;
	f32 PixelsPerMeter = (f32)TileSideInPixels / WorldMap->TileSideInMeters;

	DrawRectangle(Buffer, 
								RectMinMax(v2{0.0f, 0.0f}, v2{(f32)Buffer->Width, (f32)Buffer->Height}), 
								{0.5f, 0.5f, 0.5f});

#if 0
	DrawBitmap(Buffer, &GameState->Backdrop, {-40.0f, -40.0f}, 
						 {(f32)GameState->Backdrop.Width, (f32)GameState->Backdrop.Height});
#endif

	v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

	for(LoopOverHighEntities)
	{
		v2 CollisionMarkerPixelDim = Hadamard(Entity.Low->Dim.XY, {PixelsPerMeter, PixelsPerMeter});
		m22 GameSpaceToScreenSpace = 
		{PixelsPerMeter, 0             ,
		 0             ,-PixelsPerMeter};

		v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * Entity.High->P.XY;
		//TODO(bjorn)Add z axis jump.
		v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;

		if(Entity.Low->Type == EntityType_Stair)
		{
			v3 StairColor = {};
			if(Entity.Low->Stair.dZ == 1)
			{
				v3 LightGreen = {0.5f, 1, 0.5f};
				StairColor = LightGreen;
			}
			if(Entity.Low->Stair.dZ == -1)
			{
				v3 LightRed = {1, 0.5f, 0.5f};
				StairColor = LightRed;
			}
			DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), StairColor);
		}
		if(Entity.Low->Type == EntityType_Wall)
		{
#if 0
			v3 LightYellow = {0.5f, 0.5f, 0.0f};
			DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f), LightYellow);
#endif
			DrawBitmap(Buffer, &GameState->Rock, EntityPixelPos - GameState->Rock.Alignment, 
								 (v2)GameState->Rock.Dim);
#if 0
			u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[0];
			entity Player = GetEntityByLowIndex(Entities, ControlledEntityIndex);
			DEBUGMinkowskiSum(Buffer, &Entity, &Player, GameSpaceToScreenSpace, ScreenCenter);
#endif
		}
		if(Entity.Low->Type == EntityType_Ground)
		{
#if 1
			DrawBitmap(Buffer, &GameState->Dirt, EntityPixelPos - GameState->Dirt.Alignment, 
								 (v2)GameState->Dirt.Dim * 1.2f);
#else
			v3 LightBrown = {0.55f, 0.45f, 0.33f};
			DrawRectangle(Buffer, 
										RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f), LightBrown);
#endif
		}
		if(Entity.Low->Type == EntityType_Wheel ||
			 Entity.Low->Type == EntityType_CarFrame ||
			 Entity.Low->Type == EntityType_Engine)
		{
			v3 Color = {1, 1, 1};
			if(Entity.Low->Type == EntityType_Engine) { Color = {0, 1, 0}; }
			if(Entity.Low->Type == EntityType_Wheel) { Color = {0.2f, 0.2f, 0.2f}; }

			DrawFrame(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), 
								Entity.High->D.XY, Color);
			DrawLine(Buffer, EntityPixelPos, EntityPixelPos + 
							 Hadamard(Entity.High->D.XY, v2{1, -1}) * 40.0f, {1, 0, 0});

#if 0
			u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[0];
			entity Player = GetEntityByLowIndex(Entities, ControlledEntityIndex);
			DEBUGMinkowskiSum(Buffer, &Entity, &Player, GameSpaceToScreenSpace, ScreenCenter);
#endif
#if 0
			DrawBitmap(Buffer, &GameState->Dirt, EntityPixelPos - GameState->Dirt.Alignment, 
								 (v2)GameState->Dirt.Dim * 1.2f);
#endif
		}
		if(Entity.Low->Type == EntityType_Player)
		{
			v2 PlayerPixelDim = Hadamard({0.75f, 1.0f}, 
																	 (v2)v2s{TileSideInPixels, TileSideInPixels});

			if(Entity.Low->WorldP.ChunkP.Z == GameState->CameraP.ChunkP.Z)
			{
				v3 Yellow = {1.0f, 1.0f, 0.0f};
				DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

				hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->Player.FacingDirection]);

				DrawBitmap(Buffer, &Hero->Torso, EntityPixelPos - Hero->Torso.Alignment, 
									 (v2)Hero->Torso.Dim);
				DrawBitmap(Buffer, &Hero->Cape, EntityPixelPos - Hero->Cape.Alignment, 
									 (v2)Hero->Cape.Dim);
				DrawBitmap(Buffer, &Hero->Head, EntityPixelPos - Hero->Head.Alignment, 
									 (v2)Hero->Head.Dim);
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
