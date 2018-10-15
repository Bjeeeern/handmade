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
	v2 Offset_;
};

inline tile_map_position
CenteredTilePoint(tile_map_position TilePos)
{
	tile_map_position Result = {};

	Result.AbsTile = TilePos.AbsTile;

	return Result;
}

inline tile_map_position
CenteredTilePoint(u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ)
{
	tile_map_position Result = {};

	Result.AbsTile = {AbsTileX,	AbsTileY, AbsTileZ};

	return Result;
}

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

  internal_function u32
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
  internal_function u32
GetTileValue(tile_map *TileMap, tile_map_position Pos)
{
	return GetTileValue(TileMap, Pos.AbsTile);
}

internal_function b32
IsTileValueEmpty(u32 TileValue)
{
	return (TileValue == 1) || (TileValue == 3) || (TileValue == 4);
}

	internal_function b32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position CanonicalPosition)
{
	b32 Empty = false;

	u32 TileValue = GetTileValue(TileMap, CanonicalPosition.AbsTile);

	return IsTileValueEmpty(TileValue);
}

//TODO(bjorn): Do not use divide/multiply for recanonicalizing. There might 
//occur a loss of presicion that causes the player to get stuck when moving
//slowly with small increments.
	internal_function tile_map_position
RecanonilizePosition(tile_map *TileMap, tile_map_position NewDeltaPosition)
{
	tile_map_position Result = NewDeltaPosition;

	v2s AbsTileOffset = RoundV2ToV2S(NewDeltaPosition.Offset_ / TileMap->TileSideInMeters);
	Result.Offset_ -= AbsTileOffset * TileMap->TileSideInMeters;
	//NOTE(bjorn): TileMap is assumed to be toroidal. Edges across connect.
	Result.AbsTile += (v3s)AbsTileOffset;

	f32 HalfUnit = TileMap->TileSideInMeters / 2.0f;

	//Assert(Result.Offset_.X >= -HalfUnit);
	//Assert(Result.Offset_.X <= HalfUnit);
	//Assert(Result.Offset_.Y >= -HalfUnit);
	//Assert(Result.Offset_.Y <= HalfUnit);

	return Result;
}

struct tile_map_diff
{
	v3s AbsTileDiff;
	v3 MeterDiff;
};
	internal_function tile_map_diff
GetTileMapPosDifference(tile_map *TileMap, tile_map_position A, tile_map_position B)
{
	tile_map_diff Result = {};

	Result.AbsTileDiff = (v3s)A.AbsTile - (v3s)B.AbsTile;

	v2 InternalDiff = A.Offset_ - B.Offset_;
	Result.MeterDiff = (v3)Result.AbsTileDiff * TileMap->TileSideInMeters + (v3)InternalDiff;

	return Result;
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

inline tile_map_position
Offset(tile_map *TileMap, tile_map_position OldPosition, v2 Offset)
{
	OldPosition.Offset_ += Offset;
	return RecanonilizePosition(TileMap, OldPosition);
}

#define TILE_MAP_H
#endif
