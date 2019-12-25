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
	memory_arena WorldArena;
	world_map* WorldMap;

	memory_arena FrameBoundedTransientArena;
	memory_arena TransientArena;

	//TODO(bjorn): Should we allow split-screen?
	u64 MainCameraStorageIndex;
	rectangle3 CameraUpdateBounds;

	stored_entities Entities;

  game_bitmap GeneratedTile;

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
GenerateTile(game_state* GameState, game_bitmap* Buffer)
{
  temporary_memory TempMem = BeginTemporaryMemory(&GameState->TransientArena);
  render_group* RenderGroup = AllocateRenderGroup(&GameState->TransientArena, Megabytes(4));
  SetCamera(RenderGroup, M44Identity(), 1.0f, 1.0f, 1.0f);

  random_series Series = Seed(0);

  v4 Color = {1.0f,1.0f,1.0f,1.0f};

  u32 Count = 50;
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

    PushQuad(RenderGroup, ConstructTransform(Offset, Bitmap->Dim), Bitmap, Color);
  }

  for(u32 Index = 0; Index < Count; Index++)
  {
    v2 Offset = Hadamard(RandomBilateralV2(&Series) * 0.5f, Buffer->Dim);

    game_bitmap* Bitmap = 0;
    if(RandomChoice(&Series, 2) > 0.8f)
    {
      Bitmap = GameState->Rock + RandomChoice(&Series, 4);
    }
    else
    {
      Bitmap = GameState->Tuft + RandomChoice(&Series, 3);
    }

    PushQuad(RenderGroup, ConstructTransform(Offset, Bitmap->Dim), Bitmap, Color);
  }

  RenderGroupToOutput(RenderGroup, Buffer);

	EndTemporaryMemory(TempMem);
	CheckMemoryArena(&GameState->TransientArena);
}

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState, game_input* Input)
{
	InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
									(u8*)Memory->PermanentStorage + sizeof(game_state));
	InitializeArena(&GameState->FrameBoundedTransientArena, Memory->TransientStorageSize>>1, 
									(u8*)Memory->TransientStorage);
	InitializeArena(&GameState->TransientArena, Memory->TransientStorageSize>>1, 
									(u8*)Memory->TransientStorage + (Memory->TransientStorageSize>>1));

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
	GameState->DEBUG_VisualiseCollisionBox = true;
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
		//TODO Is there a less cheesy (and safer!) way to do this assignment of the
		//camera storage index?
		GameState->MainCameraStorageIndex = 1;
		MainCamera->Keyboard = GetKeyboard(Input, 1);
		MainCamera->Mouse = GetMouse(Input, 1);

		entity* Player = AddPlayer(SimRegion, v3{-2, 1, 0} * WorldMap->TileSideInMeters);
		Player->Keyboard = GetKeyboard(Input, 1);

		MainCamera->Prey = Player;
		MainCamera->Player = Player;
		MainCamera->FreeMover = AddInvisibleControllable(SimRegion);
		MainCamera->FreeMover->Keyboard = GetKeyboard(Input, 1);

		entity* Familiar = AddFamiliar(SimRegion, v3{4, 5, 0} * WorldMap->TileSideInMeters);
		Familiar->Prey = Player;

		AddFloor(SimRegion, v3{0, 0, -0.5f} * WorldMap->TileSideInMeters);

		AddMonstar(SimRegion, v3{ 2, 5, 0} * WorldMap->TileSideInMeters);

		entity* A = AddWall(SimRegion, v3{2, 4, 0} * WorldMap->TileSideInMeters, 10.0f);
		entity* B = AddWall(SimRegion, v3{5, 4, 0} * WorldMap->TileSideInMeters,  5.0f);

		A->dP = {1, 0, 0};
		B->dP = {-1, 0, 0};
		//TODO(bjorn): Have an object stuck to the mouse.
		//GameState->DEBUG_EntityBoundToMouse = AIndex;

		entity* E[6];
		for(u32 i=0;
				i < 6;
				i++)
		{
			E[i] = AddWall(SimRegion, v3{2.0f + 2.0f*i, 2.0f, 0.0f} * WorldMap->TileSideInMeters, 
										 10.0f + i);
			E[i]->dP = {2.0f-i, -2.0f+i};
		}

		u32 ScreenX = 0;
		u32 ScreenY = 0;
		u32 ScreenZ = 0;
		for(u32 TileY = 0;
				TileY < RoomHeightInTiles;
				++TileY)
		{
			for(u32 TileX = 0;
					TileX < RoomWidthInTiles;
					++TileX)
			{
				v3u AbsTile = {
										ScreenX * RoomWidthInTiles + TileX, 
										ScreenY * RoomHeightInTiles + TileY, 
										ScreenZ};

				u32 TileValue = 1;
				if((TileX == 0))
				{
					TileValue = 2;
				}
				if((TileX == (RoomWidthInTiles-1)))
				{
					TileValue = 2;
				}
				if((TileY == 0))
				{
					TileValue = 2;
				}
				if((TileY == (RoomHeightInTiles-1)))
				{
					TileValue = 2;
				}

				if((TileY != RoomHeightInTiles && TileX != RoomWidthInTiles/2) && TileValue ==  2)
				{
					entity* Wall = AddWall(SimRegion, (v3)AbsTile * WorldMap->TileSideInMeters);
				}
			}
		}

		EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
	}

#if HANDMADE_INTERNAL
	{ //NOTE(bjorn): Test location 1 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 &GameState->FrameBoundedTransientArena, WorldMap, 
																		 GameState->DEBUG_TestLocations[1], 
																		 GameState->CameraUpdateBounds, 0);

		AddFloor(SimRegion, v3{0, 0, -0.5f} * WorldMap->TileSideInMeters);
		entity* ForceField = AddForceField(SimRegion, v3{0, 0, 6});
		ForceField->Keyboard = GetKeyboard(Input, 1);
		for(s32 ParticleIndex = 0;
				ParticleIndex < 9;
				ParticleIndex++)
		{
			f32 x = (f32)((ParticleIndex % 3) - 3/2);
			f32 y = (f32)((ParticleIndex / 3) - 3/2);

			AddParticle(SimRegion, v3{x, y, 0.0f} * 2.0f * WorldMap->TileSideInMeters, 
									1.0f + ParticleIndex * 20.0f);
		}

		EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
	}

	{ //NOTE(bjorn): Test location 2 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 &GameState->FrameBoundedTransientArena, WorldMap, 
																		 GameState->DEBUG_TestLocations[2], 
																		 GameState->CameraUpdateBounds, 0);

		AddFloor(SimRegion, v3{0, 0, -0.5f} * WorldMap->TileSideInMeters);
		entity* Fixture = AddFixture(SimRegion, v3{0, 0, 24});

		f32 SpringConstant = 20.0f;

		entity* PrevParticle = 0;
		for(s32 ParticleIndex = 0;
				ParticleIndex < 9;
				ParticleIndex++)
		{
			f32 x = (f32)((ParticleIndex % 3) - 3/2);
			f32 y = (f32)((ParticleIndex / 3) - 3/2);

			v3 Pos = v3{x, y, 0.0f} * 2.0f * WorldMap->TileSideInMeters;
			entity* Particle = AddParticle(SimRegion, Pos, 10.0f);

			if(ParticleIndex < 7)
			{
				AddOneWaySpringAttachment(Particle, Fixture, SpringConstant, 2.0f);
			}
			else
			{ 
				AddTwoWaySpringAttachment(Particle, PrevParticle, SpringConstant*1.5f, 0.1f);
			}

			PrevParticle = Particle;
		}

		entity* A = AddMonstar(SimRegion, v3{6, 0, 0} * WorldMap->TileSideInMeters);
		entity* B = AddMonstar(SimRegion, v3{6, -4, 0} * WorldMap->TileSideInMeters);
		entity* C = AddMonstar(SimRegion, v3{6, -7, 0} * WorldMap->TileSideInMeters);
		entity* D = AddMonstar(SimRegion, v3{3, -7, 0} * WorldMap->TileSideInMeters);
		A->Keyboard = GetKeyboard(Input, 1);
		A->MoveSpec.MoveOnInput = true;

		AddTwoWayBungeeAttachment(A, B, SpringConstant*5.0f, 5.0f);
		AddTwoWayRodAttachment(A, C, 3.0f);
		AddTwoWayCableAttachment(D, C, 0.5f, 5.0f);

		EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
	}

	{ //NOTE(bjorn): Test location 3 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 &GameState->FrameBoundedTransientArena, WorldMap, 
																		 GameState->DEBUG_TestLocations[3], 
																		 GameState->CameraUpdateBounds, 0);

		AddFloor(SimRegion, v3{0, 0, -0.5f} * WorldMap->TileSideInMeters);
		entity* Fixture = AddFixture(SimRegion, v3{0, 0, 20});

		f32 SpringConstant = 20.0f;

		entity* A = AddParticle(SimRegion, v3{ 0,0,10}, 20.0f, 2.0f*v3{1,1,1});
		//TODO(bjorn): Test spinning particle A
		AddOneWaySpringAttachment(A, Fixture, SpringConstant * 4.0f, 2.0f, 
															-v3{0.5f,0.5f,0.5f}, {0,0,0});

		entity* B = AddParticle(SimRegion, v3{ 1, 1, 1}, 5.0f);
		entity* C = AddParticle(SimRegion, v3{-1, 1, 1}, 5.0f);
		entity* D = AddParticle(SimRegion, v3{-1,-1, 1}, 5.0f);
		entity* E = AddParticle(SimRegion, v3{ 1,-1, 1}, 5.0f);

		AddTwoWaySpringAttachment(A, B, SpringConstant, 2.0f, 
															A->Body.Primitives[1].Tran * v3{-0.5f,0.5f,0.5f}, 
															A->Body.Primitives[1].Tran * v3{0,0,0});
		AddTwoWaySpringAttachment(A, C, SpringConstant, 2.0f, 
															A->Body.Primitives[1].Tran * v3{0.5f,-0.5f,0.5f}, 
															A->Body.Primitives[1].Tran * v3{0,0,0});
		AddTwoWaySpringAttachment(A, D, SpringConstant, 2.0f, 
															A->Body.Primitives[1].Tran * v3{0.5f,0.5f,-0.5f}, 
															A->Body.Primitives[1].Tran * v3{0,0,0});
		AddTwoWaySpringAttachment(A, E, SpringConstant, 2.0f, 
															A->Body.Primitives[1].Tran * v3{0.5f,0.5f,0.5f}, 
															A->Body.Primitives[1].Tran * v3{0,0,0});

		EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
	}

	{ //NOTE(bjorn): Test location 4 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 &GameState->FrameBoundedTransientArena, WorldMap, 
																		 GameState->DEBUG_TestLocations[4], 
																		 GameState->CameraUpdateBounds, 0);

		AddFloor(SimRegion, v3{0, 0, -0.5f});
		entity* Fixture = AddFixture(SimRegion, v3{4, 5, 0});

		entity* A = AddParticle(SimRegion, v3{0,0,0}, 20.0f, v3{1,10,1});
		AddOneWaySpringAttachment(A, Fixture, 10.0f, 2.0f, 
															A->Body.Primitives[1].Tran * v3{0.0f,0.5f,0.0f}, 
															A->Body.Primitives[1].Tran * v3{0,0,0});

		EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
	}

	{ //NOTE(bjorn): Test location 5 setup
		sim_region* SimRegion = BeginSim(Input, &GameState->Entities, 
																		 &GameState->FrameBoundedTransientArena, WorldMap, 
																		 GameState->DEBUG_TestLocations[5], 
																		 GameState->CameraUpdateBounds, 0);

		AddFloor(SimRegion, v3{0, 0, -2.0f});

		entity* A = AddParticle(SimRegion, v3{ 0, 0, 6}, 20.0f, v3{1.0f, 1.0f, 1.0f});
		A->O = AngleAxisToQuaternion(tau32*0.125f, {0,1,0}) * AngleAxisToQuaternion(tau32*0.125f, {1,0,0});
		A->dO = {0,pi32,0};

		AddParticle(SimRegion, v3{ 0, 0, 8}, 20.0f, v3{1.0f, 1.0f, 1.0f});
		AddParticle(SimRegion, v3{ 0, 0, 10}, 20.0f, v3{1.0f, 1.0f, 1.0f});
		AddParticle(SimRegion, v3{ 0, 0, 12}, 20.0f, v3{1.0f, 1.0f, 1.0f});
		AddSphereParticle(SimRegion, v3{ 0, 0, 14}, 20.0f, 0.5f);

		entity* B = AddParticle(SimRegion, v3{-1.2f,0,3.0f}, 0.0f, v3{3.0f,6.0f,0.2f});
		B->Rotates = false;
		B->O = AngleAxisToQuaternion(tau32*0.125f, {0,1,0});
		entity* C = AddParticle(SimRegion, v3{1.2f,0,6.0f}, 0.0f, v3{3.0f,6.0f,0.2f});
		C->Rotates = false;
		C->O = AngleAxisToQuaternion(tau32*0.125f, {0,-1,0});

		entity* Q = AddParticle(SimRegion, v3{-2.0f,0,0}, 0.0f, v3{0.2f,4.0f,2.0f});
		Q->Rotates = false;
		//entity* D = AddParticle(SimRegion, v3{ 2.0f,0,0}, 0.0f, v3{0.2f,4.0f,2.0f});
		//D->Rotates = false;
		entity* E = AddParticle(SimRegion, v3{0, 2.1f,0}, 0.0f, v3{4.0f,0.2f,2.0f});
		E->Rotates = false;
		entity* F = AddParticle(SimRegion, v3{0,-2.1f,0}, 0.0f, v3{4.0f,0.2f,2.0f});
		F->Rotates = false;

		EndSim(Input, &GameState->Entities, &GameState->WorldArena, SimRegion);
	}
#endif

	EndTemporaryMemory(TempMem);

  GameState->GeneratedTile = EmptyBitmap(TransientArena, 512, 512);
  GameState->GeneratedTile.Alignment = {256, 256};
  GenerateTile(GameState, &GameState->GeneratedTile);
}

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
#if HANDMADE_INTERNAL
		DEBUG_SwitchToLocation = 5;
#endif
	}

  if(Input->ExecutableReloaded)
  {
    ZeroMemory(GameState->GeneratedTile.Memory, (GameState->GeneratedTile.Width*
                                                 GameState->GeneratedTile.Height*
                                                 GAME_BITMAP_BYTES_PER_PIXEL));
    GenerateTile(GameState, &GameState->GeneratedTile);
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
		}
	}

	render_group* RenderGroup;
#if HANDMADE_INTERNAL
	if(GameState->SimulationSpeedModifier == 0)
	{
		RenderGroup = GameState->DEBUG_OldRenderGroup;
	}
	else
#endif
	{
		RenderGroup = AllocateRenderGroup(FrameBoundedTransientArena, Megabytes(4));
#if HANDMADE_INTERNAL
		GameState->DEBUG_OldRenderGroup = RenderGroup;
		stored_entity* StoredMainCamera = 
			GetStoredEntityByIndex(Entities, GameState->MainCameraStorageIndex);
		GameState->DEBUG_MainCameraOffsetDuringPause = StoredMainCamera->Sim.WorldP;
#endif
	}

	//
	// NOTE(bjorn): Moving and Rendering
	//
  {
#if 0
    local_persist f32 t = 0.0f;
    m44 Transform = ConstructTransform({1.0f*Sin(t*pi32*0.0f), 0.0f}, 
                                       AngleAxisToQuaternion(pi32*Modulate(t*pi32*0.0f, 
                                                                           0.0f, tau32), 
                                                             {0,0,1}), 
                                       {6,6,1});
    v4 Color = {Sin(10.0f*t*pi32)*0.5f+0.5f,
                 Sin(10.0f*t*pi32 + 0.3f*pi32)*0.5f+0.5f,
                 Sin(10.0f*t*pi32 + 0.7f*pi32)*0.5f+0.5f,
                 1.0f};
    t += 0.01f;
#else
    m44 Transform = M44Identity();
    v4 Color = {1,1,1,1};
#endif

#if 1
    PushQuad(RenderGroup, Transform, &GameState->GeneratedTile, Color);
#else
    PushQuad(RenderGroup, Transform, &GameState->Tree[1], Color);
#endif
  }

	//
	// NOTE(bjorn): Create sim region by camera
	//

	sim_region* SimRegion = 0;
	{
		stored_entity* StoredMainCamera = 
			GetStoredEntityByIndex(Entities, GameState->MainCameraStorageIndex);
		SimRegion = BeginSim(Input, Entities, FrameBoundedTransientArena, WorldMap, 
												 StoredMainCamera->Sim.WorldP, GameState->CameraUpdateBounds, 
												 SecondsToUpdate);
	}
	Assert(SimRegion);

#if HANDMADE_INTERNAL
	//DEBUG_VisualizeCollisionGrid(RenderGroup, SimRegion->OuterBounds);
	b32 DEBUG_IsPaused = GameState->SimulationSpeedModifier == 0;

	if(!DEBUG_IsPaused ||
		 (DEBUG_IsPaused && DEBUG_UpdateLoopAdvance))
#endif
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

	u32 LastStep = Steps;
	for(u32 Step = 1;
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
#if HANDMADE_INTERNAL
				if(!DEBUG_UpdateLoopAdvance)
				{
					dT *= GameState->SimulationSpeedModifier;
				}
#elif
				dT *= GameState->SimulationSpeedModifier;
#endif
			}
#if HANDMADE_INTERNAL
      //TODO(bjorn): What was this about?
			// u32 DEBUG_RenderGroupCount = RenderGroup->PieceCount;

			if(!CameraOrAssociates && 
				 DEBUG_IsPaused &&
				 GameState->DEBUG_PauseStep != Step) { continue; }
#endif

			//
			// NOTE(bjorn): Moving / Collision / Game Logic
			//
			if(Entity->Updates && dT)
			{
				Assert(Entity->Body.PrimitiveCount != 1);
				if(Entity->Body.PrimitiveCount > 1) 
				{ 
#if HANDMADE_INTERNAL
					b32 DEBUG_WasInsideAtLeastOnce = false;
#endif
					entity* OtherEntity = SimRegion->Entities;
					for(u32 OtherEntityIndex = 0;
							OtherEntityIndex < SimRegion->EntityCount;
							OtherEntityIndex++, OtherEntity++)
					{
						if(Entity == OtherEntity) { continue; }
#if HANDMADE_INTERNAL
						if(OtherEntity->EntityPairUpdateGenerationIndex == Step &&
							 Entity->DEBUG_EntityCollided  == true &&
							 OtherEntity->DEBUG_EntityCollided == true) 
						{ 
							DEBUG_WasInsideAtLeastOnce = true;
						}
#endif
						if(OtherEntity->EntityPairUpdateGenerationIndex == Step) { continue; }
						if(!OtherEntity->IsSpacial) { continue; }
						if(OtherEntity->Body.PrimitiveCount <= 1) { continue; }
						if(!CollisionFilterCheck(Entity, OtherEntity)) 
						{ 
							//TODO(bjorn): Actually test if the filter did speed up anything.
							continue; 
						}

						contact_result Contacts = {};
						if(Entity->HasBody &&
							 OtherEntity->HasBody)
						{
							Contacts = GenerateContacts(Entity, OtherEntity
#if HANDMADE_INTERNAL
																					,RenderGroup
#endif
																					);
						}
						b32 Inside = Contacts.Count > 0; 

#if HANDMADE_INTERNAL
						b32 DEBUG_OneOfTheEntitiesHadMass = false;
#endif
						if(Inside &&
							 Entity->Collides && 
							 OtherEntity->Collides)
						{
							for(u32 ContactIndex = 0;
									ContactIndex < Contacts.Count;
									ContactIndex++)
							{
								contact Contact = Contacts.E[ContactIndex];
								entity* A = Contact.A;
								entity* B = Contact.B;

								f32 iM_iSum = SafeRatio0(1.0f, A->Body.iM + B->Body.iM);
								//TODO(bjorn): Should I even generate contacts for massless entities.
								if(iM_iSum == 0) { continue; }
#if HANDMADE_INTERNAL
								DEBUG_OneOfTheEntitiesHadMass = true;
#endif

								v3 AP = Contact.P - A->P;
								v3 BP = Contact.P - B->P;
								v3 VelocityDelta = ((Cross(A->dO, AP) + A->dP) - (Cross(B->dO, BP) + B->dP));

								//TODO (bjorn): Use coordinates local to the collision point and
								//treat both the velocity and the impulse as a 3d vector.
								//TODO IMPORTANT(bjorn): Friction is working but due to only
								//one contact at a time getting generated boxes slightly
								//displace and spin around when not being acted upon.
								f32 SeparatingVelocity = Dot(Contact.N, VelocityDelta);
								v3 PlanarVelocity = VelocityDelta - (Contact.N * SeparatingVelocity);
								v3 T = Normalize(PlanarVelocity);

								f32 Restitution = Contact.Restitution;
								f32 Friction = Contact.Friction;

								v3 A_iI_Temp_N = {};
								v3 B_iI_Temp_N = {};
								v3 A_iI_Temp_T = {};
								v3 B_iI_Temp_T = {};
								{
									m33 A_Rot = QuaternionToRotationMatrix(A->O);
									m33 B_Rot = QuaternionToRotationMatrix(B->O);
									m33 A_iI_world = A_Rot * (A->Body.iI * Transpose(A_Rot));
									m33 B_iI_world = B_Rot * (B->Body.iI * Transpose(B_Rot));

									A_iI_Temp_N = A_iI_world*Cross(AP, Contact.N);
									B_iI_Temp_N = B_iI_world*Cross(BP, Contact.N);
									A_iI_Temp_T = A_iI_world*Cross(AP, T);
									B_iI_Temp_T = B_iI_world*Cross(BP, T);
								}

								f32 Divisor_N = 0;
								{
									Divisor_N = 
										SafeRatio0(1.0f, 
															 (A->Body.iM + B->Body.iM) +
															 Dot(Contact.N, Cross(A_iI_Temp_N, AP) + Cross(B_iI_Temp_N, BP)));
								}
								Assert(Divisor_N);

								f32 Divisor_T = 0;
								{
									Divisor_T = 
										SafeRatio0(1.0f, 
															 (A->Body.iM + B->Body.iM) +
															 Dot(T, Cross(A_iI_Temp_T, AP) + Cross(B_iI_Temp_T, BP)));
								}
								Assert(Divisor_T);

								f32 Impulse = -SeparatingVelocity * Divisor_N * (1+Restitution);
								f32 PlanarImpulse = -Magnitude(PlanarVelocity) * Divisor_T * Friction;
								PlanarImpulse = (Sign(PlanarImpulse) * 
																 Clamp0(Absolute(PlanarImpulse), Absolute(Impulse)));

								A->dP += Contact.N * (A->Body.iM * Impulse) + T * (A->Body.iM * PlanarImpulse);
								B->dP -= Contact.N * (B->Body.iM * Impulse) + T * (B->Body.iM * PlanarImpulse);

								A->dO += A_iI_Temp_N * Impulse + A_iI_Temp_T * PlanarImpulse;
								B->dO -= B_iI_Temp_N * Impulse + B_iI_Temp_T * PlanarImpulse;

								Assert(iM_iSum);
								f32 A_MassRatio = (A->Body.iM * iM_iSum);
								f32 B_MassRatio = (B->Body.iM * iM_iSum);
								A->P -= (A_MassRatio * Contact.PenetrationDepth) * Contact.N;
								B->P += (B_MassRatio * Contact.PenetrationDepth) * Contact.N;
							}

							UpdateEntityDerivedMembers(Entity, SimRegion->OuterBounds);
							UpdateEntityDerivedMembers(OtherEntity, SimRegion->OuterBounds);
						} 
#if HANDMADE_INTERNAL
						if(Inside &&
							 DEBUG_OneOfTheEntitiesHadMass)
						{
							DEBUG_WasInsideAtLeastOnce = true;
							Entity->DEBUG_EntityCollided = true;
							OtherEntity->DEBUG_EntityCollided = true;
						}
#endif

						trigger_state_result TriggerState = 
							UpdateAndGetCurrentTriggerState(Entity, OtherEntity, dT, Inside);

						//TODO STUDY Doublecheck HMH 69 to see what this was for again.
#if 0
						entity* A = Entity;
						entity* B = OtherEntity;
						if(Entity->StorageIndex > OtherEntity->StorageIndex)
						{
							Swap(A, B, entity*);
						}
#endif

						//TODO IMPORTANT(bjorn): Both ForceFieldLogic and FloorLogic needs to be moved
						//to a trigger handling and a collision handling system respectively. 
						//NOTE(bjorn): CollisionFiltering makes this call now non-global.
						ForceFieldLogic(Entity, OtherEntity);
						//FloorLogic(Entity, OtherEntity);

						if(TriggerState.OnEnter)
						{
							ApplyDamage(Entity, OtherEntity);
						}

						if(TriggerState.OnLeave)
						{
							Bounce(Entity, OtherEntity);
						}
					}

#if HANDMADE_INTERNAL
					if(!DEBUG_WasInsideAtLeastOnce)
					{
						Entity->DEBUG_EntityCollided = false;
					}
#endif
				}

				v3 OldP = Entity->P;

				if(Step == 1 && //TODO(bjorn): This is easy to forget. Conceptualize?
					 Entity->Keyboard && 
					 Entity->Keyboard->IsConnected)
				{
					v3 WASDKeysDirection = {};
					if(Held(Entity->Keyboard, S))
					{
						if(Held(Entity->Keyboard, Shift))
						{
							WASDKeysDirection.Z += -1;
						}
						else
						{
							WASDKeysDirection.Y += -1;
						}
					}
					if(Held(Entity->Keyboard, A))
					{
						WASDKeysDirection.X += -1;
					}
					if(Held(Entity->Keyboard, W))
					{
						if(Held(Entity->Keyboard, Shift))
						{
							WASDKeysDirection.Z += 1;
						}
						else
						{
							WASDKeysDirection.Y += 1;
						}
					}
					if(Held(Entity->Keyboard, D))
					{
						WASDKeysDirection.X += 1;
					}
					if(Clicked(Entity->Keyboard, Space))
					{
						if(!Entity->IsCamera &&
							 Entity != MainCamera->FreeMover)
						{
							WASDKeysDirection.Z += 1;
						}
					}
					if(LengthSquared(WASDKeysDirection) > 1.0f)
					{
						WASDKeysDirection *= inv_root2;
					}

					Entity->MoveSpec.MovingDirection = 
						Entity->MoveSpec.MoveOnInput ? WASDKeysDirection : v3{};

					if(Entity->Sword &&
						 Entity->MoveSpec.MovingDirection.Z > 0)
					{
						Entity->dP += v3{0,0,10.0f};
					}

					if(Entity->ForceFieldRadiusSquared)
					{
						if(Held(Entity->Keyboard, Space))
						{
							Entity->ForceFieldStrenght = 100.0f;
						}
						else
						{
							Entity->ForceFieldStrenght = 0.0f;
						}
					}

					v2 ArrowKeysDirection = {};
					if(Held(Entity->Keyboard, Down))                 { ArrowKeysDirection.Y += -1; }
					if(Held(Entity->Keyboard, Left))                 { ArrowKeysDirection.X += -1; }
					if(Held(Entity->Keyboard, Up))                   { ArrowKeysDirection.Y +=  1; }
					if(Held(Entity->Keyboard, Right))                { ArrowKeysDirection.X +=  1; }
					if(ArrowKeysDirection.X && ArrowKeysDirection.Y) { ArrowKeysDirection *= inv_root2; }

					if((ArrowKeysDirection.X || ArrowKeysDirection.Y) &&
						 Clicked(Entity->Keyboard, Q) &&
						 Entity->Sword)
					{
						entity* Sword = Entity->Sword;

						if(!Sword->IsSpacial) { Sword->DistanceRemaining = 20.0f; }

						//TODO(bjorn): Use the relevant body primitive scale instead.
						v3 P = (Sword->IsSpacial ?  
										Sword->P : (Entity->P + ArrowKeysDirection * Sword->Body.Primitives[1].S.Y));

						MakeEntitySpacial(Sword, P, ArrowKeysDirection * 1.0f, 
															AngleAxisToQuaternion(ArrowKeysDirection, Normalize(v3{0,0.8f,-1})),
															Normalize({2,3,1})*tau32*0.2f);
					}

					if(Entity->IsCamera)
					{
						if(Clicked(Entity->Keyboard, C))
						{
							if(Entity->Prey == Entity->Player)
							{
								DetachToMoveFreely(Entity);
							}
							else if(Entity->Prey == Entity->FreeMover)
							{
								AttachToPlayer(Entity);
							}
						}

						//TODO Smoother rotation.
						//TODO(bjorn): Remove this hack as soon as I have full 3D rotations
						//going on for all entites!!!
						v2 RotSpeed = v2{1, 1} * (tau32 * SecondsToUpdate * 20.0f);
						f32 ZoomSpeed = (SecondsToUpdate * 30.0f);

						f32 dZoom = 0;
						v2 MouseDir = {};
						if(Entity->Mouse && 
							 Entity->Mouse->IsConnected)
						{
							if(Held(Entity->Mouse, Left))
							{
								MouseDir = Entity->Mouse->dP;
								if(MouseDir.X && MouseDir.Y)
								{
									MouseDir *= inv_root2;
								}
							}
							dZoom = Entity->Mouse->Wheel * ZoomSpeed;
						}

						v2 dCamRot = Hadamard(MouseDir, RotSpeed);

						Entity->CamZoom = Entity->CamZoom + dZoom;
						Entity->CamRot = Modulate0(Entity->CamRot + dCamRot, tau32);
					}
				}

				HunterLogic(Entity);

				//TODO(bjorn): Make ApplyAttachmentForcesAndImpulses somehow depend on
				//time in such a way that changing the simulation speed does not affect
				//the simulation.
				ApplyAttachmentForcesAndImpulses(Entity);

				MoveEntity(Entity, dT);

#if HANDMADE_SLOW
				{
					Assert(LengthSquared(Entity->dP) <= Square(SimRegion->MaxEntityVelocity));

					f32 S1 = Square(Entity->Body.Primitives[0].S.X * 0.5f * Entity->Body.S);
					f32 S2 = Square(SimRegion->MaxEntityRadius);
					Assert(S1 <= S2);
				}
#endif

				v3 NewP = Entity->P;
				f32 dP = Length(NewP - OldP);

				if(Entity->DistanceRemaining > 0)
				{
					Entity->DistanceRemaining -= dP;
					if(Entity->DistanceRemaining <= 0)
					{
						Entity->DistanceRemaining = 0;

						MakeEntityNonSpacial(Entity);
					}
				}

				if(Entity->HunterSearchRadius)
				{
					Entity->BestDistanceToPreySquared = Square(Entity->HunterSearchRadius);
					Entity->MoveSpec.MovingDirection = {};
				}
			}
			
			UpdateEntityDerivedMembers(Entity, SimRegion->OuterBounds);

			//
			// NOTE(bjorn): Push to render buffer
			//
#if HANDMADE_INTERNAL
			if((!DEBUG_IsPaused && 
					Step == LastStep) ||
				 (DEBUG_IsPaused &&
					GameState->DEBUG_PauseStep == Step &&
					DEBUG_UpdateLoopAdvance))
#elif
			if(Step == LastStep)
#endif
			{
				m44 T = Entity->Tran; 

				if(LengthSquared(Entity->dP) != 0.0f)
				{
					if(Absolute(Entity->dP.X) > Absolute(Entity->dP.Y))
					{
						Entity->FacingDirection = (Entity->dP.X > 0) ? 3 : 1;
					}
					else
					{
						Entity->FacingDirection = (Entity->dP.Y > 0) ? 2 : 0;
					}
				}

        if(Entity->IsCamera &&
           Entity == MainCamera)
        {
          //TODO(bjorn):Figure out what this was again.
#if 0
          v3 Offset = GetWorldMapPosDifference(WorldMap, 
                                               GameState->DEBUG_MainCameraOffsetDuringPause, 
                                               MainCamera->WorldP);
#endif
          v3 Offset = {0, 0, MainCamera->CamZoom};
          m33 XRot = XRotationMatrix(MainCamera->CamRot.Y);
          m33 RotMat = AxisRotationMatrix(MainCamera->CamRot.X, GetMatCol(XRot, 2)) * XRot;
          m44 CamTrans = ConstructTransform(Offset, RotMat);

          s32 TileSideInPixels = 60;
          f32 PixelsPerMeter = (f32)TileSideInPixels / WorldMap->TileSideInMeters;

#if 0
          SetCamera(RenderGroup, CamTrans, PixelsPerMeter, positive_infinity32);
#else
          SetCamera(RenderGroup, CamTrans, PixelsPerMeter, 20.0f, 20.f);
#endif
        }

				if(Entity->VisualType == EntityVisualType_Wall)
				{
          m44 QuadTran = ConstructTransform({0,0,0}, 
                                            AngleAxisToQuaternion(tau32 * 0.25f, {1,0,0}), 
                                            Entity->Body.Primitives[1].S);
          PushQuad(RenderGroup, T*QuadTran, &GameState->RockWall);
				}

				if(Entity->VisualType == EntityVisualType_Player)
				{
					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
					PushQuad(RenderGroup, T, &GameState->Shadow, {1, 1, 1, ZAlpha});
					PushQuad(RenderGroup, T, &Hero->Torso);
					PushQuad(RenderGroup, T, &Hero->Cape);
					PushQuad(RenderGroup, T, &Hero->Head);
				}
				if(Entity->VisualType == EntityVisualType_Monstar)
				{
					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
					PushQuad(RenderGroup, T, &GameState->Shadow, {1, 1, 1, ZAlpha});
					PushQuad(RenderGroup, T, &Hero->Torso);
				}
				if(Entity->VisualType == EntityVisualType_Familiar)
				{
					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					f32 ZAlpha = Clamp01(1.0f - ((Entity->P.Z + 1.4f) / 2.0f));
					PushQuad(RenderGroup, T, &GameState->Shadow, {1, 1, 1, ZAlpha});
					PushQuad(RenderGroup, T, &Hero->Head);
				}
				if(Entity->VisualType == EntityVisualType_Sword)
				{
					PushQuad(RenderGroup, T, &GameState->Sword);
				}

				if(Entity->VisualType == EntityVisualType_Monstar ||
					 Entity->VisualType == EntityVisualType_Player)
				{
					for(u32 HitPointIndex = 0;
							HitPointIndex < Entity->HitPointMax;
							HitPointIndex++)
					{
						v2 HitPointDim = {0.15f, 0.3f};
						v3 HitPointPos = Entity->P;
						HitPointPos.Y -= 0.3f;
						HitPointPos.X += ((HitPointIndex - (Entity->HitPointMax-1) * 0.5f) * 
															HitPointDim.X * 1.5f);

						m44 HitPointT = ConstructTransform(HitPointPos, Entity->O, HitPointDim);
						hit_point HP = Entity->HitPoints[HitPointIndex];
						if(HP.FilledAmount == HIT_POINT_SUB_COUNT)
						{
							v3 Green = {0.0f, 1.0f, 0.0f};

							PushBlankQuad(RenderGroup, HitPointT, V4(Green, 1.0f));
						}
						else if(HP.FilledAmount < HIT_POINT_SUB_COUNT &&
										HP.FilledAmount > 0)
						{
							v3 Green = {0.0f, 1.0f, 0.0f};
							v3 Red = {1.0f, 0.0f, 0.0f};

							//TODO(bjorn): How to push this relative to the screen?
							//PushRenderBlankPiece(RenderGroup, HitPointT, (v4)Red);
							//PushRenderBlankPiece(RenderGroup, HitPointT, (v4)Green);
						}
						else
						{
							v3 Red = {1.0f, 0.0f, 0.0f};

							PushBlankQuad(RenderGroup, HitPointT, V4(Red, 1.0f));
						}
					}
				}

#if HANDMADE_INTERNAL
				//TODO(bjorn): Do I need to always push these just to enable toggling when paused?
				if(GameState->DEBUG_VisualiseCollisionBox)
				{
					if(false)//Entity->IsFloor)
					{
						DEBUG_VisualizeCollisionTag(RenderGroup, SimRegion->OuterBounds, Entity);
					}

					for(u32 PrimitiveIndex = 0;
							PrimitiveIndex < Entity->Body.PrimitiveCount;
							PrimitiveIndex++)
					{
						body_primitive* Primitive = Entity->Body.Primitives + PrimitiveIndex;
						if(Primitive->CollisionShape == CollisionShape_Sphere)
						{
							v4 Color = {0,0,0.8f,0.05f};

							if(PrimitiveIndex == 0 &&
								 Entity->StorageIndex != 2)
							{
								if(!Entity->IsCamera &&
									 !Entity->IsFloor &&
									 Entity != MainCamera->FreeMover)
								{
									if(Entity->DEBUG_EntityCollided)
									{
										Color = {1,0,0,0.1f};
									}
									else
									{
										continue;
									}
								}
								else
								{
									continue;
								}
							}

							PushSphere(RenderGroup, T * Primitive->Tran, Color);
						}
						else if(Primitive->CollisionShape == CollisionShape_AABB)
						{
							PushWireCube(RenderGroup, T * Primitive->Tran);
						}
					}

					if(!Entity->IsCamera &&
						 !Entity->IsFloor &&
						 Entity != MainCamera->FreeMover)
					{
						m44 TranslationAndRotation = ConstructTransform(Entity->P, Entity->O, {1,1,1});
						PushCoordinateSystem(RenderGroup, Entity->Tran);

            m44 TranslationOnly = ConstructTransform(Entity->P, QuaternionIdentity(), {1,1,1});
            PushVector(RenderGroup, TranslationOnly, v3{0,0,0}, 
																			 Entity->DEBUG_MostRecent_F*(1.0f/10.0f), {1,0,1});
						PushVector(RenderGroup, TranslationOnly, v3{0,0,0}, 
																			 Entity->DEBUG_MostRecent_T*(1.0f/tau32), {1,1,0});
					}

					for(u32 AttachmentIndex = 0;
							AttachmentIndex < ArrayCount(Entity->Attachments);
							AttachmentIndex++)
					{
						entity* EndPoint = Entity->Attachments[AttachmentIndex];
						if(EndPoint)
						{
							attachment_info Info = Entity->AttachmentInfo[AttachmentIndex];

							v3 AP1 = Entity->Tran   * Info.AP1;
							v3 AP2 = EndPoint->Tran * Info.AP2;

							v3 APSize = 0.08f*v3{1,1,1};

							m44 T1 = ConstructTransform(AP1, QuaternionIdentity(), APSize);
							PushWireCube(RenderGroup, T1, {0.7f,0,0,1});
							v3 n = Normalize(AP2-AP1);
							m44 T2 = ConstructTransform(AP1 + n*Info.Length, QuaternionIdentity(), APSize);
							PushWireCube(RenderGroup, T2, {1,1,1,1});

							PushVector(RenderGroup, M44Identity(), AP1, AP2);

							PushVector(RenderGroup, M44Identity(), Entity->P, AP1, {1,1,1,1});
						}
					}
				}
#endif
			}
#if HANDMADE_INTERNAL
			else
			{
        //TODO(bjorn): What was this about?
				// RenderGroup->PieceCount = DEBUG_RenderGroupCount;
			}
#endif
		}
	}

	EndSim(Input, Entities, WorldArena, SimRegion);

  ClearScreen(RenderGroup, {0.5f, 0.5f, 0.5f});

	//
	// NOTE(bjorn): Rendering
	//
  RenderGroupToOutput(RenderGroup, Buffer);

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
