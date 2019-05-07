#if !defined(STORED_ENTITY_H)

#include "types_and_defines.h"
#include "world_map.h"

struct stored_entity
{
	sim_entity Sim;
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
		InvalidCodePath;
	}

	return Result;
}

inline void
ChangeStoredEntityWorldLocation(memory_arena* WorldArena, world_map* WorldMap, 
																stored_entity* Stored, world_map_position* NewP)
{
	Assert(Entity); 

	world_map_position* OldP = 0;
	if(IsValid(Stored->Sim.WorldP))
	{
		OldP = &(Stored->Sim.WorldP); 
	}

	if(NewP) 
	{ 
		Assert(IsValid(*NewP)); 
	}

	UpdateStoredEntityChunkLocation(WorldArena, WorldMap, Stored->Sim.StorageIndex, OldP, NewP);

	if(NewP)
	{
		Stored->Sim.WorldP = *NewP;
	}
	else
	{
		Stored->Sim.WorldP = WorldMapNullPos();
	}
}

inline void
ChangeStoredEntityWorldLocationRelativeOther(memory_arena* Arena, world_map* WorldMap, 
																						 stored_entity* Stored, world_map_position WorldP, 
																						 v3 Offset)
{
		world_map_position NewWorldP = OffsetWorldPos(WorldMap, WorldP, Offset);
		ChangeStoredEntityWorldLocation(Arena, WorldMap, Stored, &NewWorldP);
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
