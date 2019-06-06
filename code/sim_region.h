#if !defined(SIM_REGION_H)

#include "types_and_defines.h"
#include "world_map.h"
#include "entity.h"

struct stored_entity
{
	entity Sim;
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

	Assert(0 < StorageIndex && StorageIndex <= ArrayCount(Entities->Entities));
	Result = Entities->Entities + StorageIndex;
	Assert(Result->Sim.StorageIndex == StorageIndex);

	return Result;
}

	inline stored_entity*
GetNewStoredEntity(stored_entities* Entities, entity* Entity)
{
	stored_entity* Result = 0;

	Assert(!Entity->StorageIndex);

	Entities->EntityCount++;
	Assert(Entities->EntityCount < ArrayCount(Entities->Entities));

	Result = Entities->Entities + Entities->EntityCount;
	Result->Sim.StorageIndex = Entities->EntityCount;
	Entity->StorageIndex = Entities->EntityCount;

	return Result;
}

	inline void
ChangeStoredEntityWorldLocation(memory_arena* WorldArena, world_map* WorldMap, 
																stored_entity* Stored, world_map_position NewWorldP)
{
	Assert(Stored); 

	world_map_position* OldP = 0;
	if(IsValid(Stored->Sim.WorldP))
	{
		OldP = &(Stored->Sim.WorldP); 
	}

	world_map_position* NewP = 0;
	if(IsValid(NewWorldP))
	{
		NewP = &NewWorldP; 
	}

	UpdateStoredEntityChunkLocation(WorldArena, WorldMap, Stored->Sim.StorageIndex, OldP, NewP);

	if(NewP)
	{
		Stored->Sim.WorldP = *NewP;
		Stored->Sim.IsSpacial = true;
	}
	else
	{
		Stored->Sim.WorldP = WorldMapNullPos();
		Stored->Sim.IsSpacial = false;
	}
}

struct entity_hash
{
	u32 Index;
	entity* Ptr;
};

struct sim_region
{
	world_map* WorldMap;
	world_map_position Origin; 
	rectangle3 OuterBounds;
	rectangle3 UpdateBounds;

	f32 MaxEntityRadius;
	f32 MaxEntityVelocity;

	u32 EntityMaxCount;
	u32 EntityCount;
	entity* Entities;

	//TODO Is a hash here really the right way to go?
	//NOTE Must be a power of two.
	entity_hash Hash[4096];

	entity* MainCamera;
};

internal_function entity_hash* 
GetSimEntityHashSlotFromStorageIndex(sim_region* SimRegion, u32 StorageIndex)
{
	Assert(StorageIndex);

	entity_hash* Result = 0;

	u32 HashValue = StorageIndex;
	for(s32 Offset = 0;
			Offset < ArrayCount(SimRegion->Hash);
			Offset++)
	{
		entity_hash* HashEntry = SimRegion->Hash + ((HashValue + Offset) & 
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

	internal_function entity*
AddSimEntity(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
						 u32 StorageIndex, v3* SimP);

	internal_function void
LoadEntityReference(stored_entities* StoredEntities, sim_region* SimRegion, entity_reference* Ref)
{
	if(Ref->Index)
	{
		Ref->Ptr = AddSimEntity(StoredEntities, SimRegion, 
														GetStoredEntityByIndex(StoredEntities, Ref->Index), 
														Ref->Index, 0);
	}
}

	internal_function void
StoreEntityReference(stored_entities* StoredEntities, entity_reference* Ref)
{
	if(Ref->Ptr)
	{
		if(!Ref->Ptr->StorageIndex) { GetNewStoredEntity(StoredEntities, Ref->Ptr); }
		Assert(Ref->Ptr->StorageIndex);
		Assert(GetStoredEntityByIndex(StoredEntities, Ref->Ptr->StorageIndex));

		Ref->Index = Ref->Ptr->StorageIndex;
	}
}

	internal_function entity*
AddSimEntityRaw(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
								u32 StorageIndex)
{
	Assert(StorageIndex);
	entity* Entity = 0;

	entity_hash* Entry = GetSimEntityHashSlotFromStorageIndex(SimRegion, StorageIndex);
	Assert(Entry);

	if(Entry->Ptr == 0)
	{
		if(SimRegion->EntityCount < SimRegion->EntityMaxCount)
		{
			Entity = SimRegion->Entities + SimRegion->EntityCount++;

			Entry->Ptr = Entity;
			Entry->Index = StorageIndex;
		}
		else
		{
			InvalidCodePath;
		}

		Assert(Source);

		//TODO Decompression step instead of block copy!!
		*Entity = Source->Sim;

		//TODO Is this assign needed?
		//Entity->StorageIndex = StorageIndex;

		entity_reference* EntityRef = Entity->EntityReferences;
		for(s32 EntityRefIndex = 0;
				EntityRefIndex < ArrayCount(Entity->EntityReferences);
				EntityRefIndex++, EntityRef++)
		{
			LoadEntityReference(StoredEntities, SimRegion, EntityRef);
		}
		trigger_state* TrigState = Entity->TrigStates;
		for(u32 TrigStateIndex = 0;
				TrigStateIndex < ArrayCount(Entity->TrigStates);
				TrigStateIndex++, TrigState++)
		{
			LoadEntityReference(StoredEntities, SimRegion, &TrigState->Buddy);
		}
	}
	else
	{
		Entity = Entry->Ptr;
		Assert(Entry->Index == StorageIndex);
	}

	Assert(Entity);
	Assert(Entity->StorageIndex == StorageIndex);

	return Entity;
}

	internal_function entity*
AddSimEntity(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
						 u32 StorageIndex, v3* SimP)
{
	entity* Result = AddSimEntityRaw(StoredEntities, SimRegion, Source, StorageIndex);
	Assert(Result);

	//TODO(bjorn): The pos should be set to an invalid one here if the entitiy is non-spatial.
	if(SimP)
	{
		Result->P = *SimP;
	}
	else
	{
		if(Result->IsSpacial)
		{
			Result->P = GetWorldMapPosDifference(SimRegion->WorldMap, Source->Sim.WorldP, 
																					 SimRegion->Origin);
		}
		else
		{
			MakeEntityNonSpacial(Result);
		}
	}

	Result->EnityHasBeenProcessedAlready = false;
	Result->Updates = !Result->IsSpacial || IsInRectangle(SimRegion->UpdateBounds, Result->P);

	return Result;
}

	internal_function sim_region*
BeginSim(stored_entities* StoredEntities, memory_arena* SimArena, world_map* WorldMap, 
				 world_map_position RegionCenter, rectangle3 RegionBounds, f32 dT, 
				 u32 MainCameraStorageIndex = 0)
{        
	sim_region* Result = PushStruct(SimArena, sim_region);
	ZeroArray(Result->Hash);

	//TODO: Tradeoff with not having huge single entites. But Huge entities
	//should maybe be made-up out of smaller constituents anyways.
	Result->MaxEntityRadius = 5.0f;
	Result->MaxEntityVelocity = 30.0f;
	f32 SafetyUpdateMargin = Result->MaxEntityRadius + Result->MaxEntityVelocity * dT;

	Result->WorldMap = WorldMap;
	Result->Origin = RegionCenter;
	Result->UpdateBounds = RegionBounds;
	Result->OuterBounds= AddMarginToRect(RegionBounds, SafetyUpdateMargin);

	Result->EntityMaxCount = 4096;
	Result->EntityCount = 0;
	Result->Entities = PushArray(SimArena, Result->EntityMaxCount, entity);

	world_map_position MinWorldP = OffsetWorldPos(WorldMap, RegionCenter, Result->OuterBounds.Min);
	world_map_position MaxWorldP = OffsetWorldPos(WorldMap, RegionCenter, Result->OuterBounds.Max);

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
							u32 StorageIndex = Block->EntityIndexes[Index];

							stored_entity* StoredEntity = GetStoredEntityByIndex(StoredEntities, StorageIndex);
							Assert(StoredEntity);
							Assert(StoredEntity->Sim.IsSpacial);

							v3 SimPos = GetWorldMapPosDifference(WorldMap, StoredEntity->Sim.WorldP, RegionCenter);
							if(IsInRectangle(Result->OuterBounds, SimPos))
							{
								entity* Entity = AddSimEntity(StoredEntities, Result, 
																							StoredEntity, StorageIndex, &SimPos);
								if(Entity->StorageIndex == MainCameraStorageIndex) { Result->MainCamera = Entity; }
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
	entity* SimEntity = SimRegion->Entities;
	for(u32 EntityIndex = 0;
			EntityIndex < SimRegion->EntityCount;
			EntityIndex++, SimEntity++)
	{
		stored_entity* Stored = 0;
		if(SimEntity->StorageIndex) 
		{ 
			Stored = GetStoredEntityByIndex(Entities, SimEntity->StorageIndex); 
		}
		else 
		{ 
			Stored = GetNewStoredEntity(Entities, SimEntity); 
		}
		Assert(Stored);

		Stored->Sim = *SimEntity;

		entity_reference* EntityRef = Stored->Sim.EntityReferences;
		for(s32 EntityRefIndex = 0;
				EntityRefIndex < ArrayCount(Stored->Sim.EntityReferences);
				EntityRefIndex++, EntityRef++)
		{
			StoreEntityReference(Entities, EntityRef);
		}
		trigger_state* TrigState = Stored->Sim.TrigStates;
		for(u32 TrigStateIndex = 0;
			TrigStateIndex < ArrayCount(Stored->Sim.TrigStates);
			TrigStateIndex++, TrigState++)
		{
			StoreEntityReference(Entities, &TrigState->Buddy);
		}

		world_map_position NewWorldP = WorldMapNullPos();
		if(SimEntity->IsSpacial)
		{
			NewWorldP = OffsetWorldPos(SimRegion->WorldMap, SimRegion->Origin, SimEntity->P);
		}
		ChangeStoredEntityWorldLocation(WorldArena, SimRegion->WorldMap, Stored, NewWorldP);
	}
}

#define SIM_REGION_H
#endif
