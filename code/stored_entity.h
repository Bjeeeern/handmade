#if !defined(STORED_ENTITY_H)

#include "types_and_defines.h"
#include "world_map.h"

struct move_spec
{
	b32 EnforceHorizontalMovement;
	b32 EnforceVerticalGravity;
	b32 AllowRotation;
	f32 Speed;
	f32 Drag;
	f32 AngularDrag;
};

internal_function move_spec
DefaultMoveSpec()
{
	move_spec Result = {};

	Result.AllowRotation = true;

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

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	//TODO(casey): Bake this down into one variable.
	u8 Flags;
	u8 FilledAmount;
};

struct stored_entity
{
	//TODO(casey): Generation index so we know how "up to date" the entity is.

	//TODO(bjorn): Is this really needed?
	u32 StoredIndex;

	entity_type Type;

	f32 A;
	f32 dA;
	v3 dP;

	v3 R;
	world_map_position WorldP;

	v3 Dim;

	b32 Collides;
	b32 Attached;

	f32 Mass;
	move_spec MoveSpec;
	f32 GroundFriction;

	u32 FacingDirection;

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

	//NOTE(bjorn): Player, Familiar
	v3 MovingDirection;

	//NOTE(bjorn): Familiar
	f32 BestDistanceToPlayerSquared;

	//NOTE(bjorn): CarFrame
	//TODO(bjorn): Use this to move out the turning code to the cars update loop.
	b32 AutoPilot;
};

struct entities
{
	u32 EntityCount;
	stored_entity StoredEntities[100000];
};

	inline stored_entity*
GetStoredEntityByIndex(entities* Entities, u32 StoredIndex)
{
	stored_entity* Result = 0;

	if(0 < StoredIndex && StoredIndex <= Entities->EntityCount)
	{
		Result = Entities->StoredEntities + StoredIndex;
		Assert(Result->StoredIndex == StoredIndex);
	}
	else
	{
		// NOTE(bjorn): I often use this to check whether an index is valid or not.
		//InvalidCodePath;
	}

	return Result;
}

inline void
ChangeStoredEntityWorldLocation(memory_arena* WorldArena, world_map* WorldMap, 
																stored_entity* Entity, world_map_position* NewP)
{
	Assert(Entity); 

	world_map_position* OldP = 0;
	if(IsValid(Entity->WorldP))
	{
		OldP = &(Entity->WorldP); 
	}

	if(NewP) 
	{ 
		Assert(IsValid(*NewP)); 
	}

	UpdateStoredEntityChunkLocation(WorldArena, WorldMap, Entity->StoredIndex, OldP, NewP);

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
ChangeStoredEntityWorldLocationRelativeOther(memory_arena* Arena, world_map* WorldMap, 
																						 stored_entity* Entity, world_map_position WorldP, 
																						 v3 Offset)
{
		world_map_position NewWorldP = OffsetWorldPos(WorldMap, WorldP, Offset);
		ChangeStoredEntityWorldLocation(Arena, WorldMap, Entity, &NewWorldP);
}

inline v3
DefaultEntityOrientation()
{
	return {0, 1, 0};
}

	internal_function stored_entity
AddEntity(memory_arena* WorldArena, world_map* WorldMap, entities* Entities, 
					entity_type Type, world_map_position* WorldP = 0)
{
	stored_entity* Stored = {};

	Entities->EntityCount++;
	Assert(Entities->EntityCount < ArrayCount(Entities->StoredEntities));
	Result.Stored = Entities->StoredEntities + Entities->EntityCount;

	*Result.Stored = {};
	Result.Stored->StoredIndex = Entities->EntityCount;
	Result.Stored->Type = Type;
	Result.Stored->R = DefaultEntityOrientation();
	Result.Stored->WorldP = WorldMapNullPos();

	if(WorldP)
	{
		Assert(IsValid(*WorldP));
		Assert(IsCanonical(WorldMap, WorldP->Offset_));
	}

	ChangeStoredEntityWorldLocation(WorldArena, WorldMap, Result.Stored, WorldP);

	return Result;
}

#define STORED_ENTITY_H
#endif
