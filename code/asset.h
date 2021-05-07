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

struct game_mesh
{
  f32* Vertices;
  s32 VertexCount;
  s32 Dimensions;

  u32* Indicies;
  s32 FaceCount;
  s32 EdgesPerFace;
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

	GAI_QuadMesh,
	GAI_SphereMesh,

  GAI_Count,
};

enum game_asset_state
{
  AssetState_Unloaded,
  AssetState_Queued,
  AssetState_Loaded,
  AssetState_Locked,
};

struct game_asset_meta
{
  u32 RequestLoad;

  game_asset_state State;
};

enum game_asset_type
{
  AssetType_Bitmap,
  AssetType_Mesh,
};

struct game_asset
{
  game_asset_meta Meta;
  game_asset_type Type;
  union
  {
    game_bitmap* Bitmap;
    game_mesh* Mesh;
  };
};

struct game_assets
{
  memory_arena Arena;
	game_asset Assets[1024];
};

inline game_bitmap* 
GetBitmap(game_assets* Assets, game_asset_id ID)
{
  game_bitmap* Result = 0;

  Assert(Assets);
  Assert(Assets->Assets[ID].Type == AssetType_Bitmap);
  Result = Assets->Assets[ID].Bitmap;

  //NOTE(bjorn): Multi-pass so we must assert before doing the load.
  if(Assets->Assets[ID].Meta.State == AssetState_Loaded ||
     Assets->Assets[ID].Meta.State == AssetState_Locked) 
  { 
    Assert(Result); 
  }

  //TODO(Bjorn): Asset streaming.
#if 0
  if(Assets->Assets[ID].Meta.State == AssetState_Unloaded)
  {
    Assets->Assets[ID].Meta.RequestLoad += 1;
  }
#endif

  return Result;
}

inline game_mesh* 
GetMesh(game_assets* Assets, game_asset_id ID)
{
  game_mesh* Result = 0;

  Assert(Assets);
  Assert(Assets->Assets[ID].Type == AssetType_Mesh);
  Result = Assets->Assets[ID].Mesh;

  //NOTE(bjorn): Multi-pass so we must assert before doing the load.
  if(Assets->Assets[ID].Meta.State == AssetState_Loaded ||
     Assets->Assets[ID].Meta.State == AssetState_Locked) 
  { 
    Assert(Result); 
  }

  //TODO(Bjorn): Asset streaming.
#if 0
  if(Assets->Assets[ID].Meta.State == AssetState_Unloaded)
  {
    Assets->Assets[ID].RequestLoad += 1;
  }
#endif

  return Result;
}

#define ASSET_H
#endif
