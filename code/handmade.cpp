#include "platform.h"
#include "memory.h"
#include "intrinsics.h"
#include "tile_map.h"
#include "random.h"
#include "renderer.h"
#include "resource.h"

//
// NOTE(bjorn): These structs only have a meaning within the game.
// Pi32
struct world
{
	tile_map *TileMap;
};

struct hero_bitmaps
{
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;

	v2s Alignment;
};

struct entity
{
	b32 Exists;

	tile_map_position Pos;
	v2 Vel;
	u32 FacingDirection;

	b32 IsOnStairs;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	s32 RoomWidthInTiles;
	s32 RoomHeightInTiles;

	//TODO(bjorn): Should we allow split-screen?
	u32 CameraFollowingPlayerIndex;
	tile_map_position CameraPosition;

	u32 PlayerIndexForController[ArrayCount(((game_input*)0)->Keyboards)];
	u32 PlayerIndexForKeyboard[ArrayCount(((game_input*)0)->Controllers)];

	u32 EntityCount;
	entity Entities[256];

	loaded_bitmap Backdrop;

	hero_bitmaps HeroBitmaps[4];
};

	inline entity *
GetEntity(game_state *GameState, s32 EntityIndex)
{
	entity* Result = {};

	if(0 < EntityIndex && EntityIndex < ArrayCount(GameState->Entities))
	{
		Result = &GameState->Entities[EntityIndex];
	}

	return Result;
}

struct new_entity_result
{
	entity* Entity;
	u32 EntityIndex;
};
internal_function new_entity_result
AddEntity(game_state* GameState)
{
	new_entity_result Result = {};

	GameState->EntityCount++;
	Assert(GameState->EntityCount < ArrayCount(GameState->Entities));

	Result.EntityIndex = GameState->EntityCount;
	Result.Entity = GetEntity(GameState, Result.EntityIndex);

	return Result;
}

internal_function void
InitializePlayer(entity* Entity)
{
	*Entity = {};

	Entity->Exists = true;

	Entity->Pos.AbsTile = {7, 5, 0};
	Entity->Pos.Offset = {0.0f, 0.0f};
}

	internal_function void
InitializeGame(game_memory *Memory, game_state *GameState)
{
	GameState->Backdrop = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
																		 "data/test/test_background.bmp");

	hero_bitmaps Hero = {};
	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_front_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_front_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[0] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_left_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[1] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_back_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[2] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_right_torso.bmp");
	Hero.Alignment = {72, 182};
	GameState->HeroBitmaps[3] = Hero;

	new_entity_result PlayerEntityInfo = AddEntity(GameState);
	InitializePlayer(PlayerEntityInfo.Entity);

	GameState->CameraFollowingPlayerIndex = PlayerEntityInfo.EntityIndex;
	GameState->PlayerIndexForKeyboard[0] = PlayerEntityInfo.EntityIndex;

	InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
									(u8*)Memory->PermanentStorage + sizeof(game_state));

	GameState->World = PushStruct(&GameState->WorldArena, world);
	world *World = GameState->World;
	World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

	tile_map *TileMap = World->TileMap;
	TileMap->ChunkShift = 4;
	TileMap->ChunkDim = (1 << TileMap->ChunkShift);
	TileMap->ChunkMask = TileMap->ChunkDim - 1;

	TileMap->TileSideInMeters = 1.4f;

	TileMap->ChunkCount = {128, 128, 2};

	TileMap->TileChunks = PushArray(&GameState->WorldArena, 
																	TileMap->ChunkCount.X*
																	TileMap->ChunkCount.Y*
																	TileMap->ChunkCount.Z, tile_chunk);

	u32 RandomNumberIndex = 0;

	GameState->RoomWidthInTiles = 17;
	GameState->RoomHeightInTiles = 9;
	u32 TilesPerWidth = GameState->RoomWidthInTiles;
	u32 TilesPerHeight = GameState->RoomHeightInTiles;

	u32 ScreenX = 0;
	u32 ScreenY = 0;
	u32 ScreenZ = 0;

	b32 DoorLeft = false;
	b32 DoorRight = false;
	b32 DoorUp = false;
	b32 DoorDown = false;

	b32 BuildStair = false;
	b32 BuildPause = false;

	s32 BuildDirection = 0;
	s32 StairToBuild = 1;


	for(u32 ScreenIndex = 0;
			ScreenIndex < 100;
			++ScreenIndex)
	{
		//TODO(bjorn): Implement a random number generator!
		Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
		u32 RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;

		if(RandomChoice == 2)
		{
			if(BuildPause)
			{
				//NOTE(bjorn): Do nothing and re-roll.

				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
			}
			else
			{
				BuildStair = true;
				if(ScreenZ == 0)
				{
					StairToBuild = 3;
				}
				else if(ScreenZ == 1)
				{
					StairToBuild = 4;
				}
			}
		}
		if(RandomChoice == 1)
		{
			DoorUp = true;
		}
		if(RandomChoice == 0)
		{
			DoorRight = true;
		}

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
					if(TileY == (TilesPerHeight/2) &&
						 DoorLeft)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileX == (TilesPerWidth-1)))
				{
					if(TileY == (TilesPerHeight/2) &&
						 DoorRight)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileY == 0))
				{
					if(TileX == (TilesPerWidth/2) &&
						 DoorDown)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileY == (TilesPerHeight-1)))
				{
					if(TileX == (TilesPerWidth/2) &&
						 DoorUp)
					{
						TileValue = 1;
					}
					else
					{
						TileValue = 2;
					}
				}
				if((TileX == (TilesPerWidth/2)) &&
					 (TileY == (TilesPerHeight/2)))
				{
					TileValue = StairToBuild;
				}

				SetTileValue(&GameState->WorldArena, World->TileMap, AbsTile, TileValue);
			}
		}

		DoorLeft = DoorRight;
		DoorDown = DoorUp;

		DoorRight = false;
		DoorUp = false;

		if(BuildPause)
		{
			BuildPause = false;
			BuildStair = false;
			StairToBuild = 1;
		}

		if(RandomChoice == 2)
		{
			if(!BuildPause)
			{
				if(ScreenZ == 0)
				{
					ScreenZ = 1;
				}
				else if(ScreenZ == 1)
				{
					ScreenZ = 0;
				}

				BuildPause = true;

				if(ScreenZ == 0)
				{
					StairToBuild = 3;
				}
				else if(ScreenZ == 1)
				{
					StairToBuild = 4;
				}
			}
		}
		else if(RandomChoice == 1)
		{
			++ScreenY;
		}
		else
		{
			++ScreenX;
		} 
	}
}

internal_function v2 
ClosestPointInRectangle(v2 MinCorner, v2 MaxCorner, tile_map_diff RelNewPos)
{
	v2 Result = {};
	Result.X = Clamp(RelNewPos.MeterDiff.X, MinCorner.X, MaxCorner.X);
	Result.Y = Clamp(RelNewPos.MeterDiff.Y, MinCorner.Y, MaxCorner.Y);

	return Result;
}

internal_function void
MoveEntity(entity* Entity, v2 InputDirection, f32 SecondsToUpdate, tile_map* TileMap)
{
	tile_map_position OldPos = Entity->Pos;

	v2 Acceleration = InputDirection * 85.0f;
	//TODO(casey): ODE here!
	Acceleration += -8.5f * Entity->Vel;

	tile_map_position NewPos = Entity->Pos;
	v2 PlayerDelta = (0.5f * Acceleration * Square(SecondsToUpdate)
										+ (Entity->Vel * SecondsToUpdate));
	NewPos.Offset += PlayerDelta;

	Entity->Vel = (Acceleration * SecondsToUpdate) + Entity->Vel;

	NewPos = RecanonilizePosition(TileMap, NewPos);

	//
	// NOTE(bjorn): Collision check after movement
	//
	u32 MinTileX = Min(OldPos.AbsTile.X, NewPos.AbsTile.X);
	u32 MinTileY = Min(OldPos.AbsTile.Y, NewPos.AbsTile.Y);
	u32 OneAfterMaxTileX = Max(OldPos.AbsTile.X, NewPos.AbsTile.X) + 1; 
	u32 OneAfterMaxTileY = Max(OldPos.AbsTile.Y, NewPos.AbsTile.Y) + 1; 
	u32 AbsTileZ = NewPos.AbsTile.Z;
	tile_map_position BestPos = NewPos;
	f32 BestDistanceSquared = positive_infinity32;
	for(u32 AbsTileY = MinTileY;
			AbsTileY != OneAfterMaxTileY;
			AbsTileY++)
	{
		for(u32 AbsTileX = MinTileX;
				AbsTileX != OneAfterMaxTileX;
				AbsTileX++)
		{
			u32 TileValue = GetTileValue(TileMap, v3u{AbsTileX, AbsTileY, AbsTileZ});
			if(IsTileValueEmpty(TileValue))
			{
				tile_map_position TestTilePos = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
				tile_map_diff RelNewPos = GetTileMapPosDifference(TileMap, 
																																NewPos, 
																																TestTilePos);

				v2 MinCorner = -0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
				v2 MaxCorner = 0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
				TestTilePos.Offset = ClosestPointInRectangle(MinCorner, MaxCorner, RelNewPos);

				tile_map_diff RelTestTilePlayerPos = GetTileMapPosDifference(TileMap, 
																																		 NewPos, 
																																		 TestTilePos);
				f32 TestDistanceSquared = LenghtSquared(RelTestTilePlayerPos.MeterDiff.XY);
				if(TestDistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = TestDistanceSquared;
					BestPos = TestTilePos;
				}
			}
		}
	}

	Entity->Pos = BestPos;

	u32 CurrentTile = GetTileValue(TileMap, Entity->Pos);
	if((CurrentTile == 3) ||
		 (CurrentTile == 4))
	{
		if(!Entity->IsOnStairs)
		{
			if(CurrentTile == 3)
			{
				Entity->Pos.AbsTile.Z += 1;
			}
			else if(CurrentTile == 4)
			{
				Entity->Pos.AbsTile.Z -= 1;
			}
			Entity->IsOnStairs = true;
		}
	}
	else
	{
		Entity->IsOnStairs = false;
	}

	f32 Threshold = 0.0001f;
	v2s FaceDir = {};

	if(Entity->Vel.X < -Threshold)
	{
		FaceDir.X = -1;
	}
	else if(Entity->Vel.X > Threshold)
	{
		FaceDir.X = 1;
	}
	else
	{
		FaceDir.X = 0;
	}

	if(Entity->Vel.Y < -Threshold)
	{
		FaceDir.Y = -1;
	}
	else if(Entity->Vel.Y > Threshold)
	{
		FaceDir.Y = 1;
	}
	else
	{
		FaceDir.Y = 0;
	}

	if(!FaceDir.X != !FaceDir.Y)
	{
		if(FaceDir.X < 0)
		{
			Entity->FacingDirection = 1;
		}
		if(FaceDir.X > 0)
		{
			Entity->FacingDirection = 3;
		}
		if(FaceDir.Y > 0)
		{
			Entity->FacingDirection = 2;
		}
		if(FaceDir.Y < 0)
		{
			Entity->FacingDirection = 0;
		}
	}

}

	internal_function tile_map_position 
GetCenterOfRoom(tile_map_position A, s32 RoomWidthInTiles, s32 RoomHeightInTiles)
{
	tile_map_position Result = {};

	Result.AbsTile.X = 
		(A.AbsTile.X / RoomWidthInTiles) * RoomWidthInTiles + FloorF32ToS32(RoomWidthInTiles*0.5f);
	Result.AbsTile.Y = 
		(A.AbsTile.Y / RoomHeightInTiles) * RoomHeightInTiles + FloorF32ToS32(RoomHeightInTiles*0.5f);
	Result.AbsTile.Z = A.AbsTile.Z;

	return Result;
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

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	f32 PlayerWidth = (f32)TileMap->TileSideInMeters * 0.75f;

	//
	// NOTE(bjorn): Movement
	//
	for(s32 ControllerIndex = 0;
			ControllerIndex < ArrayCount(Input->Controllers);
			ControllerIndex++)
	{
		game_controller* Controller = GetController(Input, ControllerIndex);
		if(Controller->IsConnected)
		{
			entity* ControllingEntity = GetEntity(GameState, 
																						GameState->PlayerIndexForController[ControllerIndex]);
			if(ControllingEntity && ControllingEntity->Exists)
			{
				v2 InputDirection = {};

				InputDirection = Controller->LeftStick.End;

				MoveEntity(ControllingEntity, InputDirection, SecondsToUpdate, TileMap);

				if(Controller->Start.EndedDown && Controller->Start.HalfTransitionCount)
				{
					ControllingEntity->Exists = false;
					GameState->PlayerIndexForController[ControllerIndex] = 0;
				}
			}
			else
			{
				if(Controller->Start.EndedDown && Controller->Start.HalfTransitionCount)
				{
					new_entity_result NewEntityInfo = AddEntity(GameState);

					if(NewEntityInfo.Entity)
					{
						GameState->PlayerIndexForController[ControllerIndex] = NewEntityInfo.EntityIndex;
						InitializePlayer(NewEntityInfo.Entity);
					}
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
			entity* ControllingEntity = GetEntity(GameState, 
																						GameState->PlayerIndexForKeyboard[KeyboardIndex]);
			if(ControllingEntity && ControllingEntity->Exists)
			{
				v2 InputDirection = {};

				if(Keyboard->S.EndedDown)
				{
					InputDirection.Y += -1;
				}
				if(Keyboard->A.EndedDown)
				{
					InputDirection.X += -1;
				}
				if(Keyboard->W.EndedDown)
				{
					InputDirection.Y += 1;
				}
				if(Keyboard->D.EndedDown)
				{
					InputDirection.X += 1;
				}

				if(InputDirection.X && InputDirection.Y)
				{
					InputDirection *= invroot2;
				}

				MoveEntity(ControllingEntity, InputDirection, SecondsToUpdate, TileMap);
			}
		}
	}


	//
	// NOTE(bjorn): Update camera/Z-position
	//
	entity* CameraFollowingEntity = GetEntity(GameState, GameState->CameraFollowingPlayerIndex);
	if(CameraFollowingEntity && CameraFollowingEntity->Exists)
	{
		GameState->CameraPosition = GetCenterOfRoom(CameraFollowingEntity->Pos,
																								GameState->RoomWidthInTiles,
																								GameState->RoomHeightInTiles);
	}

	//
	// NOTE(bjorn): Rendering
	//
	s32 TileSideInPixels = 60;
	f32 PixelsPerMeter = (f32)TileSideInPixels / TileMap->TileSideInMeters;

	DrawRectangle(Buffer, {0.0f, 0.0f}, {(f32)Buffer->Width, (f32)Buffer->Height}, 
								{1.0f, 0.0f, 1.0f});

#if 1
	DrawBitmap(Buffer, &GameState->Backdrop, {-40.0f, -40.0f}, 
						 {(f32)GameState->Backdrop.Width, (f32)GameState->Backdrop.Height});
#endif

	v2 ScreenCenter = v2{(f32)Buffer->Width, (f32)Buffer->Height} * 0.5f;

	for(s32 RelRow = -20;
			RelRow < 40;
			++RelRow)
	{
		for(s32 RelColumn = -30;
				RelColumn < 60;
				++RelColumn)
		{
			u32 Column = GameState->CameraPosition.AbsTile.X + RelColumn;
			u32 Row = GameState->CameraPosition.AbsTile.Y + RelRow;

			u32 TileID = GetTileValue(TileMap, 
																v3u{Column, Row, GameState->CameraPosition.AbsTile.Z});
			v3 TileRGB = {};

			if(TileID == 0)
			{
				TileRGB = {1.0f, 0.0f, 0.0f};
			}
			if(TileID == 1)
			{
				f32 Gray = 0.5f;
				TileRGB = {Gray, Gray, Gray};
			}
			if(TileID == 2)
			{
				f32 White = 1.0f;
				TileRGB = {White, White, White};
			}
			if(TileID == 3)
			{
				TileRGB = {0.0f, 0.5f, 1.0f};
			}
			if(TileID == 4)
			{
				TileRGB = {0.0f, 1.0f, 0.5f};
			}

			m22 InvertY = {1, 0,
										 0,-1};

			v2 Min = ScreenCenter 
				+ ((v2)v2s{RelColumn, -RelRow} - v2{0.5f, -0.5f}) * TileSideInPixels
				- InvertY * GameState->CameraPosition.Offset;

			v2 Max = Min + v2{1.0f, -1.0f} * TileSideInPixels;

			if(TileID != 1)
			{
				DrawRectangle(Buffer, Min, Max, TileRGB);
			}
		}
	}

	entity* Entity = GetEntity(GameState, 1);
	for(s32 EntityIndex = 1;
			EntityIndex < ArrayCount(GameState->Entities);
			Entity++, EntityIndex++)
	{
		if(Entity->Exists)
		{
			entity* Player = Entity;
			v3 PlayerRGB = {1.0f, 1.0f, 0.0f};

			v2 PlayerPixelDim = {(f32)TileSideInPixels * 0.75f, (f32)TileSideInPixels};

			v3 PlayerCameraDelta = GetTileMapPosDifference(TileMap, 
																										 Player->Pos, 
																										 GameState->CameraPosition).MeterDiff;
			m22 GameSpaceToScreenSpace = 
			{PixelsPerMeter,						  0,
				0,							-PixelsPerMeter};

			v2 PlayerCameraPixelDelta = GameSpaceToScreenSpace * PlayerCameraDelta.XY;

			v2 PlayerPixelPos = ScreenCenter + PlayerCameraPixelDelta;

			f32 PlayerTop = PlayerPixelPos .Y - PlayerPixelDim.Y;
			f32 PlayerBottom = PlayerPixelPos.Y;
			f32 PlayerLeft = PlayerPixelPos.X - PlayerPixelDim.X / 2.0f;
			f32 PlayerRight = PlayerPixelPos.X + PlayerPixelDim.X / 2.0f;

			if(Player->Pos.AbsTile.Z == GameState->CameraPosition.AbsTile.Z)
			{
				DrawRectangle(Buffer, {PlayerLeft, PlayerTop}, {PlayerRight, PlayerBottom}, PlayerRGB);

				hero_bitmaps *Hero = &(GameState->HeroBitmaps[Player->FacingDirection]);

				DrawBitmap(Buffer, &Hero->Torso, PlayerPixelPos - Hero->Alignment, (v2)Hero->Torso.Dim);
				DrawBitmap(Buffer, &Hero->Cape, PlayerPixelPos - Hero->Alignment, (v2)Hero->Cape.Dim);
				DrawBitmap(Buffer, &Hero->Head, PlayerPixelPos - Hero->Alignment, (v2)Hero->Head.Dim);
			}
		}
	}
}

	internal_function void
OutputSound(game_sound_output_buffer *SoundBuffer, game_state* GameState)
{
	s32 ToneVolume = 1000;
	s16 *SampleOut = SoundBuffer->Samples;
	for(s32 SampleIndex = 0;
			SampleIndex < SoundBuffer->SampleCount;
			++SampleIndex)
	{
		s16 SampleValue = 0;
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
	}
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	OutputSound(SoundBuffer, GameState);
}
