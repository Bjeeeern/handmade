#include "platform.h"
#include "memory.h"
#include "intrinsics.h"
#include "world_map.h"
#include "random.h"
#include "renderer.h"
#include "resource.h"
#include "sim_region.h"
#include "entity.h"

// QUICK TODO
//
// Make it so that I can visually step through a frame of collision.
// generate world as you drive
// car engine that is settable by mouse click and drag
// collide with rocks
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

struct game_state
{
	memory_arena WorldArena;
	world_map* WorldMap;

	memory_arena SimArena;

	s32 RoomWidthInTiles;
	s32 RoomHeightInTiles;
	v3s RoomOrigin;

	//TODO(bjorn): Should we allow split-screen?
	u32 CameraFollowingPlayerIndex;
	world_map_position CameraP;

	u32 PlayerIndexForController[ArrayCount(((game_input*)0)->Keyboards)];
	u32 PlayerIndexForKeyboard[ArrayCount(((game_input*)0)->Controllers)];

	entities Entities;

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

	u32 DEBUG_CollisionLoopEntityIndex;
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

	u32 RandomNumberIndex = 0;

	GameState->RoomWidthInTiles = 17;
	GameState->RoomHeightInTiles = 9;

	u32 TilesPerWidth = GameState->RoomWidthInTiles;
	u32 TilesPerHeight = GameState->RoomHeightInTiles;

	GameState->RoomOrigin = (v3s)RoundV2ToV2S((v2)v2u{TilesPerWidth, TilesPerHeight} / 2.0f);

	GameState->CameraP = GetChunkPosFromAbsTile(WorldMap, GameState->RoomOrigin);

	entity Player = AddPlayer(GameState);
	GameState->PlayerIndexForKeyboard[0] = Player.LowIndex;
	AddCar(GameState, OffsetWorldPos(GameState->WorldMap, Player.Low->WorldP, {3.0f, 7.0f, 0}));

	AddMonstar(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{ 2, 5, 0}));
	AddFamiliar(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{ 4, 5, 0}));

	entity A = AddWall(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{2, 4, 0}), 10.0f);
	entity B = AddWall(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{5, 4, 0}),  5.0f);

	//TODO(bjorn): Have an object stuck to the mouse.
	//GameState->DEBUG_EntityBoundToMouse = A.LowIndex;

	entity E[6];
	for(u32 i=0;
			i < 6;
			i++)
	{
		E[i] = AddWall(GameState, GetChunkPosFromAbsTile(WorldMap, v3u{2 + 2*i, 2, 0}), 10.0f + i);
	}

	entities* Entities = &GameState->Entities;
	SetCamera(WorldMap, Entities, &GameState->CameraP, &Player.Low->WorldP);

	A = GetEntityByLowIndex(Entities, A.LowIndex);
	B = GetEntityByLowIndex(Entities, B.LowIndex);

	A.High->dP = {1, 0, 0};
	B.High->dP = {-1, 0, 0};
	for(u32 i=0;
			i < 6;
			i++)
	{
		E[i] = GetEntityByLowIndex(Entities, E[i].LowIndex);
		E[i].High->dP = {2.0f-i, -2.0f+i};
	}

	u32 ScreenX = 0;
	u32 ScreenY = 0;
	u32 ScreenZ = 0;
	for(u32 TileY = 0;
			TileY < TilesPerHeight;
			++TileY)
	{
		for(u32 TileX = 0;
				TileX < TilesPerWidth;
				++TileX)
		{
			v3u AbsTile = {
									 ScreenX * TilesPerWidth + TileX, 
									 ScreenY * TilesPerHeight + TileY, 
									 ScreenZ};

			u32 TileValue = 1;
			if((TileX == 0))
			{
				TileValue = 2;
			}
			if((TileX == (TilesPerWidth-1)))
			{
				TileValue = 2;
			}
			if((TileY == 0))
			{
				TileValue = 2;
			}
			if((TileY == (TilesPerHeight-1)))
			{
				TileValue = 2;
			}

			world_map_position WorldPos = GetChunkPosFromAbsTile(WorldMap, AbsTile);
			if((TileY != TilesPerHeight && TileX != TilesPerWidth/2) && TileValue ==  2)
			{
				entity Wall = AddWall(GameState, WorldPos);
			}
		}
	}
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert( (&GetController(Input, 0)->Struct_Terminator - 
					 &GetController(Input, 0)->Buttons[0]) ==
					ArrayCount(GetController(Input, 0)->Buttons)
				);
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
	entities* Entities = &GameState->Entities;

	for(s32 ControllerIndex = 0;
			ControllerIndex < ArrayCount(Input->Controllers);
			ControllerIndex++)
	{
		game_controller* Controller = GetController(Input, ControllerIndex);
		if(Controller->IsConnected)
		{
			u32 ControlledEntityIndex = GameState->PlayerIndexForController[ControllerIndex];
			entity ControlledEntity = GetEntityByLowIndex(Entities, ControlledEntityIndex);

			if(ControlledEntity.Low)
			{
				v2 InputDirection = {};

				InputDirection = Controller->LeftStick.End;

				if(ControlledEntity.High)
				{
					ControlledEntity.High->ddP = InputDirection * 85.0f;
				}

				if(Clicked(Controller, Start))
				{
					//TODO(bjorn) Implement RemoveEntity();
					//GameState->EntityResidencies[ControlledEntityIndex] = EntityResidence_Nonexistent;
					//GameState->PlayerIndexForController[ControllerIndex] = 0;
				}
			}
			else
			for(LoopOverHighEntitiesNamed(OtherEntity))
			{
				if(Clicked(Controller, Start))
				{
					GameState->PlayerIndexForController[ControllerIndex] = AddPlayer(GameState).LowIndex;
				}
			}
		}
	}

	for(s32 KeyboardIndex = 0;
			KeyboardIndex < ArrayCount(Input->Keyboards);
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

				//TODO(bjorn): Just setting the flag is not working anymore.
				//Memory->IsInitialized = false;

				GameState->DEBUG_StepThroughTheCollisionLoop = 
					!GameState->DEBUG_StepThroughTheCollisionLoop;
				GameState->DEBUG_CollisionLoopEntityIndex = 1;
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
			}

			if(Clicked(Keyboard, C))
			{
				GameState->DEBUG_VisualiseCollisionBox = !GameState->DEBUG_VisualiseCollisionBox;
			}

			u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[KeyboardIndex];
			stored_entity ControlledEntity = GetStoredEntityByIndex(Entities, ControlledEntityIndex);

			if(ControlledEntity)
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

				if(ControlledEntity->Type == EntityType_Player)
				{
					if(Clicked(Keyboard, E))
					{
						//if(ControlledEntity.Low->RidingVehicle)
						{
							//entity Vehicle = GetEntityByLowIndex(Entities, 
							//																		 ControlledEntity.Low->RidingVehicle);
							//DismountEntityFromCar(WorldArena, WorldMap, &ControlledEntity, &Vehicle);
						}
						else
						{
							{
								//if(Entity.Low->Type == EntityType_CarFrame && 
								//	 Distance(Entity.High->P, ControlledEntity.High->P) < 4.0f)
								{
									//MountEntityOnCar(WorldArena, WorldMap, &ControlledEntity, &Entity);
									//break;
								}
							}
						}
					}
					if(ControlledEntity->RidingVehicle)
					{
						//entity CarFrame = GetEntityByLowIndex(Entities, ControlledEntity->RidingVehicle);
						//Assert(CarFrame.Low->Type == EntityType_CarFrame);

						if(InputDirection.X)
						{ 
							//TurnWheels(Entities, &CarFrame, InputDirection.XY, SecondsToUpdate); 
						}
						else
						{ 
							//AlignWheelsForward(Entities, &CarFrame, SecondsToUpdate); 
						}

						//if(Clicked(Keyboard, One))   { CarFrame->ddP = CarFrame.Low->R * -3.0f; }
						//if(Clicked(Keyboard, Two))   { CarFrame->ddP = CarFrame.Low->R *  0.0f; }
						//if(Clicked(Keyboard, Three)) { CarFrame->ddP = CarFrame.Low->R *  3.0f; }
						//if(Clicked(Keyboard, Four))  { CarFrame->ddP = CarFrame.Low->R *  6.0f; }
						//if(Clicked(Keyboard, Five))  { CarFrame->ddP = CarFrame.Low->R *  9.0f; }
					}
					else
					{
						ControlledEntity->MovingDirection = InputDirection;
						if(Clicked(Keyboard, N))
						{
						}
					}
				}
			}
		}
	}

	//
	// NOTE(bjorn): Update camera
	//
	stored_entity CameraTarget = GetEntityByLowIndex(Entities, GameState->CameraFollowingPlayerIndex);
	if(CameraTarget)
	{
		world_map_position NewCameraP = CameraTarget.Low->WorldP;
		NewCameraP.Offset_.Z = 0;
		GameState->CameraP = NewCameraP;
	}

	v3 HighFrequencyUpdateDim = v3{2.0f, 2.0f, 2.0f}*WorldMap->ChunkSideInMeters;
	rectangle3 CameraUpdateBounds = RectCenterDim(v3{0,0,0}, HighFrequencyUpdateDim);

	sim_region* SimRegion = BeginSim(Entities, &(GameState->SimArena), WorldMap, 
																	 &(GameState->CameraP), CameraUpdateBounds);

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

	//TODO(bjorn): Implement step 2 in J.Blows framerate independence video.
	// https://www.youtube.com/watch?v=fdAOPHgW7qM
	//TODO(bjorn): Add some asserts and some limits to velocities related to the
	//delta time so that tunneling becomes virtually impossible.
	s32 Steps = 8;
	f32 dT = SecondsToUpdate / (f32)Steps;
	for(s32 Step = 0;
			Step < Steps;
			Step++)
	{
		sim_entity* Entity = SimRegion->Entities;
		for(u32 EntityIndex = 0;
				EntityIndex < SimRegion->EntityCount;
				EntityIndex++, Entity++)
		{
			//
			// NOTE(bjorn): Moving / Collision / Game Logic
			//

			//TODO(bjorn):
			// Add negative gravity for penetration if relative velocity is >= 0.
			// Get relevant contact point.
			// Do impulse calculation.
			// Test with object on mouse.
			sim_entity* OtherEntity = SimRegion->Entities;
			for(u32 OtherEntityIndex = 0;
					OtherEntityIndex < SimRegion->EntityCount;
					OtherEntityIndex++, OtherEntity++)
			{
				if(Entity.LowIndex == OtherEntity.LowIndex) { continue; }
				if(OtherEntity.High->CollisionDirtyBit) { continue; }

				b32 Inside = true; 
				f32 BestSquareDistanceToWall = positive_infinity32;
				s32 RelevantNodeIndex = -1;
				v2 P = Entity.High->P.XY;
				polygon Sum = MinkowskiSum(&OtherEntity, &Entity);
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
					 Entity.Low->Collides && 
					 OtherEntity.Low->Collides)
				{
					Assert(RelevantNodeIndex >= 0);

					f32 Penetration;
					v2 n, t, ImpactPoint;
					{
						Penetration	= SquareRoot(BestSquareDistanceToWall);
						Assert(Penetration <= (Lenght(Entity.Low->Dim) + Lenght(OtherEntity.Low->Dim)) * 0.5f);

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
					v2 AP = Entity.High->P.XY;
					v2 BP = OtherEntity.High->P.XY;
					v2 AIt = CW90M22() * (AP - ImpactPoint);
					v2 BIt = CW90M22() * (BP - ImpactPoint);
					v2 AdP = Entity.High->dP.XY      + Entity.High->dA      * AIt;
					v2 BdP = OtherEntity.High->dP.XY + OtherEntity.High->dA * BIt;
					v2 ABdP = AdP - BdP;
					f32 ndotAB = Dot(n, ABdP);

					f32 AInvMass = GetInverseOrZero(Entity.Low->Mass);
					f32 BInvMass = GetInverseOrZero(OtherEntity.Low->Mass);
					//TODO(bjorn): Automize inertia calculation.
					f32 AInvMoI  = AInvMass ? 0.08f:0.0f;//GetInverseOrZero(Entity->High->MoI);
					f32 BInvMoI  = BInvMass ? 0.08f:0.0f;//GetInverseOrZero(OtherEntity->High->MoI);

					f32 Impulse = 0;
					//f32 InverseGravity = Penetration * 60.0f;
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

					//Impulse = Max(j, InverseGravity);

					Entity.High->dP.XY      += (Impulse * AInvMass) * n;
					OtherEntity.High->dP.XY -= (Impulse * BInvMass) * n;
					Entity.High->dA         += Dot(AIt, Impulse * n) * AInvMoI;
					OtherEntity.High->dA    -= Dot(BIt, Impulse * n) * BInvMoI;

					Entity.High->P.XY       += n * Penetration * 0.5f;
					OtherEntity.High->P.XY  -= n * Penetration * 0.5f;
				} 

				//if(Step == 0)
				{
					entity* Familiar = GetEntityOfType(EntityType_Familiar, &Entity, &OtherEntity);
					if(Familiar)
					{
						entity* Target = GetRemainingEntity(Familiar, &Entity, &OtherEntity);
						UpdateFamiliarPairwise(Familiar, Target);
					}

					entity* Sword = GetEntityOfType(EntityType_Sword, &Entity, &OtherEntity);
					if(Sword)
					{
						entity* Target = GetRemainingEntity(Sword, &Entity, &OtherEntity);
						UpdateSwordPairwise(Sword, Target, Inside, WorldArena, WorldMap, Entities);
					}
				}
			}
			Entity.High->CollisionDirtyBit = true;

			{
				v3 OldP = {};

				if(Entity.Low->Type == EntityType_Sword)
				{
					OldP = Entity.High->P;
				}

				MoveEntity(WorldArena, WorldMap, &GameState->CameraP, &Entity, dT);

				if(Entity.Low->Type == EntityType_Sword)
				{
					v3 NewP = Entity.High->P;

					Entity.Low->DistanceRemaining -= Lenght(NewP - OldP);
					if(Entity.Low->DistanceRemaining <= 0)
					{
						entity* Sword = &Entity;
						ChangeEntityWorldLocation(WorldArena, WorldMap, Sword, 0);
						MapEntityOutFromHigh(Entities, Sword->LowIndex, Sword->Low->WorldP);
					}

					//TODO IMPORTANT Pass down data from the input gathering step as appropiate.
							stored_entity* Sword = GetEntityByLowIndex(Entities, ControlledEntity->Sword);
							if(Sword && 
								 !IsValid(Sword->WorldP) && 
								 LenghtSquared(InputDirection))
							{
								Sword->R = InputDirection;
								ChangeStoredEntityWorldLocationRelativeOther(&GameState->WorldArena, 
																														 GameState->WorldMap, 
																														 &Sword,
																														 ControlledEntity->WorldP, 
																														 InputDirection * Sword->Dim.Y);

								Sword->dP = Sword->R * 8.0f;
								Sword->DistanceRemaining = 20.0f;
							}
				}

				if(Entity.Low->Type == EntityType_Familiar)
				{
					Entity.High->BestDistanceToPlayerSquared = Square(6.0f);
					Entity.High->MovingDirection = {};
				}
			}

			if(Step == (Steps-1))
			{
				//
				// NOTE(bjorn): Rendering
				//
				v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

				if(Entity.Low->WorldP.ChunkP.Z != GameState->CameraP.ChunkP.Z) { continue; }

				v2 CollisionMarkerPixelDim = Hadamard(Entity.Low->Dim.XY, {PixelsPerMeter, PixelsPerMeter});
				m22 GameSpaceToScreenSpace = 
				{PixelsPerMeter, 0             ,
					0             ,-PixelsPerMeter};

				v2 EntityCameraPixelDelta = GameSpaceToScreenSpace * Entity.High->P.XY;
				v2 EntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
				f32 ZPixelOffset = PixelsPerMeter * -Entity.High->P.Z;

				if(LenghtSquared(Entity.High->dP) != 0.0f)
				{
					if(Absolute(Entity.High->dP.X) > Absolute(Entity.High->dP.Y))
					{
						Entity.High->FacingDirection = (Entity.High->dP.X > 0) ? 3 : 1;
					}
					else
					{
						Entity.High->FacingDirection = (Entity.High->dP.Y > 0) ? 2 : 0;
					}
				}

				if(Entity.Low->Type == EntityType_Stair)
				{
					v3 StairColor = {};
					if(Entity.Low->dZ == 1)
					{
						v3 LightGreen = {0.5f, 1, 0.5f};
						StairColor = LightGreen;
					}
					if(Entity.Low->dZ == -1)
					{
						v3 LightRed = {1, 0.5f, 0.5f};
						StairColor = LightRed;
					}
					DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), StairColor);
				}
				if(Entity.Low->Type == EntityType_Wall)
				{
#if 0
					v3 LightYellow = {0.5f, 0.5f, 0.0f};
					DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim * 0.9f), LightYellow);
#endif
					DrawBitmap(Buffer, &GameState->Rock, EntityPixelPos - GameState->Rock.Alignment, 
										 (v2)GameState->Rock.Dim);
				}
				if(Entity.Low->Type == EntityType_Ground)
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
				if(Entity.Low->Type == EntityType_Wheel ||
					 Entity.Low->Type == EntityType_CarFrame ||
					 Entity.Low->Type == EntityType_Engine)
				{
					v3 Color = {1, 1, 1};
					if(Entity.Low->Type == EntityType_Engine) { Color = {0, 1, 0}; }
					if(Entity.Low->Type == EntityType_Wheel) { Color = {0.2f, 0.2f, 0.2f}; }

					DrawFrame(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), 
										Entity.Low->R.XY, Color);
					DrawLine(Buffer, EntityPixelPos, EntityPixelPos + 
									 Hadamard(Entity.Low->R.XY, v2{1, -1}) * 40.0f, {1, 0, 0});
#if 0
					DrawBitmap(Buffer, &GameState->Dirt, EntityPixelPos - GameState->Dirt.Alignment, 
										 (v2)GameState->Dirt.Dim * 1.2f);
#endif
				}

				f32 ZAlpha = Clamp(1.0f - (Entity.High->P.Z / 2.0f), 0.0f, 1.0f);
				if(Entity.Low->Type == EntityType_Player)
				{
					v3 Yellow = {1.0f, 1.0f, 0.0f};
					//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

					DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
										 (v2)GameState->Shadow.Dim, ZAlpha);
					DrawBitmap(Buffer, &Hero->Torso, 
										 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Torso.Alignment, (v2)Hero->Torso.Dim);
					DrawBitmap(Buffer, &Hero->Cape, 
										 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Cape.Alignment, (v2)Hero->Cape.Dim);
					DrawBitmap(Buffer, &Hero->Head, 
										 EntityPixelPos + v2{0, ZPixelOffset} - Hero->Head.Alignment, (v2)Hero->Head.Dim);
				}
				if(Entity.Low->Type == EntityType_Monstar)
				{
					v3 Yellow = {1.0f, 1.0f, 0.0f};
					//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

					DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
										 (v2)GameState->Shadow.Dim, ZAlpha);
					DrawBitmap(Buffer, &Hero->Torso, EntityPixelPos - Hero->Torso.Alignment, 
										 (v2)Hero->Torso.Dim);
				}
				if(Entity.Low->Type == EntityType_Familiar)
				{
					v3 Yellow = {1.0f, 1.0f, 0.0f};
					//DrawRectangle(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), Yellow);

					hero_bitmaps *Hero = &(GameState->HeroBitmaps[Entity.High->FacingDirection]);

					DrawBitmap(Buffer, &GameState->Shadow, EntityPixelPos - GameState->Shadow.Alignment, 
										 (v2)GameState->Shadow.Dim, 0.2f);
					DrawBitmap(Buffer, &Hero->Head, EntityPixelPos - Hero->Head.Alignment, 
										 (v2)Hero->Head.Dim);
				}
				if(Entity.Low->Type == EntityType_Sword)
				{
					DrawBitmap(Buffer, &GameState->Sword, EntityPixelPos - GameState->Sword.Alignment, 
										 (v2)GameState->Sword.Dim, 1.0f);
				}

				if(Entity.Low->Type == EntityType_Monstar ||
					 Entity.Low->Type == EntityType_Player)
				{
					for(u32 HitPointIndex = 0;
							HitPointIndex < Entity.Low->HitPointMax;
							HitPointIndex++)
					{
						v2 HitPointDim = {0.15f, 0.3f};
						v2 HitPointPos = Entity.High->P.XY;
						HitPointPos.Y -= 0.3f;
						HitPointPos.X += ((HitPointIndex - (Entity.Low->HitPointMax-1) * 0.5f) * 
															HitPointDim.X * 1.5f);

						v2 HitPointPixelPos = ScreenCenter + (GameSpaceToScreenSpace * HitPointPos);
						v2 HitPointPixelDim = Hadamard(HitPointDim, {PixelsPerMeter, PixelsPerMeter});

						hit_point HP = Entity.Low->HitPoints[HitPointIndex];
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
					vertices Verts = GetEntityVertices(&Entity);
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
										 ScreenCenter + GameSpaceToScreenSpace * ((V0 + V1) * 0.5f + WallNormal * 0.2f), 
										 NormalColor);
					}
				}

				if(GameState->DEBUG_VisualiseMinkowskiSum)
				{
					u32 ControlledEntityIndex = GameState->PlayerIndexForKeyboard[0];
					entity Player = GetEntityByLowIndex(Entities, ControlledEntityIndex);
					entity Vehicle = GetEntityByLowIndex(Entities, Player.Low->RidingVehicle);

					if(Entity.Low->Collides)
					{
						if(Vehicle.Low)
						{
							if(Entity.LowIndex != Vehicle.LowIndex &&
								 Entity.LowIndex != Player.LowIndex)
							{
								DEBUGMinkowskiSum(Buffer, &Entity, &Vehicle, GameSpaceToScreenSpace, ScreenCenter);
							}
						}
						else
						{
							if(Entity.LowIndex != Player.LowIndex)
							{
								DEBUGMinkowskiSum(Buffer, &Entity, &Player, GameSpaceToScreenSpace, ScreenCenter);
							}
						}
					}
				}

				if(GameState->DEBUG_StepThroughTheCollisionLoop)
				{
					if(Entity.Low->HighIndex == GameState->DEBUG_CollisionLoopEntityIndex) 
					{
						DrawFrame(Buffer, RectCenterDim(EntityPixelPos, CollisionMarkerPixelDim), 
											Entity.Low->R.XY, {1.0f, 0.0f, 0.0f});

						EntityCameraPixelDelta = 
							GameSpaceToScreenSpace * GameState->DEBUG_CollisionLoopEstimatedPos.XY;

						v2 NextEntityPixelPos = ScreenCenter + EntityCameraPixelDelta;
						DrawFrame(Buffer, RectCenterDim(NextEntityPixelPos, CollisionMarkerPixelDim), 
											Entity.Low->R.XY, {0.0f, 0.0f, 1.0f});
					}
				}
#endif
			}
		}
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
