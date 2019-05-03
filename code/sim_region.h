#if !defined(SIM_REGION_H)

#include "types_and_defines.h"
#include "world_map.h"
#include "stored_entity"

struct sim_entity
{
	u32 StorageIndex;

	f32 ddA;

	v3 P;
	v3 ddP;

	b32 CollisionDirtyBit;
};

struct sim_region
{
	world_map* WorldMap;
	world_map_position Origin; 
	rectangle3 Bounds;

	u32 EntityMaxCount;
	u32 EntityCount;
	sim_entity* Entities;
};

internal_function sim_entity*
AddSimEntity(sim_region* SimRegion)
{
	sim_entity* Entity = 0;

	if(SimRegion->EntityCount < SimRegion->EntityMaxCount)
	{
		Entity = SimRegion->Entities[SimRegion->EntityCount++];
		Entity = {};
	}
	else
	{
		InvalidCodePath;
	}

	return Entity;
}

internal_function void
AddSimEntity(sim_region* SimRegion, stored_entity* Source, v3* SimP)
{
	sim_entity* Dest = AddSimEntity(SimRegion);

	if(Dest)
	{
		Dest->StoredIndex = Source->StoredIndex;
		if(SimP)
		{
			Dest->P = *SimP;
		}
		else
		{
			Dest->P = GetWorldMapPosDifference(SimRegion->WorldMap, Source->WorldP, SimRegion->Origin);
		}
	}
}

internal_function sim_region*
BeginSim(entities* Entities, memory_arena* SimArena, world_map* WorldMap, 
				 world_map_position* RegionCenter, rectangle3 RegionBounds)
{        
	sim_region* Result = PushStruct(SimArena, sim_region);

	Result->WorldMap = WorldMap;
	Result->Origin = RegionCenter;
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
							stored_entity* StoredEntity = GetStoredEntityByIndex(Entities, 
																																	 Block->EntityIndexes[Index]);
							Assert(Entity.Stored);

							v3 SimPos = GetWorldMapPosDifference(SimRegion->WorldMap->StoredEntities, 
																									 Entity->WorldP, 
																									 RegionCenter);
							if(IsInRectangle(RegionBounds, SimPos))
							{
								//TODO(bjorn): Add if entity is to be updated or not.
								AddSimEntity(SimRegion, Entity, &SimPos);
							}
						}
					}
				}
			}
		}
	}
}

internal_function void
EndSim(entities* Entities, memory_arena* WorldArena, sim_region* SimRegion)
{
	sim_entity* SimEntity = SimRegion->Entities;
	for(u32 EntityIndex = 0;
			EntityIndex < SimRegion->EntityCount;
			EntityIndex++, SimEntity++)
	{
		//TODO(bjorn): Store entity.
		stored_entity* StoredEntity = GetStoredEntityByIndex(Entities, SimEntity->StoredIndex);
		Assert(StoredEntity);

		ChangeStoredEntityWorldLocationRelativeOther(WorldArena, SimRegion->WorldMap, 
																					 StoredEntity, SimRegion->Origin, SimEntity->P);
	}
}

#define SIM_REGION_H
#endif
