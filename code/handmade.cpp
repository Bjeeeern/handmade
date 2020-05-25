#include "platform.h"
multi_thread_push_work* PushWork;
multi_thread_complete_work* CompleteWork;
render_group_to_output* GPURenderGroupToOutput;
#include "memory.h"
#include "world_map.h"
#include "render_group_builder.h"
#include "random.h"
#include "sim_region.h"
#include "entity.h"
#include "collision.h"
#include "trigger.h"
#include "software_renderer.h"

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

struct transient_state
{
	memory_arena TransientArena;
	memory_arena FrameBoundedTransientArena;

  game_assets Assets;
};

struct game_state
{
	memory_arena WorldArena;
	world_map* WorldMap;

	//TODO(bjorn): Should we allow split-screen?
	u64 MainCameraStorageIndex;
	rectangle3 CameraUpdateBounds;

	stored_entities Entities;

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

#if HANDMADE_INTERNAL
global_variable	game_state* DEBUG_GameState = 0;
global_variable	transient_state* DEBUG_TransientState = 0;
#endif

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
  if(GPURenderGroupToOutput)
  {
    GPURenderGroupToOutput(RenderGroup, Buffer, (f32)Buffer->Height);
  }
  else
  {
    SingleTileRenderGroupToOutput(RenderGroup, Buffer, (f32)Buffer->Height);
  }

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

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState, game_input* Input, 
               memory_arena* FrameBoundedTransientArena)
{
	InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
									(u8*)Memory->PermanentStorage + sizeof(game_state));

	GameState->SimulationSpeedModifier = 1;

  GameState->WorldMap = PushStruct(&GameState->WorldArena, world_map);
  world_map *WorldMap = GameState->WorldMap;
  WorldMap->ChunkSafetyMargin = 256;
  WorldMap->ChunkSideInMeters = 1.6f * 16;

  temporary_memory TempMem = BeginTemporaryMemory(FrameBoundedTransientArena);

	world_map_position RoomOriginWorldPos = {};
  Assert(IsValid(RoomOriginWorldPos));
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
	GameState->CameraUpdateBounds = 
    RectCenterDim(v3{0, 0, 0}, v3{2, 2, 2} * WorldMap->ChunkSideInMeters);

	{ //NOTE(bjorn): Test location 0 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 FrameBoundedTransientArena, WorldMap,
																		 RoomOriginWorldPos, GameState->CameraUpdateBounds, 0);
		AddWall(SimRegion, v3{0, 0, 0});

		entity* MainCamera = AddCamera(SimRegion, v3{0, 0, 0});
		MainCamera->Keyboard = GetKeyboard(Input, 1);
		MainCamera->Mouse = GetMouse(Input, 1);

    random_series Series = Seed(0);

    const u32 Row = 9;
    const u32 Col = 18;
    char CharSet[] = {'s','d','f',/*'g','b','h',*/'j','k','l'};
    s32 SubSetCount = 0;
    char SubSet[ArrayCount(CharSet)] = {};
    u32 PickSetCount = 0;
    char PickSet[ArrayCount(CharSet)] = {};
    char Field[Col * Row] = {};

    for(s32 Y = 0;
        Y < Row;
        Y++)
    {
      for(s32 X = 0;
          X < Col;
          X++)
      {
        v2s Offsets[] = {{-2,0}, {-1,1}, {0,2}, {1,1}, {2,0}, {1,-1}, {0,-2}, {-1,-1}};
        SubSetCount = 0;
        for(s32 OffsetIndex = 0;
            OffsetIndex < ArrayCount(Offsets);
            OffsetIndex++)
        {
          v2s Offset = Offsets[OffsetIndex];
          s32 FieldIndex = (Y + Offset.Y) * Col + (X + Offset.X);
          if(FieldIndex >= 0 &&
             FieldIndex < ArrayCount(Field))
          {
            SubSet[SubSetCount++] = Field[FieldIndex];
          }
        }

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

        Field[Y * Col + X] = PickSet[RandomChoice(&Series, PickSetCount)];
      }
    }

#if HANDMADE_INTERNAL
    for(s32 Y = 0;
        Y < Row;
        Y++)
    {
      for(s32 X = 0;
          X < Col;
          X++)
      {
        s32 Li = (Y  ) * Col + (X-1);
        s32 Ri = (Y  ) * Col + (X+1);
        s32 Ti = (Y+1) * Col + (X  );
        s32 Bi = (Y-1) * Col + (X  );

        s32 VerifySetCount = 0;
        char VerifySet[ArrayCount(CharSet)] = {};
        if(X>0    ) { VerifySet[VerifySetCount++] = Field[Li]; }
        if(X<Col-1) { VerifySet[VerifySetCount++] = Field[Ri]; }
        if(Y>0    ) { VerifySet[VerifySetCount++] = Field[Bi]; }
        if(Y<Row-1) { VerifySet[VerifySetCount++] = Field[Ti]; }

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

    entity* StartGlyph = 0;
    for(s32 CharIndex = 0;
        CharIndex < ArrayCount(Field);
        CharIndex++)
    {
      s32 X = CharIndex % Col;
      s32 Y = CharIndex / Col;
      entity* Entity = 
        AddGlyph(SimRegion, v3{-7.5f + 0.5f*X, -7.5f + 1.0f*Y, 0.0f}, Field[CharIndex]);
      if(X == Col/2 && Y == Row/2)
      {
        StartGlyph = Entity;
      }
    }

    MainCamera->FreeMover = AddInvisibleControllable(SimRegion);
    MainCamera->FreeMover->Keyboard = GetKeyboard(Input, 1);

    MainCamera->CurrentGlyph = StartGlyph;
    AddOneWaySpringAttachment(MainCamera, StartGlyph, 10.0f, 0.0f);

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
}

internal_function void
InitializeTransientState(game_memory* Memory, transient_state* TransientState)
{
	memory_arena* TransientArena = &TransientState->TransientArena;
	InitializeArena(TransientArena, 
                  Memory->TransientStorageSize - sizeof(transient_state), 
                  Memory->TransientStorage + sizeof(transient_state));

	memory_arena* FrameBoundedTransientArena = &TransientState->FrameBoundedTransientArena;
  SubArena(FrameBoundedTransientArena, TransientArena, Memory->TransientStorageSize/4);

  game_assets* Assets = &TransientState->Assets;
  SubArena(&Assets->Arena, TransientArena, Memory->TransientStorageSize/4);
  Assets->DEBUGPlatformReadEntireFile = Memory->DEBUGPlatformReadEntireFile;
  Assets->DEBUGPlatformFreeFileMemory = Memory->DEBUGPlatformFreeFileMemory;

  debug_read_file_result FontFile = Memory->DEBUGPlatformReadEntireFile("data/MSMINCHO.font");
  Assets->Font = (font *)FontFile.Content;

  Assets->GenFontMap = *ClearBitmap(TransientArena, 1024, 1024);
  Assets->GenFontMap.Alignment = {512, 512};

  GenerateFontMap(&Memory->LowPriorityQueue, TransientArena, Assets->Font, 
                  &Assets->GenFontMap, Assets);

#if 0
  Assets->Grass[0] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/grass00.bmp");
  Assets->Grass[1] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/grass01.bmp");

  Assets->Ground[0] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground00.bmp");
  Assets->Ground[1] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground01.bmp");
  Assets->Ground[2] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground02.bmp");
  Assets->Ground[3] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/ground03.bmp");

  Assets->Rock[0] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock00.bmp");
  Assets->Rock[1] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock01.bmp");
  Assets->Rock[2] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock02.bmp");
  Assets->Rock[3] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/rock03.bmp");

  Assets->Tuft[0] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tuft00.bmp");
  Assets->Tuft[1] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tuft01.bmp");
  Assets->Tuft[2] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tuft02.bmp");

  Assets->Tree[0] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tree00.bmp");
  Assets->Tree[1] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tree01.bmp");
  Assets->Tree[2] = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     "data/test2/tree02.bmp");
	hero_bitmaps Hero = {};
	Hero.Head = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_front_head.bmp");
	Hero.Cape = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_front_cape.bmp");
	Hero.Torso = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
														"data/test/test_hero_front_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	Assets->HeroBitmaps[0] = Hero;

	Hero.Head = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                                     Memory->DEBUGPlatformReadEntireFile, 
                                     TransientArena,
                                     
														"data/test/test_hero_left_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	Assets->HeroBitmaps[1] = Hero;

  Hero.Head = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena,
                           "data/test/test_hero_back_head.bmp");
  Hero.Cape = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena, 
                           "data/test/test_hero_back_cape.bmp");
  Hero.Torso = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                            Memory->DEBUGPlatformReadEntireFile, 
                            TransientArena, 
                            "data/test/test_hero_back_torso.bmp");
  Hero.Head.Alignment = {72, 182};
  Hero.Cape.Alignment = {72, 182};
  Hero.Torso.Alignment = {72, 182};
  Assets->HeroBitmaps[2] = Hero;

  Hero.Head = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena,
                           "data/test/test_hero_right_head.bmp");
  Hero.Cape = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                           Memory->DEBUGPlatformReadEntireFile, 
                           TransientArena,
                           "data/test/test_hero_right_cape.bmp");
  Hero.Torso = *DEBUGLoadBMP(Memory->DEBUGPlatformFreeFileMemory, 
                            Memory->DEBUGPlatformReadEntireFile, 
                            TransientArena, 
                            "data/test/test_hero_right_torso.bmp");
  Hero.Head.Alignment = {72, 182};
  Hero.Cape.Alignment = {72, 182};
  Hero.Torso.Alignment = {72, 182};
  Assets->HeroBitmaps[3] = Hero;
#endif
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
	Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
	transient_state *TransientState = (transient_state *)Memory->TransientStorage;

#if HANDMADE_INTERNAL
  DEBUG_GameState = GameState;
  DEBUG_TransientState = TransientState;

	u32 DEBUG_SwitchToLocation = 0;
#endif

  if(Input->ExecutableReloaded ||
     !Memory->IsInitialized)
  {
    PushWork = Memory->PushWork;
    CompleteWork = Memory->CompleteWork;
    GPURenderGroupToOutput = Memory->RenderGroupToOutput;

    InitializeTransientState(Memory, TransientState);
  }

	if(!Memory->IsInitialized)
	{
		InitializeGame(Memory, GameState, Input, &TransientState->FrameBoundedTransientArena);

		Memory->IsInitialized = true;
	}

  memory_arena* WorldArena = &GameState->WorldArena;
  world_map* WorldMap = GameState->WorldMap;
  stored_entities* Entities = &GameState->Entities;
  memory_arena* FrameBoundedTransientArena = &TransientState->FrameBoundedTransientArena;
  memory_arena* TransientArena = &TransientState->TransientArena;
  game_assets* Assets = &TransientState->Assets;

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
        RenderEntity(RenderGroup, TransientState, Assets, Entity, MainCamera);
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

//TODO(bjorn): Add a UI render step.
#if 0
  if(Input->EyeTracker.IsConnected)
  {
    //TODO(bjorn): Fix eye-tracker coordinates.
    Input->EyeTracker.P.Y = 1.0f - Input->EyeTracker.P.Y;
    PushSphere(RenderGroup);
    DrawCircle(Buffer, Hadamard(Input->EyeTracker.P, Buffer->Dim), 
               20, {1,0,1,0.5f}, RectMinMax(v2s{0, 0}, Buffer->Dim));
  }
#endif

  ClearScreen(RenderGroup, {0.5f, 0.5f, 0.5f});

  //
  // NOTE(bjorn): Rendering
  //
  if(GPURenderGroupToOutput)
  {
    GPURenderGroupToOutput(RenderGroup, Buffer, MainCamera->CamScreenHeight);
  }
  else
  {
    TiledRenderGroupToOutput(&Memory->HighPriorityQueue, RenderGroup, Buffer, 
                             MainCamera->CamScreenHeight);
  }

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
