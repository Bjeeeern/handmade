#if !defined(WORLD_MAP_H)

#include "math.h"
#include "platform.h"

#define WORLD_CHUNK_SAFETY_MARGIN 

struct entity_block
{
	u32 EntityIndexCount;
	u32 EntityIndexes[16];

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

  f32 TileSideInMeters;
  f32 ChunkSideInMeters;

  world_chunk HashMap[4096];

	entity_block* FreeBlock;
};

struct world_map_position
{
	v3s ChunkP;
	v2 Offset_;
};

#define TILES_PER_CHUNK 16

inline world_map_position 
GetChunkPositionFromTilePos(world_map *WorldMap, v3s AbsTile)
{
	world_map_position Result = {};

	Result.ChunkP.X = AbsTile.X / TILES_PER_CHUNK;
	Result.ChunkP.Y = AbsTile.Y / TILES_PER_CHUNK;
	Result.ChunkP.Z = AbsTile.Z;

	Result.Offset_.X = (AbsTile.X - Result.ChunkP.X * TILES_PER_CHUNK) * WorldMap->TileSideInMeters;
	Result.Offset_.Y = (AbsTile.Y - Result.ChunkP.Y * TILES_PER_CHUNK) * WorldMap->TileSideInMeters;

	return Result;
}

  internal_function world_chunk *
GetWorldChunk(world_map *WorldMap, v3s PotentialChunkP, memory_arena* Arena = 0)
{
  world_chunk *WorldChunk = 0;

	Assert(PotentialChunkP.X <  WorldMap->ChunkSafetyMargin &&
				 PotentialChunkP.Y <  WorldMap->ChunkSafetyMargin &&
				 PotentialChunkP.Z <  WorldMap->ChunkSafetyMargin &&
				 PotentialChunkP.X > -WorldMap->ChunkSafetyMargin &&
				 PotentialChunkP.Y > -WorldMap->ChunkSafetyMargin &&
				 PotentialChunkP.Z > -WorldMap->ChunkSafetyMargin);

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
				Assert(WorldChunk->Block);
				SlotFoundOrCreated = true;
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
IsCanonical(world_map *WorldMap, v2 Offset)
{
	f32 HalfUnit = WorldMap->ChunkSideInMeters / 2.0f;

	b32 Result = ( (Offset.X >= -HalfUnit) &&
								 (Offset.X <=  HalfUnit) &&
								 (Offset.Y >= -HalfUnit) &&
								 (Offset.Y <=  HalfUnit) );

	return Result;
}

//TODO(bjorn): Do not use divide/multiply for recanonicalizing. There might 
//occur a loss of presicion that causes the player to get stuck when moving
//slowly with small increments.
	internal_function world_map_position
RecanonilizePosition(world_map *WorldMap, world_map_position NewDeltaPosition)
{
	world_map_position Result = NewDeltaPosition;

	v2s ChunkOffset = RoundV2ToV2S(NewDeltaPosition.Offset_ / WorldMap->ChunkSideInMeters);
	Result.Offset_ -= ChunkOffset * WorldMap->ChunkSideInMeters;
	Result.ChunkP += (v3s)ChunkOffset;

	Assert(IsCanonical(WorldMap, Result.Offset_));
	return Result;
}

	internal_function v3
GetWorldMapPosDifference(world_map *WorldMap, world_map_position A, world_map_position B)
{
	v3 Result = {};

	v3s ChunkDiff = (v3s)A.ChunkP - (v3s)B.ChunkP;

	v2 InternalDiff = A.Offset_ - B.Offset_;
	Result = (v3)ChunkDiff * WorldMap->ChunkSideInMeters + (v3)InternalDiff;

	return Result;
}

	inline world_map_position
Offset(world_map *WorldMap, world_map_position OldPosition, v2 Offset)
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

inline void
ChangeEntityLocation(memory_arena* Arena, world_map* WorldMap, u32 LowEntityIndex,
										 world_map_position* OldP, world_map_position* NewP)
{
	if(OldP && AreInSameChunk(WorldMap, OldP, NewP))
	{
		return; //NOTE(): Entity not needed to be moved.
	}
	else
	{
		if(OldP)
		{
			world_chunk* OldChunk = GetWorldChunk(WorldMap, OldP->ChunkP);
			Assert(OldChunk);

			entity_block* FirstBlock = OldChunk->Block;
			for(entity_block* Block = FirstBlock;
					Block;
					Block = Block->Next)
			{
				for(u32 Index = 0;
						Index < Block->EntityIndexCount;
						Index++)
				{
					if(Block->EntityIndexes[Index] == LowEntityIndex)
					{
						Assert(FirstBlock->EntityIndexCount > 0);

						u32 TopEntityIndex = FirstBlock->EntityIndexes[FirstBlock->EntityIndexCount];
						FirstBlock->EntityIndexCount--;

						Block->EntityIndexes[Index] = TopEntityIndex;

						if(FirstBlock->EntityIndexCount == 0)
						{
							OldChunk->Block = FirstBlock->Next;
							FirstBlock->Next = WorldMap->FreeBlock;
							WorldMap->FreeBlock = FirstBlock;
						}

						Block = 0;
						break;
					}
				}
			}
		}
		else
		{
			//NOTE(): Entity is being moved into map for the first time.
		}
	}

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
		}
		else
		{
			NewChunk->Block = PushStruct(Arena, entity_block);
			NewChunk->Block->Next = Block;
			Block = NewChunk->Block;
		}
	}
	Block->EntityIndexes[++Block->EntityIndexCount] = LowEntityIndex;
}


#define WORLD_MAP_H
#endif
