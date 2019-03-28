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
// Make it so that I can visually step through a frame of collision.
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
	loaded_bitmap Shadow;
	loaded_bitmap Sword;

	hero_bitmaps HeroBitmaps[4];

#if HANDMADE_INTERNAL
	b32 DEBUG_VisualiseMinkowskiSum;
	b32 DEBUG_VisualiseCollisionBox;

	b32 DEBUG_StepThroughTheCollisionLoop;
	b32 DEBUG_CollisionLoopAdvance;

	u32 DEBUG_CollisionLoopEntityIndex;
	s32 DEBUG_CollisionLoopStepIndex;
	v3 DEBUG_CollisionLoopEstimatedPos;
#endif

	f32 GroundStaticFriction;
	f32 GroundDynamicFriction;

	f32 NoteTone;
	f32 NoteDuration;
	f32 NoteSecondsPassed;
};

internal_function void
MoveEntity(memory_arena* WorldArena, world_map* WorldMap, world_map_position* CameraP, 
					 entity* Entity, f32 dT)
{
	v3 P = Entity->High->P;
	v3 dP = Entity->High->dP;
	v3 ddP = {};

	v3 R = Entity->Low->R;
	f32 A = Entity->High->A;
	f32 dA = Entity->High->dA;
	f32 ddA = 0;

	//TODO(bjorn): Re-add the car.
	if(Entity->Low->MoveSpec.EnforceVerticalGravity)
	{
		if(Entity->High->MovingDirection.Z > 0 &&
			 P.Z == 0)
		{
			dP.Z = 18.0f * 0.5f;
		}
		if(P.Z > 0)
		{
			//TODO(bjorn): Why do i need the gravity to be so heavy? Because of upscaling?
			ddP.Z = -9.82f * 10.0f;
		}
	}
	if(Entity->Low->MoveSpec.EnforceHorizontalMovement)
	{
		//TODO(casey): ODE here!
		ddP = Entity->High->MovingDirection * Entity->Low->MoveSpec.Speed;
	}

	ddP.XY -= Entity->Low->MoveSpec.Drag * dP.XY;
	ddA -= Entity->Low->MoveSpec.Drag * 0.7f * dA;

	P += 0.5f * ddP * Square(dT) + dP * dT;
	dP += ddP * dT;

	A += 0.5f * ddA * Square(dT) + dA * dT;
	dA += ddA * dT;
	if(A > tau32)
	{
		A -= FloorF32ToS32(A / tau32) * tau32;
	}
	else if(A < 0)
	{
		A += RoofF32ToS32(Absolute(A) / tau32) * tau32;
	}
	R.XY = CCWM22(A) * DefaultEntityOrientation().XY;
	R = Normalize(R);

	//TODO(bjorn): Think harder about how to implement the ground.
	if(P.Z < 0.0f)
	{
		P.Z = 0.0f;
		dP.Z = 0.0f;
	}

	Entity->High->ddP = ddP;
	Entity->High->dP = dP;
	Entity->High->P = P;

	if(Entity->Low->MoveSpec.AllowRotation)
	{
		Entity->High->ddA = ddA;
		Entity->High->dA = dA;
		Entity->High->A = A;
		Entity->Low->R = R;
	}

	ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, Entity, 
																				 *CameraP, Entity->High->P);
}

	internal_function void
UpdateFamiliarPairwise(entity* Familiar, entity* Target)
{
	if(Target->Low->Type == EntityType_Player)
	{
		f32 DistanceToPlayerSquared = LenghtSquared(Target->High->P - Familiar->High->P);
		if(DistanceToPlayerSquared < Familiar->High->BestDistanceToPlayerSquared &&
			 DistanceToPlayerSquared > Square(2.0f))
		{
			Familiar->High->BestDistanceToPlayerSquared = DistanceToPlayerSquared;
			Familiar->High->MovingDirection = Normalize(Target->High->P - Familiar->High->P).XY;
		}
	}
}

	internal_function void
UpdateSwordPairwise(entity* Sword, entity* Target, b32 Intersect,
										memory_arena* WorldArena, world_map* WorldMap, entities* Entities)
{
	if(Intersect &&
		 Target->Low->Type != EntityType_Player)
	{
		//TODO(bjorn): Is it safe to let high entities just disappear in the middle of a loop?
		//TODO(bjorn): Also, the fact that this needs to be done is
		//non-obvious. As is the fact that calling SetCamera doesn't work as
		//well.
		ChangeEntityWorldLocation(WorldArena, WorldMap, Sword, 0);
		MapEntityOutFromHigh(Entities, Sword->LowIndex, Sword->Low->WorldP);
	}
}

	internal_function void
AddHitPoints(entity* Entity, u32 HitPointMax)
{
	Assert(HitPointMax <= ArrayCount(Entity->Low->HitPoints));
	Entity->Low->HitPointMax = HitPointMax;
	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity->Low->HitPointMax;
			HitPointIndex++)
	{
		Entity->Low->HitPoints[HitPointIndex].FilledAmount = HIT_POINT_SUB_COUNT;
	}
}

	internal_function entity
AddSword(game_state* GameState)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, 
														&GameState->Entities, EntityType_Sword);

	Entity.Low->Dim = v2{0.4f, 1.5f} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Mass = 8.0f;

	Entity.Low->MoveSpec.Drag = 0.0f;
	Entity.Low->Collides = false;

	return Entity;
}

	internal_function entity
AddPlayer(game_state* GameState)
{
	world_map_position InitP = OffsetWorldPos(GameState->WorldMap, GameState->CameraP, 
																						GetChunkPosFromAbsTile(GameState->WorldMap, 
																													 v3s{-2, 1, 0}).Offset_);

	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Player, &InitP);

	Entity.Low->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;

	//TODO(bjorn): Why does weight differences matter so much in the collision system.
	Entity.Low->Mass = 40.0f / 8.0f;

	Entity.Low->MoveSpec.EnforceVerticalGravity = true;
	Entity.Low->MoveSpec.EnforceHorizontalMovement = true;
	Entity.Low->MoveSpec.Speed = 85.f;
	Entity.Low->MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(&Entity, 6);
	entity Sword = AddSword(GameState);
	Entity.Low->Sword = Sword.LowIndex;

	entity Test = GetEntityByLowIndex(&GameState->Entities, GameState->CameraFollowingPlayerIndex);
	if(!Test.Low)
	{
		GameState->CameraFollowingPlayerIndex = Entity.LowIndex;
	}

	return Entity;
}

	internal_function entity
AddMonstar(game_state* GameState, world_map_position InitP)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Monstar, &InitP);

	Entity.Low->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;

	Entity.Low->Mass = 40.0f / 8.0f;

	Entity.Low->MoveSpec.EnforceHorizontalMovement = true;
	Entity.Low->MoveSpec.Speed = 85.f * 0.75;
	Entity.Low->MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(&Entity, 3);

	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity.Low->HitPointMax;
			HitPointIndex++)
	{
		Entity.Low->HitPoints[HitPointIndex].FilledAmount = (u8)(HIT_POINT_SUB_COUNT - HitPointIndex);
	}

	return Entity;
}

	internal_function entity
AddFamiliar(game_state* GameState, world_map_position InitP)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Familiar, &InitP);

	Entity.Low->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;

	Entity.Low->Mass = 40.0f / 8.0f;

	Entity.Low->MoveSpec.EnforceHorizontalMovement = true;
	Entity.Low->MoveSpec.Speed = 85.f * 0.5f;
	Entity.Low->MoveSpec.Drag = 0.2f * 30.0f;

	return Entity;
}

	internal_function entity
AddCarFrame(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityType_CarFrame, &WorldPos);

	Entity.Low->Dim = v2{4, 6};
	Entity.Low->Collides = true;

	Entity.Low->MoveSpec = DefaultMoveSpec();

	Entity.Low->Mass = 800.0f;

	return Entity;
}

	internal_function entity
AddEngine(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 	EntityType_Engine, &WorldPos);

	Entity.Low->Dim = v2{3, 2};
	Entity.Low->MoveSpec = DefaultMoveSpec();

	return Entity;
}

	internal_function entity
AddWheel(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 	EntityType_Wheel, &WorldPos);

	Entity.Low->Dim = v2{0.6f, 1.5f};
	Entity.Low->MoveSpec = DefaultMoveSpec();

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
		entity EngineEntity = GetEntityByLowIndex(Entities, CarFrameEntity.Low->Engine);
		if(EngineEntity.Low)
		{
			ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, &EngineEntity, 
																						 CarFrameEntity.Low->WorldP, v3{0, 1.5f, 0});
		}

		for(s32 WheelIndex = 0;
				WheelIndex < 4;
				WheelIndex++)
		{
			entity WheelEntity = 
				GetEntityByLowIndex(Entities, CarFrameEntity.Low->Wheels[WheelIndex]);
			if(WheelEntity.Low)
			{
				f32 dX = ((WheelIndex % 2) - 0.5f) * 2.0f;
				f32 dY = ((WheelIndex / 2) - 0.5f) * 2.0f;
				v2 D = v2{dX, dY};

				v2 O = (Hadamard(D, CarFrameEntity.Low->Dim.XY * 0.5f) - 
								Hadamard(D, WheelEntity.Low->Dim.XY * 0.5f));

				ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, &WheelEntity, 
																							 CarFrameEntity.Low->WorldP, (v3)O);
			}
		}
	}
}

internal_function void
DismountEntityFromCar(memory_arena* WorldArena, world_map* WorldMap, entity* Entity, entity* Car)
{
	Assert(Car->Low->Type == EntityType_CarFrame);

	Car->Low->DriverSeat = 0;
	Entity->Low->RidingVehicle = 0;
	Entity->Low->Attached = false;

	v3 Offset = {4.0f, 0, 0};
	if(Entity->High && Car->High)
	{
		Entity->High->P = Car->High->P + Offset;
	}
	ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, Entity, Car->Low->WorldP, Offset);
}
internal_function void
MountEntityOnCar(memory_arena* WorldArena, world_map* WorldMap, entity* Entity, entity* Car)
{
	Assert(Car->Low->Type == EntityType_CarFrame);

	Car->Low->DriverSeat = Entity->LowIndex;
	Entity->Low->RidingVehicle = Car->LowIndex;
	Entity->Low->Attached = true;

	//TODO(bjorn) What happens if you mount a car outside of the high entity zone.
	if(Entity->High && Car->High)
	{
		Entity->High->P = Car->High->P;
	}
	ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, Entity, Car->Low->WorldP, {});
}

	internal_function entity
AddCar(game_state* GameState, world_map_position WorldPos)
{
	entity CarFrameEntity = AddCarFrame(GameState, WorldPos);

	entity EngineEntity = AddEngine(GameState, WorldPos);
	CarFrameEntity.Low->Engine = EngineEntity.LowIndex;
	EngineEntity.Low->Vehicle = CarFrameEntity.LowIndex;
	EngineEntity.Low->Attached = true;

	for(s32 WheelIndex = 0;
			WheelIndex < 4;
			WheelIndex++)
	{
    entity WheelEntity = AddWheel(GameState, WorldPos);
		CarFrameEntity.Low->Wheels[WheelIndex] = WheelEntity.LowIndex;
		WheelEntity.Low->Vehicle = CarFrameEntity.LowIndex;
		WheelEntity.Low->Attached = true;
	}

	MoveCarPartsToStartingPosition(&GameState->WorldArena, GameState->WorldMap, 
																 &GameState->Entities, CarFrameEntity.LowIndex);

	return CarFrameEntity;
}

	internal_function entity
AddGround(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Ground, &WorldPos);

	Entity.Low->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = false;

	return Entity;
}

	internal_function entity
AddWall(game_state* GameState, world_map_position WorldPos, f32 Mass = 1000.0f)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 EntityType_Wall, &WorldPos);

	Entity.Low->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;

	Entity.Low->Mass = Mass;
	Entity.Low->MoveSpec = DefaultMoveSpec();

	return Entity;
}

	internal_function entity
AddStair(game_state* GameState, world_map_position WorldPos, f32 dZ)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
													 	EntityType_Stair, &WorldPos);

	Entity.Low->Dim = v3{1, 1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity.Low->Collides = true;
	Entity.Low->dZ = dZ;

	return Entity;
}

internal_function void
SetCamera(world_map* WorldMap, entities* Entities, 
					world_map_position* CameraP, world_map_position* NewCameraP)
{
	Assert(ValidateEntityPairs(Entities));

	if(NewCameraP)
	{
		*CameraP = *NewCameraP;
	}

	//TODO(bjorn): Think more about render distances.
	v3 HighFrequencyUpdateDim = v3{2.0f, 2.0f, 2.0f}*WorldMap->ChunkSideInMeters;

	rectangle3 CameraUpdateBounds = RectCenterDim(v3{0,0,0}, HighFrequencyUpdateDim);

	world_map_position MinWorldP = OffsetWorldPos(WorldMap, *CameraP, CameraUpdateBounds.Min);
	world_map_position MaxWorldP = OffsetWorldPos(WorldMap, *CameraP, CameraUpdateBounds.Max);

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

							v3 RelCamP = GetWorldMapPosDifference(WorldMap, Entity.Low->WorldP, *CameraP);

							if(Entity.High)
							{
								Entity.High->P = RelCamP;

								if(!IsInRectangle(CameraUpdateBounds, Entity.High->P))
								{
									world_map_position WorldP = OffsetWorldPos(WorldMap, *CameraP, Entity.High->P);
									MapEntityOutFromHigh(Entities, Entity.LowIndex, WorldP);
								}
							}
							else
							{
								if(IsInRectangle(CameraUpdateBounds, RelCamP))
								{
									MapEntityIntoHigh(Entities, Entity.LowIndex, RelCamP);
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
InitializeGame(game_memory *Memory, game_state *GameState)
{
	GameState->GroundStaticFriction = 2.1f; 
	GameState->GroundDynamicFriction = 2.0f; 
	GameState->Backdrop = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test/test_background.bmp");

	GameState->Rock = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test2/rock00.bmp");
	GameState->Rock.Alignment = {35, 41};

	GameState->Dirt = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test2/ground00.bmp");
	GameState->Dirt.Alignment = {133, 56};

	GameState->Shadow = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test/test_hero_shadow.bmp");
	GameState->Shadow.Alignment = {72, 182};

	GameState->Sword = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test2/ground01.bmp");
	GameState->Sword.Alignment = {256/2, 116/2};

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

	entity Player = AddPlayer(GameState);
	GameState->PlayerIndexForKeyboard[0] = Player.LowIndex;
	AddCar(GameState, OffsetWorldPos(GameState->WorldMap, Player.Low->WorldP, {3.0f, 7.0f, 0}));

	AddMonstar(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{ 2, 5, 0}));
	AddFamiliar(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{ 4, 5, 0}));

	entity A = AddWall(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{2, 4, 0}), 10.0f);
	entity B = AddWall(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{5, 4, 0}),  5.0f);

	//TODO(bjorn): Have an object stuck to the mouse.
	//GameState->DEBUG_EntityBoundToMouse = A.LowIndex;

	entity E[6];
	for(u32 i=0;
			i < 6;
			i++)
	{
		E[i] = AddWall(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{2 + 2*i, 2, 0}), 10.0f + i);
	}

	entities* Entities = &GameState->Entities;
	SetCamera(WorldMap, Entities, &GameState->CameraP, &Player.Low->WorldP);

	A = GetEntityByLowIndex(Entities, A.LowIndex);
	B = GetEntityByLowIndex(Entities, B.LowIndex);

	A.High->dP = {1, 0, 0};
	B.High->dP = {-1, 0, 0};
	for(u32 i=0;
			i < 6;
			i++)
	{
		E[i] = GetEntityByLowIndex(Entities, E[i].LowIndex);
		E[i].High->dP = {2.0f-i, -2.0f+i};
	}

	u32 ScreenX = 0;
	u32 ScreenY = 0;
	u32 ScreenZ = 0;
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
				TileValue = 2;
			}
			if((TileX == (TilesPerWidth-1)))
			{
				TileValue = 2;
			}
			if((TileY == 0))
			{
				TileValue = 2;
			}
			if((TileY == (TilesPerHeight-1)))
			{
				TileValue = 2;
			}

			world_map_position WorldPos = GetChunkPosFromAbsTile(WorldMap, AbsTile);
			if((TileY != TilesPerHeight && TileX != TilesPerWidth/2) && TileValue ==  2)
			{
				entity Wall = AddWall(GameState, WorldPos);
			}
		}
	}

#if 0
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
#endif
}

//STUDY(bjorn): Lazy fix. https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
// I dont even understand the maths behind it.
struct intersection_result
{
	b32 Valid;
	f32 t;
};
	internal_function intersection_result
GetTimeOfIntersectionWithLineSegment(v2 S, v2 E, v2 A, v2 B)
{
	intersection_result Result = {};
	f32 denominator = (S.X-E.X) * (A.Y-B.Y) - (S.Y-E.Y) * (A.X-B.X);
	if(denominator != 0)
	{
		f32 u = -(((S.X-E.X) * (S.Y-A.Y) - (S.Y-E.Y) * (S.X-A.X)) / denominator);
		if((u >= 0.0f) && (u <= 1.0f))
		{
			Result.Valid = true;
			Result.t = ((S.X-A.X) * (A.Y-B.Y) - (S.Y-A.Y) * (A.X-B.X)) / denominator;
		}
	}

	return Result;
}

struct closest_point_to_line_result
{
	f32 t;
	v2 P;
};
internal_function closest_point_to_line_result 
GetClosestPointOnLineSegment(v2 A, v2 B, v2 P)
{
	closest_point_to_line_result Result = {};
	v2 AP = P - A;
	v2 AB = B - A;

	f32 t = Dot(AP, AB) / LenghtSquared(AB);

	Result.t = t;
	if (t <= 0)
	{
		Result.P = A;
	}
	else if (t >= 1)
	{
		Result.P = B;
	}
	else
	{
		Result.P = A + AB * t;
	}

	return Result;
}

	inline f32 
GetInverseOrZero(f32 V)
{
	if(V == 0.0f) { return 0; } else { return 1.0f / V; }
}

struct collision_result
{
	b32 Hit;
	b32 Inside;
	f32 TimeOfImpact;
	v2 Normal;
};

enum minkowski_sum_origin
{
	MinkowskiGenus_Target,
	MinkowskiGenus_Movable,
};
struct polygon
{
	s32 NodeCount;
	v2 Nodes[8];
	v2 OriginalLineSeg[8][2];
	minkowski_sum_origin Genus[8];
};
	internal_function polygon 
MinkowskiSum(entity* Target, entity* Movable)
{
	polygon Result = {};
	Result.NodeCount = 8;

	v2 MovableP = Movable->High->P.XY;
	v2 TargetP  = Target->High->P.XY;

	vertices TVerts = GetEntityVertices(Target);
	vertices MVerts = GetEntityVertices(Movable);

	//TODO(bjorn): This sum is _NOT_ made for general polygons atm!
	s32 MovableStartIndex;
	{
		v2 MovableR = Movable->Low->R.XY;
		v2 TargetR  = Target->Low->R.XY;
		if(Dot(TargetR, MovableR) > 0)
		{
			MovableStartIndex = (Cross((v3)TargetR, (v3)MovableR).Z > 0) ? 1 : 0;
		}
		else
		{
			MovableStartIndex = (Cross((v3)TargetR, (v3)MovableR).Z > 0) ? 2 : 3;
		}
	}

	v2 MovingCorner = TVerts.Verts[0].XY + (MVerts.Verts[MovableStartIndex].XY - MovableP);
	for(int CornerIndex = 0; 
			CornerIndex < 4; 
			CornerIndex++)
	{
		{
			v2 V0 = TVerts.Verts[(CornerIndex+0)%4].XY;
			v2 V1 = TVerts.Verts[(CornerIndex+1)%4].XY;
			v2 NodeDiff = (V1 - V0);

			s32 ResultIndex = CornerIndex*2+0;

			Result.Genus[ResultIndex] = MinkowskiGenus_Target;
			Result.OriginalLineSeg[ResultIndex][0] = V0;
			Result.OriginalLineSeg[ResultIndex][1] = V1;
			Result.Nodes[ResultIndex] = MovingCorner;

			MovingCorner += NodeDiff;
		}
		{
			v2 V0 = MVerts.Verts[(MovableStartIndex+CornerIndex+0)%4].XY;
			v2 V1 = MVerts.Verts[(MovableStartIndex+CornerIndex+1)%4].XY;
			v2 NodeDiff = (V1 - V0);

			s32 ResultIndex = CornerIndex*2+1;

			Result.Genus[ResultIndex] = MinkowskiGenus_Movable;
			Result.OriginalLineSeg[ResultIndex][0] = V0;
			Result.OriginalLineSeg[ResultIndex][1] = V1;
			Result.Nodes[ResultIndex] = MovingCorner;

			MovingCorner += NodeDiff;
		}
	}

	return Result;
}

#if HANDMADE_INTERNAL
	internal_function void
DEBUGMinkowskiSum(game_offscreen_buffer* Buffer, 
									entity* Target, entity* Movable, 
									m22 GameSpaceToScreenSpace, v2 ScreenCenter)
{
	polygon Sum = MinkowskiSum(Target, Movable);

	for(s32 NodeIndex = 0; 
			NodeIndex < Sum.NodeCount; 
			NodeIndex++)
	{
		v2 N0 = Sum.Nodes[NodeIndex];
		v2 N1 = Sum.Nodes[(NodeIndex+1) % Sum.NodeCount];

		v2 WallNormal = Normalize(CCW90M22() * (N1 - N0));

		DrawLine(Buffer, 
						 ScreenCenter + GameSpaceToScreenSpace * N0, 
						 ScreenCenter + GameSpaceToScreenSpace * N1, 
						 {0, 0, 1});

		v3 NormalColor = {1, 0, 1};
		if(Sum.Genus[NodeIndex] == MinkowskiGenus_Target) { NormalColor = {0, 1, 1}; }

		DrawLine(Buffer, 
						 ScreenCenter + GameSpaceToScreenSpace * (N0 + N1) * 0.5f, 
						 ScreenCenter + GameSpaceToScreenSpace * ((N0 + N1) * 0.5f + WallNormal * 0.2f), 
						 NormalColor);
	}
}
#endif

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
AlignWheelsForward(entities* Entities, entity* CarFrameEntity, f32 SecondsToUpdate)
{
	entity LeftFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->Wheels[2]);
	entity RightFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->Wheels[3]);

	Assert(LeftFrontWheel.High);
	Assert(RightFrontWheel.High);

	f32 TurnRate = Lenght(CarFrameEntity->High->dP) * 0.05f;
	v3 CarDir = CarFrameEntity->Low->R;
	v3 WheelDir = LeftFrontWheel.Low->R;

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

	LeftFrontWheel.Low->R = NewDir;
	RightFrontWheel.Low->R = NewDir;
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

		entity LeftFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->Wheels[2]);
		entity RightFrontWheel = GetEntityByLowIndex(Entities, CarFrameEntity->Low->Wheels[3]);

		Assert(LeftFrontWheel.High);
		Assert(RightFrontWheel.High);

		v2 CarD = CarFrameEntity->Low->R.XY;
		v2 TD = Rot90CW * CarD;

		v2 NewD = Normalize(LeftFrontWheel.Low->R.XY + 
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

		LeftFrontWheel.Low->R = NewD;
		RightFrontWheel.Low->R = NewD;
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
						 entity* CarFrame, world_map_position* CameraP, v2 NewP, v2 NewR)
{
	v2 OldR = CarFrame->Low->R.XY;
	v2 OldP = CarFrame->High->P.XY;

	CarFrame->Low->R = NewR;
	CarFrame->High->P = NewP;

	ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, CarFrame, *CameraP, (v2)NewP);

	entity EngineEntity = GetEntityByLowIndex(Entities, CarFrame->Low->Engine);
	if(EngineEntity.High)
	{
		v2 RelPos = GetCoordinatesRelativeTransform(OldP, OldR, EngineEntity.High->P.XY);
		v2 RelDir = GetCoordinatesRelativeTransform(OldP, OldR, EngineEntity.High->P.XY + 
																								EngineEntity.Low->R.XY);
		v2 NewPos = GetGlobalPosFromRelativeCoordinates(NewP, NewR, RelPos);
		v2 NewRot = GetGlobalPosFromRelativeCoordinates(NewP, NewR, RelDir) - NewPos;

		EngineEntity.High->P = NewPos;
		EngineEntity.Low->R = Normalize(NewRot);
		ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, &EngineEntity, 
																					 *CameraP, (v3)NewPos);
	}

	entity DriverEntity = GetEntityByLowIndex(Entities, CarFrame->Low->DriverSeat);
	if(DriverEntity.High)
	{
		v2 RelPos = GetCoordinatesRelativeTransform(OldP, OldR, DriverEntity.High->P.XY);
		v2 RelRot = GetCoordinatesRelativeTransform(OldP, OldR, DriverEntity.High->P.XY + 
																								DriverEntity.Low->R.XY);
		v2 NewPos = GetGlobalPosFromRelativeCoordinates(NewP, NewR, RelPos);
		v2 NewRot = GetGlobalPosFromRelativeCoordinates(NewP, NewR, RelRot) - NewPos;

		DriverEntity.High->P = NewPos;
		DriverEntity.Low->R = Normalize(NewRot);
		ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, &DriverEntity, 
																					 *CameraP, (v3)NewPos);
	}

	for(s32 WheelIndex = 0;
			WheelIndex < 4;
			WheelIndex++)
	{
		entity WheelEntity = GetEntityByLowIndex(Entities, CarFrame->Low->Wheels[WheelIndex]);
		if(WheelEntity.High)
		{
			v2 RelPos = GetCoordinatesRelativeTransform(OldP, OldR, WheelEntity.High->P.XY);
			v2 RelRot = GetCoordinatesRelativeTransform(OldP, OldR, WheelEntity.High->P.XY + 
																									WheelEntity.Low->R.XY);
			v2 NewPos = GetGlobalPosFromRelativeCoordinates(NewP, NewR, RelPos);
			v2 NewRot = GetGlobalPosFromRelativeCoordinates(NewP, NewR, RelRot) - NewPos;

			WheelEntity.High->P = NewPos;
			WheelEntity.Low->R = Normalize(NewRot);
			ChangeEntityWorldLocationRelativeOther(WorldArena, WorldMap, &WheelEntity, 
																						 *CameraP, (v3)NewPos);
		}
	}
}

//STUDY(bjorn): This is from a book about collision. Comeback and make sure you
//understand what is happening.
inline f32 
SquareDistancePointToLineSegment(v2 A, v2 B, v2 C)
{
	v2 AB = B - A;
	v2 AC = C - A;
	v2 BC = C - B;
	f32 e = Dot(AC, AB);
	if(e <= 0.0f) { return Dot(AC, AC); }
	f32 f = Dot(AB, AB);
	if(e >= f) { return Dot(BC, BC); }
	return Dot(AC, AC) - Square(e) / f;
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
					ControlledEntity.High->ddP = InputDirection * 85.0f;
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
					GameState->PlayerIndexForController[ControllerIndex] = AddPlayer(GameState).LowIndex;
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
#if HANDMADE_INTERNAL
			if(Clicked(Keyboard, Q))
			{
				GameState->NoteTone = 500.0f;
				GameState->NoteDuration = 0.05f;
				GameState->NoteSecondsPassed = 0.0f;

				//TODO(bjorn): Just setting the flag is not working anymore.
				//Memory->IsInitialized = false;

				GameState->DEBUG_StepThroughTheCollisionLoop = 
					!GameState->DEBUG_StepThroughTheCollisionLoop;
				GameState->DEBUG_CollisionLoopEntityIndex = 1;
				GameState->DEBUG_CollisionLoopStepIndex = 0;
			}

			if(GameState->DEBUG_StepThroughTheCollisionLoop && Clicked(Keyboard, Space))
			{
				GameState->DEBUG_CollisionLoopAdvance = true;
				GameState->DEBUG_CollisionLoopAdvance = true;
			}
#endif

			if(Clicked(Keyboard, M))
			{
				GameState->DEBUG_VisualiseMinkowskiSum = !GameState->DEBUG_VisualiseMinkowskiSum;
			}

			if(Clicked(Keyboard, C))
			{
				GameState->DEBUG_VisualiseCollisionBox = !GameState->DEBUG_VisualiseCollisionBox;
			}

			u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[KeyboardIndex];
			entity ControlledEntity = GetEntityByLowIndex(Entities, ControlledEntityIndex);

			if(ControlledEntity.Low)
			{
				v3 InputDirection = {};

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
				if(Held(Keyboard, Space))
				{
					InputDirection.Z += 1;
				}

				if(InputDirection.X && InputDirection.Y)
				{
					InputDirection *= inv_root2;
				}

				if(ControlledEntity.High)
				{
					if(ControlledEntity.Low->Type == EntityType_Player)
					{
						if(Clicked(Keyboard, E))
						{
							if(ControlledEntity.Low->RidingVehicle)
							{
								entity Vehicle = GetEntityByLowIndex(Entities, 
																										 ControlledEntity.Low->RidingVehicle);
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
						if(ControlledEntity.Low->RidingVehicle)
						{
							entity CarFrame = 
								GetEntityByLowIndex(Entities, ControlledEntity.Low->RidingVehicle);
							Assert(CarFrame.Low->Type == EntityType_CarFrame);

							if(InputDirection.X)
							{ 
								TurnWheels(Entities, &CarFrame, InputDirection.XY, SecondsToUpdate); 
							}
							else
							{ 
								AlignWheelsForward(Entities, &CarFrame, SecondsToUpdate); 
							}

							if(Clicked(Keyboard, One))   { CarFrame.High->ddP = CarFrame.Low->R * -3.0f; }
							if(Clicked(Keyboard, Two))   { CarFrame.High->ddP = CarFrame.Low->R *  0.0f; }
							if(Clicked(Keyboard, Three)) { CarFrame.High->ddP = CarFrame.Low->R *  3.0f; }
							if(Clicked(Keyboard, Four))  { CarFrame.High->ddP = CarFrame.Low->R *  6.0f; }
							if(Clicked(Keyboard, Five))  { CarFrame.High->ddP = CarFrame.Low->R *  9.0f; }
						}
						else
						{
							ControlledEntity.High->MovingDirection = InputDirection;
							if(Clicked(Keyboard, N))
							{
								entity Sword = GetEntityByLowIndex(Entities, ControlledEntity.Low->Sword);
								if(Sword.Low && !IsValid(Sword.Low->WorldP) && LenghtSquared(InputDirection))
								{
									Sword.Low->R = InputDirection;
									ChangeEntityWorldLocationRelativeOther(&GameState->WorldArena, 
																												 GameState->WorldMap, 
																												 &Sword,
																												 ControlledEntity.Low->WorldP, 
																												 InputDirection * Sword.Low->Dim.Y);

									SetCamera(WorldMap, Entities, &GameState->CameraP, 0);

									Sword = GetEntityByLowIndex(Entities, ControlledEntity.Low->Sword);
									if(Sword.High)
									{
										Sword.High->dP = Sword.Low->R * 8.0f;
										Sword.Low->DistanceRemaining = 3.0f;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	//
	// NOTE(bjorn): Movement
	//

	//TODO(bjorn): Implement step 2 in J.Blows framerate independence video.
	// https://www.youtube.com/watch?v=fdAOPHgW7qM
	//TODO(bjorn): Add some asserts and some limits to velocities related to the
	//delta time so that tunneling becomes virtually impossible.
	s32 Steps = 8;
	f32 dT = SecondsToUpdate / (f32)Steps;
	for(s32 Step = 0;
			Step < Steps;
			Step++)
	{
		//
		// NOTE(bjorn): Move all entities.
		//
		for(LoopOverHighEntities)
		{

			//TODO(bjorn):
			// Add negative gravity for penetration if relative velocity is >= 0.
			// Get relevant contact point.
			// Do impulse calculation.
			// Test with object on mouse.
			for(LoopOverHighEntitiesNamed(OtherEntity))
			{
				if(Entity.LowIndex == OtherEntity.LowIndex) { continue; }
				if(OtherEntity.High->CollisionDirtyBit) { continue; }

				b32 Inside = true; 
				f32 BestSquareDistanceToWall = positive_infinity32;
				s32 RelevantNodeIndex = -1;
				v2 P = Entity.High->P.XY;
				polygon Sum = MinkowskiSum(&OtherEntity, &Entity);
				for(s32 NodeIndex = 0; 
								NodeIndex < Sum.NodeCount; 
								NodeIndex++)
				{
					v2 N0 = Sum.Nodes[NodeIndex];
					v2 N1 = Sum.Nodes[(NodeIndex+1) % Sum.NodeCount];

					f32 Det = Determinant(N1-N0, P-N0);
					if(Inside && (Det >= 0.0f)) 
					{ 
						Inside = false; 
					}

					f32 SquareDistanceToWall = SquareDistancePointToLineSegment(N0, N1, P);
					if(SquareDistanceToWall < BestSquareDistanceToWall)
					{
						BestSquareDistanceToWall = SquareDistanceToWall;
						RelevantNodeIndex = NodeIndex;
					}
				}	

				if(Inside &&
					 Entity.Low->Collides && 
					 OtherEntity.Low->Collides)
				{
					Assert(RelevantNodeIndex >= 0);

					f32 Penetration;
					v2 n, t, ImpactPoint;
					{
						Penetration	= SquareRoot(BestSquareDistanceToWall);
						Assert(Penetration <= (Lenght(Entity.Low->Dim) + Lenght(OtherEntity.Low->Dim)) * 0.5f);

						v2 N0 = Sum.Nodes[RelevantNodeIndex];
						v2 N1 = Sum.Nodes[(RelevantNodeIndex+1) % Sum.NodeCount];
						v2 V0 = Sum.OriginalLineSeg[RelevantNodeIndex][0];
						v2 V1 = Sum.OriginalLineSeg[RelevantNodeIndex][1];

						//TODO(bjorn): This normalization might not be needed.
						t = Normalize(N1 - N0);
						n = CCW90M22() * t;

						closest_point_to_line_result ClosestPoint = GetClosestPointOnLineSegment(N0, N1, P);

						ImpactPoint = V0 + (V1-V0) * ClosestPoint.t;
						v2 ImpactCorrection = n * Penetration * 0.5f;
						ImpactCorrection *= Sum.Genus[RelevantNodeIndex] == MinkowskiGenus_Movable ? 1.0f:-1.0f;
						ImpactPoint += ImpactCorrection;
					}

					//NOTE(bjorn): Normal always points away from other entity.
					v2 AP = Entity.High->P.XY;
					v2 BP = OtherEntity.High->P.XY;
					v2 AdP = Entity.High->dP.XY;
					v2 BdP = OtherEntity.High->dP.XY;
					v2 ABdP = AdP - BdP;
					f32 ndotAB = Dot(n, ABdP);
					v2 AIt = CW90M22() * (AP - ImpactPoint);
					v2 BIt = CW90M22() * (BP - ImpactPoint);

					f32 AInvMass = GetInverseOrZero(Entity.Low->Mass);
					f32 BInvMass = GetInverseOrZero(OtherEntity.Low->Mass);
					//TODO(bjorn): Automize inertia calculation.
					f32 AInvMoI  = AInvMass ? 0.08f:0.0f;//GetInverseOrZero(Entity->High->MoI);
					f32 BInvMoI  = BInvMass ? 0.08f:0.0f;//GetInverseOrZero(OtherEntity->High->MoI);

					f32 Impulse = 0;
					f32 InverseGravity = Penetration * 60.0f;

					if(ndotAB < 0)
					{
						f32 e = 0.0f;
						f32 j = ((-(1+e) * Dot(ABdP, n)) / 
										 (/*Dot(n, n) * */(AInvMass + BInvMass) + 
											Square(Dot(AIt, n))*AInvMoI + 
											Square(Dot(BIt, n))*BInvMoI
										 )
										);
						Impulse = Max(j, InverseGravity);

						Entity.High->dA         += Dot(AIt, Impulse * n) * AInvMoI;
						OtherEntity.High->dA    -= Dot(BIt, Impulse * n) * BInvMoI;
					}
					//STUDY TODO (bjorn): Excerpt from []
					//The general sequence of a simulation step using impulse-based
					//dynamics is somewhat different from that of force-based engines:
					//
					//1 Compute all external forces.
					//2 Apply the forces and determine the resulting velocities, using the
					//  techniques from Part I.
					//3 Calculate the constraint velocities based on the behavior functions.
					//4 Apply the constraint velocities and simulate the resulting motion.

					//TODO(bjorn): Account for rotational momentum. 
					Entity.High->dP.XY      += (Impulse * AInvMass) * n;
					OtherEntity.High->dP.XY -= (Impulse * BInvMass) * n;
				} 

				if(Step == 0)
				{
					entity* Familiar = GetEntityOfType(EntityType_Familiar, &Entity, &OtherEntity);
					if(Familiar)
					{
						entity* Target = GetRemainingEntity(Familiar, &Entity, &OtherEntity);
						UpdateFamiliarPairwise(Familiar, Target);
					}

					entity* Sword = GetEntityOfType(EntityType_Sword, &Entity, &OtherEntity);
					if(Sword)
					{
						entity* Target = GetRemainingEntity(Sword, &Entity, &OtherEntity);
						UpdateSwordPairwise(Sword, Target, Inside, WorldArena, WorldMap, Entities);
					}
				}
			}

			Entity.High->CollisionDirtyBit = true;
			{
				v3 OldP = {};

				if(Entity.Low->Type == EntityType_Sword)
				{
					OldP = Entity.High->P;
				}

				MoveEntity(WorldArena, WorldMap, &GameState->CameraP, &Entity, dT);

				if(Entity.Low->Type == EntityType_Sword)
				{
					v3 NewP = Entity.High->P;
					
					Entity.Low->DistanceRemaining -= Lenght(NewP - OldP);
					if(Entity.Low->DistanceRemaining <= 0)
					{
						entity* Sword = &Entity;
						ChangeEntityWorldLocation(WorldArena, WorldMap, Sword, 0);
						MapEntityOutFromHigh(Entities, Sword->LowIndex, Sword->Low->WorldP);
					}
				}

				if(Entity.Low->Type == EntityType_Familiar)
				{
					Entity.High->BestDistanceToPlayerSquared = Square(6.0f);
					Entity.High->MovingDirection = {};
				}
			}
		}
	}

#if 0
	if(Entity.Low->Type == EntityType_CarFrame)
	{
		entity* Vehicle = &Entity;
		entity LeftFrontWheel = GetEntityByLowIndex(Entities, Vehicle->Low->Wheels[2]);
		entity RightFrontWheel = GetEntityByLowIndex(Entities, Vehicle->Low->Wheels[3]);

		ddP = Vehicle->High->ddP;
		dP = Vehicle->High->dP;
		//ddP += Vehicle->Low->DecelerationFactor * Vehicle->High->dP;

		v3 PrelDeltaP = (0.5f * ddP * Square(SecondsToUpdate) + 
										 (Vehicle->High->dP * SecondsToUpdate));
		dP += (ddP * SecondsToUpdate);

		v3 OldFrontP = Vehicle->High->P + Vehicle->High->R * Vehicle->Low->Dim.Y * 0.5f;
		v3 NewFrontP;
		f32 DeltaPSign;
		if(LeftFrontWheel.High)
		{
			DeltaPSign = Sign(Dot(PrelDeltaP, LeftFrontWheel.High->R));
			NewFrontP = OldFrontP + LeftFrontWheel.High->R * Lenght(PrelDeltaP) * DeltaPSign;
				}
				else if(RightFrontWheel.High)
				{
					DeltaPSign = Sign(Dot(PrelDeltaP, RightFrontWheel.High->R));
					NewFrontP = OldFrontP + RightFrontWheel.High->R * Lenght(PrelDeltaP) * DeltaPSign;
				}
				else
				{
					DeltaPSign = Sign(Dot(PrelDeltaP, Vehicle->High->R));
					NewFrontP = OldFrontP + Vehicle->High->R * Lenght(PrelDeltaP) * DeltaPSign;
				}

				v3 OldP = Vehicle->High->P;
				NewR = Normalize(NewFrontP - OldP);
				v3 NewP = NewFrontP - NewR * Distance(OldP, OldFrontP);

				P = NewP.XY;
				DeltaP = (NewP - OldP).XY;
			}
			else
#endif
#if 0
			if(Entity.Low->Type == EntityType_CarFrame)
			{
				v3 NewP = P;
				entity* Vehicle = &Entity;
				Vehicle->High->ddP = (NewR * Lenght(Vehicle->High->ddP) * 
															Sign(Dot(Vehicle->High->ddP, Vehicle->High->R)));

				if(Lenght(Vehicle->High->ddP) < 0.1f && Lenght(Vehicle->High->dP) < 0.4f)
				{
					Vehicle->High->dP = {};
				}

				UpdateCarPos(WorldArena, WorldMap, Entities, Vehicle, &GameState->CameraP, NewP.XY, NewR.XY);
			}
			else
#endif


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
		world_map_position NewCameraP = CameraTarget.Low->WorldP;
		NewCameraP.Offset_.Z = 0;
		SetCamera(WorldMap, Entities, &GameState->CameraP, &NewCameraP);
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
		Entity.High->CollisionDirtyBit = false;
		if(Entity.Low->WorldP.ChunkP.Z != GameState->CameraP.ChunkP.Z) { continue; }

		v2 CollisionMarkerPixelDim = Hadamard(Entity.Low->Dim.XY, {PixelsPerMeter, PixelsPerMeter});
		m22 GameSpaceToScreenSpace = 
		{PixelsPerMeter, 0             ,
			0             ,-PixelsPerMeter};

		v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * Entity.High->P.XY;
		v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
		f32 ZPixelOffset = PixelsPerMeter * -Entity.High->P.Z;

		if(LenghtSquared(Entity.High->dP) != 0.0f)
		{
			if(Absolute(Entity.High->dP.X) > Absolute(Entity.High->dP.Y))
			{
				Entity.High->FacingDirection = (Entity.High->dP.X > 0) ? 3 : 1;
			}
			else
			{
				Entity.High->FacingDirection = (Entity.High->dP.Y > 0) ? 2 : 0;
			}
		}

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
#if 0
			v3 LightYellow = {0.5f, 0.5f, 0.0f};
			DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f), LightYellow);
#endif
			DrawBitmap(Buffer, &GameState->Rock, EntityPixelPos - GameState->Rock.Alignment, 
								 (v2)GameState->Rock.Dim);
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
								Entity.Low->R.XY, Color);
			DrawLine(Buffer, EntityPixelPos, EntityPixelPos + 
							 Hadamard(Entity.Low->R.XY, v2{1, -1}) * 40.0f, {1, 0, 0});
#if 0
			DrawBitmap(Buffer, &GameState->Dirt, EntityPixelPos - GameState->Dirt.Alignment, 
								 (v2)GameState->Dirt.Dim * 1.2f);
#endif
		}

		f32 ZAlpha = Clamp(1.0f - (Entity.High->P.Z / 2.0f), 0.0f, 1.0f);
		if(Entity.Low->Type == EntityType_Player)
		{
			v3 Yellow = {1.0f, 1.0f, 0.0f};
			//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

			hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

			DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
								 (v2)GameState->Shadow.Dim, ZAlpha);
			DrawBitmap(Buffer, &Hero->Torso, 
								 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Torso.Alignment, (v2)Hero->Torso.Dim);
			DrawBitmap(Buffer, &Hero->Cape, 
								 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Cape.Alignment, (v2)Hero->Cape.Dim);
			DrawBitmap(Buffer, &Hero->Head, 
								 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Head.Alignment, (v2)Hero->Head.Dim);
		}
		if(Entity.Low->Type == EntityType_Monstar)
		{
			v3 Yellow = {1.0f, 1.0f, 0.0f};
			//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

			hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

			DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
								 (v2)GameState->Shadow.Dim, ZAlpha);
			DrawBitmap(Buffer, &Hero->Torso, EntityPixelPos - Hero->Torso.Alignment, 
								 (v2)Hero->Torso.Dim);
		}
		if(Entity.Low->Type == EntityType_Familiar)
		{
			v3 Yellow = {1.0f, 1.0f, 0.0f};
			//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

			hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

			DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
								 (v2)GameState->Shadow.Dim, 0.2f);
			DrawBitmap(Buffer, &Hero->Head, EntityPixelPos - Hero->Head.Alignment, 
								 (v2)Hero->Head.Dim);
		}
		if(Entity.Low->Type == EntityType_Sword)
		{
			DrawBitmap(Buffer, &GameState->Sword, EntityPixelPos - GameState->Sword.Alignment, 
								 (v2)GameState->Sword.Dim, 1.0f);
		}

		if(Entity.Low->Type == EntityType_Monstar ||
			 Entity.Low->Type == EntityType_Player)
		{
			for(u32 HitPointIndex = 0;
					HitPointIndex < Entity.Low->HitPointMax;
					HitPointIndex++)
			{
				v2 HitPointDim = {0.15f, 0.3f};
				v2 HitPointPos = Entity.High->P.XY;
				HitPointPos.Y -= 0.3f;
				HitPointPos.X += ((HitPointIndex - (Entity.Low->HitPointMax-1) * 0.5f) * 
													HitPointDim.X * 1.5f);

				v2 HitPointPixelPos = ScreenCenter + (GameSpaceToScreenSpace * HitPointPos);
				v2 HitPointPixelDim = Hadamard(HitPointDim, {PixelsPerMeter, PixelsPerMeter});

				hit_point HP = Entity.Low->HitPoints[HitPointIndex];
				if(HP.FilledAmount == HIT_POINT_SUB_COUNT)
				{
					v3 Green = {0.0f, 1.0f, 0.0f};

					rectangle2 GreenRect = RectCenterDim(HitPointPixelPos, HitPointPixelDim);

					DrawRectangle(Buffer, GreenRect, Green);
				}
				else if(HP.FilledAmount < HIT_POINT_SUB_COUNT &&
								HP.FilledAmount > 0)
				{
					v3 Green = {0.0f, 1.0f, 0.0f};
					v3 Red = {1.0f, 0.0f, 0.0f};

					rectangle2 RedRect = RectCenterDim(HitPointPixelPos, HitPointPixelDim);
					v2 GreenRectMax = HitPointPixelPos + HitPointPixelDim * 0.5f;
					v2 GreenRectMin = HitPointPixelPos - HitPointPixelDim * 0.5f;
					GreenRectMin.Y += ((HitPointPixelDim.Y/HIT_POINT_SUB_COUNT) * 
														 (HIT_POINT_SUB_COUNT - HP.FilledAmount));
					rectangle2 GreenRect = RectMinMax(GreenRectMin, GreenRectMax);

					DrawRectangle(Buffer, RedRect, Red);
					DrawRectangle(Buffer, GreenRect, Green);
				}
				else
				{
					v3 Red = {1.0f, 0.0f, 0.0f};

					rectangle2 RedRect = RectCenterDim(HitPointPixelPos, HitPointPixelDim);

					DrawRectangle(Buffer, RedRect, Red);
				}
			}
		}

#if HANDMADE_INTERNAL
		if(GameState->DEBUG_VisualiseCollisionBox)
		{
			vertices Verts = GetEntityVertices(&Entity);
			for(u32 VertIndex = 0; VertIndex < Verts.Count; VertIndex++)
			{
				v2 V0 = Verts.Verts[VertIndex].XY;
				v2 V1 = Verts.Verts[(VertIndex+1) % Verts.Count].XY;
				DrawLine(Buffer, 
								 ScreenCenter + GameSpaceToScreenSpace * V0, 
								 ScreenCenter + GameSpaceToScreenSpace * V1, 
								 {0, 0, 1});

				v2 WallNormal = Normalize(CCW90M22() * (V1 - V0));
				v3 NormalColor = {1, 0, 1};

				DrawLine(Buffer, 
								 ScreenCenter + GameSpaceToScreenSpace * (V0 + V1) * 0.5f, 
								 ScreenCenter + GameSpaceToScreenSpace * ((V0 + V1) * 0.5f + WallNormal * 0.2f), 
								 NormalColor);
			}
		}

		if(GameState->DEBUG_VisualiseMinkowskiSum)
		{
			u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[0];
			entity Player = GetEntityByLowIndex(Entities, ControlledEntityIndex);
			entity Vehicle = GetEntityByLowIndex(Entities, Player.Low->RidingVehicle);

			if(Entity.Low->Collides)
			{
				if(Vehicle.Low)
				{
					if(Entity.LowIndex != Vehicle.LowIndex &&
						 Entity.LowIndex != Player.LowIndex)
					{
						DEBUGMinkowskiSum(Buffer, &Entity, &Vehicle, GameSpaceToScreenSpace, ScreenCenter);
					}
				}
				else
				{
					if(Entity.LowIndex != Player.LowIndex)
					{
						DEBUGMinkowskiSum(Buffer, &Entity, &Player, GameSpaceToScreenSpace, ScreenCenter);
					}
				}
			}
		}

		if(GameState->DEBUG_StepThroughTheCollisionLoop)
		{
			if(Entity.Low->HighIndex == GameState->DEBUG_CollisionLoopEntityIndex) 
			{
				DrawFrame(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), 
									Entity.Low->R.XY, {1.0f, 0.0f, 0.0f});

				EntityCameraPixelDelta = 
					GameSpaceToScreenSpace * GameState->DEBUG_CollisionLoopEstimatedPos.XY;

				v2 NextEntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
				DrawFrame(Buffer, RectCenterDim(NextEntityPixelPos, CollisionMarkerPixelDim), 
									Entity.Low->R.XY, {0.0f, 0.0f, 1.0f});
			}
		}
#endif
	}
}

	internal_function void
OutputSound(game_sound_output_buffer *SoundBuffer, game_state* GameState)
{
	s32 ToneVolume = 11000;
	s16 *SampleOut = SoundBuffer->Samples;
	for(s32 SampleIndex = 0;
			SampleIndex < SoundBuffer->SampleCount;
			++SampleIndex)
	{
		f32 Value = 0;
		if(GameState->NoteSecondsPassed < GameState->NoteDuration)
		{
			f32 t = GameState->NoteSecondsPassed * GameState->NoteTone * tau32;
			t -= FloorF32ToS32(t * inv_tau32) * tau32;
			Value = Sin(t);
			f32 TimeToGo = (GameState->NoteDuration - GameState->NoteSecondsPassed);
			if(TimeToGo < 0.01f)
			{
				Value *= TimeToGo / 0.01f;
			}
			GameState->NoteSecondsPassed += (1.0f / SoundBuffer->SamplesPerSecond);
		}

		s16 SampleValue = (s16)(ToneVolume * Value);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
	}

}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	OutputSound(SoundBuffer, GameState);
}
