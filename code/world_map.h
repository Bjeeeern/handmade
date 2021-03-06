#if !defined(WORLD_MAP_H)

#include "math.h"
#include "platform.h"

struct entity_block
{
	u32 EntityIndexCount;
	u64 EntityIndexes[16];

	entity_block* Next;
};

struct world_chunk
{
	v3s ChunkP;
  entity_block* Block;

	world_chunk* Next;
};

struct world_map
{
	s32 ChunkSafetyMargin;

  f32 ChunkSideInMeters;

  world_chunk HashMap[4096];

	entity_block* FreeBlock;
};

#define INVALID_CHUNK_POS max_s32
struct world_map_position
{
	v3s ChunkP;
	v3 Offset_;
};

inline world_map_position
WorldMapNullPos()
{
	world_map_position Result = {};
	Result.ChunkP.X = INVALID_CHUNK_POS;
	return Result;
}

inline b32
IsValid(world_map_position Pos)
{
	return Pos.ChunkP.X != INVALID_CHUNK_POS;
}

  internal_function world_chunk *
GetWorldChunk(world_map *WorldMap, v3s PotentialChunkP, memory_arena* Arena = 0)
{
  world_chunk *WorldChunk = 0;

	Assert(PotentialChunkP.X < (max_s32 - WorldMap->ChunkSafetyMargin) &&
				 PotentialChunkP.Y < (max_s32 - WorldMap->ChunkSafetyMargin) &&
				 PotentialChunkP.Z < (max_s32 - WorldMap->ChunkSafetyMargin) &&
				 PotentialChunkP.X > (min_s32 + WorldMap->ChunkSafetyMargin) &&
				 PotentialChunkP.Y > (min_s32 + WorldMap->ChunkSafetyMargin) &&
				 PotentialChunkP.Z > (min_s32 + WorldMap->ChunkSafetyMargin));

	s32 HashValue = 19 * PotentialChunkP.X + 7 * PotentialChunkP.Y + 3 * PotentialChunkP.Z;
	s32 HashSlot = HashValue & (ArrayCount(WorldMap->HashMap) - 1);
	WorldChunk = WorldMap->HashMap + HashSlot;

	b32 SlotFoundOrCreated = false;
	if(WorldChunk->Block)
	{
		do
		{
			if(WorldChunk->ChunkP == PotentialChunkP)
			{
				//NOTE(bjorn): There is a chunk in this area but possibly no block.
				SlotFoundOrCreated = WorldChunk->Block != 0;
				break;
			}
			else if(WorldChunk->Next)
			{
				WorldChunk = WorldChunk->Next;
			}
			else if(Arena)
			{
				world_chunk* NewChunk = PushStruct(Arena, world_chunk);
				WorldChunk->Next = NewChunk;
				WorldChunk = NewChunk;

				SlotFoundOrCreated = true;
				break;
			}
			else
			{
				//NOTE(bjorn): There is yet no chunk in this area.
				break;
			}
		}
		while(true);
	}

	if(Arena && !WorldChunk->Block)
	{
		WorldChunk->ChunkP = PotentialChunkP;

		if(WorldMap->FreeBlock)
		{
			WorldChunk->Block = WorldMap->FreeBlock;
			WorldMap->FreeBlock = WorldMap->FreeBlock->Next;
			WorldChunk->Block->Next = 0;
		}
		else
		{
			WorldChunk->Block = PushStruct(Arena, entity_block);
		}

		Assert(WorldChunk->Block);
		SlotFoundOrCreated = true;
	}

	if(!SlotFoundOrCreated)
	{
		WorldChunk = 0;
	}

	return WorldChunk;
}

	inline b32
IsCanonical(world_map *WorldMap, v3 Offset)
{
	f32 HalfUnit = WorldMap->ChunkSideInMeters * 0.5f;

	HalfUnit *= 1.00001f;

	b32 Result = ( (Offset.X >= -HalfUnit) &&
								 (Offset.X <=  HalfUnit) &&
								 (Offset.Y >= -HalfUnit) &&
								 (Offset.Y <=  HalfUnit) &&
								 (Offset.Z >= -HalfUnit) &&
								 (Offset.Z <=  HalfUnit) );

	return Result;
}

//TODO(bjorn): Do not use divide/multiply for recanonicalizing. There might 
//occur a loss of presicion that causes the player to get stuck when moving
//slowly with small increments.
	internal_function world_map_position
RecanonilizePosition(world_map *WorldMap, world_map_position NewDeltaPosition)
{
	world_map_position Result = NewDeltaPosition;

	v3s ChunkOffset = RoundV3ToV3S(NewDeltaPosition.Offset_ / WorldMap->ChunkSideInMeters);
	Result.Offset_ -= ChunkOffset * WorldMap->ChunkSideInMeters;
	Result.ChunkP += ChunkOffset;

	Assert(IsCanonical(WorldMap, Result.Offset_));
	return Result;
}

	internal_function v3
GetWorldMapPosDifference(world_map *WorldMap, world_map_position A, world_map_position B)
{
	v3 Result = {};

	v3s ChunkDiff = (v3s)A.ChunkP - (v3s)B.ChunkP;

	v3 InternalDiff = A.Offset_ - B.Offset_;
	Result = (v3)ChunkDiff * WorldMap->ChunkSideInMeters + (v3)InternalDiff;

	//TODO(bjorn): Handle differences that are far away in a way that lets the
	//caller know so that this does not change an entity's position with respect
	//to floating point error!!!

	return Result;
}

	inline world_map_position
OffsetWorldPos(world_map *WorldMap, world_map_position OldPosition, v3 Offset)
{
	OldPosition.Offset_ += Offset;
	return RecanonilizePosition(WorldMap, OldPosition);
}

inline b32
AreInSameChunk(world_map* WorldMap, world_map_position* A, world_map_position* B)
{
	Assert(IsCanonical(WorldMap, A->Offset_));
	Assert(IsCanonical(WorldMap, B->Offset_));

	b32 Result = (A->ChunkP == B->ChunkP);
	return Result;
}

//TODO(bjorn): Should hashmap chunks that contains no blocks in the hashmap be
//recycled like blocks are?
inline void
UpdateStoredEntityChunkLocation(memory_arena* Arena, world_map* WorldMap, u64 StoredEntityIndex,
																world_map_position* OldP, world_map_position* NewP)
{
	Assert(!OldP || IsValid(*OldP));
	Assert(!NewP || IsValid(*NewP));

	if(OldP && NewP && AreInSameChunk(WorldMap, OldP, NewP))
	{
		return; //NOTE(Bjorn): Entity not needed to be moved.
	}
	else
	{
		if(OldP)
		{
			world_chunk* OldChunk = GetWorldChunk(WorldMap, OldP->ChunkP);
			Assert(OldChunk);
			if(OldChunk)
			{
				entity_block* FirstBlock = OldChunk->Block;
				for(entity_block* Block = FirstBlock;
						Block;
						Block = Block->Next)
				{
					for(u32 Index = 0;
							Index < Block->EntityIndexCount;
							Index++)
					{
						if(Block->EntityIndexes[Index] == StoredEntityIndex)
						{
							Assert(FirstBlock->EntityIndexCount > 0);

							u64 TopEntityIndex = FirstBlock->EntityIndexes[--FirstBlock->EntityIndexCount];

							Assert(TopEntityIndex);

							Block->EntityIndexes[Index] = TopEntityIndex;

							if(FirstBlock->EntityIndexCount == 0)
							{
								OldChunk->Block = FirstBlock->Next;
								FirstBlock->Next = WorldMap->FreeBlock;
								WorldMap->FreeBlock = FirstBlock;
							}

							goto IndexInOldChunkFound;
						}
					}
					Assert(Block->Next);
				}
			}
IndexInOldChunkFound:; 
		}
		else
		{
			//NOTE(): Entity is being moved into map for the first time.
		}
	}

	if(NewP)
	{
		world_chunk* NewChunk = GetWorldChunk(WorldMap, NewP->ChunkP, Arena);
		Assert(NewChunk);

		entity_block* Block = NewChunk->Block;
		Assert(Block);

		if(Block->EntityIndexCount >= ArrayCount(Block->EntityIndexes))
		{
			if(WorldMap->FreeBlock)
			{
				NewChunk->Block = WorldMap->FreeBlock;
				WorldMap->FreeBlock = WorldMap->FreeBlock->Next;
				NewChunk->Block->Next = Block;
				Block = NewChunk->Block;
			}
			else
			{
				NewChunk->Block = PushStruct(Arena, entity_block);
				NewChunk->Block->Next = Block;
				Block = NewChunk->Block;
			}
		}
		Block->EntityIndexes[Block->EntityIndexCount++] = StoredEntityIndex;
	}
}

#define WORLD_MAP_H
#endif
