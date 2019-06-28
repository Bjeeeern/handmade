#if !defined(ENTITY_H)

#if !defined(ENTITY_H_FIRST_PASS)

struct move_spec
{
	b32 AllowRotation : 1;
	b32 MoveOnInput : 1;

	v3 Gravity;
	f32 Damping;

	f32 Speed;
	f32 Drag_k1;
	f32 Drag_k2;
	//f32 AngularDrag;

	//TODO(bjorn): Could this just be ddP?
	v3 MovingDirection;
};

internal_function move_spec
DefaultMoveSpec()
{
	move_spec Result = {};

	Result.Damping = 0.9999f;

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
#include "attachment.h"

struct entity
{
	//TODO(bjorn): Minimize this to a 32-bit index when actually storing the entity.
	u64 StorageIndex;
	world_map_position WorldP;
	b32 Updates : 1;

	b32 IsSpacial : 1;
	b32 Collides : 1;
	b32 IsFloor : 1;
	b32 IsCamera : 1;
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

	q O;
	v3 dO;
	v3 ddO;

	v3 P;
	v3 dP;
	v3 ddP;
	v3 F;

	v3 Dim;

	v2 CamRot;
	f32 CamZoom;

	f32 iM; //NOTE(bjorn): Inverse mass
	f32 Restitution;
	f32 GroundFriction;

	move_spec MoveSpec;

	u32 FacingDirection;

	//TODO(casey): Should hitpoints themselves be entities?
	u32 HitPointMax;
	hit_point HitPoints[16];

	u32 TriggerDamage;

	//NOTE(bjorn): Stair
	f32 dZ;

	union
	{
#define ENTITY_REFERENCES 16
		entity_reference EntityReferences[ENTITY_REFERENCES];
		struct
		{
			union
			{
#define ENTITY_ATTACHMENTS (ENTITY_REFERENCES / 2)
				entity_reference Attachments[ENTITY_ATTACHMENTS];
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
			//TODO STUDY(bjorn): Does a parent-child entity grouping have in anyway
			//be a dependency on the attachment system? Maybe I could keep them
			//somewhat separable.
			entity_reference Parent;

			//TODO(bjorn): I use these to identify the camera and the player rigth
			//now. Maybe the player is just undefined and the real "player" could in
			//principle play any entity in the game? The camera should maybe just be
			//its own system together with a flag.
			//TODO STUDY(bjorn): How do I distribute the references among the
			//systems? Does a system just hog the first best reference spot or should
			//each system just have the neccesary references they need as it is right
			//now? This goes for the attachments too I feel...
			entity_reference Player;
			entity_reference FreeMover;
			//NOTE(bjorn): Player
			entity_reference Sword;
			entity_reference Prey;

			entity_reference struct_terminator1_;
		};
	};
	u32 AttachmentCount;
	attachment_info AttachmentInfo[ENTITY_ATTACHMENTS];

	//NOTE(bjorn): Sword
	f32 DistanceRemaining;

	//NOTE(bjorn): Familiar
	b32 StickToPrey;
	f32 HunterSearchRadius;
	f32 BestDistanceToPreySquared;
	f32 MinimumHuntRangeSquared;

	//NOTE(bjorn): Force field
	f32 ForceFieldRadiusSquared;
	f32 ForceFieldStrenght;
};

#include "trigger.h"
#include "attachment.h"

	inline void
MakeEntitySpacial(entity* Entity, v3 P, v3 dP = {}, q O = QuaternionIdentity(), v3 dO = {})
{
	Entity->IsSpacial = true;
	Entity->O = O;
	Entity->dO = dO;
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
	v4 InvalidV4 = {signaling_not_a_number32, 
									signaling_not_a_number32, 
									signaling_not_a_number32,
									signaling_not_a_number32};
#elif
	v3 InvalidV3 = {-10000, -10000, -10000};
	v3 InvalidV4 = {-10000, -10000, -10000, -10000};
#endif
	Entity->O = InvalidV4;
	Entity->dO = InvalidV3;
	Entity->P = InvalidV3;
	Entity->dP = InvalidV3;
}

#define ENTITY_H_FIRST_PASS
#else

struct aabb_verts_result
{
	u32 Count;
	v3 Verts[8];
};

internal_function aabb_verts_result
GetAABBVertices(m44* ObjToWorldTransform)
{
	aabb_verts_result Result = {};
	Result.Count = 8;

	v3 UnscaledAABB[8] = {{-0.5f,  0.5f, 0.5f}, 
												{ 0.5f,  0.5f, 0.5f}, 
												{ 0.5f, -0.5f, 0.5f}, 
												{-0.5f, -0.5f, 0.5f},
												{-0.5f,  0.5f,-0.5f}, 
												{ 0.5f,  0.5f,-0.5f}, 
												{ 0.5f, -0.5f,-0.5f}, 
												{-0.5f, -0.5f,-0.5f}};

	for(u32 CornerIndex = 0; 
			CornerIndex < ArrayCount(UnscaledAABB); 
			CornerIndex++)
	{
		v3 Corner = UnscaledAABB[CornerIndex];
		Result.Verts[CornerIndex] = *ObjToWorldTransform * Corner;
	}

	return Result;
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
	Entity->MoveSpec = DefaultMoveSpec();

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

	Entity->MoveSpec.Speed = 80.f * 2.0f;
	Entity->MoveSpec.Damping = 0.98f;

	Entity->StickToPrey = true;
	Entity->HunterSearchRadius = positive_infinity32;
	Entity->MinimumHuntRangeSquared = Square(0.2f);

	Entity->IsCamera = true;
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

	Entity->MoveSpec.MoveOnInput = true;
	Entity->MoveSpec.Speed = 80.f * 2.0f;
	Entity->MoveSpec.Damping = 0.98f;

	return Entity;
}

	internal_function entity*
AddSword(sim_region* SimRegion)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Sword);

	Entity->Dim = v3{1.5f, 0.4f, 0.1f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->iM = SafeRatio0(1.0f, 8.0f);

	Entity->TriggerDamage = 1;

	Entity->MoveSpec.Drag_k1 = 0.3f;
	Entity->MoveSpec.Drag_k2 = 0.1f;

	return Entity;
}

internal_function void
SetStandardFrictionForGroundObjects(move_spec* MoveSpec)
{
	MoveSpec->Drag_k1 = 400.0f;
	MoveSpec->Drag_k2 = 5.0f;
}

	internal_function entity*
AddPlayer(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Player, InitP);

	Entity->Dim = v3{0.5f, 0.3f, 1.0f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	//TODO(bjorn): Why does weight differences matter so much in the collision system.
	Entity->iM = SafeRatio0(1.0f, 70.0f);

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 20.0f;
	Entity->MoveSpec.MoveOnInput = true;
	Entity->MoveSpec.Speed = 85.f;
	SetStandardFrictionForGroundObjects(&Entity->MoveSpec);

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

	Entity->iM = SafeRatio0(1.0f, 70.0f);

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 20.0f;
	Entity->MoveSpec.Speed = 85.f * 0.75;
	SetStandardFrictionForGroundObjects(&Entity->MoveSpec);

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

	Entity->iM = SafeRatio0(1.0f, 40.0f / 8.0f);
	Entity->Restitution = 0.5f;

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 20.0f;
	Entity->MoveSpec.Speed = 85.f * 0.7f;
	SetStandardFrictionForGroundObjects(&Entity->MoveSpec);

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
AddFloor(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_NotRendered, InitP);

	Entity->Dim = v3{50, 50, 1} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;
	Entity->IsFloor = true;

	Entity->iM = 0.0f;

	return Entity;
}

	internal_function entity*
AddFixture(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Sword, InitP);

	Entity->Dim = v3{1, 1, 1} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->iM = 0.0f;

	return Entity;
}

	internal_function entity*
AddWall(sim_region* SimRegion, v3 InitP, f32 Mass = 1000.0f)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Wall, InitP);

	Entity->Dim = v3{1, 1, 1} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;

	Entity->iM = SafeRatio0(1.0f, Mass);

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 20.0f;
	SetStandardFrictionForGroundObjects(&Entity->MoveSpec);

	return Entity;
}

	internal_function entity*
AddForceField(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Sword, InitP);

	Entity->Dim = v3{0.1f, 0.1f, 0.1f} * SimRegion->WorldMap->TileSideInMeters;
	Entity->Collides = true;
	Entity->iM = 0.0f;

	Entity->ForceFieldRadiusSquared = Square(10.0f);
	Entity->ForceFieldStrenght = 0.0f;

	return Entity;
}

	internal_function entity*
AddParticle(sim_region* SimRegion, v3 InitP, f32 Mass = 0.0f)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Wall, InitP);

	Entity->Dim = v3{1, 1, 1} * SimRegion->WorldMap->TileSideInMeters * Mass/100.0f;
	Entity->Collides = true;

	Entity->iM = SafeRatio0(1.0f, Mass);
	Entity->Restitution = 0.9f;

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 10.0f;
	Entity->MoveSpec.Drag_k1 = 0.0f;
	Entity->MoveSpec.Drag_k2 = 0.5f;

	return Entity;
}

	internal_function void
MoveEntity(entity* Entity, f32 dT)
{
	Assert(Entity->IsSpacial);
	v3 P = Entity->P;
	v3 dP = Entity->dP;
	v3 ddP = Entity->ddP;
	v3 F = Entity->F;

	q O = Entity->O;
	v3 dO = Entity->dO;
	v3 ddO = Entity->ddO;

	move_spec* MoveSpec = &Entity->MoveSpec;

	if(Entity->iM)
	{ 
		if(MoveSpec->Drag_k1 || MoveSpec->Drag_k2)
		{
			f32 dP_m = Length(dP);
			F -= dP * (MoveSpec->Drag_k1 + MoveSpec->Drag_k2 * dP_m);
		}

		ddP += F * Entity->iM;
		//TODO(bjorn): Why do i need the gravity to be so heavy? Because of upscaling?
		ddP += MoveSpec->Gravity;
	}
	//TODO(casey): ODE here!
	ddP += MoveSpec->MovingDirection * MoveSpec->Speed;

	//TODO(bjorn): Damping with precalculated Pow(Damping, dT) per update step to
	//support different frame rates.
	dP = dP * MoveSpec->Damping + ddP * dT;
	P = P + dP * dT;

	dO = dO * MoveSpec->Damping + ddO * dT;
	O = UpdateOrientationByAngularVelocity(O, dO*dT);

	Entity->F = {};
	Entity->ddP = {};
	Entity->dP = dP;
	Entity->P = P;

	Entity->ddO = {};
	Entity->dO = dO;
	Entity->O = QuaternionNormalize(O);
#if 0
	ddA -= MoveSpec->Drag * 0.7f * dA;

	A += 0.5f * ddA * Square(dT) + dA * dT;
	dA += ddA * dT;
	A = Modulate0(A, tau32);

	R.XY = CCWM22(A) * DefaultEntityOrientation().XY;
	R = Normalize(R);

	if(MoveSpec->AllowRotation)
	{
		Entity->ddA = 0;
		Entity->dA = dA;
		Entity->A = A;
		Entity->R = R;
	}
#endif
}

	internal_function void
HunterLogic(entity* Hunter)
{
	if(Hunter->Prey)
	{
		entity* Prey = Hunter->Prey;

		v3 DistanceToPrey = Prey->P - Hunter->P;
		f32 DistanceToPreySquared = LengthSquared(DistanceToPrey);

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

	internal_function void
ForceFieldLogicRaw(entity* ForceField, entity* Other)
{
	v3 ToCenter = ForceField->P - Other->P;
	if(LengthSquared(ToCenter) < ForceField->ForceFieldRadiusSquared)
	{
		Other->F += ToCenter * ForceField->ForceFieldStrenght;
	}
}

	internal_function void
ForceFieldLogic(entity* A, entity* B)
{
	if(A->ForceFieldRadiusSquared && !B->ForceFieldRadiusSquared) { ForceFieldLogicRaw(A, B); }
	if(B->ForceFieldRadiusSquared && !A->ForceFieldRadiusSquared) { ForceFieldLogicRaw(B, A); }
}

	//TODO(bjorn): Think harder about how to implement the ground.
	internal_function void
FloorLogicRaw(entity* Floor, entity* Other)
{
	f32 FloorZ = (Floor->P.Z + Floor->Dim.Z * 0.5f);
	if(Other->P.Z < FloorZ,
		 IsInRectangle(RectCenterDim(Floor->P, Floor->Dim), Other->P))
	{
		Other->P.Z = FloorZ;
		Other->dP.Z = 0.0f;
	}
}

	internal_function void
FloorLogic(entity* A, entity* B)
{
	if(A->IsFloor && !B->IsFloor) { FloorLogicRaw(A, B); }
	if(B->IsFloor && !A->IsFloor) { FloorLogicRaw(B, A); }
}

//TODO IMPORTANT: What is the best way to apply a force over time to make this a smooth bounce?
	internal_function void
BounceRaw(entity* Flyer, entity* Other)
{
	Assert(Flyer->DistanceRemaining);
	Flyer->dP = Length(Flyer->dP) * Normalize(Flyer->P - Other->P);
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

//TODO(bjorn): This feels like a future camera.h
internal_function void
AttachToPlayer(entity* Camera)
{
	Assert(Camera->IsCamera);
	Assert(Camera->Player && Camera->FreeMover);
	Assert(Camera->Player != Camera->Prey)

	MakeEntityNonSpacial(Camera->FreeMover);
	Camera->Player->MoveSpec.MoveOnInput = true;
	Camera->Prey = Camera->Player;

	if(!Camera->Player->Updates)
	{
		Camera->P = Camera->Player->P;
	}
}

	internal_function void
DetachToMoveFreely(entity* Camera)
{
	Assert(Camera->IsCamera);
	Assert(Camera->Player && Camera->FreeMover);
	Assert(Camera->FreeMover != Camera->Prey);

	MakeEntitySpacial(Camera->FreeMover, Camera->P);
	Camera->Player->MoveSpec.MoveOnInput = false;
	Camera->Prey = Camera->FreeMover;
}

	internal_function entity*
FindPlayerInSimUpdateRegion(sim_region* SimRegion, entity* Camera)
{
	Assert(Camera->IsCamera);
	Assert(Camera->FreeMover);

	entity* Result = 0;

	entity* Entity = SimRegion->Entities;
	for(u32 EntityIndex = 0;
			EntityIndex < SimRegion->EntityCount;
			EntityIndex++, Entity++)
	{
		if(Entity->MoveSpec.MoveOnInput && 
			 Entity->Updates &&
			 Entity != Camera->FreeMover)
		{
			Result = Entity;
		}
	}

	return Result;
}

#define ENTITY_H
#endif //ENTITY_H_FIRST_PASS
#endif //ENTITY_H
