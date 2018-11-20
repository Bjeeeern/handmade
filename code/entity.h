#if !defined(ENTITY_H)

#include "types_and_defines.h"
#include "world_map.h"

enum entity_type
{
	EntityType_Player,
	EntityType_Wall,
	EntityType_Stair,
	EntityType_Ground,
	EntityType_Wheel,
	EntityType_CarFrame,
	EntityType_Engine,
};

struct high_entity
{
	u32 LowEntityIndex;

	v3 D;
	v3 P;
	v3 dP;

	union
	{
		struct
		{
			b32 IsOnStairs;
			u32 FacingDirection;
		} Player;
	};
};

struct low_entity
{
	entity_type Type;
	u32 HighEntityIndex;

	world_map_position WorldP;
	v3 Dim;

	b32 Collides;

	union
	{
		struct
		{
			f32 dZ;
		} Stair;
		struct
		{
			u32 Vehicle;
		} Wheel;
		struct
		{
			u32 RidingVehicle;
		} Player;
		struct
		{
			u32 Wheels[4];
			u32 DriverSeat;
			u32 Engine;
		} CarFrame;
		struct
		{
			u32 Vehicle;
		} Engine;
	};
};

struct entity
{
	u32 LowEntityIndex;
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

internal_function void
MapEntityIntoHigh(entities* Entities, u32 LowIndex, v3 P)
{
	low_entity* Low = Entities->LowEntities + LowIndex;
	if(!Low->HighEntityIndex)
	{
		if(Entities->HighEntityCount < (ArrayCount(Entities->HighEntities) - 1))
		{
			u32 HighIndex = ++Entities->HighEntityCount;
			Low->HighEntityIndex = HighIndex;

			high_entity* High = Entities->HighEntities + HighIndex;

			*High = {};
			High->LowEntityIndex = LowIndex;
			High->P = P;
			High->D = {0, 1, 0};
		}
		else
		{
			InvalidCodePath;
		}
	}
}

	internal_function void
MapEntityIntoLow(entities* Entities, u32 LowIndex, world_map_position WorldP)
{
	low_entity* Low = Entities->LowEntities + LowIndex;
	u32 HighIndex = Low->HighEntityIndex;
	if(HighIndex)
	{
		high_entity* High = Entities->HighEntities + HighIndex;
		Low->WorldP = WorldP;
		Low->HighEntityIndex = 0;

		if(HighIndex != Entities->HighEntityCount)
		{
			high_entity* HighAtEnd = Entities->HighEntities + Entities->HighEntityCount;
			*High = *HighAtEnd;

			low_entity* AffectedLow = Entities->LowEntities + HighAtEnd->LowEntityIndex;
			AffectedLow->HighEntityIndex = HighIndex;
		}
		Entities->HighEntityCount -= 1;
	}
}

	inline entity
GetEntityByLowIndex(entities* Entities, u32 LowEntityIndex)
{
	entity Result = {};

	if(0 < LowEntityIndex && LowEntityIndex <= Entities->LowEntityCount)
	{
		Result.LowEntityIndex = LowEntityIndex;
		Result.Low = Entities->LowEntities + LowEntityIndex;
		if(Result.Low->HighEntityIndex)
		{
			Result.High = Entities->HighEntities + Result.Low->HighEntityIndex;
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
GetEntityByHighIndex(entities* Entities, u32 HighEntityIndex)
{
	entity Result = {};

	if(0 < HighEntityIndex && HighEntityIndex <= Entities->HighEntityCount)
	{
		Result.High = Entities->HighEntities + HighEntityIndex;

		Assert(Result.High->LowEntityIndex);
		Result.Low = Entities->LowEntities + Result.High->LowEntityIndex;
		Result.LowEntityIndex = Result.High->LowEntityIndex;
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
OffsetAndChangeEntityLocation(memory_arena* Arena, world_map* WorldMap, entity* Entity, 
															world_map_position WorldP, v3 dP)
{
	if(Entity->Low)
	{
		world_map_position NewWorldP = OffsetWorldPos(WorldMap, WorldP, dP);
		ChangeEntityLocation(Arena, WorldMap, Entity->LowEntityIndex, 
												 &(Entity->Low->WorldP), &NewWorldP);
		Entity->Low->WorldP = NewWorldP;
	}
}

	internal_function entity
AddEntity(memory_arena* WorldArena, world_map* WorldMap, entities* Entities, 
					entity_type Type, world_map_position WorldPos)
{
	entity Result = {};

	Entities->LowEntityCount++;
	Assert(Entities->LowEntityCount < ArrayCount(Entities->LowEntities));
	Assert(IsCanonical(WorldMap, WorldPos.Offset_));

	Result.LowEntityIndex = Entities->LowEntityCount;
	Result.Low = Entities->LowEntities + Result.LowEntityIndex;
	*Result.Low = {};
	Result.Low->Type = Type;
	Result.Low->WorldP = WorldPos;

	ChangeEntityLocation(WorldArena, WorldMap, Result.LowEntityIndex, 0, &Result.Low->WorldP);

	return Result;
}

inline b32
ValidateEntityPairs(entities* Entities)
{
	for(u32 HighEntityIndex = 1;
			HighEntityIndex <= Entities->HighEntityCount;
			HighEntityIndex++)
	{
		u32 LowEntityIndex = Entities->HighEntities[HighEntityIndex].LowEntityIndex;
		if(Entities->LowEntities[LowEntityIndex].HighEntityIndex != HighEntityIndex) { return false; }
	}

	return true;
}

#define ENTITY_H
#endif
