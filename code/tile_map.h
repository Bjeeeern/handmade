#if !defined(TILE_MAP_H)

#include "math.h"
#include "platform.h"

struct tile_chunk
{
  u32 *Tiles;
};

struct tile_chunk_position
{
	v3u TileChunk;

	v2u RelTile;
};

struct tile_map
{
  s32 ChunkShift;
  u32 ChunkMask;
	s32 ChunkDim;

  f32 TileSideInMeters;

	//TODO(bjorn): REAL sparsness so that anywhere in the world can be
	//represented without the giant pointer array.
	v3u ChunkCount;

  tile_chunk *TileChunks;
};

struct tile_map_position
{
	v3u AbsTile;
  
	v2 Offset;
};

inline tile_chunk_position 
GetChunkPosition(tile_map *TileMap, v3u AbsTile)
{
	tile_chunk_position Result;

	Result.TileChunk.X = AbsTile.X >> TileMap->ChunkShift;
	Result.TileChunk.Y = AbsTile.Y >> TileMap->ChunkShift;
	Result.TileChunk.Z = AbsTile.Z;
	Result.RelTile.X = AbsTile.X & TileMap->ChunkMask;
	Result.RelTile.Y = AbsTile.Y & TileMap->ChunkMask;

	return Result;
}

  internal_function tile_chunk *
GetTileChunk(tile_map *TileMap, v3u PotentialTileChunk)
{
  tile_chunk *TileChunk = 0;

	b32 IsInsideTileMap = (PotentialTileChunk.X < TileMap->ChunkCount.X &&
												 PotentialTileChunk.Y < TileMap->ChunkCount.Y &&
												 PotentialTileChunk.Z < TileMap->ChunkCount.Z);
  if(IsInsideTileMap)
  {
    TileChunk = &(TileMap->TileChunks[
									PotentialTileChunk.Z * TileMap->ChunkCount.X * TileMap->ChunkCount.Y +
									PotentialTileChunk.Y * TileMap->ChunkCount.X +
								 	PotentialTileChunk.X]);
  }
  return TileChunk;
}

  inline u32
GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, v2u Tile)
{
  Assert(TileChunk);
  Assert(0 <= Tile.X && Tile.X < (u32)TileMap->ChunkDim);
  Assert(0 <= Tile.Y && Tile.Y < (u32)TileMap->ChunkDim);

  u32 TileValue = TileChunk->Tiles[Tile.Y * TileMap->ChunkDim + Tile.X];

  return TileValue;
}

  internal_function b32
GetTileValue(tile_map *TileMap, v3u AbsTile)
{
  u32 TileValue = 0;

	tile_chunk_position ChunkPosition = GetChunkPosition(TileMap, AbsTile);
	tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPosition.TileChunk);

  if(TileChunk && TileChunk->Tiles)
	{
		TileValue = GetTileValueUnchecked(TileMap, TileChunk, ChunkPosition.RelTile);
	}

	return TileValue;
}
  internal_function b32
GetTileValue(tile_map *TileMap, tile_map_position Pos)
{
	return GetTileValue(TileMap, Pos.AbsTile);
}

	internal_function b32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position CanonicalPosition)
{
	b32 Empty = false;

	u32 TileValue = GetTileValue(TileMap, CanonicalPosition.AbsTile);

	Empty = (TileValue == 1) ||
		(TileValue == 3) ||
		(TileValue == 4);

	return Empty;
}

//TODO(bjorn): Do not use divide/multiply for recanonicalizing. There might 
//occur a loss of presicion that causes the player to get stuck when moving
//slowly with small increments.
	internal_function tile_map_position
RecanonilizePosition(tile_map *TileMap, tile_map_position NewDeltaPosition)
{
	tile_map_position Result = NewDeltaPosition;

	v2s AbsTileOffset = RoundV2ToV2S(NewDeltaPosition.Offset / TileMap->TileSideInMeters);
	Result.Offset -= AbsTileOffset * TileMap->TileSideInMeters;
	//NOTE(bjorn): TileMap is assumed to be toroidal. Edges across connect.
	Result.AbsTile += (v3s)AbsTileOffset;

	f32 HalfUnit = TileMap->TileSideInMeters / 2.0f;
	//Assert(Result.Offset.X >= -HalfUnit);
	//Assert(Result.Offset.X <= HalfUnit);
	//Assert(Result.Offset.Y >= -HalfUnit);
	//Assert(Result.Offset.Y <= HalfUnit);

	return Result;
}

	internal_function v3
CalculateMeterDelta(tile_map *TileMap, tile_map_position A, tile_map_position B)
{
	v3 Delta = {};

	v3s TileDelta = (v3u)A.AbsTile - (v3u)B.AbsTile;
	v2 Offset = A.Offset - B.Offset;

	Delta = (v3)TileDelta * TileMap->TileSideInMeters + (v3)Offset;

	return Delta;
}

	internal_function void
SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, v2u Tile, u32 TileValue)
{
	Assert(TileChunk);
	Assert(0 <= Tile.X && Tile.X < (u32)TileMap->ChunkDim);
	Assert(0 <= Tile.Y && Tile.Y < (u32)TileMap->ChunkDim);

	TileChunk->Tiles[Tile.Y * TileMap->ChunkDim + Tile.X] = TileValue;
}

	internal_function void
SetTileValue(memory_arena *Arena, tile_map *TileMap, v3u AbsTile, u32 TileValue)
{
	tile_chunk_position ChunkPosition = GetChunkPosition(TileMap, AbsTile);
	tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPosition.TileChunk);

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

	SetTileValueUnchecked(TileMap, TileChunk, ChunkPosition.RelTile, TileValue);
}

#define TILE_MAP_H
#endif
