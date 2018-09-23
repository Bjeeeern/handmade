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

struct game_state
{
	memory_arena WorldArena;
	world *World;

	s32 RoomWidthInTiles;
	s32 RoomHeightInTiles;
	tile_map_position CameraPosition;
	tile_map_position PlayerPosition;
	v2 PlayerVelocity;

	b32 PlayerIsOnStairs;

	loaded_bitmap Backdrop;

	hero_bitmaps HeroBitmaps[4];
	s32 HeroFacingDirection;
};

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

	GameState->PlayerPosition.AbsTile = {7, 5, 0};
	GameState->PlayerPosition.Offset = {0.0f, 0.0f};

	GameState->CameraPosition.AbsTile = {8, 4, 0};
	GameState->CameraPosition.Offset = {};

	GameState->PlayerIsOnStairs;

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

internal_function v2 
ClosestPointInRectangle(v2 MinCorner, v2 MaxCorner, tile_map_diff RelNewPlayerPos)
{
	v2 Result = {};
	Result.X = Clamp(RelNewPlayerPos.MeterDiff.X, MinCorner.X, MaxCorner.X);
	Result.Y = Clamp(RelNewPlayerPos.MeterDiff.Y, MinCorner.Y, MaxCorner.Y);

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
	game_controller *Input0 = GetController(Input, 0);
	if(Input0->ActionDown.EndedDown)
	{
		InitializeGame(Memory, GameState);
	}

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;
	tile_map_position OldPlayerPos = GameState->PlayerPosition;

	f32 PlayerWidth = (f32)TileMap->TileSideInMeters * 0.75f;

	//
	// NOTE(bjorn): Movement
	//
	v2 InputDirection = {};

	if(Input0->LeftStickVrtBtn.Down.EndedDown)
	{
		InputDirection.Y += -1;
		GameState->HeroFacingDirection = 0;
	}
	if(Input0->LeftStickVrtBtn.Left.EndedDown)
	{
		InputDirection.X += -1;
		GameState->HeroFacingDirection = 1;
	}
	if(Input0->LeftStickVrtBtn.Up.EndedDown)
	{
		InputDirection.Y += 1;
		GameState->HeroFacingDirection = 2;
	}
	if(Input0->LeftStickVrtBtn.Right.EndedDown)
	{
		InputDirection.X += 1;
		GameState->HeroFacingDirection = 3;
	}

	if(InputDirection.X && InputDirection.Y)
	{
		InputDirection *= invroot2;
	}

	v2 PlayerAcceleration = InputDirection * 70.0f;

	//TODO(casey): ODE here!
	PlayerAcceleration += -8.0f * GameState->PlayerVelocity;

	tile_map_position NewPlayerPos = GameState->PlayerPosition;
	v2 PlayerDelta = (0.5f * PlayerAcceleration * Square(SecondsToUpdate)
										+ (GameState->PlayerVelocity * SecondsToUpdate));
	NewPlayerPos.Offset += PlayerDelta;
	GameState->PlayerVelocity = (PlayerAcceleration * SecondsToUpdate) + GameState->PlayerVelocity;

	NewPlayerPos = RecanonilizePosition(TileMap, NewPlayerPos);

	//
	// NOTE(bjorn): Collision check after movement
	//
	u32 MinTileX = Min(OldPlayerPos.AbsTile.X, NewPlayerPos.AbsTile.X);
	u32 MinTileY = Min(OldPlayerPos.AbsTile.Y, NewPlayerPos.AbsTile.Y);
	u32 OneAfterMaxTileX = Max(OldPlayerPos.AbsTile.X, NewPlayerPos.AbsTile.X) + 1; 
	u32 OneAfterMaxTileY = Max(OldPlayerPos.AbsTile.Y, NewPlayerPos.AbsTile.Y) + 1; 
	u32 AbsTileZ = NewPlayerPos.AbsTile.Z;
	tile_map_position BestPlayerPos = NewPlayerPos;
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
				tile_map_diff RelNewPlayerPos = GetTileMapPosDifference(TileMap, 
																																NewPlayerPos, 
																																TestTilePos);

				v2 MinCorner = -0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
				v2 MaxCorner = 0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
				TestTilePos.Offset = ClosestPointInRectangle(MinCorner, MaxCorner, RelNewPlayerPos);

				tile_map_diff RelTestTilePlayerPos = GetTileMapPosDifference(TileMap, 
																																		 NewPlayerPos, 
																																		 TestTilePos);
				f32 TestDistanceSquared = LenghtSquared(RelTestTilePlayerPos.MeterDiff.XY);
				if(TestDistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = TestDistanceSquared;
					BestPlayerPos = TestTilePos;
				}
			}
		}
	}

	GameState->PlayerPosition = BestPlayerPos;

	//
	// NOTE(bjorn): Update camera/Z-position
	//
	u32 CurrentTile = GetTileValue(TileMap, GameState->PlayerPosition);
	if((CurrentTile == 3) ||
		 (CurrentTile == 4))
	{
		if(!GameState->PlayerIsOnStairs)
		{
			if(CurrentTile == 3)
			{
				GameState->PlayerPosition.AbsTile.Z += 1;
			}
			else if(CurrentTile == 4)
			{
				GameState->PlayerPosition.AbsTile.Z -= 1;
			}
			GameState->PlayerIsOnStairs = true;
		}
	}
	else
	{
		GameState->PlayerIsOnStairs = false;
	}

	GameState->CameraPosition = GetCenterOfRoom(GameState->PlayerPosition,
																							GameState->RoomWidthInTiles,
																							GameState->RoomHeightInTiles);

	//
	// NOTE(bjorn): Rendering
	//
	s32 TileSideInPixels = 60;
	f32 PixelsPerMeter = (f32)TileSideInPixels / TileMap->TileSideInMeters;

	DrawRectangle(Buffer, {0.0f, 0.0f}, {(f32)Buffer->Width, (f32)Buffer->Height}, 
								{1.0f, 0.0f, 1.0f});

#if 0
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

	{
		v3 PlayerRGB = {1.0f, 1.0f, 0.0f};

		v2 PlayerPixelDim = {(f32)TileSideInPixels * 0.75f, (f32)TileSideInPixels};

		v3 PlayerCameraDelta = GetTileMapPosDifference(TileMap, 
																									 GameState->PlayerPosition, 
																									 GameState->CameraPosition).MeterDiff;
		m22 GameSpaceToScreenSpace = 
		{PixelsPerMeter,						  0,
		 0,							-PixelsPerMeter};

		v2 PlayerCameraPixelDelta = GameSpaceToScreenSpace * PlayerCameraDelta.XY;

		v2 Player = ScreenCenter + PlayerCameraPixelDelta;

		f32 PlayerTop = Player.Y - PlayerPixelDim.Y;
		f32 PlayerBottom = Player.Y;
		f32 PlayerLeft = Player.X - PlayerPixelDim.X / 2.0f;
		f32 PlayerRight = Player.X + PlayerPixelDim.X / 2.0f;

		if(GameState->PlayerPosition.AbsTile.Z <= GameState->CameraPosition.AbsTile.Z)
		{
			DrawRectangle(Buffer, {PlayerLeft, PlayerTop}, {PlayerRight, PlayerBottom}, PlayerRGB);

			hero_bitmaps *Hero = &(GameState->HeroBitmaps[GameState->HeroFacingDirection]);

			DrawBitmap(Buffer, &Hero->Torso, Player - Hero->Alignment, (v2)Hero->Torso.Dim);
			DrawBitmap(Buffer, &Hero->Cape, Player - Hero->Alignment, (v2)Hero->Cape.Dim);
			DrawBitmap(Buffer, &Hero->Head, Player - Hero->Alignment, (v2)Hero->Head.Dim);
		}
	}
}

	internal_function void
OutputSound(game_sound_output_buffer *SoundBuffer)
{
	s32 ToneVolume = 1000;
	s16 *SampleOut = SoundBuffer->Samples;
	for(s32 SampleIndex = 0;
			SampleIndex < SoundBuffer->SampleCount;
			++SampleIndex)
	{
		/*
			 local_persist s32 t = 0;
			 t++;
			 s16 SampleValue = 1000 * ((t / 1000) % 2);
			 */
		s16 SampleValue = 0;
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
	}
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	OutputSound(SoundBuffer);
}
