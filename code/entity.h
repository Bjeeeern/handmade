#if !defined(ENTITY_H)

#include"sim_region.h"

struct entity_pair
{
	sim_entity Obj;
	sim_entity Sub;
};

struct vertices
{
	u32 Count;
	v3 Verts[16];
};

internal_function vertices
GetEntityVertices(sim_entity* Entity)
{
	vertices Result = {};
	Result.Count = 4;

	v2 OrderOfCorners[4] = {{-0.5f,  0.5f}, 
													{ 0.5f,  0.5f}, 
													{ 0.5f, -0.5f}, 
													{-0.5f, -0.5f}};
	
	m22 Transform = M22(CW90M22() * Entity->R.XY, 
											Entity->R.XY);
	for(u32 VertexIndex = 0; 
			VertexIndex < Result.Count; 
			VertexIndex++)
	{
		Result.Verts[VertexIndex] = (Transform * 
																 Hadamard(OrderOfCorners[VertexIndex], Entity->Dim.XY) + 
																 Entity->P);
	}

	return Result;
}

inline sim_entity*
GetEntityOfType(entity_type Type, sim_entity* A, sim_entity* B)
{
	sim_entity* Result = 0;
	if(A->Type == Type)
	{
		Result = A;
	}
	if(B->Type == Type)
	{
		Result = B;
	}
	return Result;
}

inline sim_entity*
GetRemainingEntity(sim_entity* E, sim_entity* A, sim_entity* B)
{
	Assert(E == A || E == B);
	return E==A ? B:A;
}

inline v3
DefaultEntityOrientation()
{
	return {0, 1, 0};
}

	internal_function stored_entity*
AddEntity(memory_arena* WorldArena, world_map* WorldMap, stored_entities* StoredEntities, 
					entity_type Type, world_map_position* WorldP = 0)
{
	stored_entity* Stored = {};

	StoredEntities->EntityCount++;
	Assert(StoredEntities->EntityCount < ArrayCount(StoredEntities->Entities));
	Stored = StoredEntities->Entities + StoredEntities->EntityCount;

	*Stored = {};
	Stored->Sim.StorageIndex = StoredEntities->EntityCount;
	Stored->Sim.Type = Type;
	Stored->Sim.R = DefaultEntityOrientation();
	Stored->Sim.WorldP = WorldMapNullPos();

	if(WorldP)
	{
		Assert(IsValid(*WorldP));
		Assert(IsCanonical(WorldMap, WorldP->Offset_));
	}

	ChangeStoredEntityWorldLocation(WorldArena, WorldMap, Stored, WorldP);

	return Stored;
}

	internal_function void
AddHitPoints(stored_entity* Entity, u32 HitPointMax)
{
	Assert(HitPointMax <= ArrayCount(Entity->HitPoints));
	Entity->HitPointMax = HitPointMax;
	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity->HitPointMax;
			HitPointIndex++)
	{
		Entity->HitPoints[HitPointIndex].FilledAmount = HIT_POINT_SUB_COUNT;
	}
}

	internal_function stored_entity*
AddSword(game_state* GameState)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, 
																		&GameState->Entities, EntityType_Sword);

	Entity->Dim = v2{0.4f, 1.5f} * GameState->WorldMap->TileSideInMeters;
	Entity->Mass = 8.0f;

	Entity->MoveSpec.Drag = 0.0f;
	Entity->Collides = false;

	return Entity;
}

	internal_function stored_entity*
AddPlayer(game_state* GameState)
{
	world_map_position InitP = OffsetWorldPos(GameState->WorldMap, GameState->CameraP, 
																						GetChunkPosFromAbsTile(GameState->WorldMap, 
																																	 v3s{-2, 1, 0}).Offset_);

	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
																		EntityType_Player, &InitP);

	Entity->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	//TODO(bjorn): Why does weight differences matter so much in the collision system.
	Entity->Mass = 40.0f / 8.0f;

	Entity->MoveSpec.EnforceVerticalGravity = true;
	Entity->MoveSpec.EnforceHorizontalMovement = true;
	Entity->MoveSpec.Speed = 85.f;
	Entity->MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(&Entity, 6);
	entity Sword = AddSword(GameState);
	Entity->Sword = Sword.StoredIndex;

	stored_entity* Test = GetStoredEntityByIndex(&GameState->Entities, 
																							 GameState->CameraFollowingPlayerIndex);
	if(!Test)
	{
		GameState->CameraFollowingPlayerIndex = Entity.StoredIndex;
	}

	return Entity;
}

	internal_function stored_entity*
AddMonstar(game_state* GameState, world_map_position InitP)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
																		EntityType_Monstar, &InitP);

	Entity->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->Mass = 40.0f / 8.0f;

	Entity->MoveSpec.EnforceHorizontalMovement = true;
	Entity->MoveSpec.Speed = 85.f * 0.75;
	Entity->MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(&Entity, 3);

	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity->HitPointMax;
			HitPointIndex++)
	{
		Entity->HitPoints[HitPointIndex].FilledAmount = (u8)(HIT_POINT_SUB_COUNT - HitPointIndex);
	}

	return Entity;
}

	internal_function stored_entity*
AddFamiliar(game_state* GameState, world_map_position InitP)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, 
																		&GameState->Entities, EntityType_Familiar, &InitP);

	Entity->Dim = v2{0.5f, 0.3f} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->Mass = 40.0f / 8.0f;

	Entity->MoveSpec.EnforceHorizontalMovement = true;
	Entity->MoveSpec.Speed = 85.f * 0.5f;
	Entity->MoveSpec.Drag = 0.2f * 30.0f;

	return Entity;
}

	internal_function entity
AddCarFrame(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityType_CarFrame, &WorldPos);

	Entity.Stored->Dim = v2{4, 6};
	Entity.Stored->Collides = true;

	Entity.Stored->MoveSpec = DefaultMoveSpec();

	Entity.Stored->Mass = 800.0f;

	return Entity;
}

	internal_function entity
AddEngine(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityType_Engine, &WorldPos);

	Entity.Stored->Dim = v2{3, 2};
	Entity.Stored->MoveSpec = DefaultMoveSpec();

	return Entity;
}

	internal_function entity
AddWheel(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityType_Wheel, &WorldPos);

	Entity.Stored->Dim = v2{0.6f, 1.5f};
	Entity.Stored->MoveSpec = DefaultMoveSpec();

	return Entity;
}

#if 0
	internal_function void
MoveCarPartsToStartingPosition(memory_arena* WorldArena, world_map* WorldMap, entities* Entities,
															 u32 CarFrameIndex)
{
	Assert(CarFrameIndex);
	entity CarFrameEntity = GetEntityByLowIndex(Entities, CarFrameIndex);
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
#endif

	internal_function stored_entity*
AddGround(game_state* GameState, world_map_position WorldPos)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
																		EntityType_Ground, &WorldPos);

	Entity->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = false;

	return Entity;
}

	internal_function stored_entity*
AddWall(game_state* GameState, world_map_position WorldPos, f32 Mass = 1000.0f)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
																		EntityType_Wall, &WorldPos);

	Entity->Dim = v2{1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->Mass = Mass;
	Entity->MoveSpec = DefaultMoveSpec();

	return Entity;
}

	internal_function stored_entity*
AddStair(game_state* GameState, world_map_position WorldPos, f32 dZ)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
																		EntityType_Stair, &WorldPos);

	Entity->Dim = v3{1, 1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = true;
	Entity->dZ = dZ;

	return Entity;
}

	internal_function void
MoveEntity(entity* Entity, f32 dT)
{
	v3 P = Entity->Sim->P;
	v3 dP = Entity->Stored->dP;
	v3 ddP = {};

	v3 R = Entity->Stored->R;
	f32 A = Entity->Stored->A;
	f32 dA = Entity->Stored->dA;
	f32 ddA = 0;

	if(Entity->Stored->MoveSpec.EnforceVerticalGravity)
	{
		if(Entity->Stored->MovingDirection.Z > 0 &&
			 P.Z == 0)
		{
			dP.Z = 18.0f;
		}
		if(P.Z > 0)
		{
			//TODO(bjorn): Why do i need the gravity to be so heavy? Because of upscaling?
			ddP.Z = -9.82f * 7.0f;
		}
	}

	if(Entity->Stored->MoveSpec.EnforceHorizontalMovement)
	{
		//TODO(casey): ODE here!
		ddP.XY = Entity->Stored->MovingDirection.XY * Entity->Stored->MoveSpec.Speed;
	}

	ddP.XY -= Entity->Stored->MoveSpec.Drag * dP.XY;
	ddA -= Entity->Stored->MoveSpec.Drag * 0.7f * dA;

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

	Entity->Sim->ddP = ddP;
	Entity->Stored->dP = dP;
	Entity->Sim->P = P;

	if(Entity->Stored->MoveSpec.AllowRotation)
	{
		Entity->Sim->ddA = ddA;
		Entity->Stored->dA = dA;
		Entity->Stored->A = A;
		Entity->Stored->R = R;
	}
}

	internal_function void
UpdateFamiliarPairwise(entity* Familiar, entity* Target)
{
	if(Target->Stored->Type == EntityType_Player)
	{
		f32 DistanceToPlayerSquared = LenghtSquared(Target->Sim->P - Familiar->Sim->P);
		if(DistanceToPlayerSquared < Familiar->Stored->BestDistanceToPlayerSquared &&
			 DistanceToPlayerSquared > Square(2.0f))
		{
			Familiar->Stored->BestDistanceToPlayerSquared = DistanceToPlayerSquared;
			Familiar->Sim->MovingDirection = Normalize(Target->Sim->P - Familiar->Sim->P).XY;
		}
	}
}

	internal_function void
UpdateSwordPairwise(entity* Sword, entity* Target, b32 Intersect,
										memory_arena* WorldArena, world_map* WorldMap)
{
	if(Intersect &&
		 Target->Stored->Type != EntityType_Player)
	{
		//TODO(bjorn): Is it safe to let high entities just disappear in the middle of a loop?
		//TODO(bjorn): Also, the fact that this needs to be done is
		//non-obvious. As is the fact that calling SetCamera doesn't work as
		//well.
		//TODO IMPORTANT this does not work anymore since the world pos gets
		//overwritten when the SimRegion gets released!!!
		ChangeStoredEntityWorldLocation(WorldArena, WorldMap, Sword->Stored, 0);
	}
}

#define ENTITY_H
#endif
