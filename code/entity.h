#if !defined(ENTITY_H)

#if !defined(ENTITY_H_FIRST_PASS)
struct entity;

union entity_reference
{
	u32 Index;
	entity* Ptr;
};

#include "trigger.h"

struct entity
{
	u32 StorageIndex;
	world_map_position WorldP;
	b32 Updates : 1;
	b32 EnityHasBeenProcessedAlready : 1;

	b32 IsSpacial : 1;
	b32 Collides : 1;
	b32 Attached : 1;
	//NOTE(bjorn): CarFrame
	//TODO(bjorn): Use this to move out the turning code to the cars update loop.
	b32 AutoPilot : 1;

	//TODO(casey): Generation index so we know how "up to date" the entity is.

	u32 OldestTrigStateIndex;
	//NOTE Must be a power of two.
	trigger_state TrigStates[8];

	entity_visual_type VisualType;

	v3 R;
	f32 A;
	f32 dA;
	f32 ddA;

	v3 P;
	v3 dP;
	v3 ddP;

	v3 Dim;

	f32 Mass;
	move_spec MoveSpec;
	f32 GroundFriction;

	u32 FacingDirection;

	//TODO(casey): Should hitpoints themselves be entities?
	u32 HitPointMax;
	hit_point HitPoints[16];

	u32 TriggerDamage;

	//NOTE(bjorn): Stair
	f32 dZ;

	union
	{
		entity_reference EntityReferences[16];
		struct
		{
			union
			{
				entity_reference Attatchments[8];
				struct
				{
					//NOTE(bjorn): Car-parts
					entity_reference Vehicle;
					//NOTE(bjorn): CarFrame
					entity_reference Wheels[4];
					entity_reference DriverSeat;
					entity_reference Engine;

				};
			};

			//NOTE(bjorn): Player
			entity_reference Sword;
			entity_reference RidingVehicle;

			entity_reference Prey;
			entity_reference Struct_Terminator;
		};
	};

	//NOTE(bjorn): Sword
	f32 DistanceRemaining;

	//NOTE(bjorn): Familiar
	f32 HunterSearchRadius;
	f32 BestDistanceToPreySquared;
};

#include "trigger.h"

	inline void
MakeEntitySpacial(entity* Entity, v3 R, v3 P, v3 dP)
{
	Entity->IsSpacial = true;
	Entity->R = R;
	Entity->P = P;
	Entity->dP = dP;
}

	inline void
MakeEntityNonSpacial(entity* Entity)
{
	Entity->IsSpacial = false;
#if HANDMADE_INTERNAL
	ActivateSignalingNaNs();
	v3 InvalidV3 = {signaling_not_a_number32, 
									signaling_not_a_number32, 
									signaling_not_a_number32 };
#elif
	v3 InvalidV3 = {-10000, -10000, -10000};
#endif
	Entity->R = InvalidV3;
	Entity->P = InvalidV3;
	Entity->dP = InvalidV3;
}

#define ENTITY_H_FIRST_PASS
#else

struct vertices
{
	u32 Count;
	v3 Verts[16];
};

internal_function vertices
GetEntityVertices(entity* Entity)
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

inline v3
DefaultEntityOrientation()
{
	return {0, 1, 0};
}

//TODO IMPORTANT(bjorn): Update the initialization code and the adding of
//entities!!! Figure out how to set the camera to reference the player.
	internal_function entity*
AddEntity(sim_region* SimRegion, entity_visual_type VisualType, v3 P = {})
{
	entity* Entity = AddBrandNewSimEntity(SimRegion);

	Entity->P = P;
	Entity->R = DefaultEntityOrientation();
	Entity->VisualType = VisualType;

	return Entity;
}

	internal_function void
AddHitPoints(stored_entity* Entity, u32 HitPointMax)
{
	Assert(HitPointMax <= ArrayCount(Entity->Sim.HitPoints));
	Entity->Sim.HitPointMax = HitPointMax;
	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity->Sim.HitPointMax;
			HitPointIndex++)
	{
		Entity->Sim.HitPoints[HitPointIndex].FilledAmount = HIT_POINT_SUB_COUNT;
	}
}

	internal_function stored_entity*
AddSword(memory_arena* WorldArena, world_map* WorldMap, stored_entities* Entities)
{
	stored_entity* Entity = AddEntity(WorldArena, WorldMap, Entities, EntityVisualType_Sword);

	Entity->Sim.Dim = v2{0.4f, 1.5f} * WorldMap->TileSideInMeters;
	Entity->Sim.Mass = 8.0f;

	Entity->Sim.MoveSpec = {};
	Entity->Sim.Collides = false;

	Entity->Sim.TriggerDamage = 1;

	return Entity;
}

	internal_function stored_entity*
AddPlayer(memory_arena* WorldArena, world_map* WorldMap, 
					stored_entities* Entities, world_map_position InitP)
{
	stored_entity* Entity = AddEntity(WorldArena, WorldMap, Entities, EntityVisualType_Player, InitP);

	Entity->Sim.Dim = v2{0.5f, 0.3f} * WorldMap->TileSideInMeters;
	Entity->Sim.Collides = true;

	//TODO(bjorn): Why does weight differences matter so much in the collision system.
	Entity->Sim.Mass = 40.0f / 8.0f;

	Entity->Sim.MoveSpec.EnforceVerticalGravity = true;
	Entity->Sim.MoveSpec.EnforceHorizontalMovement = true;
	Entity->Sim.MoveSpec.Speed = 85.f;
	Entity->Sim.MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(Entity, 6);
	stored_entity* Sword = AddSword(WorldArena, WorldMap, Entities);
	Entity->Sim.Sword.Index = Sword->Sim.StorageIndex;

	return Entity;
}

	internal_function stored_entity*
AddMonstar(memory_arena* WorldArena, world_map* WorldMap, stored_entities* Entities,
					 world_map_position InitP)
{
	stored_entity* Entity = AddEntity(WorldArena, WorldMap, Entities, EntityVisualType_Monstar, InitP);

	Entity->Sim.Dim = v2{0.5f, 0.3f} * WorldMap->TileSideInMeters;
	Entity->Sim.Collides = true;

	Entity->Sim.Mass = 40.0f / 8.0f;

	Entity->Sim.MoveSpec.EnforceHorizontalMovement = true;
	Entity->Sim.MoveSpec.Speed = 85.f * 0.75;
	Entity->Sim.MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(Entity, 3);

	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity->Sim.HitPointMax;
			HitPointIndex++)
	{
		Entity->Sim.HitPoints[HitPointIndex].FilledAmount = (u8)(HIT_POINT_SUB_COUNT - HitPointIndex);
	}

	return Entity;
}

	internal_function stored_entity*
AddFamiliar(memory_arena* WorldArena, world_map* WorldMap, stored_entities* Entities, 
						world_map_position InitP)
{
	stored_entity* Entity = AddEntity(WorldArena, WorldMap, Entities, 
																		EntityVisualType_Familiar, InitP);

	Entity->Sim.Dim = v2{0.5f, 0.3f} * WorldMap->TileSideInMeters;
	Entity->Sim.Collides = true;

	Entity->Sim.Mass = 40.0f / 8.0f;

	Entity->Sim.MoveSpec.EnforceHorizontalMovement = true;
	Entity->Sim.MoveSpec.Speed = 85.f * 0.7f;
	Entity->Sim.MoveSpec.Drag = 0.2f * 30.0f;

	Entity->Sim.HunterSearchRadius = 6.0f;

	return Entity;
}

#if 0
	internal_function entity
AddCarFrame(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityVisualType_CarFrame, &WorldPos);

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
														EntityVisualType_Engine, &WorldPos);

	Entity.Stored->Dim = v2{3, 2};
	Entity.Stored->MoveSpec = DefaultMoveSpec();

	return Entity;
}

	internal_function entity
AddWheel(game_state* GameState, world_map_position WorldPos)
{
	entity Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, &GameState->Entities,
														EntityVisualType_Wheel, &WorldPos);

	Entity.Stored->Dim = v2{0.6f, 1.5f};
	Entity.Stored->MoveSpec = DefaultMoveSpec();

	return Entity;
}

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
	Assert(Car->Low->VisualType == EntityVisualType_CarFrame);

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
	Assert(Car->Low->VisualType == EntityVisualType_CarFrame);

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
AddGround(memory_arena* WorldArena, world_map* WorldMap, stored_entities* Entities, 
					world_map_position WorldPos)
{
	stored_entity* Entity = AddEntity(WorldArena, WorldMap, Entities,
																		EntityVisualType_Ground, WorldPos);

	Entity->Sim.Dim = v2{1, 1} * WorldMap->TileSideInMeters;
	Entity->Sim.Collides = false;

	return Entity;
}

	internal_function stored_entity*
AddWall(memory_arena* WorldArena, world_map* WorldMap, stored_entities* Entities, 
				world_map_position WorldPos, f32 Mass = 1000.0f)
{
	stored_entity* Entity = AddEntity(WorldArena, WorldMap, Entities, EntityVisualType_Wall, WorldPos);

	Entity->Sim.Dim = v2{1, 1} * WorldMap->TileSideInMeters;
	Entity->Sim.Collides = true;

	Entity->Sim.Mass = Mass;
	Entity->Sim.MoveSpec = DefaultMoveSpec();

	return Entity;
}

#if 0
	internal_function stored_entity*
AddStair(game_state* GameState, world_map_position WorldPos, f32 dZ)
{
	stored_entity* Entity = AddEntity(&GameState->WorldArena, GameState->WorldMap, 
																		&GameState->Entities,
																		EntityVisualType_Stair, &WorldPos);

	Entity->Dim = v3{1, 1, 1} * GameState->WorldMap->TileSideInMeters;
	Entity->Collides = true;
	Entity->dZ = dZ;

	return Entity;
}
#endif

	internal_function void
MoveEntity(entity* Entity, f32 dT)
{
	Assert(Entity->IsSpacial);

	v3 P = Entity->P;
	v3 dP = Entity->dP;
	v3 ddP = Entity->ddP;

	v3 R = Entity->R;
	f32 A = Entity->A;
	f32 dA = Entity->dA;
	f32 ddA = Entity->ddA;

	if(Entity->MoveSpec.EnforceVerticalGravity)
	{
		if(Entity->MoveSpec.MovingDirection.Z > 0 &&
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

	if(Entity->MoveSpec.EnforceHorizontalMovement)
	{
		//TODO(casey): ODE here!
		ddP.XY = Entity->MoveSpec.MovingDirection.XY * Entity->MoveSpec.Speed;
	}

	ddP.XY -= Entity->MoveSpec.Drag * dP.XY;
	ddA -= Entity->MoveSpec.Drag * 0.7f * dA;

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

	Entity->ddP = {};
	Entity->dP = dP;
	Entity->P = P;

	if(Entity->MoveSpec.AllowRotation)
	{
		Entity->ddA = 0;
		Entity->dA = dA;
		Entity->A = A;
		Entity->R = R;
	}
}

	internal_function void
HunterLogic(entity* Hunter)
{
	if(Hunter->Prey.Ptr)
	{
		entity* Prey = Hunter->Prey.Ptr;

		v3 DistanceToPrey = Prey->P - Hunter->P;
		f32 DistanceToPreySquared = LenghtSquared(DistanceToPrey);

		if(DistanceToPreySquared < Hunter->BestDistanceToPreySquared &&
			 DistanceToPreySquared > Square(2.0f))
		{
			Hunter->BestDistanceToPreySquared = DistanceToPreySquared;
			Hunter->MoveSpec.MovingDirection = Normalize(DistanceToPrey).XY;
		}
	}
}

//TODO IMPORTANT: What is the best way to apply a force over time to make this a smooth bounce?
	internal_function void
BounceRaw(entity* Flyer, entity* Other)
{
	Assert(Flyer->DistanceRemaining);
	Flyer->dP = Lenght(Flyer->dP) * Normalize(Flyer->P - Other->P);
}

	internal_function void
Bounce(entity* A, entity* B)
{
	if(A->DistanceRemaining) { BounceRaw(A, B); }
	if(B->DistanceRemaining) { BounceRaw(B, A); }
}

	internal_function void
ApplyDamageRaw(entity* Assailant, entity* Victim)
{
	Assert(Assailant->TriggerDamage && Victim->HitPointMax);

	if(Victim->HitPointMax >= Assailant->TriggerDamage)
	{
		Victim->HitPointMax -= Assailant->TriggerDamage;
	}
	else
	{
		Victim->HitPointMax = 0;
	}
}
	internal_function void
ApplyDamage(entity* A, entity* B)
{
	if(A->TriggerDamage && B->HitPointMax) { ApplyDamageRaw(A, B); }
	if(B->TriggerDamage && A->HitPointMax) { ApplyDamageRaw(B, A); }
}
#define ENTITY_H
#endif //ENTITY_H_FIRST_SECOND

#endif //ENTITY_H
