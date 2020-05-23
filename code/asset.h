#if !defined(ASSET_H)

#include "file_format_BMP.h"

internal_function game_bitmap*
ClearBitmap(memory_arena* Arena, u32 Width, u32 Height)
{
  game_bitmap* Result = PushStruct(Arena, game_bitmap);

  Result->Width = Width;
  Result->Height = Height;
  Result->Pitch = Align4(Result->Width);
  Result->WidthOverHeight = SafeRatio0((f32)Result->Width, (f32)Result->Height);

  u32* BitmapMemory = PushArray(Arena, Result->Pitch*Result->Height + 4, u32);
  Result->Memory = (u32*)Align16((memi)BitmapMemory);
  ZeroMemory((u8*)Result->Memory, Result->Pitch*Result->Height*GAME_BITMAP_BYTES_PER_PIXEL);

  return Result;
}

internal_function game_bitmap*
JunkBitmap(memory_arena* Arena, u32 Width, u32 Height)
{
  game_bitmap* Result = PushStruct(Arena, game_bitmap);

  Result->Width = Width;
  Result->Height = Height;
  Result->Pitch = Align4(Result->Width);
  Result->WidthOverHeight = SafeRatio0((f32)Result->Width, (f32)Result->Height);

  u32* BitmapMemory = PushArray(Arena, Result->Pitch*Result->Height + 4, u32);
  Result->Memory = (u32*)Align16((memi)BitmapMemory);

  return Result;
}

internal_function void 
ClearEdgeXPix(game_bitmap* Bitmap, u32 PixelsToClear)
{
  //TODO(bjorn): Support wider frames when sampling is more than 2x2.
  Assert(PixelsToClear == 1);

  u32* Row1 = Bitmap->Memory + (Bitmap->Pitch * 0) + 0;
  u32* Row2 = Bitmap->Memory + (Bitmap->Pitch * (Bitmap->Height-1)) + 0;
  for(u32 X = 0; 
      X < Bitmap->Width; 
      X++) 
  { 
    *Row1++ = 0;
    *Row2++ = 0;
  }
  u32* Col1 = Bitmap->Memory + (Bitmap->Pitch * 0) + 0;
  u32* Col2 = Bitmap->Memory + (Bitmap->Pitch * 0) + (Bitmap->Width-1);
  for(u32 Y = 0; 
      Y < Bitmap->Height; 
      Y++) 
  { 
    *Col1 = 0;
    *Col2 = 0;

    Col1 += Bitmap->Pitch; 
    Col2 += Bitmap->Pitch;
  } 
}

struct hero_bitmaps
{
	game_bitmap Head;
	game_bitmap Cape;
	game_bitmap Torso;
};

enum game_asset_id
{
	GAI_Backdrop,
	GAI_RockWall,
	GAI_Dirt,
	GAI_Shadow,
	GAI_Sword,

  GAI_Count,
};

enum game_asset_state
{
  AssetState_NeedFetch,
  AssetState_Loading,
  AssetState_Loaded,
};

struct game_bitmap_meta
{
  v2s Rez;
  b32 RezKnown;
  game_asset_state State;
};

struct game_assets
{
  memory_arena Arena;
  debug_platform_read_entire_file* DEBUGPlatformReadEntireFile;
  debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;


  font* Font;
  game_bitmap GenFontMap;
  game_bitmap GenTile;

	game_bitmap_meta BitmapsMeta[GAI_Count];
	game_bitmap* Bitmaps[GAI_Count];

	hero_bitmaps HeroBitmaps[4];

  game_bitmap Grass[2];
  game_bitmap Ground[4];
  game_bitmap Rock[4];
  game_bitmap Tuft[3];
  game_bitmap Tree[3];
};

internal_function void
LoadAsset(game_assets* Assets, game_asset_id ID);

inline game_bitmap* 
GetBitmap(game_assets* Assets, game_asset_id ID)
{
  game_bitmap* Result = 0;

  Assert(Assets);
  Result = Assets->Bitmaps[ID];

  //NOTE(bjorn): Multi-pass so we must assert before doing the load.
  if(Assets->BitmapsMeta[ID].State == AssetState_Loaded) { Assert(Result); }

  if(Assets->BitmapsMeta[ID].State == AssetState_NeedFetch)
  {
    LoadAsset(Assets, ID);
  }

  return Result;
}

internal_function void
LoadAsset(game_assets* Assets, game_asset_id ID)
{
  Assets->BitmapsMeta[ID].State = AssetState_Loading;

  char* Path = 0;
  switch(ID)
  {
    case GAI_Backdrop:
      {
        Path = "data/test/test_background.bmp";
      } break;
    case GAI_RockWall:
      {
        Path = "data/test2/rock00.bmp";
        //GetBitmap(Assets, GAI_RockWall)->Alignment = {35, 41};
      } break;
    case GAI_Dirt:
      {
        Path = "data/test2/ground00.bmp";
        //GetBitmap(Assets, GAI_Dirt)->Alignment = {133, 56};
      } break;
    case GAI_Shadow:
      {
        Path = "data/test/test_hero_shadow.bmp";
        //GetBitmap(Assets, GAI_Shadow)->Alignment = {72, 182};
      } break;
    case GAI_Sword:
      {
        Path = "data/test2/ground01.bmp";
        //GetBitmap(Assets, GAI_Sword)->Alignment = {256/2, 116/2};
      } break;

      InvalidDefaultCase;
  }

  if(!Assets->BitmapsMeta[ID].RezKnown)
  {
    //TODO(bjorn): Multi-thread.
    debug_read_file_result File = Assets->DEBUGPlatformReadEntireFile(Path); 

    v2s Rez = PeekAtBMPResolution(File.Content, File.ContentSize);
    Assets->BitmapsMeta[ID].Rez = Rez;

    CompileTimeWriteBarrier;
    Assets->BitmapsMeta[ID].RezKnown = true;
    Assets->BitmapsMeta[ID].State = AssetState_NeedFetch;

    Assets->DEBUGPlatformFreeFileMemory(File.Content);
  }
  else
  {
    v2s Rez = Assets->BitmapsMeta[ID].Rez;
    Assets->Bitmaps[ID] = JunkBitmap(&Assets->Arena, Rez.Width, Rez.Height);

    //TODO(bjorn): Multi-thread.
    debug_read_file_result File = Assets->DEBUGPlatformReadEntireFile(Path); 
    LoadBMPIntoBitmap(File.Content, File.ContentSize, Assets->Bitmaps[ID]);

    CompileTimeWriteBarrier;
    Assets->BitmapsMeta[ID].State = AssetState_Loaded;

    Assets->DEBUGPlatformFreeFileMemory(File.Content);
  }
}

#define ASSET_H
#endif
