#if !defined(ASSET_H)

//                                                              high     low
// NOTE(bjorn): Expected pixel layout in memory is top to bottom AA RR GG BB.
//              Pixels are laid out in a bottom-up, left-right order.
#define GAME_BITMAP_BYTES_PER_PIXEL 4
struct game_bitmap
{
  u32 *Memory;
	union
	{
		v2u Dim;
		struct
		{
			u32 Width;
			u32 Height;
		};
	};
  s32 Pitch;
  v2s Alignment;

  f32 WidthOverHeight;

  s32 GPUHandle; //TODO(bjorn): Think about this.
};

struct hero_bitmaps
{
	game_bitmap Head;
	game_bitmap Cape;
	game_bitmap Torso;
};

enum game_asset_id
{
	GAI_MainScreen,
	GAI_Backdrop,

	GAI_RockWall,
	GAI_Dirt,
	GAI_Shadow,
	GAI_Sword,

  GAI_Count,
};

enum game_asset_state
{
  AssetState_Unloaded,
  AssetState_Queued,
  AssetState_Loaded,
  AssetState_Locked,
};

struct game_bitmap_meta
{
  u32 RequestLoad;

  v2s Rez;
  b32 RezKnown;
  game_asset_state State;
};

struct game_assets
{
  memory_arena Arena;

  //TODO(bjorn): Merge into one asset thing containing needed data.
	game_bitmap_meta BitmapsMeta[GAI_Count];
	game_bitmap* Bitmaps[GAI_Count];

  //TODO(bjorn): What HMHM did.
#if 0
  font* Font;
  game_bitmap GenFontMap;
  game_bitmap GenTile;

	hero_bitmaps HeroBitmaps[4];

  game_bitmap Grass[2];
  game_bitmap Ground[4];
  game_bitmap Rock[4];
  game_bitmap Tuft[3];
  game_bitmap Tree[3];
#endif
};

inline game_bitmap* 
GetBitmap(game_assets* Assets, game_asset_id ID)
{
  game_bitmap* Result = 0;

  Assert(Assets);
  Result = Assets->Bitmaps[ID];

  //NOTE(bjorn): Multi-pass so we must assert before doing the load.
  if(Assets->BitmapsMeta[ID].State == AssetState_Loaded ||
     Assets->BitmapsMeta[ID].State == AssetState_Locked) 
  { 
    Assert(Result); 
  }

  //TODO(Bjorn): Asset streaming.
#if 0
  if(Assets->BitmapsMeta[ID].State == AssetState_Unloaded)
  {
    Assets->BitmapsMeta[ID].RequestLoad += 1;
  }
#endif

  return Result;
}

#define ASSET_H
#endif
