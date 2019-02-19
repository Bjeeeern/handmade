#if !defined(ENTITY_H)

#include "types_and_defines.h"
#include "world_map.h"

#define LoopOverHighEntitiesNamed(name) \
	entity name = GetEntityByHighIndex(Entities, 1); \
	name##.High; \
	name = GetEntityByHighIndex(Entities, name##.Low->HighIndex+1)
#define LoopOverHighEntities LoopOverHighEntitiesNamed(Entity)

struct move_spec
{
	b32 EnforceHorizontalMovement;
	b32 EnforceVerticalGravity;
	f32 Speed;
	f32 Drag;
};

internal_function move_spec
DefaultMoveSpec()
{
	move_spec Result = {};

	Result.EnforceHorizontalMovement = true;
	Result.Drag = 0.4f * 30.0f;

	return Result;
}

enum entity_type
{
	EntityType_Player,
	EntityType_Wall,
	EntityType_Stair,
	EntityType_Ground,
	EntityType_Wheel,
	EntityType_CarFrame,
	EntityType_Engine,
	EntityType_Monstar,
	EntityType_Familiar,
	EntityType_Sword,
};

struct high_entity
{
	u32 LowIndex;

	f32 dR;
	f32 ddR;
	v3 P;
	v3 dP;
	v3 ddP;

	//v3 Displacement;
	b32 CollisionDirtyBit;

	u32 FacingDirection;

	//NOTE(bjorn): Player
	v3 MovingDirection;

	//NOTE(bjorn): Familiar
	f32 BestDistanceToPlayerSquared;

	//NOTE(bjorn): CarFrame
	//TODO(bjorn): Use this to move out the turning code to the cars update loop.
	b32 AutoPilot;
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	//TODO(casey): Bake this down into one variable.
	u8 Flags;
	u8 FilledAmount;
};

struct low_entity
{
	entity_type Type;
	u32 HighIndex;

	v3 R;
	world_map_position WorldP;
	v3 Dim;

	b32 Collides;
	b32 Attached;

	f32 Mass;
	move_spec MoveSpec;
	f32 GroundFriction;

	//TODO(casey): Should hitpoints themselves be entities?
	u32 HitPointMax;
	hit_point HitPoints[16];

	//NOTE(bjorn): Stair
	f32 dZ;

	//NOTE(bjorn): Car-parts
	u32 Vehicle;

	//NOTE(bjorn): Player
	u32 RidingVehicle;
	u32 Sword;

	//NOTE(bjorn): CarFrame
	u32 Wheels[4];
	u32 DriverSeat;
	u32 Engine;

	//NOTE(bjorn): Sword
	f32 DistanceRemaining;
};

struct entity
{
	u32 LowIndex;
	high_entity* High;
	low_entity* Low;
};

struct entities
{
	u32 HighEntityCount;
	u32 LowEntityCount;
	high_entity HighEntities[1024];
	low_entity LowEntities[100000];
};

struct entity_pair
{
	entity Obj;
	entity Sub;
};

struct vertices
{
	u32 VertCount;
	v3 Verts[16];
};

internal_function vertices
GetEntityVertices(entity Entity)
{
	vertices Result = {};
	Result.VertCount = 4;

	v2 OrderOfCorners[4] = {{-0.5f,  0.5f}, 
													{ 0.5f,  0.5f}, 
													{ 0.5f, -0.5f}, 
													{-0.5f, -0.5f}};
	m22 ClockWise = { 0,-1,
									  1, 0};
	m22 Transform = M22(Hadamard(ClockWise * Entity.Low->R.XY, Entity.Low->Dim.XY), 
											Hadamard(            Entity.Low->R.XY, Entity.Low->Dim.XY));
	for(u32 VertexIndex = 0; 
			VertexIndex < Result.VertCount; 
			VertexIndex++)
	{
		Result.Verts[VertexIndex] = Transform * OrderOfCorners[VertexIndex] + Entity.High->P;
	}

	return Result;
}

inline entity*
GetEntityOfType(entity_type Type, entity* A, entity* B)
{
	entity* Result = 0;
	if(A->Low->Type == Type)
	{
		Result = A;
	}
	if(B->Low->Type == Type)
	{
		Result = B;
	}
	return Result;
}

inline entity*
GetRemainingEntity(entity* E, entity* A, entity* B)
{
	Assert(E == A || E == B);
	return E==A ? B:A;
}


internal_function void
MapEntityIntoHigh(entities* Entities, u32 LowIndex, v3 P)
{
	low_entity* Low = Entities->LowEntities + LowIndex;
	if(!Low->HighIndex)
	{
		if(Entities->HighEntityCount < (ArrayCount(Entities->HighEntities) - 1))
		{
			u32 HighIndex = ++Entities->HighEntityCount;
			Low->HighIndex = HighIndex;

			high_entity* High = Entities->HighEntities + HighIndex;

			*High = {};
			High->LowIndex = LowIndex;
			High->P = P;
		}
		else
		{
			InvalidCodePath;
		}
	}
}

	internal_function void
MapEntityOutFromHigh(entities* Entities, u32 LowIndex, world_map_position WorldP)
{
	low_entity* Low = Entities->LowEntities + LowIndex;
	u32 HighIndex = Low->HighIndex;
	if(HighIndex)
	{
		high_entity* High = Entities->HighEntities + HighIndex;
		Low->WorldP = WorldP;
		Low->HighIndex = 0;

		if(HighIndex != Entities->HighEntityCount)
		{
			high_entity* HighAtEnd = Entities->HighEntities + Entities->HighEntityCount;
			*High = *HighAtEnd;

			low_entity* AffectedLow = Entities->LowEntities + HighAtEnd->LowIndex;
			AffectedLow->HighIndex = HighIndex;
		}
		Entities->HighEntityCount -= 1;
	}
}

	inline entity
GetEntityByLowIndex(entities* Entities, u32 LowIndex)
{
	entity Result = {};

	if(0 < LowIndex && LowIndex <= Entities->LowEntityCount)
	{
		Result.LowIndex = LowIndex;
		Result.Low = Entities->LowEntities + LowIndex;
		if(Result.Low->HighIndex)
		{
			Result.High = Entities->HighEntities + Result.Low->HighIndex;
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
GetEntityByHighIndex(entities* Entities, u32 HighIndex)
{
	entity Result = {};

	if(0 < HighIndex && HighIndex <= Entities->HighEntityCount)
	{
		Result.High = Entities->HighEntities + HighIndex;

		Assert(Result.High->LowIndex);
		Result.Low = Entities->LowEntities + Result.High->LowIndex;
		Result.LowIndex = Result.High->LowIndex;
	}
	else
	{
		// NOTE(bjorn): I use this in looping over high entities to check whether I
		// finished looping or not.
		//InvalidCodePath;
	}

	return Result;
}

inline void
ChangeEntityWorldLocation(memory_arena* Arena, world_map* WorldMap, entity* Entity,
													world_map_position* NewP)
{
	Assert(Entity); 

	world_map_position* OldP = 0;
	if(IsValid(Entity->Low->WorldP))
	{
		OldP = &(Entity->Low->WorldP); 
	}

	if(NewP) 
	{ 
		Assert(IsValid(*NewP)); 
	}

	UpdateEntityChunkLocation(Arena, WorldMap, Entity->LowIndex, OldP, NewP);

	if(NewP)
	{
		Entity->Low->WorldP = *NewP;
	}
	else
	{
		Entity->Low->WorldP = WorldMapNullPos();
	}
}

inline void
ChangeEntityWorldLocationRelativeOther(memory_arena* Arena, world_map* WorldMap, entity* Entity, 
															world_map_position WorldP, v3 offset)
{
		world_map_position NewWorldP = OffsetWorldPos(WorldMap, WorldP, offset);
		ChangeEntityWorldLocation(Arena, WorldMap, Entity, &NewWorldP);
}

	internal_function entity
AddEntity(memory_arena* WorldArena, world_map* WorldMap, entities* Entities, 
					entity_type Type, world_map_position* WorldP = 0)
{
	entity Result = {};

	Entities->LowEntityCount++;
	Assert(Entities->LowEntityCount < ArrayCount(Entities->LowEntities));

	Result.LowIndex = Entities->LowEntityCount;
	Result.Low = Entities->LowEntities + Result.LowIndex;
	*Result.Low = {};
	Result.Low->Type = Type;
	Result.Low->R = {0, 1, 0};
	Result.Low->WorldP = WorldMapNullPos();

	if(WorldP)
	{
		Assert(IsValid(*WorldP));
		Assert(IsCanonical(WorldMap, WorldP->Offset_));
	}

	ChangeEntityWorldLocation(WorldArena, WorldMap, &Result, WorldP);

	return Result;
}

	inline b32
ValidateEntityPairs(entities* Entities)
{
	for(u32 HighIndex = 1;
			HighIndex <= Entities->HighEntityCount;
			HighIndex++)
	{
		u32 LowIndex = Entities->HighEntities[HighIndex].LowIndex;
		if(Entities->LowEntities[LowIndex].HighIndex != HighIndex) { return false; }
	}

	return true;
}

#define ENTITY_H
#endif
