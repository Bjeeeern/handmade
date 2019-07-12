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
	f32 AngularDrag_k1;
	f32 AngularDrag_k2;

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

struct aabb_verts_result
{
	u32 Count;
	v3 Verts[8];
};

v3 UnscaledAABB[8] = {{-0.5f,  0.5f, 0.5f}, 
											 { 0.5f,  0.5f, 0.5f}, 
											 { 0.5f, -0.5f, 0.5f}, 
											 {-0.5f, -0.5f, 0.5f},
											 {-0.5f,  0.5f,-0.5f}, 
											 { 0.5f,  0.5f,-0.5f}, 
											 { 0.5f, -0.5f,-0.5f}, 
											 {-0.5f, -0.5f,-0.5f}};

	internal_function aabb_verts_result
GetAABBVertices(m44* Tran)
{
	aabb_verts_result Result = {};
	Result.Count = 8;

	for(u32 CornerIndex = 0; 
			CornerIndex < ArrayCount(UnscaledAABB); 
			CornerIndex++)
	{
		v3 Corner = UnscaledAABB[CornerIndex];
		Result.Verts[CornerIndex] = *Tran * Corner;
	}

	return Result;
}

enum collision_shape
{
	CollisionShape_Sphere = 0,
	CollisionShape_Ellipsoid = 1,
	CollisionShape_AABB = 2,
	CollisionShape_Convex = 3,
};

//TODO(bjorn): Maybe add bodies that are trigger only, for the cases where I
//want entities hovering above ground for example.
struct body_primitive
{
	union
	{
		collision_shape CollisionShape;
		u32 CollisionShapeIndex;
	};

	v3 P;
	q O;
	v3 S;

	m44 Tran;
	m44 TranUnscaled;
	m44 iTran;
	m44 iTranUnscaled;
};

struct physical_body
{
	u32 PrimitiveCount;
	body_primitive Primitives[4];

	f32 iM; //NOTE(bjorn): Inverse mass
	m33 iI; //NOTE(bjorn): Inverse inertia tensor
	f32 Restitution;

	v3 S;
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
	b32 HasBody : 1;
	b32 Collides : 1;
	b32 Rotates : 1;
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
	v3 T;

	v3 P;
	v3 dP;
	v3 ddP;
	v3 F;

#if HANDMADE_INTERNAL
	b32 DEBUG_EntityCollided;
	v3 DEBUG_MostRecent_F;
	v3 DEBUG_MostRecent_T;
#endif

	m44 Tran;
	m44 TranUnscaled;
	m44 iTran;
	m44 iTranUnscaled;

	v2 CamRot;
	f32 CamZoom;

	//TODO(bjorn): Make this settable by an aggregate function
	physical_body Body;

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

internal_function void
UpdateEntityDerivedMembers(entity* Entity)
{
	if(Entity->HasBody && Entity->Body.S != v3{1,1,1})
	{
		Entity->Tran = ConstructTransform(Entity->P, Entity->O, Entity->Body.S);
		Entity->TranUnscaled = ConstructTransform(Entity->P, Entity->O);
		Entity->iTran = InverseTransform(Entity->Tran);
		Entity->iTranUnscaled = InverseUnscaledTransform(Entity->TranUnscaled);
	}
	else
	{
		Entity->Tran = ConstructTransform(Entity->P, Entity->O);
		Entity->TranUnscaled = Entity->Tran;
		Entity->iTran = InverseUnscaledTransform(Entity->Tran);
		Entity->iTranUnscaled = Entity->iTran;
	}
}

#include "trigger.h"
#include "attachment.h"

//TODO IMPORTANT(bjorn): Break up CalculatePhysicalBody into:
// A: Adding primitives to the body
// B: Calculating final body inertia tensor etc.
internal_function body_primitive*
AddBodyPrimitive(entity* Entity, v3 Offset, q Orientation, v3 Scale)
{
	body_primitive* Result = 0;
	physical_body* Body = &Entity->Body;

	if(Body->PrimitiveCount == 0)
	{
		Assert(!Entity->HasBody);
		Body->Primitives[0].CollisionShape = CollisionShape_Sphere;
		Body->PrimitiveCount += 1;
	}
	Assert(Body->PrimitiveCount < ArrayCount(Body->Primitives));
	Result = Body->Primitives + Body->PrimitiveCount++;

	Result->P = Offset;
	Result->O = Orientation;
	Result->S = Scale;
	Result->Tran = ConstructTransform(Offset, Orientation, Scale);
	Result->TranUnscaled = ConstructTransform(Offset, Orientation);
	Result->iTran = InverseTransform(Result->Tran);
	Result->iTranUnscaled = InverseUnscaledTransform(Result->TranUnscaled);

	Entity->HasBody = true;
	return Result;
}
internal_function void
AddSphereToPhysicalBody(entity* Entity, v3 Offset, f32 Radius)
{
	body_primitive* Primitive = AddBodyPrimitive(Entity, 
																							 Offset, 
																							 QuaternionIdentity(), 
																							 (2.0f*Radius)*v3{1,1,1});
	Assert(Primitive);
	Primitive->CollisionShape = CollisionShape_Sphere;
}
internal_function void
AddEllipsoidToPhysicalBody(entity* Entity, v3 Offset, v3 Scale)
{
	body_primitive* Primitive = AddBodyPrimitive(Entity, Offset, QuaternionIdentity(), Scale);
	Assert(Primitive);
	Primitive->CollisionShape = CollisionShape_Ellipsoid;
}
internal_function void
AddAABBToPhysicalBody(entity* Entity, v3 Offset, q Orientation, v3 Scale)
{
	body_primitive* Primitive = AddBodyPrimitive(Entity, Offset, Orientation, Scale);
	Assert(Primitive);
	Primitive->CollisionShape = CollisionShape_AABB;
}

internal_function void
CalibratePhysicalBody(entity* Entity, f32 Mass, v3 Scale, f32 Restitution)
{
	Assert(Entity->HasBody);
	physical_body* Body = &Entity->Body;
	//TODO(bjorn): Support other shapes even when scaled.
	//TODO(bjorn): Support other centers of mass apart from the origin.
	Assert(Body->PrimitiveCount > 1);

	{
		//TODO(bjorn): Smarter bounding collision sphere calculation that moves
		//the center of the volume.
		f32 R = 0;
		for(u32 PrimitiveIndex = 1;
				PrimitiveIndex < Body->PrimitiveCount;
				PrimitiveIndex++)
		{
			body_primitive* Prim = Body->Primitives + PrimitiveIndex;
			if(Prim->CollisionShape == CollisionShape_AABB)
			{
				aabb_verts_result AABB = GetAABBVertices(&Prim->Tran);
				for(u32 VertIndex = 0; 
						VertIndex < AABB.Count; 
						VertIndex++)
				{
					R = Max(Magnitude(AABB.Verts[VertIndex]), R);
				}
			}
			else if(Prim->CollisionShape == CollisionShape_Sphere ||
							Prim->CollisionShape == CollisionShape_Ellipsoid)
			{
				v3 C0 = Prim->P;
				f32 MaxRadii = Max(Max(Prim->S.X, Prim->S.Y), Prim->S.Z) * 0.5f;
				f32 DistFromOrigo = Magnitude(C0) + MaxRadii;
				R = Max(DistFromOrigo, R);
			}
			else
			{
				//TODO(bjorn): Add system for receiving vertices of more shapes.
				Assert(!"Invalid code path!");
			}
		}

		body_primitive* BoundingVolume = Body->Primitives + 0;

		Assert(BoundingVolume->CollisionShape == CollisionShape_Sphere);
		BoundingVolume->P = {0,0,0};
		BoundingVolume->O = QuaternionIdentity();
		BoundingVolume->S = 2.0f*v3{R,R,R};
		BoundingVolume->Tran = ConstructTransform(BoundingVolume->P, 
																							BoundingVolume->O, 
																							BoundingVolume->S);
		BoundingVolume->iTran = InverseTransform(BoundingVolume->Tran);
	}

	Body->Restitution = Restitution;
	Body->S = Scale;

	Body->iM = SafeRatio0(1.0f, Mass);
	if(Body->iM == 0)
	{
		Body->iI = {};
	}
	else
	{
		body_primitive* Prim = Body->Primitives + 1;

		Assert(Body->S == v3{1,1,1});
		if(Body->PrimitiveCount == 2 &&
			 Body->S == v3{1,1,1})
		{
			Assert(Prim->O == QuaternionIdentity());
			Assert(Prim->P == v3{0,0,0});
			if(Prim->CollisionShape == CollisionShape_AABB &&
				 Prim->O == QuaternionIdentity() &&
				 Prim->P == v3{0,0,0})
			{
				m33 I = {};
				I.E_[0] = Square(Prim->S.Y) + Square(Prim->S.Z);
				I.E_[4] = Square(Prim->S.X) + Square(Prim->S.Z);
				I.E_[8] = Square(Prim->S.X) + Square(Prim->S.Y);
				I *= Mass * (1.0f/12.0f);

				inverse_m33_result iI = InverseMatrix(I);
				Assert(iI.Valid);
				Body->iI = iI.M;
			}
			else if((Prim->CollisionShape == CollisionShape_Sphere ||
							 Prim->CollisionShape == CollisionShape_Ellipsoid) &&
							Prim->O == QuaternionIdentity() &&
							Prim->P == v3{0,0,0})
			{
				m33 I = {};
				I.E_[0] = Square(Prim->S.Y*0.5f) + Square(Prim->S.Z*0.5f);
				I.E_[4] = Square(Prim->S.X*0.5f) + Square(Prim->S.Z*0.5f);
				I.E_[8] = Square(Prim->S.X*0.5f) + Square(Prim->S.Y*0.5f);
				I *= Mass * (2.0f/5.0f);

				inverse_m33_result iI = InverseMatrix(I);
				Assert(iI.Valid);
				Body->iI = iI.M;
			}
			else
			{
				//TODO(bjorn): More specific inertial tensor calculations.
				Assert(!"Invalid code path!");
			}
		}
		else
		{
			//TODO(bjorn): A catch-all whole body intertial tensor calculation.
			Assert(!"Invalid code path!");
		}
	}
}

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

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, 
												v3{1.5f, 0.4f, 0.1f}*SimRegion->WorldMap->TileSideInMeters);
	CalibratePhysicalBody(Entity, 8.0f, {1,1,1}, 0.5f);

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

	//TODO(bjorn): Why does weight differences matter so much in the collision system.
	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{0.5f,0.3f,1.0f}*SimRegion->WorldMap->TileSideInMeters);
	CalibratePhysicalBody(Entity, 70.0f, {1,1,1}, 1.0f);

	Entity->Collides = true;

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

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{0.5f,0.3f,1.0f}*SimRegion->WorldMap->TileSideInMeters);
	CalibratePhysicalBody(Entity, 40.0f, {1,1,1}, 1.0f);

	Entity->Collides = true;

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

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{0.5f,0.3f,1.0f}*SimRegion->WorldMap->TileSideInMeters);
	CalibratePhysicalBody(Entity, 5.0f, {1,1,1}, 0.2f);

	Entity->Collides = true;

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 20.0f;
	Entity->MoveSpec.Speed = 85.f * 0.7f;
	SetStandardFrictionForGroundObjects(&Entity->MoveSpec);

	Entity->HunterSearchRadius = 6.0f;
	Entity->MinimumHuntRangeSquared = Square(2.0f);

	return Entity;
}

	internal_function entity*
AddFloor(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_NotRendered, InitP);

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{50,50,1});
	CalibratePhysicalBody(Entity, 0.0, {1,1,1}, 0.2f);

	Entity->Collides = true;
	Entity->IsFloor = true;

	return Entity;
}

	internal_function entity*
AddFixture(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Sword, InitP);

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{1,1,1}*SimRegion->WorldMap->TileSideInMeters);
	CalibratePhysicalBody(Entity, 0.0f, {1,1,1}, 0.0f);

	Entity->Collides = true;

	return Entity;
}

	internal_function entity*
AddWall(sim_region* SimRegion, v3 InitP, f32 Mass = 1000.0f)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Wall, InitP);

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{1,1,1}*SimRegion->WorldMap->TileSideInMeters);
	CalibratePhysicalBody(Entity, Mass, {1,1,1}, 0.5f);

	Entity->Collides = true;
	Entity->Rotates = true;

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 20.0f;
	SetStandardFrictionForGroundObjects(&Entity->MoveSpec);

	return Entity;
}

	internal_function entity*
AddForceField(sim_region* SimRegion, v3 InitP)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_Sword, InitP);

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, v3{0.1f, 0.1f, 0.1f});
	CalibratePhysicalBody(Entity, 0.0f, {1,1,1}, 0.0f);

	Entity->ForceFieldRadiusSquared = Square(10.0f);
	Entity->ForceFieldStrenght = 0.0f;

	return Entity;
}

	internal_function entity*
AddParticle(sim_region* SimRegion, v3 InitP, f32 Mass = 0.01f, v3 Scale = v3{1, 1, 1})
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_NotRendered, InitP);

	AddAABBToPhysicalBody(Entity, {0,0,0}, q{1,0,0,0}, Scale);
	CalibratePhysicalBody(Entity, Mass, {1,1,1}, 0.4f);

	Entity->Collides = true;
	Entity->Rotates = true;

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 10.0f;
	Entity->MoveSpec.Drag_k1 = 0.0f;
	Entity->MoveSpec.Drag_k2 = 0.5f;

	Entity->MoveSpec.AngularDrag_k1 = 0.2f;
	Entity->MoveSpec.AngularDrag_k2 = 1.0f;

	return Entity;
}

	internal_function entity*
AddSphereParticle(sim_region* SimRegion, v3 InitP, f32 Mass, f32 Radius)
{
	entity* Entity = AddEntity(SimRegion, EntityVisualType_NotRendered, InitP);

	AddSphereToPhysicalBody(Entity, {0,0,0}, Radius);
	CalibratePhysicalBody(Entity, Mass, {1,1,1}, 0.4f);

	Entity->Collides = true;
	Entity->Rotates = true;

	Entity->MoveSpec.Gravity = v3{0, 0,-1} * 10.0f;
	Entity->MoveSpec.Drag_k1 = 0.0f;
	Entity->MoveSpec.Drag_k2 = 0.5f;

	Entity->MoveSpec.AngularDrag_k1 = 0.2f;
	Entity->MoveSpec.AngularDrag_k2 = 1.0f;

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
	v3 T = Entity->T;

	move_spec* MoveSpec = &Entity->MoveSpec;
	physical_body* Body = &Entity->Body;

	if(Body->iM)
	{ 
		if(MoveSpec->Drag_k1 || MoveSpec->Drag_k2)
		{
			f32 dP_m = Magnitude(dP);
			F -= dP * (MoveSpec->Drag_k1 + MoveSpec->Drag_k2 * dP_m);
		}

		ddP += F * Body->iM;
		//TODO(bjorn): Why do i need the gravity to be so heavy? Because of upscaling?
		ddP += MoveSpec->Gravity;

		//TODO(bjorn): In addition to a general rotational drag, add a drag+impulse
		//response for twisted attachments.
		if(MoveSpec->AngularDrag_k1 || MoveSpec->AngularDrag_k2)
		{
			f32 dO_m = Magnitude(dO);
			T -= dO * (MoveSpec->AngularDrag_k1 + MoveSpec->AngularDrag_k2 * dO_m);
		}

		m33 Rot = QuaternionToRotationMatrix(O);
		m33 iI_world = Rot * (Body->iI * Transpose(Rot));
		ddO += iI_world * T;
	}
	//TODO(casey): ODE here!
	ddP += MoveSpec->MovingDirection * MoveSpec->Speed;

	//TODO(bjorn): Damping with precalculated Pow(Damping, dT) per update step to
	//support different frame rates.
	dP = dP * MoveSpec->Damping + ddP * dT;
	P = P + dP * dT;

	dO = dO * MoveSpec->Damping + ddO * dT;
	O = UpdateOrientationByAngularVelocity(O, dO*dT);

#if HANDMADE_INTERNAL
	Entity->DEBUG_MostRecent_F = F;
	Entity->DEBUG_MostRecent_T = T;
#endif

	Entity->F = {};
	Entity->ddP = {};
	Entity->dP = dP;
	Entity->P = P;

	Entity->T = {};
	Entity->ddO = {};
	if(Entity->Rotates)
	{
		Entity->dO = dO;
		Entity->O = QuaternionNormalize(O);
	}
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
	if(Other->P.Z < Floor->P.Z)
	{
		Other->P.Z = Floor->P.Z;
		Other->dP.Z = 0;
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
