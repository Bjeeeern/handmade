#if !defined(TILE_MAP_H)

#include "math.h"
#include "platform.h"

struct tile_chunk
{
  u32 *Tiles;
};

struct tile_chunk_position
{
	u32 TileChunkX;
	u32 TileChunkY;
	u32 TileChunkZ;

	u32 RelTileX;
	u32 RelTileY;
};

struct tile_map
{
  s32 ChunkShift;
  u32 ChunkMask;
	s32 ChunkDim;

  f32 TileSideInMeters;

	//TODO(bjorn): REAL sparsness so that anywhere in the world can be
	//represented without the giant pointer array.
	u32 ChunkCountX;
	u32 ChunkCountY;
	u32 ChunkCountZ;

  tile_chunk *TileChunks;
};

struct tile_map_position
{
  u32 AbsTileX;
  u32 AbsTileY;
  u32 AbsTileZ;
  
  f32 OffsetX;
  f32 OffsetY;
};

inline tile_chunk_position 
GetChunkPosition(tile_map *TileMap, u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ)
{
	tile_chunk_position Result;

	Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
	Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
	Result.TileChunkZ = AbsTileZ;
	Result.RelTileX = AbsTileX & TileMap->ChunkMask;
	Result.RelTileY = AbsTileY & TileMap->ChunkMask;

	return Result;
}

  internal_function tile_chunk *
GetTileChunk(tile_map *TileMap, u32 TileChunkX, u32 TileChunkY, u32 TileChunkZ)
{
  tile_chunk *TileChunk = 0;

	b32 IsInsideTileMap = (TileChunkX < TileMap->ChunkCountX &&
												 TileChunkY < TileMap->ChunkCountY &&
												 TileChunkZ < TileMap->ChunkCountZ);
  if(IsInsideTileMap)
  {
    TileChunk = &(TileMap->TileChunks[
									TileChunkZ * TileMap->ChunkCountX * TileMap->ChunkCountY +
									TileChunkY * TileMap->ChunkCountX +
								 	TileChunkX]);
  }
  return TileChunk;
}

  inline u32
GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, u32 TileX, u32 TileY)
{
  Assert(TileChunk);
  Assert(0 <= TileX && TileX < (u32)TileMap->ChunkDim);
  Assert(0 <= TileY && TileY < (u32)TileMap->ChunkDim);

  u32 TileValue = TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];

  return TileValue;
}

  internal_function b32
GetTileValue(tile_map *TileMap, u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ)
{
  u32 TileValue = 0;

	tile_chunk_position ChunkPosition = GetChunkPosition(TileMap,
																											 AbsTileX,
																											 AbsTileY,
																											 AbsTileZ);
	tile_chunk *TileChunk = GetTileChunk(TileMap, 
																			 ChunkPosition.TileChunkX,
																			 ChunkPosition.TileChunkY,
																			 ChunkPosition.TileChunkZ);
  if(TileChunk && TileChunk->Tiles)
	{
		TileValue = GetTileValueUnchecked(TileMap, TileChunk, 
																			ChunkPosition.RelTileX, 
																			ChunkPosition.RelTileY);
	}

	return TileValue;
}
  internal_function b32
GetTileValue(tile_map *TileMap, tile_map_position Pos)
{
	return GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);
}

	internal_function b32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position CanonicalPosition)
{
	b32 Empty = false;

	u32 TileValue = GetTileValue(TileMap, 
															 CanonicalPosition.AbsTileX, 
															 CanonicalPosition.AbsTileY,
															 CanonicalPosition.AbsTileZ);
	Empty = (TileValue == 1) ||
		(TileValue == 3) ||
		(TileValue == 4);

	return Empty;
}

//TODO(bjorn): Do not use divide/multiply for recanonicalizing. There might 
//occur a loss of presicion that causes the player to get stuck when moving
//slowly with small increments.
	internal_function void
RecanonilizeSubTileRelativeCenter(f32 *InnerDimension, f32 UnitLenghtOfInner, 
																	u32 *OuterDimension)
{
	s32 Offset = RoundF32ToS32(*InnerDimension / UnitLenghtOfInner);
	*InnerDimension -= UnitLenghtOfInner * Offset;
	//NOTE(bjorn): TileMap is assumed to be toroidal. Edges across connect.
	*OuterDimension += Offset;

	f32 HalfUnit = UnitLenghtOfInner / 2.0f;
	Assert(*InnerDimension >= -HalfUnit);
	Assert(*InnerDimension <= HalfUnit);
}

	internal_function tile_map_position
RecanonilizePosition(tile_map *TileMap, tile_map_position NewDeltaPosition)
{
	tile_map_position Result = NewDeltaPosition;

	RecanonilizeSubTileRelativeCenter(&Result.OffsetX, TileMap->TileSideInMeters, 
																		&Result.AbsTileX);
	RecanonilizeSubTileRelativeCenter(&Result.OffsetY, TileMap->TileSideInMeters, 
																		&Result.AbsTileY);

	return Result;
}

	internal_function v3
CalculateMeterDelta(tile_map *TileMap, tile_map_position A, tile_map_position B)
{
	v3 Delta = {};

	s32 TileDeltaX = (A.AbsTileX - B.AbsTileX);
	s32 TileDeltaY = (A.AbsTileY - B.AbsTileY);
	s32 TileDeltaZ = (A.AbsTileZ - B.AbsTileZ);

	Delta.X = A.OffsetX - B.OffsetX + TileDeltaX*TileMap->TileSideInMeters;
	Delta.Y = A.OffsetY - B.OffsetY + TileDeltaY*TileMap->TileSideInMeters;
	Delta.Z = TileDeltaZ*TileMap->TileSideInMeters;

	return Delta;
}

	internal_function void
SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, u32 TileX, u32 TileY, 
											u32 TileValue)
{
	Assert(TileChunk);
	Assert(0 <= TileX && TileX < (u32)TileMap->ChunkDim);
	Assert(0 <= TileY && TileY < (u32)TileMap->ChunkDim);

	TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
}

	internal_function void
SetTileValue(memory_arena *Arena, tile_map *TileMap, 
						 u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ, u32 TileValue)
{
	tile_chunk_position ChunkPosition = GetChunkPosition(TileMap,
																											 AbsTileX,
																											 AbsTileY,
																											 AbsTileZ);
	tile_chunk *TileChunk = GetTileChunk(TileMap, 
																			 ChunkPosition.TileChunkX,
																			 ChunkPosition.TileChunkY,
																			 ChunkPosition.TileChunkZ);

	Assert(TileChunk);
	if(!TileChunk->Tiles)
	{
		u32 TileChunkTileCount = TileMap->ChunkDim * TileMap->ChunkDim;
		TileChunk->Tiles = PushArray(Arena, TileChunkTileCount, u32);
		for(u32 TileChunkIndex = 0;
				TileChunkIndex < TileChunkTileCount;
				++TileChunkIndex)
		{
			TileChunk->Tiles[TileChunkIndex] = 1;
		}
	}

	SetTileValueUnchecked(TileMap, TileChunk, 
												ChunkPosition.RelTileX, 
												ChunkPosition.RelTileY,
											 	TileValue);
}

#define TILE_MAP_H
#endif
