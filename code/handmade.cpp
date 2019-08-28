#include "platform.h"
#include "memory.h"
#include "world_map.h"
#include "random.h"
#include "render_group.h"
#include "resource.h"
#include "sim_region.h"
#include "entity.h"
#include "collision.h"
#include "trigger.h"
#include "glyph_data.h"

// @IMPORTANT @IDEA
// Maybe sort the order of entity execution while creating the sim regions so
// that there is a guaranteed hirarchiy to the collision resolution. Depending
// on how I want to do the collision this migt be a nice tool.

// QUICK TODO
// BUG The first location floor and some random walls are not getting unloaded
//		 because of cascading trigger references.
// BUG Hunting is super shaky.

// TODO
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

internal_function void
DrawUTF16Glyph(game_offscreen_buffer* Buffer, font* Font, u16 UnicodeCodePoint, rectangle2 DrawRect)
{
	v2 Scale = DrawRect.Max - DrawRect.Min;
	unicode_to_glyph_data *Entries = (unicode_to_glyph_data *)(Font + 1);
	for(s32 EntryIndex = 0;
			EntryIndex < Font->UnicodeCodePointCount;
			EntryIndex++)
	{
		if(UnicodeCodePoint == Entries[EntryIndex].UnicodeCodePoint)
		{
			if(Entries[EntryIndex].OffsetToGlyphData)
			{
				quadratic_curve *Curves = 
					(quadratic_curve *)((u8 *)Font + Entries[EntryIndex].OffsetToGlyphData);
				for(s32 CurveIndex = 0;
						CurveIndex < Entries[EntryIndex].QuadraticCurveCount;
						CurveIndex++)
				{
					quadratic_curve Curve = Curves[CurveIndex];

					v2 Start = Hadamard(Curve.Srt, Scale) + DrawRect.Min;
					v2 End = Hadamard(Curve.End, Scale) + DrawRect.Min;

					Start.Y = Buffer->Height - Start.Y;

					End.Y = Buffer->Height - End.Y;

					DrawLine(Buffer, Start, End, {1.0f, 1.0f, 1.0f});
				}
			}
		}
	}
}

struct hero_bitmaps
{
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;
};

struct game_state
{
	font* Font;
	memory_arena WorldArena;
	world_map* WorldMap;

	memory_arena FrameBoundedTransientArena;
	memory_arena TransientArena;

	//TODO(bjorn): Should we allow split-screen?
	u64 MainCameraStorageIndex;
	rectangle3 CameraUpdateBounds;

	//stored_entities Entities;
	u16 UTF16Buffer_Kanji[1024*1024];
	s32 UTF16Buffer_KanjiCount;
	u16 UTF16Buffer_Primitives[1024];
	s32 UTF16Buffer_PrimitivesCount;

	loaded_bitmap Backdrop;
	loaded_bitmap Rock;
	loaded_bitmap Dirt;
	loaded_bitmap Shadow;
	loaded_bitmap Sword;

	hero_bitmaps HeroBitmaps[4];

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
InitializeGame(game_memory *Memory, game_state *GameState, game_input* Input)
{
	GameState->SimulationSpeedModifier = 1;

	GameState->Backdrop = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test/test_background.bmp");

	GameState->Rock = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																 "data/test2/rock00.bmp");
	GameState->Rock.Alignment = {35, 41};

	GameState->Dirt = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																 "data/test2/ground00.bmp");
	GameState->Dirt.Alignment = {133, 56};

	GameState->Shadow = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																	 "data/test/test_hero_shadow.bmp");
	GameState->Shadow.Alignment = {72, 182};

	GameState->Sword = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																	"data/test2/ground01.bmp");
	GameState->Sword.Alignment = {256/2, 116/2};

	hero_bitmaps Hero = {};
	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_front_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[0] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_left_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[1] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_back_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[2] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_right_torso.bmp");
	Hero.Head.Alignment = {72, 182};
	Hero.Cape.Alignment = {72, 182};
	Hero.Torso.Alignment = {72, 182};
	GameState->HeroBitmaps[3] = Hero;

	InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
									(u8*)Memory->PermanentStorage + sizeof(game_state));

	GameState->WorldMap = PushStruct(&GameState->WorldArena, world_map);

	world_map *WorldMap = GameState->WorldMap;
	WorldMap->ChunkSafetyMargin = 256;
	WorldMap->TileSideInMeters = 1.4f;
	WorldMap->ChunkSideInMeters = WorldMap->TileSideInMeters * TILES_PER_CHUNK;

	InitializeArena(&GameState->FrameBoundedTransientArena, Memory->TransientStorageSize>>1, 
									(u8*)Memory->TransientStorage);
	InitializeArena(&GameState->TransientArena, Memory->TransientStorageSize>>1, 
									(u8*)Memory->TransientStorage + (Memory->TransientStorageSize>>1));

	GameState->UTF16Buffer_PrimitivesCount = 
		UnicodeStringToUnicodeCodePointArray((u8*)GlobalPrimitives, 
																				 GameState->UTF16Buffer_Primitives, 
																				 ArrayCount(GameState->UTF16Buffer_Primitives)).Count;
	GameState->UTF16Buffer_KanjiCount = 
		UnicodeStringToUnicodeCodePointArray((u8*)GlobalKanji, 
																				 GameState->UTF16Buffer_Kanji, 
																				 ArrayCount(GameState->UTF16Buffer_Kanji)).Count;

#if 1
	debug_read_file_result TTF = Memory->DEBUGPlatformReadEntireFile("data/MSMINCHO.TTF");
	font* Font = TrueTypeFontToFont(&GameState->TransientArena, (true_type_font*)TTF.Content);
	Memory->DEBUGPlatformWriteEntireFile("data/MSMINCHO.font", 
																			 (u32)GameState->TransientArena.Used, Font);
#endif
	debug_read_file_result FontFile = Memory->DEBUGPlatformReadEntireFile("data/MSMINCHO.font");
	GameState->Font = (font *)FontFile.Content;

}

	internal_function void
TransformLineToScreenSpaceAndRender(game_offscreen_buffer* Buffer, render_piece* RenderPiece,
																		f32 CamDist, m44 CamTrans, v3 V0, v3 V1,
																		v2 ScreenCenter, m22 GameSpaceToScreenSpace,
																		v3 RGB)
{
	transform_result Tran0 = TransformPoint(CamDist, CamTrans, V0);
	transform_result Tran1 = TransformPoint(CamDist, CamTrans, V1);

	if(Tran0.Valid && Tran1.Valid)
	{
		v2 PixelV0 = ScreenCenter + (GameSpaceToScreenSpace * Tran0.P) * Tran0.f;
		v2 PixelV1 = ScreenCenter + (GameSpaceToScreenSpace * Tran1.P) * Tran1.f;
		DrawLine(Buffer, PixelV0, PixelV1, RGB);
	}
};

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
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
		InitializeGame(Memory, GameState, Input);
		Memory->IsInitialized = true;
	}

	memory_arena* WorldArena = &GameState->WorldArena;
	world_map* WorldMap = GameState->WorldMap;
	//stored_entities* Entities = &GameState->Entities;
	memory_arena* FrameBoundedTransientArena = &GameState->FrameBoundedTransientArena;
	memory_arena* TransientArena = &GameState->TransientArena;

	temporary_memory TempMem = BeginTemporaryMemory(FrameBoundedTransientArena);

	//
	//NOTE(bjorn): General input logic unrelated to individual entities.
	//

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
		}
	}

	//
	// NOTE(bjorn): Moving and Rendering
	//
	s32 TileSideInPixels = 60;
	f32 PixelsPerMeter = (f32)TileSideInPixels / WorldMap->TileSideInMeters;

	DrawRectangle(Buffer, 
								RectMinMax(v2{0.0f, 0.0f}, v2{(f32)Buffer->Width, (f32)Buffer->Height}), 
								{0.5f, 0.5f, 0.5f});

	//
	// NOTE(bjorn): Rendering
	//
	for(s32 i = 0; i < 20; i++)
	{
		for(s32 j = 0; j < 10; j++)
		{
			s32 Index = (10-j) * 20 + i;
			if(Index < GameState->UTF16Buffer_PrimitivesCount)
			{
				rectangle2 Rect = RectCenterDim(v2{i * 30.0f + 10.0f, j * 30.0f + 10.0f}, v2{25.0f, 25.0f});
				DrawUTF16Glyph(Buffer, GameState->Font, GameState->UTF16Buffer_Primitives[Index], Rect);
			}
		}
	}

	EndTemporaryMemory(TempMem);
	CheckMemoryArena(FrameBoundedTransientArena);
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
