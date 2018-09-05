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

	s32 AlignmentX;
	s32 AlignmentY;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	s32 RoomWidthInTiles;
	s32 RoomHeightInTiles;
	tile_map_position CameraPosition;
	tile_map_position PlayerPosition;
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
	Hero.AlignmentX = 72;
	Hero.AlignmentY = 182;
	GameState->HeroBitmaps[0] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_left_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_left_torso.bmp");
	Hero.AlignmentX = 72;
	Hero.AlignmentY = 182;
	GameState->HeroBitmaps[1] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_back_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_back_torso.bmp");
	Hero.AlignmentX = 72;
	Hero.AlignmentY = 182;
	GameState->HeroBitmaps[2] = Hero;

	Hero.Head = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_head.bmp");
	Hero.Cape = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
													 "data/test/test_hero_right_cape.bmp");
	Hero.Torso = DEBUGLoadBMP(Memory->DEBUGPlatformReadEntireFile, 
														"data/test/test_hero_right_torso.bmp");
	Hero.AlignmentX = 72;
	Hero.AlignmentY = 182;
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

	f32 PlayerWidth = (f32)TileMap->TileSideInMeters * 0.75f;

	v2 PlayerDirection = {};

	if(Input0->LeftStickVrtBtn.Down.EndedDown)
	{
		PlayerDirection.Y += -1;
		GameState->HeroFacingDirection = 0;
	}
	if(Input0->LeftStickVrtBtn.Left.EndedDown)
	{
		PlayerDirection.X += -1;
		GameState->HeroFacingDirection = 1;
	}
	if(Input0->LeftStickVrtBtn.Up.EndedDown)
	{
		PlayerDirection.Y += 1;
		GameState->HeroFacingDirection = 2;
	}
	if(Input0->LeftStickVrtBtn.Right.EndedDown)
	{
		PlayerDirection.X += 1;
		GameState->HeroFacingDirection = 3;
	}

	if(PlayerDirection.X && PlayerDirection.Y)
	{
		PlayerDirection *= invroot2;
	}

	f32 PlayerMetersPerSecondSpeed = 10.0f;

	f32 PlayerDistanceToTravel = PlayerMetersPerSecondSpeed * SecondsToUpdate;
	v2 NewPlayerPosition = 
		(GameState->PlayerPosition.Offset + PlayerDirection * PlayerDistanceToTravel);

	tile_map_position NewDeltaPlayerPosition = GameState->PlayerPosition;
	NewDeltaPlayerPosition.Offset = NewPlayerPosition;

	tile_map_position NewDeltaLeft = NewDeltaPlayerPosition;
	NewDeltaLeft.Offset.X = (NewPlayerPosition.X - PlayerWidth/2.0f); 

	tile_map_position NewDeltaRight = NewDeltaPlayerPosition;
	NewDeltaRight.Offset.X = (NewPlayerPosition.X + PlayerWidth/2.0f); 

	tile_map_position PlayerCanonicalPosition = RecanonilizePosition(TileMap, 
																																	 NewDeltaPlayerPosition);
	tile_map_position PlayerCanonicalLeft = RecanonilizePosition(TileMap, NewDeltaLeft);
	tile_map_position PlayerCanonicalRight = RecanonilizePosition(TileMap, NewDeltaRight);

	v2u DebugPlayerTile = PlayerCanonicalPosition.AbsTile.XY;

	if(IsTileMapPointEmpty(TileMap, PlayerCanonicalLeft) &&
		 IsTileMapPointEmpty(TileMap, PlayerCanonicalRight))
	{
		GameState->PlayerPosition = PlayerCanonicalPosition;
	}
	
	u32 CurrentTile = GetTileValue(TileMap, PlayerCanonicalPosition);
	if((CurrentTile == 3) ||
		(CurrentTile == 4))
	{
		if(!GameState->PlayerIsOnStairs)
		{
			if(CurrentTile == 3)
			{
				GameState->PlayerPosition.AbsTile.Z += 1;
				//GameState->CameraPosition.AbsTile.Z += 1;
			}
			else if(CurrentTile == 4)
			{
				GameState->PlayerPosition.AbsTile.Z -= 1;
				//GameState->CameraPosition.AbsTile.Z -= 1;
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
	// NOTE(bjorn): Rendering below.
	//
	s32 TileSideInPixels = 50;
	f32 PixelsPerMeter = (f32)TileSideInPixels / TileMap->TileSideInMeters;


	DrawRectangle(Buffer, 0.0f, (f32)Buffer->Width, 0.0f, (f32)Buffer->Height, 
								1.0f, 0.0f, 1.0f);

	DrawBitmap(Buffer, &GameState->Backdrop, 
						 -40.0f, -40.0f, 
						 (f32)GameState->Backdrop.Width, (f32)GameState->Backdrop.Height);

	f32 ScreenCenterX = (f32)Buffer->Width / 2.0f;
	f32 ScreenCenterY = (f32)Buffer->Height / 2.0f;

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
			f32 TileR = 0.0f;
			f32 TileG = 0.0f;
			f32 TileB = 0.0f;

			if(TileID == 0)
			{
				TileR = 1.0f;
				TileG = 0.0f;
				TileB = 0.0f;
			}
			if(TileID == 1)
			{
				f32 Gray = 0.5f;
				TileR = Gray;
				TileG = Gray;
				TileB = Gray;
			}
			if(TileID == 2)
			{
				f32 White = 1.0f;
				TileR = White;
				TileG = White;
				TileB = White;
			}
			if(TileID == 3)
			{
				TileR = 0.0f;
				TileG = 0.5f;
				TileB = 1.0f;
			}
			if(TileID == 4)
			{
				TileR = 0.0f;
				TileG = 1.0f;
				TileB = 0.5f;
			}
			if(Row == DebugPlayerTile.Y && Column == DebugPlayerTile.X)
			{
				TileR = 1.0f;
				TileG = 0.5f;
				TileB = 0.0f;
			}

			f32 MinX = (ScreenCenterX + 
									(f32)RelColumn * (f32)TileSideInPixels -
									(f32)TileSideInPixels / 2.0f -
									PixelsPerMeter * GameState->CameraPosition.Offset.X
								 );
			f32 MinY = (ScreenCenterY - 
									(f32)RelRow * (f32)TileSideInPixels +
									(f32)TileSideInPixels / 2.0f +
									PixelsPerMeter * GameState->CameraPosition.Offset.Y
								 );

			f32 MaxX = MinX + (f32)TileSideInPixels;
			f32 MaxY = MinY - (f32)TileSideInPixels;

			if(TileID != 1)
			{
				DrawRectangle(Buffer, MinX, MaxX, MaxY, MinY, TileR, TileG, TileB);
			}
		}
	}

	{
		f32 PlayerR = 1.0f;
		f32 PlayerG = 1.0f;
		f32 PlayerB = 0.0f;

		f32 PlayerPixelHeight = (f32)TileSideInPixels;
		f32 PlayerPixelWidth = (f32)TileSideInPixels * 0.75f;

		v3 PlayerCameraDelta = CalculateMeterDelta(TileMap, 
																							 GameState->PlayerPosition, 
																							 GameState->CameraPosition);
		PlayerCameraDelta *= PixelsPerMeter;

		f32 PlayerX = ScreenCenterX + PlayerCameraDelta.X;
		f32 PlayerY = ScreenCenterY - PlayerCameraDelta.Y;

		f32 PlayerTop = PlayerY - PlayerPixelHeight;
		f32 PlayerBottom = PlayerY;
		f32 PlayerLeft = PlayerX - PlayerPixelWidth / 2.0f;
		f32 PlayerRight = PlayerX + PlayerPixelWidth / 2.0f;

		if(GameState->PlayerPosition.AbsTile.Z <= GameState->CameraPosition.AbsTile.Z)
		{
			DrawRectangle(Buffer, PlayerLeft, PlayerRight, PlayerTop, PlayerBottom, 
										PlayerR, PlayerG, PlayerB);

			hero_bitmaps *Hero = &(GameState->HeroBitmaps[GameState->HeroFacingDirection]);

			DrawBitmap(Buffer, &Hero->Torso, 
								 PlayerX - Hero->AlignmentX, PlayerY - Hero->AlignmentY,
								 (f32)Hero->Torso.Width, (f32)Hero->Torso.Height);
			DrawBitmap(Buffer, &Hero->Cape,
								 PlayerX - Hero->AlignmentX, PlayerY - Hero->AlignmentY,
								 (f32)Hero->Cape.Width, (f32)Hero->Cape.Height);
			DrawBitmap(Buffer, &Hero->Head,
								 PlayerX - Hero->AlignmentX, PlayerY - Hero->AlignmentY,
								 (f32)Hero->Head.Width, (f32)Hero->Head.Height);
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
