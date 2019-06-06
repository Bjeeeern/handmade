#include "platform.h"
#include "memory.h"
#include "intrinsics.h"
#include "world_map.h"
#include "random.h"
#include "renderer.h"
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
// Make it so that I can visually step through a frame of collision.
// generate world as you drive
// car engine that is settable by mouse click and drag
// ai cars

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

//TODO: Move this to the entity as a thing it either do or do not have.
struct simulation_request
{
	u32 PlayerStorageIndex;
	v3 ddP;
	v2 FireSword;
};

internal_function simulation_request*
GetSimRequestForEntity(simulation_request* SimReq, u32 SimReqCount, u32 StorageIndex)
{
	simulation_request* Result = 0;

	for(u32 SimReqIndex = 0;
			SimReqIndex < SimReqCount;
			SimReqIndex++, SimReq++)
	{
		if(SimReq->PlayerStorageIndex == StorageIndex) { Result = SimReq; }
	}

	return Result;
}

struct game_state
{
	memory_arena WorldArena;
	world_map* WorldMap;

	memory_arena SimArena;

	//TODO(bjorn): Should we allow split-screen?
	u32 MainCameraStorageIndex;
	rectangle3 CameraUpdateBounds;
	//u32 CameraFollowingPlayerIndex;
	//world_map_position CameraP;

	union
	{
		simulation_request PlayerSimulationRequests[(ArrayCount(((game_input*)0)->Keyboards) + 
																								 ArrayCount(((game_input*)0)->Controllers))];
		struct
		{
			simulation_request KeyboardSimulationRequests[ArrayCount(((game_input*)0)->Keyboards)]; 
			simulation_request ControllerSimulationRequests[ArrayCount(((game_input*)0)->Controllers)]; 
		};
	};

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
InitializeGame(game_memory *Memory, game_state *GameState)
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

	GameState->WorldMap = PushStruct(&GameState->WorldArena, world_map);

	world_map *WorldMap = GameState->WorldMap;
	WorldMap->ChunkSafetyMargin = 256;
	WorldMap->TileSideInMeters = 1.4f;
	WorldMap->ChunkSideInMeters = WorldMap->TileSideInMeters * TILES_PER_CHUNK;

	InitializeArena(&GameState->SimArena, Memory->TransientStorageSize >> 2, 
									(u8*)Memory->TransientStorage);

	u32 RoomWidthInTiles = 17;
	u32 RoomHeightInTiles = 9;

	v3s RoomOrigin = (v3s)RoundV2ToV2S((v2)v2u{RoomWidthInTiles, RoomHeightInTiles} / 2.0f);
	GameState->CameraUpdateBounds = RectCenterDim(v3{0, 0, 0}, 
																								v3{2, 2, 2} * WorldMap->ChunkSideInMeters);
                                                                                                    
	sim_region* SimRegion = BeginSim(&GameState->Entities, &GameState->SimArena, WorldMap, 
																	 GetChunkPosFromAbsTile(WorldMap, v3s{0, 0, 0}), 
																	 GameState->CameraUpdateBounds, 0);

	entity* MainCamera = AddCamera(SimRegion, RoomOrigin);
	//TODO Is there a less cheesy (and safer!) way to do this assignment of the camera storage index?
	GameState->MainCameraStorageIndex = 1;

	entity* Player = AddPlayer(SimRegion, v3{-2, 1, 0} * WorldMap->TileSideInMeters);
	MainCamera->CameraTarget.Ptr = Player;

	entity* Familiar = AddFamiliar(SimRegion, v3{4, 5, 0} * WorldMap->TileSideInMeters);
	Familiar->Prey.Ptr = Player;

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

	EndSim(&GameState->Entities, &GameState->WorldArena, SimRegion);

	stored_entity* StoredMainCamera = 
		GetStoredEntityByIndex(&GameState->Entities, GameState->MainCameraStorageIndex);
	GameState->KeyboardSimulationRequests[0].PlayerStorageIndex = 
		StoredMainCamera->Sim.CameraTarget.Index;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	{
		s64 Amount = (&GetController(Input, 0)->Struct_Terminator - 
									&GetController(Input, 0)->Buttons[0]);
		s64 Limit = ArrayCount(GetController(Input, 0)->Buttons);
		Assert(Amount == Limit);
	}

	{
		entity TestEntity = {};
		s64 Amount = (&(TestEntity.struct_terminator_) - &(TestEntity.EntityReferences[0]));
		s64 Limit = ArrayCount(TestEntity.EntityReferences);
		Assert(Amount <= Limit);
	}

	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	if(!Memory->IsInitialized)
	{
		InitializeGame(Memory, GameState);
		Memory->IsInitialized = true;
	}

	InitializeArena(&GameState->SimArena, Memory->TransientStorageSize >> 2, 
									(u8*)Memory->TransientStorage);

	memory_arena* WorldArena = &GameState->WorldArena;
	world_map* WorldMap = GameState->WorldMap;
	stored_entities* Entities = &GameState->Entities;

	//
	//NOTE(bjorn): Gather input for this frame.
	//
	{
		simulation_request* SimReq = GameState->ControllerSimulationRequests;
		for(s32 ControllerIndex = 0;
				ControllerIndex < ArrayCount(Input->Controllers);
				ControllerIndex++, SimReq++)
		{
			game_controller* Controller = GetController(Input, ControllerIndex);
			if(Controller->IsConnected)
			{
				if(SimReq->PlayerStorageIndex)
				{
					SimReq->ddP = Controller->LeftStick.End;

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
				else
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

		SimReq = GameState->KeyboardSimulationRequests;
		for(s32 KeyboardIndex = 0;
				KeyboardIndex < ArrayCount(Input->Keyboards);
				KeyboardIndex++, SimReq++)
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

					//TODO(bjorn): Just setting the flag is not working anymore.
					//Memory->IsInitialized = false;

					GameState->DEBUG_StepThroughTheCollisionLoop = 
						!GameState->DEBUG_StepThroughTheCollisionLoop;
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

				if(Clicked(Keyboard, C))
				{
					//TODO IMPORTANT Move this to the player handling code.
					stored_entity* MainCameraStored = 
						GetStoredEntityByIndex(Entities, GameState->MainCameraStorageIndex); 
					if(MainCameraStored->Sim.CameraTarget.Index)
					{
						MainCameraStored->Sim.CameraTarget.Index = 0;
					}
					else
					{
						MainCameraStored->Sim.CameraTarget.Index = SimReq->PlayerStorageIndex;
					}
				}

				if(SimReq->PlayerStorageIndex)
				{
					v3 InputDirection = {};

					if(Held(Keyboard, S))
					{
						InputDirection.Y += -1;
					}
					if(Held(Keyboard, A))
					{
						InputDirection.X += -1;
					}
					if(Held(Keyboard, W))
					{
						InputDirection.Y += 1;
					}
					if(Held(Keyboard, D))
					{
						InputDirection.X += 1;
					}
					if(Held(Keyboard, Space))
					{
						InputDirection.Z += 1;
					}

					if(InputDirection.X && InputDirection.Y)
					{
						InputDirection *= inv_root2;
					}

					SimReq->ddP = InputDirection;

					v2 ArrowKeysDirection = {};

					if(Held(Keyboard, Down))
					{
						ArrowKeysDirection.Y += -1;
					}
					if(Held(Keyboard, Left))
					{
						ArrowKeysDirection.X += -1;
					}
					if(Held(Keyboard, Up))
					{
						ArrowKeysDirection.Y += 1;
					}
					if(Held(Keyboard, Right))
					{
						ArrowKeysDirection.X += 1;
					}

					if(Clicked(Keyboard, Q) && LenghtSquared(ArrowKeysDirection))
					{
						SimReq->FireSword = ArrowKeysDirection;
					}
					else
					{
						SimReq->FireSword = {};
					}

#if 0
					if(Clicked(Keyboard, E))
					{
						//if(ControlledEntity->RidingVehicle)
						{
							//entity Vehicle = GetEntityByLowIndex(Entities, 
							//																		 ControlledEntity->RidingVehicle);
							//DismountEntityFromCar(WorldArena, WorldMap, &ControlledEntity, &Vehicle);
						}
						{
							{
								//if(Entity->VisualType == EntityVisualType_CarFrame && 
								//	 Distance(Entity->P, ControlledEntity->P) < 4.0f)
								{
									//MountEntityOnCar(WorldArena, WorldMap, &ControlledEntity, &Entity);
									//break;
								}
							}
						}
					}
					{
						//entity CarFrame = GetEntityByLowIndex(Entities, ControlledEntity->RidingVehicle);
						//Assert(CarFrame->VisualType == EntityVisualType_CarFrame);

						if(InputDirection.X)
						{ 
							//TurnWheels(Entities, &CarFrame, InputDirection.XY, SecondsToUpdate); 
						}
						else
						{ 
							//AlignWheelsForward(Entities, &CarFrame, SecondsToUpdate); 
						}

						//if(Clicked(Keyboard, One))   { CarFrame->ddP = CarFrame->R * -3.0f; }
						//if(Clicked(Keyboard, Two))   { CarFrame->ddP = CarFrame->R *  0.0f; }
						//if(Clicked(Keyboard, Three)) { CarFrame->ddP = CarFrame->R *  3.0f; }
						//if(Clicked(Keyboard, Four))  { CarFrame->ddP = CarFrame->R *  6.0f; }
						//if(Clicked(Keyboard, Five))  { CarFrame->ddP = CarFrame->R *  9.0f; }
					}
#endif
				}
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
	stored_entity* StoredMainCamera = GetStoredEntityByIndex(Entities, 
																														 GameState->MainCameraStorageIndex);
	sim_region* SimRegion = BeginSim(Entities, &(GameState->SimArena), WorldMap, 
																	 StoredMainCamera->Sim.WorldP, GameState->CameraUpdateBounds, 
																	 SecondsToUpdate, GameState->MainCameraStorageIndex);


	//TODO(bjorn): Implement step 2 in J.Blows framerate independence video.
	// https://www.youtube.com/watch?v=fdAOPHgW7qM
	//TODO(bjorn): Add some asserts and some limits to velocities related to the
	//delta time so that tunneling becomes virtually impossible.
	u32 Steps = 8;
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
							//Assert(Penetration <= (Lenght(Entity->Dim) + Lenght(OtherEntity->Dim)) * 0.5f);

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
							ImpactCorrection *= Sum.Genus[RelevantNodeIndex] == MinkowskiGenus_Movable ? 1.0f:-1.0f;
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

					//TODO Is this a thing that we need to handle interactions?
#if 0
					simulation_request* OtherPlayerSimReq = 
						GetSimRequestForEntity(GameState->PlayerSimulationRequests, 
																	 ArrayCount(GameState->PlayerSimulationRequests),
																	 Entity->StorageIndex);
#endif

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
					if(GameState->DEBUG_VisualiseMinkowskiSum)
					{
						v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;
						m22 GameSpaceToScreenSpace = 
						{PixelsPerMeter, 0             ,
							0             ,-PixelsPerMeter};

						u32 Player1StorageIndex = GameState->KeyboardSimulationRequests[0].PlayerStorageIndex;
						entity* Player = 0;
						if(Entity->StorageIndex == Player1StorageIndex) { Player = Entity;}
						if(OtherEntity->StorageIndex == Player1StorageIndex) { Player = OtherEntity;}

						if(Player) 
						{
							entity* Target = Entity == Player ? OtherEntity : Entity;
							DEBUGMinkowskiSum(Buffer, Target, Player, GameSpaceToScreenSpace, ScreenCenter);
						}
#if 0 
						entity* CarFrame = GetEntityOfVisualType(EntityVisualType_CarFrame, Entity, OtherEntity);
						if(CarFrame &&
							 CarFrame->DriverSeat.Ptr &&
							 CarFrame->DriverSeat.Ptr->StorageIndex == Player1StorageIndex)
						{
							entity* Target = GetRemainingEntity(CarFrame, Entity, OtherEntity);
							DEBUGMinkowskiSum(Buffer, Target, CarFrame, GameSpaceToScreenSpace, ScreenCenter);
						}
#endif
					}
#endif
				}

				simulation_request* SimReq = 
					GetSimRequestForEntity(GameState->PlayerSimulationRequests, 
																 ArrayCount(GameState->PlayerSimulationRequests),
																 Entity->StorageIndex);
				v3 OldP = Entity->P;

				if(SimReq)
				{
					entity* Sword = Entity->Sword.Ptr;
					if(Sword &&
						 LenghtSquared(SimReq->FireSword))
					{

						MakeEntitySpacial(Sword, SimReq->FireSword,
															(Sword->IsSpacial ? 
															 Sword->P : 
															 (Entity->P + SimReq->FireSword * Sword->Dim.Y)),
															SimReq->FireSword * 8.0f);
						Sword->DistanceRemaining = 20.0f;
					}

					Entity->MoveSpec.MovingDirection = SimReq->ddP;
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
			// NOTE(bjorn): Rendering
			//
			if(Step == LastStep)
			{
				if(!Entity->IsSpacial) { continue; }

				v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

				v2 CollisionMarkerPixelDim = Hadamard(Entity->Dim.XY, {PixelsPerMeter, PixelsPerMeter});
				m22 GameSpaceToScreenSpace = 
				{PixelsPerMeter, 0             ,
					0             ,-PixelsPerMeter};

				v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * Entity->P.XY;
				v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
				f32 ZPixelOffset = PixelsPerMeter * -Entity->P.Z;

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

				if(Entity->VisualType == EntityVisualType_Stair)
				{
					v3 StairColor = {};
					if(Entity->dZ == 1)
					{
						v3 LightGreen = {0.5f, 1, 0.5f};
						StairColor = LightGreen;
					}
					if(Entity->dZ == -1)
					{
						v3 LightRed = {1, 0.5f, 0.5f};
						StairColor = LightRed;
					}
					DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), StairColor);
				}
				if(Entity->VisualType == EntityVisualType_Wall)
				{
#if 0
					v3 LightYellow = {0.5f, 0.5f, 0.0f};
					DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f), LightYellow);
#endif
					DrawBitmap(Buffer, &GameState->Rock, EntityPixelPos - GameState->Rock.Alignment, 
										 (v2)GameState->Rock.Dim);
				}
				if(Entity->VisualType == EntityVisualType_Ground)
				{
#if 1
					DrawBitmap(Buffer, &GameState->Dirt, EntityPixelPos - GameState->Dirt.Alignment, 
										 (v2)GameState->Dirt.Dim * 1.2f);
#else
					v3 LightBrown = {0.55f, 0.45f, 0.33f};
					DrawRectangle(Buffer, 
												RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f), LightBrown);
#endif
				}
				if(Entity->VisualType == EntityVisualType_Wheel ||
					 Entity->VisualType == EntityVisualType_CarFrame ||
					 Entity->VisualType == EntityVisualType_Engine)
				{
					v3 Color = {1, 1, 1};
					if(Entity->VisualType == EntityVisualType_Engine) { Color = {0, 1, 0}; }
					if(Entity->VisualType == EntityVisualType_Wheel) { Color = {0.2f, 0.2f, 0.2f}; }

					DrawFrame(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), 
										Entity->R.XY, Color);
					DrawLine(Buffer, EntityPixelPos, EntityPixelPos + 
									 Hadamard(Entity->R.XY, v2{1, -1}) * 40.0f, {1, 0, 0});
#if 0
					DrawBitmap(Buffer, &GameState->Dirt, EntityPixelPos - GameState->Dirt.Alignment, 
										 (v2)GameState->Dirt.Dim * 1.2f);
#endif
				}

				f32 ZAlpha = Clamp01(1.0f - (Entity->P.Z / 2.0f));
				if(Entity->VisualType == EntityVisualType_Player)
				{
					v3 Yellow = {1.0f, 1.0f, 0.0f};
					//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
										 (v2)GameState->Shadow.Dim, ZAlpha);
					DrawBitmap(Buffer, &Hero->Torso, 
										 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Torso.Alignment, 
										 (v2)Hero->Torso.Dim);
					DrawBitmap(Buffer, &Hero->Cape, 
										 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Cape.Alignment, 
										 (v2)Hero->Cape.Dim);
					DrawBitmap(Buffer, &Hero->Head, 
										 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Head.Alignment, 
										 (v2)Hero->Head.Dim);
				}
				if(Entity->VisualType == EntityVisualType_Monstar)
				{
					v3 Yellow = {1.0f, 1.0f, 0.0f};
					//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
										 (v2)GameState->Shadow.Dim, ZAlpha);
					DrawBitmap(Buffer, &Hero->Torso, EntityPixelPos - Hero->Torso.Alignment, 
										 (v2)Hero->Torso.Dim);
				}
				if(Entity->VisualType == EntityVisualType_Familiar)
				{
					v3 Yellow = {1.0f, 1.0f, 0.0f};
					//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity->FacingDirection]);

					DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
										 (v2)GameState->Shadow.Dim, 0.2f);
					DrawBitmap(Buffer, &Hero->Head, EntityPixelPos - Hero->Head.Alignment, 
										 (v2)Hero->Head.Dim);
				}
				if(Entity->VisualType == EntityVisualType_Sword)
				{
					DrawBitmap(Buffer, &GameState->Sword, EntityPixelPos - GameState->Sword.Alignment, 
										 (v2)GameState->Sword.Dim, 1.0f);
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

						v2 HitPointPixelPos = ScreenCenter + (GameSpaceToScreenSpace * HitPointPos);
						v2 HitPointPixelDim = Hadamard(HitPointDim, {PixelsPerMeter, PixelsPerMeter});

						hit_point HP = Entity->HitPoints[HitPointIndex];
						if(HP.FilledAmount == HIT_POINT_SUB_COUNT)
						{
							v3 Green = {0.0f, 1.0f, 0.0f};

							rectangle2 GreenRect = RectCenterDim(HitPointPixelPos, HitPointPixelDim);

							DrawRectangle(Buffer, GreenRect, Green);
						}
						else if(HP.FilledAmount < HIT_POINT_SUB_COUNT &&
										HP.FilledAmount > 0)
						{
							v3 Green = {0.0f, 1.0f, 0.0f};
							v3 Red = {1.0f, 0.0f, 0.0f};

							rectangle2 RedRect = RectCenterDim(HitPointPixelPos, HitPointPixelDim);
							v2 GreenRectMax = HitPointPixelPos + HitPointPixelDim * 0.5f;
							v2 GreenRectMin = HitPointPixelPos - HitPointPixelDim * 0.5f;
							GreenRectMin.Y += ((HitPointPixelDim.Y/HIT_POINT_SUB_COUNT) * 
																 (HIT_POINT_SUB_COUNT - HP.FilledAmount));
							rectangle2 GreenRect = RectMinMax(GreenRectMin, GreenRectMax);

							DrawRectangle(Buffer, RedRect, Red);
							DrawRectangle(Buffer, GreenRect, Green);
						}
						else
						{
							v3 Red = {1.0f, 0.0f, 0.0f};

							rectangle2 RedRect = RectCenterDim(HitPointPixelPos, HitPointPixelDim);

							DrawRectangle(Buffer, RedRect, Red);
						}
					}
				}

#if HANDMADE_INTERNAL
				if(GameState->DEBUG_VisualiseCollisionBox)
				{
					vertices Verts = GetEntityVertices(Entity);
					for(u32 VertIndex = 0; VertIndex < Verts.Count; VertIndex++)
					{
						v2 V0 = Verts.Verts[VertIndex].XY;
						v2 V1 = Verts.Verts[(VertIndex+1) % Verts.Count].XY;
						DrawLine(Buffer, 
										 ScreenCenter + GameSpaceToScreenSpace * V0, 
										 ScreenCenter + GameSpaceToScreenSpace * V1, 
										 {0, 0, 1});

						v2 WallNormal = Normalize(CCW90M22() * (V1 - V0));
						v3 NormalColor = {1, 0, 1};

						DrawLine(Buffer, 
										 ScreenCenter + GameSpaceToScreenSpace * (V0 + V1) * 0.5f, 
										 ScreenCenter + GameSpaceToScreenSpace * 
										 ((V0 + V1) * 0.5f + WallNormal * 0.2f), 
										 NormalColor);
					}
				}

				if(GameState->DEBUG_StepThroughTheCollisionLoop)
				{
					if(Entity == GameState->DEBUG_CollisionLoopEntity) 
					{
						DrawFrame(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), 
											Entity->R.XY, {1.0f, 0.0f, 0.0f});

						EntityCameraPixelDelta = 
							GameSpaceToScreenSpace * GameState->DEBUG_CollisionLoopEstimatedPos.XY;

						v2 NextEntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
						DrawFrame(Buffer, RectCenterDim(NextEntityPixelPos, CollisionMarkerPixelDim), 
											Entity->R.XY, {0.0f, 0.0f, 1.0f});
					}
				}
#endif
			}
		}
	}

	//
	// NOTE(bjorn): Update camera
	//
	//
	if(SimRegion->MainCamera->CameraTarget.Ptr)
	{
		SimRegion->MainCamera->P = SimRegion->MainCamera->CameraTarget.Ptr->P;
	}

	EndSim(Entities, WorldArena, SimRegion);
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
