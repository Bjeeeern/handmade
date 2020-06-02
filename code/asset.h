#if !defined(ASSET_H)

#include "file_format_BMP.h"
#include "font.h"

//STUDY(bjorn):
//
// As far as I see it, to get asset eviction working safely multithreaded I have two options:
//
// * Every asset has a LockCounter that gets atomically inc/decremented. Evict at zero. 
//   ** This could be optimized by having worker threads not being able to cause loads.
//      (Load on main thread and only start the work when all is present and locked)
//      NOTE E.G. ONLY MAINTHREAD LOAD/EVICT
// * Every thread has it's own Assets object. 

// TODO(bjorn) ONLY MAINTHREAD LOAD/EVICT
#if 0 //Example code
DoRenderWork()
{
  // Work
  UnlockAssetGroup(Assets, ID);
}
if(AssetGroupIsLoaded(Assets, ID))
{
  LockAssetGroup(Assets, ID);
  DoRenderWork();
}
#endif

//STUDY(bjorn): Font map generation sortof depends on the render group builder thingie but that sortof depends on assets. 
#if 0
internal_function void
GenerateGlyph(render_group* RenderGroup, font* Font,
              game_bitmap* Buffer, u8 Character)
{
  unicode_to_glyph_data *Entries = (unicode_to_glyph_data *)(Font + 1);

  s32 CharEntryIndex = 0;
  for(s32 EntryIndex = 0;
      EntryIndex < Font->UnicodeCodePointCount;
      EntryIndex++)
  {
    if(Character == Entries[EntryIndex].UnicodeCodePoint)
    {
      CharEntryIndex = EntryIndex;
      break;
    }
	}

  PushCamera(RenderGroup, Buffer, (f32)Buffer->Height, M44Identity(), positive_infinity32, 0.5f);

	s32 Offset = Entries[CharEntryIndex].OffsetToGlyphData;
	s32 CurveCount = Entries[CharEntryIndex].QuadraticCurveCount;
	v2 GlyphDim = {(f32)Buffer->Width, Buffer->Height*(3.0f/4.0f)};
	v2 GlyphOrigin = {-(Buffer->Width*(2.0f/4.0f)), -(Buffer->Height*(1.0f/4.0f))};

	quadratic_curve *Curves = (quadratic_curve *)((u8 *)Font + Offset);
	for(s32 CurveIndex = 0;
			CurveIndex < CurveCount;
			CurveIndex++)
	{
		quadratic_curve Curve = Curves[CurveIndex];

		v2 Sta = Hadamard(Curve.Srt, GlyphDim) + GlyphOrigin;
		v2 Mid = Hadamard(Curve.Con, GlyphDim) + GlyphOrigin;
		v2 End = Hadamard(Curve.End, GlyphDim) + GlyphOrigin;

    PushTriangleFillFlip(RenderGroup, Sta, {1.0f,0.5f}, End);
  }

	for(s32 CurveIndex = 0;
			CurveIndex < CurveCount;
			CurveIndex++)
	{
		quadratic_curve Curve = Curves[CurveIndex];

		v2 Sta = Hadamard(Curve.Srt, GlyphDim) + GlyphOrigin;
		v2 Mid = Hadamard(Curve.Con, GlyphDim) + GlyphOrigin;
		v2 End = Hadamard(Curve.End, GlyphDim) + GlyphOrigin;

    PushQuadBezierFlip(RenderGroup, Sta, Mid, End);
  }

#if 0
	for(s32 CurveIndex = 0;
			CurveIndex < CurveCount;
			CurveIndex++)
	{
		quadratic_curve Curve = Curves[CurveIndex];

		v2 Sta = Hadamard(Curve.Srt, GlyphDim) + GlyphOrigin;
		v2 Mid = Hadamard(Curve.Con, GlyphDim) + GlyphOrigin;
		v2 End = Hadamard(Curve.End, GlyphDim) + GlyphOrigin;

    PushVector(RenderGroup, M44Identity(), Sta, End, {0,0,1});
    PushVector(RenderGroup, M44Identity(), Sta, Mid, {0,1,0});
    PushVector(RenderGroup, M44Identity(), End, Mid, {0,1,0});
  }

  PushQuadBezierFlip(RenderGroup, Hadamard({-0.5f,0.5f}, GlyphDim), {0,0}, Hadamard({0.5f,0.5f}, GlyphDim));
  PushQuadBezierFlip(RenderGroup, Hadamard({-0.3f,0.4f}, GlyphDim), {0,0}, Hadamard({0.3f,0.4f}, GlyphDim));
#endif
}

internal_function rectangle2 
CharacterToFontMapLocation(u8 Character)
{
  rectangle2 Result;

  Assert(Character <= 126); 

  u32 CharPerRow = 16;
  v2 Dim = {1.0f/(f32)CharPerRow, 1.0f/(CharPerRow * 0.5f) };
  v2 BottomLeft = {Dim.X * (Character%CharPerRow), Dim.Y * (Character/CharPerRow)};

  return RectMinDim(BottomLeft, Dim);
}

struct font_gen_work
{
  game_bitmap* Buffer; 
  s8 Character;
  font* Font;
  game_assets* Assets;
};
WORK_QUEUE_CALLBACK(DoFontGenWork)
{
  Assert(Memory);
  Assert(Size > 0);

  font_gen_work* FontGenWork = (font_gen_work*)Data;

  game_bitmap* Buffer = FontGenWork->Buffer;
  s8 Character        = FontGenWork->Character;
  font* Font          = FontGenWork->Font;
  game_assets* Assets = FontGenWork->Assets;

  memory_arena Arena = InitializeArena(Size, Memory);

  game_bitmap* Glyph = ClearBitmap(&Arena, 512, 1024);
  Glyph->Alignment = {256, 512};

  render_group* RenderGroup = AllocateRenderGroup(&Arena, Megabytes(1));

  GenerateGlyph(RenderGroup, Font, Glyph, Character);
  ClearRenderGroup(RenderGroup);

  rectangle2 TargetLoc = CharacterToFontMapLocation(Character);
  m44 GlyphPos = ConstructTransform(GetRectCenter(TargetLoc) * Buffer->Width, 
                                    GetRectDim(TargetLoc) * Buffer->Width);

  PushQuad(RenderGroup, GlyphPos, Glyph);

  SetCamera(RenderGroup, ConstructTransform(v2{-(Buffer->Width*0.5f), -(Buffer->Height*0.5f)}), 
            positive_infinity32, 0.5f);
  if(GPURenderGroupToOutput)
  {
    GPURenderGroupToOutput(RenderGroup, Buffer, (f32)Buffer->Height);
  }
  else
  {
    SingleTileRenderGroupToOutput(RenderGroup, Buffer, (f32)Buffer->Height);
  }
}

  internal_function void
GenerateFontMap(work_queue* WorkQueue, memory_arena* TransientArena, font* Font, 
                game_bitmap* Buffer, game_assets* Assets)
{
  //TODO(bjorn): What to do about this memory, since it is so small.
  // Maybe this should be like a 'per queue' memoryblock that automatically
  // clears when the queue empties.
  font_gen_work* FontGenWork = PushArray(TransientArena, 127, font_gen_work);
  s32 Index = 0;
  for(s8 Character = 0;
      Character < 127;
      Character++)
  {
    FontGenWork[Index].Buffer           = Buffer;
    FontGenWork[Index].Character        = Character;
    FontGenWork[Index].Font             = Font;
    FontGenWork[Index].Assets           = Assets;

    PushWork(WorkQueue, DoFontGenWork, FontGenWork + Index);
    Index += 1;
  }
}
#endif

internal_function game_bitmap*
ClearBitmap(memory_arena* Arena, u32 Width, u32 Height)
{
  game_bitmap* Result = PushStruct(Arena, game_bitmap);

  Result->Width = Width;
  Result->Height = Height;
  Result->Pitch = Align4(Result->Width);
  Result->WidthOverHeight = SafeRatio0((f32)Result->Width, (f32)Result->Height);
  Result->GPUHandle = 0;

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
  Result->GPUHandle = 0;

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
  if(Assets->BitmapsMeta[ID].State == AssetState_Loaded ||
     Assets->BitmapsMeta[ID].State == AssetState_Locked) 
  { 
    Assert(Result); 
  }

  if(Assets->BitmapsMeta[ID].State == AssetState_Unloaded)
  {
    LoadAsset(Assets, ID);
  }

  return Result;
}

internal_function void
LoadAsset(game_assets* Assets, game_asset_id ID)
{
  Assets->BitmapsMeta[ID].State = AssetState_Queued;

  char* Path = 0;
  switch(ID)
  {
    case GAI_MainScreen:
      {
        InvalidCodePath;
      } break;
    case GAI_Backdrop:
      {
        Path = "data/test/test_background.bmp";
      } break;
    case GAI_RockWall:
      {
        Path = "data/test2/tuft00.bmp";
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
    Assets->BitmapsMeta[ID].State = AssetState_Unloaded;

    Assets->DEBUGPlatformFreeFileMemory(File.Content);
  }
  else
  {
    v2s Rez = Assets->BitmapsMeta[ID].Rez;
    Assets->Bitmaps[ID] = JunkBitmap(&Assets->Arena, Rez.Width, Rez.Height);

    //TODO(bjorn): Multi-thread.
    debug_read_file_result File = Assets->DEBUGPlatformReadEntireFile(Path); 
    LoadBMPIntoBitmap(File.Content, File.ContentSize, Assets->Bitmaps[ID]);

    ClearEdgeXPix(Assets->Bitmaps[ID], 1);

    CompileTimeWriteBarrier;
    Assets->BitmapsMeta[ID].State = AssetState_Loaded;

    Assets->DEBUGPlatformFreeFileMemory(File.Content);
  }
}

#define ASSET_H
#endif
