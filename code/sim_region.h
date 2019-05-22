#if !defined(SIM_REGION_H)

#include "types_and_defines.h"
#include "world_map.h"

struct move_spec
{
	b32 EnforceHorizontalMovement : 1;
	b32 EnforceVerticalGravity : 1;
	b32 AllowRotation : 1;

	f32 Speed;
	f32 Drag;
	f32 AngularDrag;

	v3 MovingDirection;
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

enum entity_visual_type
{
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

union entity_reference
{
	u32 Index;
	entity* Ptr;
};

struct entity
{
	u32 StorageIndex;
	world_map_position WorldP;
	b32 Updates : 1;
	b32 EnityHasBeenProcessedAlready : 1;

	b32 IsSpacial : 1;
	b32 Collides : 1;
	b32 Attached : 1;
	//NOTE(bjorn): CarFrame
	//TODO(bjorn): Use this to move out the turning code to the cars update loop.
	b32 AutoPilot : 1;

	//TODO(casey): Generation index so we know how "up to date" the entity is.

	entity_visual_type VisualType;

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

	u32 TriggerDamage;

	//NOTE(bjorn): Stair
	f32 dZ;

	union
	{
		entity_reference EntityReferences[16];
		struct
		{
			union
			{
				entity_reference Attatchments[8];
				struct
				{
					//NOTE(bjorn): Car-parts
					entity_reference Vehicle;
					//NOTE(bjorn): CarFrame
					entity_reference Wheels[4];
					entity_reference DriverSeat;
					entity_reference Engine;

				};
			};

			//NOTE(bjorn): Player
			entity_reference Sword;
			entity_reference RidingVehicle;

			entity_reference Prey;
			entity_reference Struct_Terminator;
		};
	};

	//NOTE(bjorn): Sword
	f32 DistanceRemaining;

	//NOTE(bjorn): Familiar
	f32 HunterSearchRadius;
	f32 BestDistanceToPreySquared;
};

	inline void
MakeEntitySpacial(entity* Entity, v3 R, v3 P, v3 dP)
{
	Entity->IsSpacial = true;
	Entity->R = R;
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
#elif
	v3 InvalidV3 = {-10000, -10000, -10000};
#endif
	Entity->R = InvalidV3;
	Entity->P = InvalidV3;
	Entity->dP = InvalidV3;
}

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

	u32 EntityMaxCount;
	u32 EntityCount;
	entity* Entities;

	//TODO Is a hash here really the right way to go?
	//NOTE Must be a power of two.
	entity_hash Hash[4096];
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
						 u32 StoredIndex, v3* SimP);

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
StoreEntityReference(entity_reference* Ref)
{
	if(Ref->Ptr != 0)
	{
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
	}
	else
	{
		Entity = Entry->Ptr;
		Assert(Entry->Index == StorageIndex);
	}
	Assert(Entity);

	if(Source)
	{
		//TODO Decompression step instead of block copy!!
		*Entity = Source->Sim;

		entity_reference* EntityRef = Entity->EntityReferences;
		for(s32 EntityRefIndex = 0;
				EntityRefIndex < ArrayCount(Entity->EntityReferences);
				EntityRefIndex++, EntityRef++)
		{
			LoadEntityReference(StoredEntities, SimRegion, EntityRef);
		}
	}

	Entity->StorageIndex = StorageIndex;

	return Entity;
}

	internal_function entity*
AddSimEntity(stored_entities* StoredEntities, sim_region* SimRegion, stored_entity* Source, 
						 u32 StoredIndex, v3* SimP)
{
	entity* Result = AddSimEntityRaw(StoredEntities, SimRegion, Source, StoredIndex);
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
				 world_map_position RegionCenter, rectangle3 RegionBounds)
{        
	sim_region* Result = PushStruct(SimArena, sim_region);
	ZeroArray(Result->Hash);

	//TODO: Calculate this somehow-or-other.
	f32 SafetyUpdateMargin = 10.0f;

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
								AddSimEntity(StoredEntities, Result, StoredEntity, StorageIndex, &SimPos);
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
		stored_entity* Stored = GetStoredEntityByIndex(Entities, SimEntity->StorageIndex);
		Assert(Stored);

		Stored->Sim = *SimEntity;

		entity_reference* EntityRef = Stored->Sim.EntityReferences;
		for(s32 EntityRefIndex = 0;
				EntityRefIndex < ArrayCount(Stored->Sim.EntityReferences);
				EntityRefIndex++, EntityRef++)
		{
			StoreEntityReference(EntityRef);
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
