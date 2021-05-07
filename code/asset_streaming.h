#if !defined(ASSET_STREAMING_H)

#include "asset.h"
#include "ascii_parse.h"
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

internal_function game_bitmap*
EmptyBitmap(memory_arena* Arena, u32 Width, u32 Height)
{
  game_bitmap* Result = PushStruct(Arena, game_bitmap);

  Result->Width = Width;
  Result->Height = Height;
  Result->WidthOverHeight = SafeRatio0((f32)Result->Width, (f32)Result->Height);
  Result->GPUHandle = 0;

  Result->Memory = 0;

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

  //TODO(Bjorn): Asset streaming.
internal_function void
LoadAsset(game_assets* Assets, game_asset_id ID)
{
  Assert(Assets);

  Assets->Assets[ID].Meta.State = AssetState_Queued;

  char* Path = 0;
  switch(ID)
  {
    case GAI_MainScreen:
      {
        InvalidCodePath;
      } break;
    case GAI_Backdrop:
      {
        Assets->Assets[ID].Type = AssetType_Bitmap;
        Path = "data/test/test_background.bmp";
      } break;
    case GAI_RockWall:
      {
        Assets->Assets[ID].Type = AssetType_Bitmap;
        Path = "data/test2/tuft00.bmp";
        //GetBitmap(Assets, GAI_RockWall)->Alignment = {35, 41};
      } break;
    case GAI_Dirt:
      {
        Assets->Assets[ID].Type = AssetType_Bitmap;
        Path = "data/test2/ground00.bmp";
        //GetBitmap(Assets, GAI_Dirt)->Alignment = {133, 56};
      } break;
    case GAI_Shadow:
      {
        Assets->Assets[ID].Type = AssetType_Bitmap;
        Path = "data/test/test_hero_shadow.bmp";
        //GetBitmap(Assets, GAI_Shadow)->Alignment = {72, 182};
      } break;
    case GAI_Sword:
      {
        Assets->Assets[ID].Type = AssetType_Bitmap;
        Path = "data/test2/ground01.bmp";
        //GetBitmap(Assets, GAI_Sword)->Alignment = {256/2, 116/2};
      } break;
    case GAI_QuadMesh:
      {
        Assets->Assets[ID].Type = AssetType_Mesh;
      } break;
    case GAI_SphereMesh:
      {
        Assets->Assets[ID].Type = AssetType_Mesh;
        Path = "data/sphere.obj";
      } break;

      InvalidDefaultCase;
  }

  if(Assets->Assets[ID].Type == AssetType_Bitmap)
  {
    debug_read_file_result File = DEBUGPlatformReadEntireFile(Path); 

    v2s Rez = PeekAtBMPResolution(File.Content, File.ContentSize);
    Assets->Assets[ID].Bitmap = JunkBitmap(&Assets->Arena, Rez.Width, Rez.Height);

    LoadBMPIntoBitmap(File.Content, File.ContentSize, Assets->Assets[ID].Bitmap);
    ClearEdgeXPix(Assets->Assets[ID].Bitmap, 1);

    DEBUGPlatformFreeFileMemory(File.Content);
  }

  if(Assets->Assets[ID].Type == AssetType_Mesh)
  {
    if(ID == GAI_QuadMesh)
    {
      // TODO(bjorn): Make and use a quadmesh
    }
    if(ID == GAI_SphereMesh)
    {
      debug_read_file_result File = DEBUGPlatformReadEntireFile(Path); 

      s32 VertexCount = 0;
      s32 FaceCount = 0;
      {
        u8* Cursor = File.Content;
        u8* End = File.Content + File.ContentSize - 2;

        do
        {
          if(Cursor[0] == 'v' && Cursor[1] == ' ') { VertexCount += 1; }
          if(Cursor[0] == 'f' && Cursor[1] == ' ') { FaceCount += 1; }
        }
        while(Cursor++ != End);
      }

      game_mesh* Mesh = PushStruct(&Assets->Arena, game_mesh);
      Mesh->Dimensions = 3;
      Mesh->VertexCount = VertexCount;
      Mesh->Vertices = PushArray(&Assets->Arena, Mesh->VertexCount * Mesh->Dimensions, f32);

      Mesh->EdgesPerFace = 3;
      Mesh->FaceCount = FaceCount;
      Mesh->Indicies = PushArray(&Assets->Arena, Mesh->FaceCount * Mesh->EdgesPerFace, u32);
      {
        u8* Cursor = File.Content;
        u8* End = File.Content + File.ContentSize - 2;

        s32 VertIndex = 0;
        s32 FaceIndex = 0;
        do
        {
          if(Cursor[0] == 'v' && Cursor[1] == ' ') 
          {
            while(*Cursor++ != ' ') {}
            Mesh->Vertices[VertIndex*3 + 0] = ParseDecimal(Cursor);
            while(*Cursor++ != ' ') {}
            Mesh->Vertices[VertIndex*3 + 1] = ParseDecimal(Cursor);
            while(*Cursor++ != ' ') {}
            Mesh->Vertices[VertIndex*3 + 2] = ParseDecimal(Cursor);

            VertIndex += 1;
          }
          if(Cursor[0] == 'f' && Cursor[1] == ' ')
          {
            while(*Cursor++ != ' ') {}
            Mesh->Indicies[FaceIndex*3 + 0] = ParseUnsignedInteger(Cursor);
            while(*Cursor++ != ' ') {}
            Mesh->Indicies[FaceIndex*3 + 1] = ParseUnsignedInteger(Cursor);
            while(*Cursor++ != ' ') {}
            Mesh->Indicies[FaceIndex*3 + 2] = ParseUnsignedInteger(Cursor);

            FaceIndex += 1;
          }
        }
        while(Cursor++ != End);

        Assert(VertIndex == VertexCount);
        Assert(FaceIndex == FaceCount);
      }
      Assets->Assets[ID].Mesh = Mesh;

      DEBUGPlatformFreeFileMemory(File.Content);
    }
  }

  CompileTimeWriteBarrier;
  Assets->Assets[ID].Meta.State = AssetState_Loaded;
}

internal_function void
LoadAllAssets(game_assets* Assets)
{
  //TODO(bjorn): Load assets on demand
  LoadAsset(Assets, GAI_RockWall);
  LoadAsset(Assets, GAI_QuadMesh);
  LoadAsset(Assets, GAI_SphereMesh);
}

#define ASSET_STREAMING_H
#endif
