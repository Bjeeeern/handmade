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

inline void
OffsetAndChangeEntityLocation(memory_arena* Arena, world_map* WorldMap, entity Entity, 
															world_map_position WorldP, v3 dP)
{
	if(Entity.Low)
	{
		world_map_position NewWorldP = OffsetWorldPos(WorldMap, WorldP, dP);
		ChangeEntityLocation(Arena, WorldMap, Entity.LowEntityIndex, &WorldP, &NewWorldP);
		Entity.Low->WorldP = NewWorldP;
	}
}

#define ENTITY_H
#endif
