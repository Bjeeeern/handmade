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
//
// * Solve weird entity visual artefact when jumping.

// TODO
// * Make it so that I can visually step through a frame of collision.
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
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;
};

struct game_state
{
	memory_arena WorldArena;
	world_map* WorldMap;

	memory_arena TransientArena;

	//TODO(bjorn): Should we allow split-screen?
	u64 MainCameraStorageIndex;
	rectangle3 CameraUpdateBounds;

	stored_entities Entities;

	loaded_bitmap Backdrop;
	loaded_bitmap Rock;
	loaded_bitmap Dirt;
	loaded_bitmap Shadow;
	loaded_bitmap Sword;

	hero_bitmaps HeroBitmaps[4];

#if HANDMADE_INTERNAL
	b32 DEBUG_VisualiseMinkowskiSum;
	b32 DEBUG_VisualiseCollisionBox;

	b32 DEBUG_StepThroughTheCollisionLoop;
	b32 DEBUG_CollisionLoopAdvance;

	entity* DEBUG_CollisionLoopEntity;
	s32 DEBUG_CollisionLoopStepIndex;
	v3 DEBUG_CollisionLoopEstimatedPos;
#endif

	f32 GroundStaticFriction;
	f32 GroundDynamicFriction;

	f32 NoteTone;
	f32 NoteDuration;
	f32 NoteSecondsPassed;
};

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState, game_input* Input)
{
	GameState->GroundStaticFriction = 2.1f; 
	GameState->GroundDynamicFriction = 2.0f; 
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
	InitializeArena(&GameState->TransientArena, Memory->TransientStorageSize, 
									(u8*)Memory->TransientStorage);

	GameState->WorldMap = PushStruct(&GameState->WorldArena, world_map);

	world_map *WorldMap = GameState->WorldMap;
	WorldMap->ChunkSafetyMargin = 256;
	WorldMap->TileSideInMeters = 1.4f;
	WorldMap->ChunkSideInMeters = WorldMap->TileSideInMeters * TILES_PER_CHUNK;

	InitializeArena(&GameState->TransientArena, Memory->TransientStorageSize, 
									(u8*)Memory->TransientStorage);

	u32 RoomWidthInTiles = 17;
	u32 RoomHeightInTiles = 9;

	v3s RoomOrigin = (v3s)RoundV2ToV2S((v2)v2u{RoomWidthInTiles, RoomHeightInTiles} / 2.0f);
	GameState->CameraUpdateBounds = RectCenterDim(v3{0, 0, 0}, 
																								v3{2, 2, 2} * WorldMap->ChunkSideInMeters);
                                                                                                    
	sim_region* SimRegion = BeginSim(Input, &GameState->Entities, &GameState->TransientArena,
																	 WorldMap, GetChunkPosFromAbsTile(WorldMap, RoomOrigin), 
																	 GameState->CameraUpdateBounds, 0);

	entity* MainCamera = AddCamera(SimRegion, v3{0, 0, 0});
	//TODO Is there a less cheesy (and safer!) way to do this assignment of the camera storage index?
	GameState->MainCameraStorageIndex = 1;
	MainCamera->Keyboard = GetKeyboard(Input, 1);

	entity* Player = AddPlayer(SimRegion, v3{-2, 1, 0} * WorldMap->TileSideInMeters);
	Player->Keyboard = GetKeyboard(Input, 1);

	MainCamera->Prey = Player;
	MainCamera->Player = Player;
	MainCamera->FreeMover = AddInvisibleControllable(SimRegion);
	MainCamera->FreeMover->Keyboard = GetKeyboard(Input, 1);

	entity* Familiar = AddFamiliar(SimRegion, v3{4, 5, 0} * WorldMap->TileSideInMeters);
	Familiar->Prey = Player;

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
		s64 Amount = (&(TestEntity.struct_terminator0_) - &(TestEntity.Attatchments[0]));
		s64 Limit = ArrayCount(TestEntity.Attatchments);
		Assert(Amount <= Limit);
	}
#endif

	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	if(!Memory->IsInitialized)
	{
		InitializeGame(Memory, GameState, Input);
		Memory->IsInitialized = true;
	}

	memory_arena* WorldArena = &GameState->WorldArena;
	world_map* WorldMap = GameState->WorldMap;
	stored_entities* Entities = &GameState->Entities;
	memory_arena* TransientArena = &GameState->WorldArena;

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

#if 0
				//TODO IMPORTANT Collect the relevant interpretation of the input and
				//pass it on to the game logic.
				ControlledEntity->ddP = InputDirection * 85.0f;
#endif

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

	for(s32 KeyboardIndex = 1;
			KeyboardIndex <= ArrayCount(Input->Keyboards);
			KeyboardIndex++)
	{
		game_keyboard* Keyboard = GetKeyboard(Input, KeyboardIndex);
		if(Keyboard->IsConnected)
		{
#if HANDMADE_INTERNAL
			if(Clicked(Keyboard, Q))
			{
				GameState->NoteTone = 500.0f;
				GameState->NoteDuration = 0.05f;
				GameState->NoteSecondsPassed = 0.0f;

				//TODO STUDY(bjorn): Just setting the flag is not working anymore.
				//Memory->IsInitialized = false;

				GameState->DEBUG_StepThroughTheCollisionLoop = !GameState->DEBUG_StepThroughTheCollisionLoop;
				GameState->DEBUG_CollisionLoopEntity = 0;
				GameState->DEBUG_CollisionLoopStepIndex = 0;
			}

			if(GameState->DEBUG_StepThroughTheCollisionLoop && Clicked(Keyboard, Space))
			{
				GameState->DEBUG_CollisionLoopAdvance = true;
				GameState->DEBUG_CollisionLoopAdvance = true;
			}
#endif

			if(Clicked(Keyboard, M))
			{
				GameState->DEBUG_VisualiseMinkowskiSum = !GameState->DEBUG_VisualiseMinkowskiSum;
				GameState->DEBUG_VisualiseCollisionBox = !GameState->DEBUG_VisualiseCollisionBox;
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

#if 0
	DrawBitmap(Buffer, &GameState->Backdrop, {-40.0f, -40.0f}, 
						 {(f32)GameState->Backdrop.Width, (f32)GameState->Backdrop.Height});
#endif

	//
	// NOTE(bjorn): Create sim region by camera
	//

	temporary_memory TempMem = BeginTemporaryMemory(TransientArena);
	sim_region* SimRegion = 0;
	{
		u64 MainCamIndex = GameState->MainCameraStorageIndex;
		stored_entity* StoredMainCamera = GetStoredEntityByIndex(Entities, MainCamIndex);
		SimRegion = BeginSim(Input, Entities, TransientArena, WorldMap, 
												 StoredMainCamera->Sim.WorldP, GameState->CameraUpdateBounds, 
												 SecondsToUpdate);
	}

	render_group* RenderGroup = AllocateRenderGroup(TransientArena);

	//TODO(bjorn): Implement step 2 in J.Blows framerate independence video.
	// https://www.youtube.com/watch?v=fdAOPHgW7qM
	//TODO(bjorn): Add some asserts and some limits to velocities related to the
	//delta time so that tunneling becomes virtually impossible.
	u32 Steps = 8; 
	//NOTE(bjorn): Asserts at least one iteration to find the main camera entity pointer.
	Assert(Steps > 1);

	entity* MainCamera = 0;

	u32 LastStep = Steps;
	f32 dT = SecondsToUpdate / (f32)Steps;
	for(u32 Step = 1;
			Step <= LastStep;
			Step++)
	{
		entity* Entity = SimRegion->Entities;
		for(u32 EntityIndex = 0;
				EntityIndex < SimRegion->EntityCount;
				EntityIndex++, Entity++)
		{
			Entity->EntityPairUpdateGenerationIndex = Step;
			//TODO Do non-spacial entities ever do logic/Render? Do they affect other entities then? 
			if(!Entity->IsSpacial) { continue; }

			if(!MainCamera && Entity->Player && Entity->FreeMover) 
			{ 
				MainCamera = Entity; 
			}

			//
			// NOTE(bjorn): Moving / Collision / Game Logic
			//
			if(Entity->Updates)
			{
				//TODO(bjorn):
				// Add negative gravity for penetration if relative velocity is >= 0.
				// Get relevant contact point.
				// Test with object on mouse.
				entity* OtherEntity = SimRegion->Entities;
				for(u32 OtherEntityIndex = 0;
						OtherEntityIndex < SimRegion->EntityCount;
						OtherEntityIndex++, OtherEntity++)
				{
					if(Entity == OtherEntity) { continue; }
					if(OtherEntity->EntityPairUpdateGenerationIndex == Step) { continue; }
					if(!OtherEntity->IsSpacial) { continue; }

					b32 Inside = true; 
					f32 BestSquareDistanceToWall = positive_infinity32;
					s32 RelevantNodeIndex = -1;
					v2 P = Entity->P.XY;
					polygon Sum = MinkowskiSum(OtherEntity, Entity);

					for(s32 NodeIndex = 0; 
							NodeIndex < Sum.NodeCount; 
							NodeIndex++)
					{
						v2 N0 = Sum.Nodes[NodeIndex];
						v2 N1 = Sum.Nodes[(NodeIndex+1) % Sum.NodeCount];

						f32 Det = Determinant(N1-N0, P-N0);
						if(Inside && (Det >= 0.0f)) 
						{ 
							Inside = false; 
						}

						f32 SquareDistanceToWall = SquareDistancePointToLineSegment(N0, N1, P);
						if(SquareDistanceToWall < BestSquareDistanceToWall)
						{
							BestSquareDistanceToWall = SquareDistanceToWall;
							RelevantNodeIndex = NodeIndex;
						}
					}	

					if(Inside &&
						 Entity->Collides && 
						 OtherEntity->Collides)
					{
						Assert(RelevantNodeIndex >= 0);

						f32 Penetration;
						v2 n, t, ImpactPoint;
						{
							Penetration	= SquareRoot(BestSquareDistanceToWall);
							Assert(Penetration <= (Lenght(Entity->Dim) + Lenght(OtherEntity->Dim)) * 0.5f);

							v2 N0 = Sum.Nodes[RelevantNodeIndex];
							v2 N1 = Sum.Nodes[(RelevantNodeIndex+1) % Sum.NodeCount];
							v2 V0 = Sum.OriginalLineSeg[RelevantNodeIndex][0];
							v2 V1 = Sum.OriginalLineSeg[RelevantNodeIndex][1];

							//TODO(bjorn): This normalization might not be needed.
							t = Normalize(N1 - N0);
							n = CCW90M22() * t;

							closest_point_to_line_result ClosestPoint = GetClosestPointOnLineSegment(N0, N1, P);

							ImpactPoint = V0 + (V1-V0) * ClosestPoint.t;
							v2 ImpactCorrection = n * Penetration * 0.5f;
							ImpactCorrection *= 
								Sum.Genus[RelevantNodeIndex] == MinkowskiGenus_Movable ? 1.0f:-1.0f;
							ImpactPoint += ImpactCorrection;
						}

						//NOTE(bjorn): Normal always points away from other entity.
						v2 AP = Entity->P.XY;
						v2 BP = OtherEntity->P.XY;
						v2 AIt = CW90M22() * (AP - ImpactPoint);
						v2 BIt = CW90M22() * (BP - ImpactPoint);
						v2 AdP = Entity->dP.XY      + Entity->dA      * AIt;
						v2 BdP = OtherEntity->dP.XY + OtherEntity->dA * BIt;
						v2 ABdP = AdP - BdP;
						f32 ndotAB = Dot(n, ABdP);

						f32 AInvMass = SafeRatio0(1.0f, Entity->Mass);
						f32 BInvMass = SafeRatio0(1.0f, OtherEntity->Mass);
						//TODO(bjorn): Automize inertia calculation.
						f32 AInvMoI  = AInvMass ? 0.08f:0.0f;//GetInverseOrZero(Entity->High->MoI);
						f32 BInvMoI  = BInvMass ? 0.08f:0.0f;//GetInverseOrZero(OtherEntity->High->MoI);

						f32 Impulse = 0;
						f32 j = 0;

						if(ndotAB < 0)
						{
							f32 e = 0.0f;
							j = ((-(1+e) * Dot(ABdP, n)) / 
									 (/*Dot(n, n) * */(AInvMass + BInvMass) + 
										Square(Dot(AIt, n))*AInvMoI + 
										Square(Dot(BIt, n))*BInvMoI
									 )
									);

							Impulse = j;
						}

						Entity->dP.XY      += (Impulse * AInvMass) * n;
						OtherEntity->dP.XY -= (Impulse * BInvMass) * n;
						Entity->dA         += Dot(AIt, Impulse * n) * AInvMoI;
						OtherEntity->dA    -= Dot(BIt, Impulse * n) * BInvMoI;

						Entity->P.XY       += n * Penetration * 0.5f;
						OtherEntity->P.XY  -= n * Penetration * 0.5f;
					} 

					trigger_state_result TriggerState = 
						UpdateAndGetCurrentTriggerState(Entity, OtherEntity, dT, Inside);

					entity* A = Entity;
					entity* B = OtherEntity;
					//TODO STUDY Doublecheck HMH 69 to see what this was for again.
					if(Entity->StorageIndex > OtherEntity->StorageIndex)
					{
						Swap(A, B, entity*);
					}

					if(TriggerState.OnEnter)
					{
						ApplyDamage(Entity, OtherEntity);
					}

					if(TriggerState.OnLeave)
					{
						Bounce(Entity, OtherEntity);
					}

#if HANDMADE_INTERNAL
					if(Step == LastStep)
					{
						if(GameState->DEBUG_VisualiseMinkowskiSum)
						{
#if 0 
							v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;
							m22 GameSpaceToScreenSpace = 
							{PixelsPerMeter, 0             ,
								0             ,-PixelsPerMeter};

							u64 Player1StorageIndex = 2;
							entity* Player = 0;
							if(Entity->StorageIndex == Player1StorageIndex) { Player = Entity;}
							if(OtherEntity->StorageIndex == Player1StorageIndex) { Player = OtherEntity;}

							if(Player) 
							{
								entity* Target = Entity == Player ? OtherEntity : Entity;
								DEBUGMinkowskiSum(Buffer, Target, Player, GameSpaceToScreenSpace, ScreenCenter);
							}
#endif
						}
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
						WASDKeysDirection.Y += -1;
					}
					if(Held(Entity->Keyboard, A))
					{
						WASDKeysDirection.X += -1;
					}
					if(Held(Entity->Keyboard, W))
					{
						WASDKeysDirection.Y += 1;
					}
					if(Held(Entity->Keyboard, D))
					{
						WASDKeysDirection.X += 1;
					}
					if(Held(Entity->Keyboard, Space))
					{
						WASDKeysDirection.Z += 1;
					}
					if(WASDKeysDirection.X && WASDKeysDirection.Y)
					{
						WASDKeysDirection *= inv_root2;
					}

					Entity->MoveSpec.MovingDirection = Entity->MoveSpec.MoveByInput ? WASDKeysDirection : v3{};

					v2 ArrowKeysDirection = {};
					if(Held(Entity->Keyboard, Down))
					{
						ArrowKeysDirection.Y += -1;
					}
					if(Held(Entity->Keyboard, Left))
					{
						ArrowKeysDirection.X += -1;
					}
					if(Held(Entity->Keyboard, Up))
					{
						ArrowKeysDirection.Y += 1;
					}
					if(Held(Entity->Keyboard, Right))
					{
						ArrowKeysDirection.X += 1;
					}
					if(ArrowKeysDirection.X && ArrowKeysDirection.Y)
					{
						ArrowKeysDirection *= inv_root2;
					}

					if(Clicked(Entity->Keyboard, Q) &&
						 Entity->Sword)
					{
						entity* Sword = Entity->Sword;

						if(!Sword->IsSpacial) { Sword->DistanceRemaining = 20.0f; }

						v3 P = (Sword->IsSpacial ?  Sword->P : (Entity->P + ArrowKeysDirection * Sword->Dim.Y));
						MakeEntitySpacial(Sword, P, ArrowKeysDirection * 8.0f, ArrowKeysDirection);
					}

					if(Entity->FreeMover && 
						 Entity->Player)
					{
						if(Clicked(Entity->Keyboard, C))
						{
							if(Entity->Prey == Entity->Player)
							{
								MakeEntitySpacial(Entity->FreeMover, Entity->P);
								Entity->Player->MoveSpec.MoveByInput = false;
								Entity->Prey = Entity->FreeMover;
								Entity->MoveSpec.Speed = Entity->FreeMover->MoveSpec.Speed;
								Entity->MoveSpec.Drag = Entity->FreeMover->MoveSpec.Drag;
							}
							else if(Entity->Prey == Entity->FreeMover)
							{
								MakeEntityNonSpacial(Entity->FreeMover);
								Entity->Player->MoveSpec.MoveByInput = true;
								Entity->Prey = Entity->Player;
								Entity->MoveSpec.Speed = Entity->Player->MoveSpec.Speed;
								Entity->MoveSpec.Drag = Entity->Player->MoveSpec.Drag;
							}
						}

						//TODO Smoother rotation.
						//TODO(bjorn): Remove this hack as soon as I have full 3D rotations
						//going on for all entites!!!
						v2 RotSpeed = v2{-1, 1} * (tau32 * SecondsToUpdate * 0.2f);
						f32 ZoomSpeed = (SecondsToUpdate * 10.0f);

						v2 dCamRot = Hadamard(ArrowKeysDirection, RotSpeed);

						b32 ZoomIn = (Held(Entity->Keyboard, Shift) &&
													Held(Entity->Keyboard, Up));
						b32 ZoomOut = (Held(Entity->Keyboard, Shift) &&
													 Held(Entity->Keyboard, Down));
						f32 dZoom = 0;
						if(ZoomIn || ZoomOut)
						{
							dCamRot = {};
							dZoom = ZoomIn ? -1.0f : 1.0f;
						}

						Entity->CamZoom = Entity->CamZoom + dZoom * ZoomSpeed;
						Entity->CamRot = Modulate0(Entity->CamRot + dCamRot, tau32);
					}
				}

				HunterLogic(Entity);

				MoveEntity(Entity, dT);

#if HANDMADE_SLOW
				{
					Assert(LenghtSquared(Entity->dP) <= Square(SimRegion->MaxEntityVelocity));

					f32 S1 = LenghtSquared(Entity->Dim * 0.5f);
					f32 S2 = Square(SimRegion->MaxEntityRadius);
					Assert(S1 <= S2);
				}
#endif

				v3 NewP = Entity->P;
				f32 dP = Lenght(NewP - OldP);

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

			//
			// NOTE(bjorn): Push to render buffer
			//
			if(Step == LastStep)
			{
				if(LenghtSquared(Entity->dP) != 0.0f)
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

				if(Entity->VisualType == EntityVisualType_Wall)
				{
					PushRenderPieceQuad(RenderGroup, Entity->P, &GameState->Rock);
				}

				if(Entity->VisualType == EntityVisualType_Player)
				{
					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
					PushRenderPieceQuad(RenderGroup, Entity->P, &GameState->Shadow, {1, 1, 1, ZAlpha});
					PushRenderPieceQuad(RenderGroup, Entity->P, &Hero->Torso);
					PushRenderPieceQuad(RenderGroup, Entity->P, &Hero->Cape);
					PushRenderPieceQuad(RenderGroup, Entity->P, &Hero->Head);
				}
				if(Entity->VisualType == EntityVisualType_Monstar)
				{
					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
					PushRenderPieceQuad(RenderGroup, Entity->P, &GameState->Shadow, {1, 1, 1, ZAlpha});
					PushRenderPieceQuad(RenderGroup, Entity->P, &Hero->Torso);
				}
				if(Entity->VisualType == EntityVisualType_Familiar)
				{
					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					f32 ZAlpha = Clamp01(1.0f - ((Entity->P.Z + 1.4f) / 2.0f));
					PushRenderPieceQuad(RenderGroup, Entity->P, &GameState->Shadow, {1, 1, 1, ZAlpha});
					PushRenderPieceQuad(RenderGroup, Entity->P, &Hero->Head);
				}
				if(Entity->VisualType == EntityVisualType_Sword)
				{
					PushRenderPieceQuad(RenderGroup, Entity->P, &GameState->Sword);
				}

				if(Entity->VisualType == EntityVisualType_Monstar ||
					 Entity->VisualType == EntityVisualType_Player)
				{
					for(u32 HitPointIndex = 0;
							HitPointIndex < Entity->HitPointMax;
							HitPointIndex++)
					{
						v2 HitPointDim = {0.15f, 0.3f};
						v2 HitPointPos = Entity->P.XY;
						HitPointPos.Y -= 0.3f;
						HitPointPos.X += ((HitPointIndex - (Entity->HitPointMax-1) * 0.5f) * 
															HitPointDim.X * 1.5f);

						hit_point HP = Entity->HitPoints[HitPointIndex];
						if(HP.FilledAmount == HIT_POINT_SUB_COUNT)
						{
							v3 Green = {0.0f, 1.0f, 0.0f};

							PushRenderPieceQuad(RenderGroup, HitPointPos, HitPointDim, V4(Green, 1.0f));
						}
						else if(HP.FilledAmount < HIT_POINT_SUB_COUNT &&
										HP.FilledAmount > 0)
						{
							v3 Green = {0.0f, 1.0f, 0.0f};
							v3 Red = {1.0f, 0.0f, 0.0f};

							//TODO(bjorn): How to push this relative to the screen?
							//PushRenderPiece(RenderGroup, HitPointPos, HitPointDim, (v4)Red);
							//PushRenderPiece(RenderGroup, HitPointPos, HitPointDim, (v4)Green);
						}
						else
						{
							v3 Red = {1.0f, 0.0f, 0.0f};

							PushRenderPieceQuad(RenderGroup, HitPointPos, HitPointDim, V4(Red, 1.0f));
						}
					}
				}

#if HANDMADE_INTERNAL
				if(GameState->DEBUG_VisualiseCollisionBox)
				{
					PushRenderPieceWireFrame(RenderGroup, Entity->P, Entity->Dim);
				}
#endif
			}
		}
	}
	EndSim(Input, Entities, WorldArena, SimRegion);

	//
	// NOTE(bjorn): Rendering
	//
	{
		//TODO(bjorn): Remove this hack as soon as I have full 3D rotations
		//going on for all entites!!!
		Assert(MainCamera);
		m33 XRot = XRotationMatrix(MainCamera->CamRot.Y);
		m33 RotMat = AxisRotationMatrix(MainCamera->CamRot.X, GetMatCol(XRot, 2)) * XRot;

		v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

		m22 GameSpaceToScreenSpace = 
		{PixelsPerMeter, 0             ,
			0            ,-PixelsPerMeter};

		render_piece* RenderPiece = RenderGroup->RenderPieces;
		for(u32 RenderPieceIndex = 0;
				RenderPieceIndex < RenderGroup->PieceCount;
				RenderPiece++, RenderPieceIndex++)
		{
			if(RenderPiece->Type == RenderPieceType_Quad)
			{

				transform_result Tran = TransformPoint(MainCamera->CamZoom, RotMat, RenderPiece->P);
				if(!Tran.Valid) { continue; }

				f32 f = Tran.f;
				v2 Pos = Tran.P;

				v2 PixelPos = ScreenCenter + (GameSpaceToScreenSpace * Pos) * f;

				if(RenderPiece->BMP)
				{
					DrawBitmap(Buffer, RenderPiece->BMP, 
										 PixelPos - RenderPiece->BMP->Alignment * f, 
										 RenderPiece->BMP->Dim * f, RenderPiece->Color.A);
				}
				else
				{
					v2 PixelDim = RenderPiece->Dim.XY * (PixelsPerMeter * f);
					rectangle2 Rect = RectCenterDim(PixelPos, PixelDim);

					DrawRectangle(Buffer, Rect, RenderPiece->Color.RGB);
				}
			}
			else if(RenderPiece->Type == RenderPieceType_DimCube)
			{
				vertices Verts = GetEntityVerticesRaw(DefaultEntityOrientation(), 
																							 RenderPiece->P, RenderPiece->Dim);

				for(int VertIndex = 0; 
						VertIndex < 4; 
						VertIndex++)
				{
					v2 V0 = Verts.Verts[(VertIndex+0)%4].XY;
					v2 V1 = Verts.Verts[(VertIndex+1)%4].XY;
					transform_result Tran0 = TransformPoint(MainCamera->CamZoom, RotMat, V0);
					transform_result Tran1 = TransformPoint(MainCamera->CamZoom, RotMat, V1);

					if(!Tran0.Valid || !Tran1.Valid) { continue; }

					v2 PixelV0 = ScreenCenter + (GameSpaceToScreenSpace * Tran0.P) * Tran0.f;
					v2 PixelV1 = ScreenCenter + (GameSpaceToScreenSpace * Tran1.P) * Tran1.f;
					DrawLine(Buffer, PixelV0, PixelV1, RenderPiece->Color.RGB);
				}
			}
		}
	}

	EndTemporaryMemory(TempMem);
	CheckMemoryArena(TransientArena);
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
