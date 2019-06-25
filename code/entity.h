#if !defined(ENTITY_H)

#if !defined(ENTITY_H_FIRST_PASS)

struct move_spec
{
	b32 EnforceFloor : 1;
	b32 EnforceHorizontalMovement : 1;
	b32 EnforceVerticalGravity : 1;
	b32 AllowRotation : 1;
	b32 MoveByInput : 1;

	f32 Speed;
	f32 Drag;
	f32 AngularDrag;

	//TODO(bjorn): Could this just be ddP?
	v3 MovingDirection;
};

internal_function move_spec
DefaultMoveSpec()
{
	move_spec Result = {};

	Result.AllowRotation = true;

	Result.EnforceFloor = true;
	Result.EnforceHorizontalMovement = true;
	Result.Drag = 0.4f * 30.0f;

	return Result;
}

enum entity_visual_type
{
	EntityVisualType_NotRendered,
	EntityVisualType_Player,
	EntityVisualType_Wall,
	EntityVisualType_Stair,
	EntityVisualType_Ground,
	EntityVisualType_Wheel,
	EntityVisualType_CarFrame,
	EntityVisualType_Engine,
	EntityVisualType_Monstar,
	EntityVisualType_Familiar,
	EntityVisualType_Sword,
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	//TODO(casey): Bake this down into one variable.
	u8 Flags;
	u8 FilledAmount;
};

struct entity;

typedef entity* entity_reference;

typedef game_controller* controller_reference;
typedef game_keyboard* keyboard_reference;
typedef game_mouse* mouse_reference;

#include "trigger.h"

struct entity
{
	//TODO(bjorn): Minimize this to a 32-bit index when actually storing the entity.
	u64 StorageIndex;
	world_map_position WorldP;
	b32 Updates : 1;

	b32 IsSpacial : 1;
	b32 Collides : 1;
	b32 Attached : 1;
	//NOTE(bjorn): CarFrame
	//TODO(bjorn): Use this to move out the turning code to the cars update loop.
	b32 AutoPilot : 1;

	u32 EntityPairUpdateGenerationIndex;
	//TODO(casey): Generation index so we know how "up to date" the entity is.

	u32 OldestTrigStateIndex;
	//NOTE Must be a power of two.
	trigger_state TrigStates[8];

	entity_visual_type VisualType;

	controller_reference Controller;
	keyboard_reference Keyboard;
	mouse_reference Mouse;

	v3 R;
	f32 A;
	f32 dA;
	f32 ddA;

	v3 P;
	v3 dP;
	v3 ddP;

	v3 Dim;

	v2 CamRot;
	f32 CamZoom;

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
					//entity_reference Vehicle;
					//NOTE(bjorn): CarFrame
					//entity_reference Wheels[4];
					//entity_reference DriverSeat;
					//entity_reference Engine;
					entity_reference struct_terminator0_;
				};
			};
			//NOTE(bjorn): Camera
			entity_reference Player;
			entity_reference FreeMover;
			//NOTE(bjorn): Player
			entity_reference Sword;
			//entity_reference RidingVehicle;

			entity_reference Prey;
			entity_reference struct_terminator1_;
		};
	};

	//NOTE(bjorn): Sword
	f32 DistanceRemaining;

	//NOTE(bjorn): Familiar
	b32 StickToPrey;
	f32 HunterSearchRadius;
	f32 BestDistanceToPreySquared;
	f32 MinimumHuntRangeSquared;
};

#include "trigger.h"

inline v3
DefaultEntityOrientation()
{
	return {0, 1, 0};
}

	inline void
MakeEntitySpacial(entity* Entity, v3 P, v3 dP = {}, v3 R = DefaultEntityOrientation())
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
GetEntityVerticesRaw(v3 R, v3 P, v3 Dim)
{
	vertices Result = {};
	Result.Count = 8;

	v2 OrderOfCorners[4] = {{-0.5f,  0.5f}, 
													{ 0.5f,  0.5f}, 
													{ 0.5f, -0.5f}, 
													{-0.5f, -0.5f}};
	
	m22 Transform = M22ByCol(CW90M22() * R.XY, R.XY);
	for(u32 VertexIndex = 0; 
			VertexIndex < Result.Count/2; 
			VertexIndex++)
	{
		Result.Verts[VertexIndex] = (Transform * Hadamard(OrderOfCorners[VertexIndex], Dim.XY) + P);

		Result.Verts[VertexIndex].Z += Dim.Z * 0.5f;
	}

	for(u32 VertexIndex = 0; 
			VertexIndex < Result.Count/2; 
			VertexIndex++)
	{
		Result.Verts[Result.Count/2 + VertexIndex] = 
			(Transform * Hadamard(OrderOfCorners[VertexIndex], Dim.XY) + P);
		Result.Verts[Result.Count/2 + VertexIndex].Z -= Dim.Z * 0.5f;
	}

	return Result;
}
internal_function vertices
GetEntityVertices(entity* Entity)
{
	return GetEntityVerticesRaw(Entity->R, Entity->P, Entity->Dim);
}

	internal_function entity*
AddEntity(sim_region* SimRegion, entity_visual_type VisualType)
{
	Assert(SimRegion->EntityCount < SimRegion->EntityMaxCount);

	entity* Entity = SimRegion->Entities + SimRegion->EntityCount++;
	*Entity = {};

	Entity->WorldP = WorldMapNullPos();
	Entity->VisualType = VisualType;
	MakeEntityNonSpacial(Entity);

	return Entity;
}

	internal_function entity*
AddEntity(sim_region* SimRegion, entity_visual_type VisualType, v3 P)
{
	entity* Entity = AddEntity(SimRegion, VisualType);

	MakeEntitySpacial(Entity, P);

	return Entity;
}

	internal_function entity*
AddCamera(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_NotRendered, InitP);

	Entity->MoveSpec.EnforceHorizontalMovement = false;
	Entity->MoveSpec.Speed = 85.f;
	Entity->MoveSpec.Drag = 0.24f * 30.0f;

	Entity->StickToPrey = true;
	Entity->HunterSearchRadius = positive_infinity32;
	Entity->MinimumHuntRangeSquared = Square(0.2f);

	Entity->CamZoom = 1.0f;

	return Entity;
}

	internal_function void
AddHitPoints(entity* Entity, u32 HitPointMax)
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

	internal_function entity*
AddInvisibleControllable(sim_region* SimRegion)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_NotRendered);

	Entity->MoveSpec.MoveByInput = true;
	Entity->MoveSpec.EnforceHorizontalMovement = false;
	Entity->MoveSpec.Speed = 80.f * 2.0f;
	Entity->MoveSpec.Drag = 0.24f * 20.0f;

	return Entity;
}

	internal_function entity*
AddSword(sim_region* SimRegion)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Sword);

	Entity->Dim = v3{0.4f, 1.5f, 0.1f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Mass = 8.0f;

	Entity->TriggerDamage = 1;

	return Entity;
}

	internal_function entity*
AddPlayer(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Player, InitP);

	Entity->Dim = v3{0.5f, 0.3f, 1.0f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	//TODO(bjorn): Why does weight differences matter so much in the collision system.
	Entity->Mass = 40.0f / 8.0f;

	Entity->MoveSpec.MoveByInput = true;
	Entity->MoveSpec.EnforceVerticalGravity = true;
	Entity->MoveSpec.EnforceHorizontalMovement = true;
	Entity->MoveSpec.Speed = 85.f;
	Entity->MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(Entity, 6);
	Entity->Sword = AddSword(SimRegion);

	return Entity;
}

	internal_function entity*
AddMonstar(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Monstar, InitP);

	Entity->Dim = v3{0.5f, 0.3f, 1.0f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->Mass = 40.0f / 8.0f;

	Entity->MoveSpec.EnforceHorizontalMovement = true;
	Entity->MoveSpec.Speed = 85.f * 0.75;
	Entity->MoveSpec.Drag = 0.24f * 30.0f;

	AddHitPoints(Entity, 3);

	for(u32 HitPointIndex = 0;
			HitPointIndex < Entity->HitPointMax;
			HitPointIndex++)
	{
		Entity->HitPoints[HitPointIndex].FilledAmount = (u8)(HIT_POINT_SUB_COUNT - HitPointIndex);
	}

	return Entity;
}

	internal_function entity*
AddFamiliar(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Familiar, InitP);

	Entity->Dim = v3{0.5f, 0.3f, 1.0f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->Mass = 40.0f / 8.0f;

	Entity->MoveSpec.EnforceHorizontalMovement = true;
	Entity->MoveSpec.Speed = 85.f * 0.7f;
	Entity->MoveSpec.Drag = 0.2f * 30.0f;

	Entity->HunterSearchRadius = 6.0f;
	Entity->MinimumHuntRangeSquared = Square(2.0f);

	return Entity;
}

	internal_function entity*
AddGround(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Ground, InitP);

	Entity->Dim = v3{1, 1, 0.1f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = false;

	return Entity;
}

	internal_function entity*
AddWall(sim_region* SimRegion, v3 InitP, f32 Mass = 1000.0f)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Wall, InitP);

	Entity->Dim = v3{1, 1, 1} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->Mass = Mass;
	Entity->MoveSpec = DefaultMoveSpec();

	return Entity;
}

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

	move_spec MoveSpec = Entity->MoveSpec;
	if(MoveSpec.EnforceVerticalGravity)
	{
		if(MoveSpec.MovingDirection.Z > 0 &&
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

	if(MoveSpec.EnforceHorizontalMovement)
	{
		//TODO(casey): ODE here!
		ddP.XY = MoveSpec.MovingDirection.XY * MoveSpec.Speed;
		ddP.XY -= MoveSpec.Drag * dP.XY;
	}
	else
	{
		ddP = MoveSpec.MovingDirection * MoveSpec.Speed;
		ddP -= MoveSpec.Drag * dP;
	}

	ddA -= MoveSpec.Drag * 0.7f * dA;

	P += 0.5f * ddP * Square(dT) + dP * dT;
	dP += ddP * dT;

	A += 0.5f * ddA * Square(dT) + dA * dT;
	dA += ddA * dT;
	A = Modulate0(A, tau32);

	R.XY = CCWM22(A) * DefaultEntityOrientation().XY;
	R = Normalize(R);

	//TODO(bjorn): Think harder about how to implement the ground.
	if(MoveSpec.EnforceFloor &&
		 P.Z < 0.0f)
	{
		P.Z = 0.0f;
		dP.Z = 0.0f;
	}

	Entity->ddP = {};
	Entity->dP = dP;
	Entity->P = P;

	if(MoveSpec.AllowRotation)
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
	if(Hunter->Prey)
	{
		entity* Prey = Hunter->Prey;

		v3 DistanceToPrey = Prey->P - Hunter->P;
		f32 DistanceToPreySquared = LenghtSquared(DistanceToPrey);

		if(DistanceToPreySquared < Hunter->BestDistanceToPreySquared &&
			 DistanceToPreySquared > Hunter->MinimumHuntRangeSquared)
		{
			Hunter->BestDistanceToPreySquared = DistanceToPreySquared;
			Hunter->MoveSpec.MovingDirection = Normalize(DistanceToPrey).XY;
		}

		if(Hunter->StickToPrey &&
			 DistanceToPreySquared <= Hunter->MinimumHuntRangeSquared)
		{
			Hunter->P = Prey->P;
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
