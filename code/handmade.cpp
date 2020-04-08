#include "platform.h"
multi_thread_push_work* PushWork;
multi_thread_complete_work* CompleteWork;
#include "memory.h"
#include "world_map.h"
#include "random.h"
#include "render_group.h"
#include "resource.h"
#include "sim_region.h"
#include "entity.h"
#include "collision.h"
#include "trigger.h"

// @IMPORTANT @IDEA
// Maybe sort the order of entity execution while creating the sim regions so
// that there is a guaranteed hirarchiy to the collision resolution. Depending
// on how I want to do the collision this migt be a nice tool.

// QUICK TODO
// BUG The first location floor and some random walls are not getting unloaded
//		 because of cascading trigger references.
// BUG Hunting is super shaky.

// TODO
// * Make the camera stick to the orientation and position of an entity just for fun.
// * Trigger-only collision bodies
// & Clip lines that are drawn behind the screen instead of not rendering them at all.
// * Test with object on mouse.
// * Make it so that I can visually step through a frame of collision, collision by collision.
// * generate world as you drive
// * car engine that is settable by mouse click and drag
// * ai cars

//STUDY TODO (bjorn): Excerpt from 
// [https://www.toptal.com/game/video-game-physics-part-iii-constrained-rigid-body-simulation]
//
// The general sequence of a simulation step using impulse-based
// dynamics is somewhat different from that of force-based engines:
//
//1 Compute all external forces.
//2 Apply the forces and determine the resulting velocities, using the
//  techniques from Part I.
//3 Calculate the constraint velocities based on the behavior functions.
//4 Apply the constraint velocities and simulate the resulting motion.

struct hero_bitmaps
{
	game_bitmap Head;
	game_bitmap Cape;
	game_bitmap Torso;
};

struct game_state
{
  //TODO STUDY(bjorn): transient_state
	memory_arena TransientArena;
	memory_arena FrameBoundedTransientArena;

  game_bitmap GenTile;
  game_bitmap GenFontMap;

  font* Font;
  //END STUDY

	memory_arena WorldArena;
	world_map* WorldMap;

	//TODO(bjorn): Should we allow split-screen?
	u64 MainCameraStorageIndex;
	rectangle3 CameraUpdateBounds;

	stored_entities Entities;

	game_bitmap Backdrop;
	game_bitmap RockWall;
	game_bitmap Dirt;
	game_bitmap Shadow;
	game_bitmap Sword;

	hero_bitmaps HeroBitmaps[4];

  game_bitmap Grass[2];
  game_bitmap Ground[4];
  game_bitmap Rock[4];
  game_bitmap Tuft[3];
  game_bitmap Tree[3];

	f32 SimulationSpeedModifier;
#if HANDMADE_INTERNAL
	b32 DEBUG_VisualiseCollisionBox;
	world_map_position DEBUG_TestLocations[10];
	world_map_position DEBUG_MainCameraOffsetDuringPause;

	temporary_memory DEBUG_RenderGroupTempMem;
	render_group* DEBUG_OldRenderGroup;

	u32 DEBUG_PauseStep;
	u32 DEBUG_SkipXSteps;
#endif

	f32 NoteTone;
	f32 NoteDuration;
	f32 NoteSecondsPassed;
};

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

  SetCamera(RenderGroup, M44Identity(), positive_infinity32, 0.5f);
  RenderGroupToOutput(RenderGroup, Buffer, (f32)Buffer->Height);

  for(u32 Y = 1;
      Y < (Buffer->Height-1);
      Y++)
  {
    for(u32 X = 1;
        X < (Buffer->Width);
        X++)
    {
      u32* Mid = Buffer->Memory + Y * Buffer->Pitch + X;
      u32 Lef = Buffer->Memory[Y * Buffer->Pitch + (X-1)];
      u32 Rig = Buffer->Memory[Y * Buffer->Pitch + (X+1)];
      u32 Bot = Buffer->Memory[(Y-1) * Buffer->Pitch + X];
      u32 Top = Buffer->Memory[(Y+1) * Buffer->Pitch + X];

      b32 DoFlip = (Lef == Rig && Top == Bot && Lef == Top && Top != *Mid);
      if(DoFlip)
      {
        *Mid = (*Mid == 0) ? 0xFFFFFFFF : 0;
      }
    }
  }

#if 0
  for(u32 Y = 1;
      Y < (Buffer->Height-1);
      Y++)
  {
    for(u32 X = 1;
        X < (Buffer->Width);
        X++)
    {
      u32* Mid = Buffer->Memory + Y * Buffer->Pitch + X;
      *Mid = (*Mid == 0) ? ((Character%2)==0 ? 0x0200FF00 : 0x020000FF) : *Mid;
    }
  }
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
};
WORK_QUEUE_CALLBACK(DoFontGenWork)
{
  Assert(Memory);
  Assert(Size > 0);

  font_gen_work* FontGenWork = (font_gen_work*)Data;

  game_bitmap* Buffer = FontGenWork->Buffer;
  s8 Character        = FontGenWork->Character;
  font* Font          = FontGenWork->Font;

  memory_arena Arena = InitializeArena(Size, Memory);

  game_bitmap Glyph = EmptyBitmap(&Arena, 512, 1024);
  Glyph.Alignment = {256, 512};

  render_group* RenderGroup = AllocateRenderGroup(&Arena, Megabytes(1));

  GenerateGlyph(RenderGroup, Font, &Glyph, Character);
  ClearRenderGroup(RenderGroup);

  rectangle2 TargetLoc = CharacterToFontMapLocation(Character);
  m44 GlyphPos = ConstructTransform(GetRectCenter(TargetLoc) * Buffer->Width, 
                                    GetRectDim(TargetLoc) * Buffer->Width);

  PushQuad(RenderGroup, GlyphPos, &Glyph);

  SetCamera(RenderGroup, ConstructTransform(v2{-(Buffer->Width*0.5f), -(Buffer->Height*0.5f)}), 
            positive_infinity32, 0.5f);
  RenderGroupToOutput(RenderGroup, Buffer, (f32)Buffer->Height);
}

  internal_function void
GenerateFontMap(work_queue* WorkQueue, memory_arena* TransientArena, font* Font, 
                game_bitmap* Buffer)
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

    PushWork(WorkQueue, DoFontGenWork, FontGenWork + Index);
    Index += 1;
  }
}

  internal_function void
GenerateTile(game_state* GameState, work_queue* RenderQueue, game_bitmap* Buffer)
{
  temporary_memory TempMem = BeginTemporaryMemory(&GameState->TransientArena);
  render_group* RenderGroup = AllocateRenderGroup(&GameState->TransientArena, Megabytes(4));

  random_series Series = Seed(0);

#if 1
  u32 Count = 200;
#else
  u32 Count = 0;
#endif
  for(u32 Index = 0; Index < Count; Index++)
  {
    v2 Offset = Hadamard(RandomBilateralV2(&Series) * 0.5f, Buffer->Dim);

    game_bitmap* Bitmap = 0;
    switch(RandomChoice(&Series, 2))
    {
      case 0: {
                Bitmap = GameState->Grass  + RandomChoice(&Series, 2);
              } break;
      case 1: {
                Bitmap = GameState->Ground + RandomChoice(&Series, 4);
              } break;
    }

    PushQuad(RenderGroup, ConstructTransform(Offset, Bitmap->Dim), Bitmap);
  }

  for(u32 Index = 0; Index < Count/2; Index++)
  {
    v2 Offset = Hadamard(RandomBilateralV2(&Series) * 0.5f, Buffer->Dim);

    game_bitmap* Bitmap = 0;
    if(RandomUnilateral(&Series) > 0.8f)
    {
      Bitmap = GameState->Rock + RandomChoice(&Series, 4);
    }
    else
    {
      Bitmap = GameState->Tuft + RandomChoice(&Series, 3);
    }

    PushQuad(RenderGroup, ConstructTransform(Offset, Bitmap->Dim), Bitmap);
  }

  SetCamera(RenderGroup, M44Identity(), positive_infinity32, 0.5f);
  TiledRenderGroupToOutput(RenderQueue, RenderGroup, Buffer, (f32)Buffer->Height);

	EndTemporaryMemory(TempMem);
	CheckMemoryArena(&GameState->TransientArena);

  ClearEdgeXPix(Buffer, 1);
}

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState, game_input* Input)
{
	InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
									(u8*)Memory->PermanentStorage + sizeof(game_state));
	InitializeArena(&GameState->TransientArena, Memory->TransientStorageSize, 
                  (u8*)Memory->TransientStorage);
  SubArena(&GameState->FrameBoundedTransientArena, &GameState->TransientArena, 
           Memory->TransientStorageSize/4);

	GameState->SimulationSpeedModifier = 1;

	memory_arena* TransientArena = &GameState->TransientArena;
  GameState->Grass[0] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/grass00.bmp");
  GameState->Grass[1] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/grass01.bmp");

  GameState->Ground[0] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground00.bmp");
  GameState->Ground[1] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground01.bmp");
  GameState->Ground[2] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground02.bmp");
  GameState->Ground[3] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground03.bmp");

  GameState->Rock[0] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock00.bmp");
  GameState->Rock[1] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock01.bmp");
  GameState->Rock[2] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock02.bmp");
  GameState->Rock[3] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock03.bmp");

  GameState->Tuft[0] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tuft00.bmp");
  GameState->Tuft[1] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tuft01.bmp");
  GameState->Tuft[2] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tuft02.bmp");

  GameState->Tree[0] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tree00.bmp");
  GameState->Tree[1] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tree01.bmp");
  GameState->Tree[2] = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tree02.bmp");

	GameState->Backdrop = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
																		 "data/test/test_background.bmp");

	GameState->RockWall = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
																 "data/test2/rock00.bmp");
	GameState->RockWall.Alignment = {35, 41};

	GameState->Dirt = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
																 "data/test2/ground00.bmp");
	GameState->Dirt.Alignment = {133, 56};

	GameState->Shadow = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
																	 "data/test/test_hero_shadow.bmp");
	GameState->Shadow.Alignment = {72, 182};

	GameState->Sword = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
																	"data/test2/ground01.bmp");
	GameState->Sword.Alignment = {256/2, 116/2};

	hero_bitmaps Hero = {};
	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_front_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_front_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
														"data/test/test_hero_front_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[0] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
														"data/test/test_hero_left_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[1] = Hero;

  Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena,
                           "data/test/test_hero_back_head.bmp");
  Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena, 
                           "data/test/test_hero_back_cape.bmp");
  Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                            Memory->DEBUGPlatformReadEntireFile, 
                            TransientArena, 
                            "data/test/test_hero_back_torso.bmp");
  Hero.Head.Alignment = {72, 182};
  Hero.Cape.Alignment = {72, 182};
  Hero.Torso.Alignment = {72, 182};
  GameState->HeroBitmaps[2] = Hero;

  Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena,
                           "data/test/test_hero_right_head.bmp");
  Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena,
                           "data/test/test_hero_right_cape.bmp");
  Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                            Memory->DEBUGPlatformReadEntireFile, 
                            TransientArena, 
                            "data/test/test_hero_right_torso.bmp");
  Hero.Head.Alignment = {72, 182};
  Hero.Cape.Alignment = {72, 182};
  Hero.Torso.Alignment = {72, 182};
  GameState->HeroBitmaps[3] = Hero;

  debug_read_file_result FontFile = Memory->DEBUGPlatformReadEntireFile("data/MSMINCHO.font");
  GameState->Font = (font *)FontFile.Content;

  GameState->WorldMap = PushStruct(&GameState->WorldArena, world_map);
  world_map *WorldMap = GameState->WorldMap;
  WorldMap->ChunkSafetyMargin = 256;
  WorldMap->TileSideInMeters = 1.4f;
  WorldMap->ChunkSideInMeters = WorldMap->TileSideInMeters * TILES_PER_CHUNK;

  temporary_memory TempMem = BeginTemporaryMemory(&GameState->FrameBoundedTransientArena);

	u32 RoomWidthInTiles = 17;
	u32 RoomHeightInTiles = 9;

	v3s RoomOrigin = (v3s)RoundV2ToV2S((v2)v2u{RoomWidthInTiles, RoomHeightInTiles} / 2.0f);
	world_map_position RoomOriginWorldPos = GetChunkPosFromAbsTile(WorldMap, RoomOrigin);
#if HANDMADE_INTERNAL
	GameState->DEBUG_VisualiseCollisionBox = false;
	for(s32 Index = 0;
			Index < 10;
			Index++)
	{
		world_map_position* WorldP = GameState->DEBUG_TestLocations + Index;
		WorldP->Offset_ = RoomOriginWorldPos.Offset_;
		WorldP->ChunkP = RoomOriginWorldPos.ChunkP + v3s{Index * 4, 0, 0};
	}
#endif
	GameState->CameraUpdateBounds = RectCenterDim(v3{0, 0, 0}, 
																								v3{2, 2, 2} * WorldMap->ChunkSideInMeters);

	{ //NOTE(bjorn): Test location 0 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 &GameState->FrameBoundedTransientArena, WorldMap,
																		 RoomOriginWorldPos, GameState->CameraUpdateBounds, 0);

		entity* MainCamera = AddCamera(SimRegion, v3{0, 0, 0});
		MainCamera->Keyboard = GetKeyboard(Input, 1);
		MainCamera->Mouse = GetMouse(Input, 1);

    random_series Series = Seed(0);

    //TODO IMPORTANT Finish this!!
    const u32 Rows = 3;
    const u32 Columns = 6;
    char CharSet[] = {'s','d','f','j','k','l'};
    s32 SubSetCount = 0;
    char SubSet[ArrayCount(CharSet)] = {};
    u32 PickSetCount = 0;
    char PickSet[ArrayCount(CharSet)] = {};
    char Field[Columns * Rows] = {};

    for(s32 Y = 0;
        Y < Rows;
        Y++)
    {
      for(s32 X = 0;
          X < Columns;
          X++)
      {
        s32 Li = (Y  ) * Columns + (X-1);
        s32 Ri = (Y  ) * Columns + (X+1);
        s32 Ti = (Y+1) * Columns + (X  );
        s32 Bi = (Y-1) * Columns + (X  );

        SubSetCount = 0;
        if(X>0         && Field[Li]) 
        { SubSet[SubSetCount++] = Field[Li]; }
        if(X<Columns-1 && Field[Ri]) 
        { SubSet[SubSetCount++] = Field[Ri]; }
        if(Y>0         && Field[Bi]) 
        { SubSet[SubSetCount++] = Field[Bi]; }
        if(Y<Rows-1    && Field[Ti]) 
        { SubSet[SubSetCount++] = Field[Ti]; }

        PickSetCount = 0;
        for(s32 CharSetIndex = 0;
            CharSetIndex < ArrayCount(CharSet);
            CharSetIndex++)
        {
          b32 InSubSet = false;
          for(s32 SubSetIndex = 0;
              SubSetIndex < SubSetCount;
              SubSetIndex++)
          {
            if(SubSet[SubSetIndex] == CharSet[CharSetIndex])
            {
              InSubSet = true;
            }
          }

          if(!InSubSet)
          {
            PickSet[PickSetCount++] = CharSet[CharSetIndex];
          }
        }

        if(X>0         && Field[Li] == 0) 
        { 
          u32 DelIndex = RandomChoice(&Series, PickSetCount--);

          Field[Li] = PickSet[DelIndex];

          for(u32 PickSetIndex = 0;
              PickSetIndex < PickSetCount;
              PickSetIndex++)
          {
            if(PickSetIndex >= DelIndex)
            {
              PickSet[PickSetIndex] = PickSet[PickSetIndex+1];
            }
          }
        }
        if(X<Columns-1 && Field[Ri] == 0)
        { 
          u32 DelIndex = RandomChoice(&Series, PickSetCount--);

          Field[Ri] = PickSet[DelIndex];

          for(u32 PickSetIndex = 0;
              PickSetIndex < PickSetCount;
              PickSetIndex++)
          {
            if(PickSetIndex >= DelIndex)
            {
              PickSet[PickSetIndex] = PickSet[PickSetIndex+1];
            }
          }
        }
        if(Y>0         && Field[Bi] == 0)
        { 
          u32 DelIndex = RandomChoice(&Series, PickSetCount--);

          Field[Bi] = PickSet[DelIndex];

          for(u32 PickSetIndex = 0;
              PickSetIndex < PickSetCount;
              PickSetIndex++)
          {
            if(PickSetIndex >= DelIndex)
            {
              PickSet[PickSetIndex] = PickSet[PickSetIndex+1];
            }
          }
        }
        if(Y<Rows-1    && Field[Ti] == 0)
        { 
          u32 DelIndex = RandomChoice(&Series, PickSetCount--);

          Field[Ti] = PickSet[DelIndex];

          for(u32 PickSetIndex = 0;
              PickSetIndex < PickSetCount;
              PickSetIndex++)
          {
            if(PickSetIndex >= DelIndex)
            {
              PickSet[PickSetIndex] = PickSet[PickSetIndex+1];
            }
          }
        }
      }
    }

#if 0//HANDMADE_INTERNAL
    for(s32 Y = 0;
        Y < Rows;
        Y++)
    {
      for(s32 X = 0;
          X < Columns;
          X++)
      {
        s32 Li = (Y  ) * Columns + (X-1);
        s32 Ri = (Y  ) * Columns + (X+1);
        s32 Ti = (Y+1) * Columns + (X  );
        s32 Bi = (Y-1) * Columns + (X  );

        s32 VerifySetCount = 0;
        char VerifySet[ArrayCount(CharSet)] = {};
        if(X>0        ) { VerifySet[VerifySetCount++] = Field[Li]; }
        if(X<Columns-1) { VerifySet[VerifySetCount++] = Field[Ri]; }
        if(Y>0        ) { VerifySet[VerifySetCount++] = Field[Bi]; }
        if(Y<Rows-1   ) { VerifySet[VerifySetCount++] = Field[Ti]; }

        for(s32 VerifySetIndex = 0;
            VerifySetIndex < VerifySetCount-1;
            VerifySetIndex++)
        {
          Assert(VerifySet[VerifySetIndex] && 
                 VerifySet[VerifySetIndex] != VerifySet[VerifySetIndex+1]);
        }
      }
    }
#endif

    entity* LastGlyph = 0;
    for(s32 CharIndex = 0;
        CharIndex < ArrayCount(Field);
        CharIndex++)
    {
      s32 X = CharIndex % Columns;
      s32 Y = CharIndex / Columns;
      LastGlyph = AddGlyph(SimRegion, v3{-7.5f + 0.5f*X, -7.5f + 1.0f*Y, 0.0f}, Field[CharIndex]);
    }

    MainCamera->FreeMover = AddInvisibleControllable(SimRegion);
    MainCamera->FreeMover->Keyboard = GetKeyboard(Input, 1);

    MainCamera->CurrentGlyph = LastGlyph;
    AddOneWaySpringAttachment(MainCamera, LastGlyph, 10.0f, 0.0f);

    EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
  }

  EndTemporaryMemory(TempMem);

  //TODO Is there a way to do this assignment of the camera storage index?
  for(u32 StorageIndex = 1;
      StorageIndex < GameState->Entities.EntityCount;
      StorageIndex++)
  {
    if(GameState->Entities.Entities[StorageIndex].Sim.IsCamera)
    {
      GameState->MainCameraStorageIndex = StorageIndex;
    }
  }

#if 0
  GameState->GenTile = EmptyBitmap(TransientArena, 3, 3);
  GameState->GenTile.Alignment = {1, 1};
#else
  GameState->GenTile = EmptyBitmap(TransientArena, 512, 512);
  GameState->GenTile.Alignment = {256, 256};
#endif
  GenerateTile(GameState, &Memory->HighPriorityQueue, &GameState->GenTile);

  GameState->GenFontMap = EmptyBitmap(TransientArena, 1024, 1024);
  GameState->GenFontMap.Alignment = {512, 512};
  GenerateFontMap(&Memory->LowPriorityQueue, &GameState->TransientArena, GameState->Font, 
                  &GameState->GenFontMap);
}

#include "game_loop_logic.h"

#if HANDMADE_INTERNAL
game_memory* DebugGlobalMemory; 
#endif
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
#if HANDMADE_INTERNAL
  DebugGlobalMemory = Memory;
#endif
  BEGIN_TIMED_BLOCK(GameUpdateAndRender);

#if HANDMADE_SLOW
	{
		s64 Amount = (&GetController(Input, 1)->Struct_Terminator - 
									&GetController(Input, 1)->Buttons[0]);
		s64 Limit = ArrayCount(GetController(Input, 1)->Buttons);
		Assert(Amount == Limit);
	}

	{
		entity TestEntity = {};
		s64 Amount = (&(TestEntity.struct_terminator1_) - &(TestEntity.EntityReferences[0]));
		s64 Limit = ArrayCount(TestEntity.EntityReferences);
		Assert(Amount <= Limit);
	}

	{
		entity TestEntity = {};
		s64 Amount = (&(TestEntity.struct_terminator0_) - &(TestEntity.Attachments[0]));
		s64 Limit = ArrayCount(TestEntity.Attachments);
		Assert(Amount <= Limit);
	}
#endif

	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state *GameState = (game_state *)Memory->PermanentStorage;

#if HANDMADE_INTERNAL
	u32 DEBUG_SwitchToLocation = 0;
#endif
	if(!Memory->IsInitialized)
	{
    PushWork = Memory->PushWork;
    CompleteWork = Memory->CompleteWork;

		InitializeGame(Memory, GameState, Input);

		Memory->IsInitialized = true;
	}

  if(Input->ExecutableReloaded)
  {
    PushWork = Memory->PushWork;
    CompleteWork = Memory->CompleteWork;

    ZeroMemory(GameState->GenTile.Memory, (GameState->GenTile.Height*
                                           GameState->GenTile.Pitch*
                                           GAME_BITMAP_BYTES_PER_PIXEL));
    GenerateTile(GameState, &Memory->LowPriorityQueue, &GameState->GenTile);

    ZeroMemory(GameState->GenFontMap.Memory, (GameState->GenFontMap.Height*
                                            GameState->GenFontMap.Pitch*
                                            GAME_BITMAP_BYTES_PER_PIXEL));
    GenerateFontMap(&Memory->LowPriorityQueue, &GameState->TransientArena,
                    GameState->Font, &GameState->GenFontMap);
  }

  memory_arena* WorldArena = &GameState->WorldArena;
  world_map* WorldMap = GameState->WorldMap;
  stored_entities* Entities = &GameState->Entities;
  memory_arena* FrameBoundedTransientArena = &GameState->FrameBoundedTransientArena;
  memory_arena* TransientArena = &GameState->TransientArena;

  temporary_memory TempMem = BeginTemporaryMemory(FrameBoundedTransientArena);

  //
  //NOTE(bjorn): General input logic unrelated to individual entities.
  //
  for(s32 ControllerIndex = 1;
      ControllerIndex <= ArrayCount(Input->Controllers);
      ControllerIndex++)
  {
    game_controller* Controller = GetController(Input, ControllerIndex);
    if(Controller->IsConnected)
    {
      {
        //SimReq->ddP = Controller->LeftStick.End;

        if(Clicked(Controller, Start))
        {
          //TODO(bjorn) Implement RemoveEntity();
          //GameState->EntityResidencies[ControlledEntityIndex] = EntityResidence_Nonexistent;
          //GameState->PlayerIndexForController[ControllerIndex] = 0;
        }
			}
			//else
			{
				if(Clicked(Controller, Start))
				{
					//v3 OffsetInTiles = GetChunkPosFromAbsTile(WorldMap, v3s{-2, 1, 0}).Offset_;
					//world_map_position InitP = OffsetWorldPos(WorldMap, GameState->CameraP, OffsetInTiles);

					//SimReq->PlayerStorageIndex = AddPlayer(SimRegion, InitP);
				}
			}
		}
	}

#if HANDMADE_INTERNAL
	b32 DEBUG_UpdateLoopAdvance = false;
#endif

	for(s32 KeyboardIndex = 1;
			KeyboardIndex <= ArrayCount(Input->Keyboards);
			KeyboardIndex++)
	{
		game_keyboard* Keyboard = GetKeyboard(Input, KeyboardIndex);
		if(Keyboard->IsConnected)
		{
			if(Clicked(Keyboard, Q))
			{
				GameState->NoteTone = 500.0f;
				GameState->NoteDuration = 0.05f;
				GameState->NoteSecondsPassed = 0.0f;

				//TODO STUDY(bjorn): Just setting the flag is not working anymore.
				//Memory->IsInitialized = false;
			}

#if 0
			if(Clicked(Keyboard, P))
			{
				if(GameState->SimulationSpeedModifier)
				{
					GameState->SimulationSpeedModifier = 0;

#if HANDMADE_INTERNAL
					//NOTE(bjorn): I am playing with freed memory here!!!
					render_group* OldRenderGroup = GameState->DEBUG_OldRenderGroup;

					GameState->DEBUG_RenderGroupTempMem = BeginTemporaryMemory(TransientArena);
					GameState->DEBUG_OldRenderGroup = AllocateRenderGroup(TransientArena, Megabytes(4));

					*GameState->DEBUG_OldRenderGroup = *OldRenderGroup;
					GameState->DEBUG_PauseStep = 1;
#endif
				}
				else
				{
					GameState->SimulationSpeedModifier = 1;

#if HANDMADE_INTERNAL
					EndTemporaryMemory(GameState->DEBUG_RenderGroupTempMem);
					CheckMemoryArena(TransientArena);
#endif
				}
			}
#endif
#if HANDMADE_INTERNAL
			if(Clicked(Keyboard, M))
			{
				GameState->DEBUG_VisualiseCollisionBox = !GameState->DEBUG_VisualiseCollisionBox;
			}

			if((Clicked(Keyboard, O) || (!Held(Keyboard, O) && GameState->DEBUG_SkipXSteps)) && 
				 GameState->SimulationSpeedModifier == 0.0f)
			{
				DEBUG_UpdateLoopAdvance = true;

				EndTemporaryMemory(GameState->DEBUG_RenderGroupTempMem);
				CheckMemoryArena(TransientArena);
				GameState->DEBUG_RenderGroupTempMem = BeginTemporaryMemory(TransientArena);
				GameState->DEBUG_OldRenderGroup = AllocateRenderGroup(TransientArena, Megabytes(4));
				GameState->DEBUG_PauseStep = Modulate(GameState->DEBUG_PauseStep+1, 1, 9);

				GameState->DEBUG_SkipXSteps = 
					GameState->DEBUG_SkipXSteps ? GameState->DEBUG_SkipXSteps-1 : 0;
				if(Held(Keyboard, Shift))
				{
					GameState->DEBUG_SkipXSteps = 8;
				}
			}

#if 0
			for(s32 NumKeyIndex = 0;
					NumKeyIndex < ArrayCount(Keyboard->Numbers);
					NumKeyIndex++)
			{
				b32 DEBUG_ManualSwitch = ((NumKeyIndex == 0) && 
																	DEBUG_SwitchToLocation);
				if(DEBUG_ManualSwitch ||
					 (Held(Keyboard, Shift) &&
						Clicked(Keyboard, Numbers[NumKeyIndex])))
				{
					if(DEBUG_ManualSwitch)
					{
						NumKeyIndex = DEBUG_SwitchToLocation;
					}
					sim_region* SimRegion = BeginSim(Input, Entities, FrameBoundedTransientArena, WorldMap, 
																					 GameState->DEBUG_TestLocations[NumKeyIndex], 
																					 GameState->CameraUpdateBounds, SecondsToUpdate);

					entity* MainCamera = AddEntityToSimRegionManually(Input, Entities, SimRegion, 
																														GameState->MainCameraStorageIndex);
					Assert(MainCamera);
					Assert(MainCamera->IsSpacial);
					Assert(MainCamera->IsCamera);

					if(!MainCamera->Updates)
					{
						if(MainCamera->Prey != MainCamera->FreeMover)
						{
							DetachToMoveFreely(MainCamera);
						}
						//TODO(bjorn): Add a MoveFarAwayEntityToUpdateRegion(v3 P) that bounds check properly.
						MainCamera->P = {0,0,0};
						//NOTE(bjorn): Make sure our changes to the entities persists.
						MainCamera->Updates = true;

						entity* Player = FindPlayerInSimUpdateRegion(SimRegion, MainCamera);

						if(!Player)
						{
							Assert(MainCamera->FreeMover->IsSpacial);
							MainCamera->FreeMover->P = {0,0,0};
							MainCamera->FreeMover->Updates = true;
						}
						else
						{
							MainCamera->Player->MoveSpec.MoveOnInput = true;
							MainCamera->Player = Player;
							AttachToPlayer(MainCamera);
						}
					}

					EndSim(Input, Entities, WorldArena, SimRegion);
				}
			}
#endif
#endif
		}
	}

	render_group* RenderGroup = AllocateRenderGroup(FrameBoundedTransientArena, Megabytes(4));

	//
	// NOTE(bjorn): Moving and Rendering
	//

	// NOTE(bjorn): Create sim region by camera
	sim_region* SimRegion = 0;
	{
		stored_entity* StoredMainCamera = 
			GetStoredEntityByIndex(Entities, GameState->MainCameraStorageIndex);
		SimRegion = BeginSim(Input, Entities, FrameBoundedTransientArena, WorldMap, 
												 StoredMainCamera->Sim.WorldP, GameState->CameraUpdateBounds, 
												 SecondsToUpdate);
	}
	Assert(SimRegion);

	{
		PushWireCube(RenderGroup, SimRegion->UpdateBounds, v4{0,1,1,1});
		PushWireCube(RenderGroup, SimRegion->OuterBounds, v4{1,1,0,1});
	}

	//TODO(bjorn): Implement step 2 in J.Blows framerate independence video.
	// https://www.youtube.com/watch?v=fdAOPHgW7qM
	//TODO(bjorn): Add some asserts and some limits to velocities related to the
	//delta time so that tunneling becomes virtually impossible.
	u32 Steps = 8; 

	entity* MainCamera = AddEntityToSimRegionManually(Input, Entities, SimRegion, 
																										GameState->MainCameraStorageIndex);
	Assert(MainCamera);

	u32 FirstStep = 1;
	u32 LastStep = Steps;
	for(u32 Step = FirstStep;
			Step <= LastStep;
			Step++)
	{
		entity* Entity = SimRegion->Entities;
		for(u32 EntityIndex = 0;
				EntityIndex < SimRegion->EntityCount;
				EntityIndex++, Entity++)
		{
			if(Entity->Updates) { Entity->EntityPairUpdateGenerationIndex = Step; }
			//TODO Do non-spacial entities ever do logic/Render? Do they affect other entities then? 
			if(!Entity->IsSpacial) { continue; }

			f32 dT = SecondsToUpdate / (f32)Steps;
			b32 CameraOrAssociates = Entity->IsCamera || (MainCamera->FreeMover == Entity);
			if(!CameraOrAssociates)
			{
				dT *= GameState->SimulationSpeedModifier;
			}

			if(Entity->Updates && GameState->SimulationSpeedModifier != 0)
			{
				if(Entity->HasBody) 
				{ 
					entity* OtherEntity = SimRegion->Entities;
					for(u32 OtherEntityIndex = 0;
							OtherEntityIndex < SimRegion->EntityCount;
							OtherEntityIndex++, OtherEntity++)
					{
						if(Entity == OtherEntity ||
						   OtherEntity->EntityPairUpdateGenerationIndex == Step ||
               !OtherEntity->IsSpacial ||
               //TODO(bjorn): Should spacial non bodies also be interactable?
               !OtherEntity->HasBody ||
               //TODO(bjorn): Actually test if the filter did speed up anything.
               !CollisionFilterCheck(Entity, OtherEntity)) 
            { continue; }

						contact_result Contacts = {};
            Contacts = GenerateContacts(Entity, OtherEntity);
						b32 Inside = Contacts.Count > 0; 

						if(Inside &&
							 Entity->Collides && 
							 OtherEntity->Collides)
						{
              ResolveContacts(Contacts);

							ComputeSecondOrderEntityState(Entity, SimRegion->OuterBounds);
							ComputeSecondOrderEntityState(OtherEntity, SimRegion->OuterBounds);
						} 

						trigger_result TriggerResult = 
							UpdateAndGetCurrentTriggerState(Entity, OtherEntity, Inside, dT);

            EntityPairUpdate(Entity, OtherEntity, TriggerResult, dT);
					}
				}

        PrePhysicsStepEntityUpdate(Entity, MainCamera, dT);
        if(Step == FirstStep)
        {
          PrePhysicsFisrtStepOnlyEntityUpdate(Entity, MainCamera, SecondsToUpdate);
        }

        entity OldEntity = *Entity;
				//TODO(bjorn): Make ApplyAttachmentForcesAndImpulses somehow depend on
				//time in such a way that changing the simulation speed does not affect
				//the simulation.
				ApplyAttachmentForcesAndImpulses(Entity);

				MoveEntity(Entity, dT);

#if HANDMADE_SLOW
        if(Entity->HasBody)
				{
					Assert(LengthSquared(Entity->dP) <= Square(SimRegion->MaxEntityVelocity));

					f32 S1 = Square(Entity->Body.Primitives[0].S.X * 0.5f * Entity->Body.S);
					f32 S2 = Square(SimRegion->MaxEntityRadius);
					Assert(S1 <= S2);
				}
#endif

        PostPhysicsStepEntityUpdate(Entity, &OldEntity, dT);
        ComputeSecondOrderEntityState(Entity, SimRegion->OuterBounds);
      }

			if(Step == LastStep)
			{
        RenderEntity(RenderGroup, GameState, Entity, MainCamera);
			}
		}
	}

  //TODO IMPORTANT(bjorn): Create a functionality for having entities
  //responding after all other entities have had a chance to be iterated. Using
  //the EntityPairUpdateGenerationIndex maybe.
  {
    MainCamera->GlyphSwitchCooldown = Max(0, MainCamera->GlyphSwitchCooldown - SecondsToUpdate);
    MainCamera->BestDistanceToGlyphSquared = positive_infinity32;
    if(MainCamera->PotentialGlyph &&
       MainCamera->GlyphSwitchCooldown == 0)
    {
      MoveAllOneWayAttachments(MainCamera, MainCamera->CurrentGlyph, MainCamera->PotentialGlyph);
      MainCamera->CurrentGlyph = MainCamera->PotentialGlyph;
      MainCamera->PotentialGlyph = 0;
      MainCamera->GlyphSwitchCooldown = 0.1f;
    }
  }


  EndSim(Input, Entities, WorldArena, SimRegion);

  ClearScreen(RenderGroup, {0.5f, 0.5f, 0.5f});

  //
  // NOTE(bjorn): Rendering
  //
#if 1
  TiledRenderGroupToOutput(&Memory->HighPriorityQueue, RenderGroup, Buffer, 
                           MainCamera->CamScreenHeight);
#else
  DrawRectangle(Buffer, RectMinMax(v2s{0, 0}, Buffer->Dim), 
                v3{1,1,1}*0.5f, RectMinMax(v2s{0, 0}, Buffer->Dim), true);
  DrawRectangle(Buffer, RectMinMax(v2s{0, 0}, Buffer->Dim), 
                v3{1,1,1}*0.5f, RectMinMax(v2s{0, 0}, Buffer->Dim), false);

  game_mouse* Mouse = GetMouse(Input, 1);
  DrawLine(Buffer, Buffer->Dim*0.5f, Hadamard(Buffer->Dim, Mouse->P), 
           v3{1,1,1}*0.5f, RectMinMax(v2s{0, 0}, Buffer->Dim), true);
  DrawLine(Buffer, Buffer->Dim*0.5f, Hadamard(Buffer->Dim, Mouse->P), 
                v3{1,1,1}*0.5f, RectMinMax(v2s{0, 0}, Buffer->Dim), false);
#endif

	EndTemporaryMemory(TempMem);
	CheckMemoryArena(FrameBoundedTransientArena);

  END_TIMED_BLOCK(GameUpdateAndRender);
}

	internal_function void
OutputSound(game_sound_output_buffer *SoundBuffer, game_state* GameState)
{
	s32 ToneVolume = 11000;
	s16 *SampleOut = SoundBuffer->Samples;
	for(s32 SampleIndex = 0;
			SampleIndex < SoundBuffer->SampleCount;
			++SampleIndex)
	{
		f32 Value = 0;
		if(GameState->NoteSecondsPassed < GameState->NoteDuration)
		{
			f32 t = GameState->NoteSecondsPassed * GameState->NoteTone * tau32;
			t -= FloorF32ToS32(t * inv_tau32) * tau32;
			Value = Sin(t);
			f32 TimeToGo = (GameState->NoteDuration - GameState->NoteSecondsPassed);
			if(TimeToGo < 0.01f)
			{
				Value *= TimeToGo / 0.01f;
			}
			GameState->NoteSecondsPassed += (1.0f / SoundBuffer->SamplesPerSecond);
		}

		s16 SampleValue = (s16)(ToneVolume * Value);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
	}
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	OutputSound(SoundBuffer, GameState);
}
