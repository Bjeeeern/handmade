#include "platform.h"
#include "memory.h"
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
// * Solve weird entity storage artefact when jumping to the next level.
// * Rendering artefact making player not center in screen.

// TODO
// * Create a perspective camera that is rotatable.
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

	memory_arena SimArena;

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
                                                                                                    
	sim_region* SimRegion = BeginSim(Input, &GameState->Entities, &GameState->SimArena, WorldMap, 
																	 GetChunkPosFromAbsTile(WorldMap, RoomOrigin), 
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
		E[i] = AddWall(SimRegion, v3{2.0f + 2.0f*i, 2.0f, 0.0f} * WorldMap->TileSideInMeters, 10.0f + i);
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

	InitializeArena(&GameState->SimArena, Memory->TransientStorageSize >> 2, 
									(u8*)Memory->TransientStorage);

	memory_arena* WorldArena = &GameState->WorldArena;
	world_map* WorldMap = GameState->WorldMap;
	stored_entities* Entities = &GameState->Entities;

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

	sim_region* SimRegion = 0;
	{
		stored_entity* StoredMainCamera = GetStoredEntityByIndex(Entities, 
																														 GameState->MainCameraStorageIndex);
		SimRegion = BeginSim(Input, Entities, &(GameState->SimArena), WorldMap, 
												 StoredMainCamera->Sim.WorldP, GameState->CameraUpdateBounds, 
												 SecondsToUpdate);
	}

	//TODO(bjorn): Implement step 2 in J.Blows framerate independence video.
	// https://www.youtube.com/watch?v=fdAOPHgW7qM
	//TODO(bjorn): Add some asserts and some limits to velocities related to the
	//delta time so that tunneling becomes virtually impossible.
	u32 Steps = 8; 
	//NOTE(bjorn): Asserts at least one iteration to find the main camera entity pointer.
	Assert(Steps > 1);

	entity* MainCamera = 0;
	m33 RotMat = M33Identity();

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

				//TODO(bjorn): Remove this hack as soon as I have full 3D rotations
				//going on for all entites!!!
				m33 XRot = XRotationMatrix(MainCamera->CamRot.Y);
				RotMat = AxisRotationMatrix(MainCamera->CamRot.X, GetMatCol(XRot, 2)) * XRot;
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
#if 0 
							entity* CarFrame = GetEntityOfVisualType(EntityVisualType_CarFrame, 
																											 Entity, OtherEntity);
							if(CarFrame &&
								 CarFrame->DriverSeat &&
								 CarFrame->DriverSeat->StorageIndex == Player1StorageIndex)
							{
								entity* Target = GetRemainingEntity(CarFrame, Entity, OtherEntity);
								DEBUGMinkowskiSum(Buffer, Target, CarFrame, GameSpaceToScreenSpace, ScreenCenter);
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

						//TODO(bjorn): Remove this hack as soon as I have full 3D rotations
						//going on for all entites!!!
						v2 RotSpeed = v2{-1, 1} * (tau32 * SecondsToUpdate * 0.2f);

						//TODO Smoother rotation.
#if 0
						f32 MinimumHuntRangeSquared = Square(0.1f);
						Entity->TargetCamRot += Modulate0(ArrowKeysDirection * RotSpeed, tau32);
#endif
						Entity->dCamRot = Hadamard(ArrowKeysDirection, RotSpeed);
						Entity->CamRot = Modulate0(Entity->CamRot + Entity->dCamRot, tau32);
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
			// NOTE(bjorn): Rendering
			//
			if(Step == LastStep)
			{
				Assert(MainCamera);

				v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

				v3 CamP = RotMat * v3{1,0,0};

				v3 P = RotMat * (Entity->P - CamP);
				f32 de = 20.0f;
				f32 dp = 1.0f;
				f32 s = -P.Z + (de+dp);
				if(s < de*0.1) { continue; }
				f32 f = de/s;
				P.XY *= f;

				v2 CollisionMarkerPixelDim = Hadamard(Entity->Dim.XY * f, {PixelsPerMeter, PixelsPerMeter});
				m22 GameSpaceToScreenSpace = 
				{PixelsPerMeter, 0             ,
					0            ,-PixelsPerMeter};

				v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * P.XY;
				v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
				f32 ZPixelOffset = 0;//PixelsPerMeter * -Entity->P.Z;

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
					DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f),
												LightYellow);
#endif
					DrawBitmap(Buffer, &GameState->Rock, EntityPixelPos - GameState->Rock.Alignment, 
										 (v2)GameState->Rock.Dim * f);
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
						v2 HitPointPos = P.XY;
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

	EndSim(Input, Entities, WorldArena, SimRegion);
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
