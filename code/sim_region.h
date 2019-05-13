#if !defined(SIM_REGION_H)

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

struct sim_entity;

union entity_reference
{
	u32 Index;
	sim_entity* Ptr;
};

struct sim_entity
{
	u32 StorageIndex;

	b32 HasPositionInWorld : 1;
	b32 CollisionDirtyBit : 1;
	b32 Collides : 1;
	b32 Attached : 1;
	//NOTE(bjorn): CarFrame
	//TODO(bjorn): Use this to move out the turning code to the cars update loop.
	b32 AutoPilot : 1;

	world_map_position WorldP;

	//TODO(casey): Generation index so we know how "up to date" the entity is.

	entity_type Type;

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

	//NOTE(bjorn): Stair
	f32 dZ;

	//NOTE(bjorn): Car-parts
	entity_reference Vehicle;

	//NOTE(bjorn): Player
	entity_reference RidingVehicle;
	entity_reference Sword;

	//NOTE(bjorn): CarFrame
	entity_reference Wheels[4];
	entity_reference DriverSeat;
	entity_reference Engine;

	//NOTE(bjorn): Sword
	f32 DistanceRemaining;

	//NOTE(bjorn): Familiar
	f32 BestDistanceToPlayerSquared;
	v3 MovingDirection;
};

struct stored_entity
{
	sim_entity Sim;
};

struct stored_entities
{
	u32 EntityCount;
	stored_entity Entities[100000];
};

	inline stored_entity*
GetStoredEntityByIndex(stored_entities* Entities, u32 StorageIndex)
{
	stored_entity* Result = 0;

	if(0 < StorageIndex && StorageIndex <= Entities->EntityCount)
	{
		Result = Entities->Entities + StorageIndex;
		Assert(Result->Sim.StorageIndex == StorageIndex);
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
	Assert(Stored); 

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
		Stored->Sim.HasPositionInWorld = true;
	}
	else
	{
		Stored->Sim.WorldP = WorldMapNullPos();
		Stored->Sim.HasPositionInWorld = false;
	}
}

struct sim_entity_hash
{
	u32 Index;
	sim_entity* Ptr;
};

struct sim_region
{
	world_map* WorldMap;
	world_map_position Origin; 
	rectangle3 Bounds;

	u32 EntityMaxCount;
	u32 EntityCount;
	sim_entity* Entities;

	//TODO Is a hash here really the right way to go?
	//NOTE Must be a power of two.
	sim_entity_hash Hash[4096];
};

internal_function sim_entity_hash* 
GetSimEntityHashSlotFromStorageIndex(sim_region* SimRegion, u32 StorageIndex)
{
	Assert(StorageIndex);

	sim_entity_hash* Result = 0;

	u32 HashValue = StorageIndex;
	for(s32 Offset = 0;
			Offset < ArrayCount(SimRegion->Hash);
			Offset++)
	{
		sim_entity_hash* HashEntry = SimRegion->Hash + ((HashValue + Offset) & 
																										(ArrayCount(SimRegion->Hash)-1));

		if((HashEntry->Index == StorageIndex) ||
			 (HashEntry->Index == 0))
		{
			Result = HashEntry;
			break;
		}
	}

	return Result;
}

	internal_function void
MapStorageIndexToEntity(sim_region* SimRegion, u32 StorageIndex, sim_entity* Entity)
{
	sim_entity_hash* Entry = GetSimEntityHashSlotFromStorageIndex(SimRegion, StorageIndex);
	Assert(Entry->Index == 0 || Entry->Index == StorageIndex);

	Entry->Index = StorageIndex;
	Entry->Ptr = Entity;
}

	internal_function sim_entity*
AddSimEntity(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
						 u32 StorageIndex);

	internal_function void
LoadEntityReference(stored_entities* StoredEntities, sim_region* SimRegion, entity_reference* Ref)
{
	if(Ref->Index)
	{
		sim_entity_hash* Entry = GetSimEntityHashSlotFromStorageIndex(SimRegion, Ref->Index);
		Assert(Entry);

		if(Entry->Ptr == 0)
		{
			Entry->Index = Ref->Index;
			Entry->Ptr = AddSimEntity(StoredEntities, SimRegion, 
														 GetStoredEntityByIndex(StoredEntities, Ref->Index), Ref->Index);
		}

		Ref->Ptr = Entry->Ptr;
	}
}

	internal_function void
StoreEntityReference(entity_reference* Ref)
{
	if(Ref->Ptr != 0)
	{
		Ref->Index = Ref->Ptr->StorageIndex;
	}
}

	internal_function sim_entity*
AddSimEntity(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
						 u32 StorageIndex)
{
	Assert(StorageIndex);
	sim_entity* Entity = 0;

	if(SimRegion->EntityCount < SimRegion->EntityMaxCount)
	{
		Entity = SimRegion->Entities + SimRegion->EntityCount++;
		MapStorageIndexToEntity(SimRegion, StorageIndex, Entity);

		if(Source)
		{
			//TODO Decompression step instead of block copy!!
			*Entity = Source->Sim;
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Vehicle);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->RidingVehicle);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Sword);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Wheels[0]);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Wheels[1]);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Wheels[2]);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Wheels[3]);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->DriverSeat);
			LoadEntityReference(StoredEntities, SimRegion, &Entity->Engine);
		}

		Entity->StorageIndex = StorageIndex;
	}
	else
	{
		InvalidCodePath;
	}

	return Entity;
}

	internal_function void
AddSimEntity(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
						 u32 StoredIndex, v3* SimP)
{
	sim_entity* Dest = AddSimEntity(StoredEntities, SimRegion, Source, StoredIndex);

	if(Dest)
	{
		Dest->CollisionDirtyBit = false;

		if(SimP)
		{
			Dest->P = *SimP;
		}
		else
		{
			Dest->P = GetWorldMapPosDifference(SimRegion->WorldMap, Source->Sim.WorldP, SimRegion->Origin);
		}
	}
}

	internal_function sim_region*
BeginSim(stored_entities* StoredEntities, memory_arena* SimArena, world_map* WorldMap, 
				 world_map_position* RegionCenter, rectangle3 RegionBounds)
{        
	//TODO IMPORTANT Clear the hash table.
	//TODO IMPORTANT Active vs inactive entities for the apron.
	sim_region* Result = PushStruct(SimArena, sim_region);

	Result->WorldMap = WorldMap;
	Result->Origin = *RegionCenter;
	Result->Bounds = RegionBounds;

	Result->EntityMaxCount = 4096;
	Result->EntityCount = 0;
	Result->Entities = PushArray(SimArena, Result->EntityMaxCount, sim_entity);

	world_map_position MinWorldP = OffsetWorldPos(WorldMap, *RegionCenter, RegionBounds.Min);
	world_map_position MaxWorldP = OffsetWorldPos(WorldMap, *RegionCenter, RegionBounds.Max);
	//rectangle3s CameraUpdateAbsBounds = RectMinMax(MinWorldP.ChunkP, MaxWorldP.ChunkP);

	for(s32 Z = MinWorldP.ChunkP.Z; 
			Z <= MaxWorldP.ChunkP.Z; 
			Z++)
	{
		for(s32 Y = MinWorldP.ChunkP.Y; 
				Y <= MaxWorldP.ChunkP.Y; 
				Y++)
		{
			for(s32 X = MinWorldP.ChunkP.X; 
					X <= MaxWorldP.ChunkP.X; 
					X++)
			{
				world_chunk* Chunk = GetWorldChunk(WorldMap, v3s{X,Y,Z});
				if(Chunk)
				{
					for(entity_block* Block = Chunk->Block;
							Block;
							Block = Block->Next)
					{
						for(u32 Index = 0;
								Index < Block->EntityIndexCount;
								Index++)
						{
							u32 StoredIndex = Block->EntityIndexes[Index];
							stored_entity* StoredEntity = GetStoredEntityByIndex(StoredEntities, StoredIndex);
							Assert(StoredEntity);

							v3 SimPos = GetWorldMapPosDifference(WorldMap, StoredEntity->Sim.WorldP,
																									 *RegionCenter);
							if(IsInRectangle(RegionBounds, SimPos))
							{
								//TODO(bjorn): Add if entity is to be updated or not.
								AddSimEntity(StoredEntities, Result, StoredEntity, StoredIndex, &SimPos);
							}
						}
					}
				}
			}
		}
	}

	return Result;
}

internal_function void
EndSim(stored_entities* Entities, memory_arena* WorldArena, sim_region* SimRegion)
{
	sim_entity* SimEntity = SimRegion->Entities;
	for(u32 EntityIndex = 0;
			EntityIndex < SimRegion->EntityCount;
			EntityIndex++, SimEntity++)
	{
		stored_entity* Stored = GetStoredEntityByIndex(Entities, SimEntity->StorageIndex);
		Assert(Stored);

		Stored->Sim = *SimEntity;
		StoreEntityReference(&Stored->Sim.Vehicle);
		StoreEntityReference(&Stored->Sim.RidingVehicle);
		StoreEntityReference(&Stored->Sim.Sword);
		StoreEntityReference(&Stored->Sim.Wheels[0]);
		StoreEntityReference(&Stored->Sim.Wheels[1]);
		StoreEntityReference(&Stored->Sim.Wheels[2]);
		StoreEntityReference(&Stored->Sim.Wheels[3]);
		StoreEntityReference(&Stored->Sim.DriverSeat);
		StoreEntityReference(&Stored->Sim.Engine);

		if(SimEntity->HasPositionInWorld)
		{
			world_map_position NewWorldP = OffsetWorldPos(SimRegion->WorldMap, 
																										SimRegion->Origin, SimEntity->P);
			ChangeStoredEntityWorldLocation(WorldArena, SimRegion->WorldMap, Stored, &NewWorldP);
		}
		else
		{
			ChangeStoredEntityWorldLocation(WorldArena, SimRegion->WorldMap, Stored, 0);
		}
	}
}

#define SIM_REGION_H
#endif
