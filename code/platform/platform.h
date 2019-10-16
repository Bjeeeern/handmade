#if !defined(PLATFORM_H)

#include "types_and_defines.h"
#include "math.h"

/* NOTE(bjorn):

   :HANDMADE_INTERNAL:
   0 - Build for public release. 
   1 - Build for developer only.

   :HANDMADE_SLOW:
   0 - No slow code allowed!
   1 - Slow code welcome.
 */

//
// NOTE(bjorn): Stuff the platform provides to the game.
//

struct thread_context
{
  s32 PlaceHolder;
};

#if HANDMADE_INTERNAL
/* IMPORTANT(bjorn):
   These are NOT for doing anything in the shipping game, these are blocking
   and the write doesn't protect against lost data!
 */
struct debug_read_file_result
{
  s32 ContentSize;
  void *Content;
};
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *FileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(char *FileName, u32 FileSize, \
                                                       void *FileMemory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *FileMemory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_GET_FILE_EDIT_TIMESTAMP(name) u64 name(char *FileName)
typedef DEBUG_PLATFORM_GET_FILE_EDIT_TIMESTAMP(debug_platform_get_file_edit_timestamp);
#endif

//
// NOTE(bjorn): Stuff the game provides to the platform.
//
struct game_offscreen_buffer
{
  void *Memory;
	union
	{
		v2s Dim;
		struct
		{
			s32 Width;
			s32 Height;
		};
	};
	//                                                              high     low
	// NOTE(bjorn): Expected pixel layout in memory is top to bottom AA RR GG BB.
  s32 BytesPerPixel;
  s32 Pitch;
};

struct game_sound_output_buffer
{
  s16 *Samples;
  s32 SamplesPerSecond;
  s32 SampleCount;
};

struct game_button
{
  s32 HalfTransitionCount;
  b32 EndedDown;
};

#define Clicked(controller, button) ( controller##->##button##.EndedDown && \
																			controller##->##button##.HalfTransitionCount )
#define Released(controller, button) ( !controller##->##button##.EndedDown && \
																			  controller##->##button##.HalfTransitionCount )

#define Held(controller, button) ( controller##->##button##.EndedDown )

struct game_stick
{
  v2 Start;
  v2 Average;
  v2 End;
};

#define GAME_STICK_VIRTUAL_BUTTON_THRESHOLD 0.5f
struct game_stick_virtual_buttons
{
  game_button Up;
  game_button Left;
  game_button Right;
  game_button Down;
};

struct game_controller
{
  b32 IsConnected;
  union
  {
    game_button Buttons[16];
    struct
    {
      game_button ActionUp;
      game_button ActionLeft;
      game_button ActionRight;
      game_button ActionDown;
      game_stick_virtual_buttons LeftStickVrtBtn;
      game_stick_virtual_buttons RightStickVrtBtn;
      game_button RightShoulder;
      game_button LeftShoulder;
      game_button Back;
      game_button Start;

      // NOTE(bjorn): All buttons must be added above this line.
      //              Struct_Terminator is a hack in order to bounds-check Buttons number.
      game_button Struct_Terminator;
    };
  };

  //TODO(bjorn): Should a stick just be masked as a button. Are raw vectors needed?
  b32 LeftIsAnalog;
  b32 RightIsAnalog;
  game_stick RightStick;
  game_stick LeftStick;

  f32 LeftMotor;
  f32 RightMotor;
};

struct game_mouse
{
	b32 IsConnected;
	// TODO(bjorn): Check math notation on [] vs ()
	// Normalized [0,1]
	v2 P;
	v2 dP;
  f32 Wheel;
	union
	{
		game_button Buttons[5];
		struct{
			game_button Left;
			game_button Middle;
			game_button Right;
			game_button ThumbForward;
			game_button ThumbBackward;
		};
	};
};

// TODO(bjorn): Later add more relevant keys like f1-f12, esc, del etc.
// Game being able to toggle windows-key on and off?
struct game_keyboard
{
  b32 IsConnected;
	//IMPORTANT(bjorn): '\r\n' '\n' and '\r' should _ALL_ be converted to '\n'
	u32 UnicodeCodePointsWritten[256];
	union
	{
		game_button Buttons[45];
		struct
		{
			union
			{
				game_button Numbers[10];
				struct
				{
					game_button Zero, One, Two, Three, Four, 
											Five, Six, Seven, Eight, Nine;
				};
			};
			union
			{
				game_button Letters[26];
				struct
				{
					game_button A, B, C, D, E, F, G, H, I, J, 
											K, L, M, N, O, P, Q, R, S, T, 
											U, V, W, X, Y, Z; 
				};
			};
			game_button Shift;
			game_button Control;
			game_button Alt;
			game_button Enter;
			game_button Space;

			game_button Up;
			game_button Right;
			game_button Down;
			game_button Left;
		};
	};
};

struct game_input
{
	game_controller Controllers[4];
	game_keyboard Keyboards[2];
	game_mouse Mice[2];
};

//NOTE Access to the input devices is in the 1-n range for freeing up 0 as a null index
	inline game_controller *
GetController(game_input *Input, u64 Index)
{
	game_controller* Result = 0;
	if(Index)
	{
		Assert(Index <= ArrayCount(Input->Controllers));
		Result = &Input->Controllers[Index-1];
	}
	return Result;
}
inline u64
GetControllerIndex(game_input* Input, game_controller* Controller)
{
	u64 Result = 0;
	if(Controller)
	{
		Result = (u64)(Controller - Input->Controllers) + 1;
		Assert(0 < Result && Result <= ArrayCount(Input->Controllers));
	}
	return Result;
}

	inline game_keyboard *
GetKeyboard(game_input *Input, u64 Index)
{
	game_keyboard* Result = 0;
	if(Index)
	{
		Assert(Index <= ArrayCount(Input->Keyboards));
		Result = &Input->Keyboards[Index-1];
	}
	return Result;
}
inline u64
GetKeyboardIndex(game_input* Input, game_keyboard* Keyboard)
{
	u64 Result = 0;
	if(Keyboard)
	{
		Result = (u64)(Keyboard - Input->Keyboards) + 1;
		Assert(0 < Result && Result <= ArrayCount(Input->Keyboards));
	}
	return Result;
}

	inline game_mouse *
GetMouse(game_input *Input, u64 Index)
{
	game_mouse* Result = 0;
	if(Index)
	{
		Assert(Index <= ArrayCount(Input->Mice));
		Result = &Input->Mice[Index-1];
	}
	return Result;
}
inline u64
GetMouseIndex(game_input* Input, game_mouse* Mouse)
{
	u64 Result = 0;
	if(Mouse)
	{
		Result = (u64)(Mouse - Input->Mice) + 1;
		Assert(0 < Result && Result <= ArrayCount(Input->Mice));
	}
	return Result;
}

// NOTE(bjorn): Memory REQUIRED to be initialized to 0 on startup.
struct game_memory
{
	b32 IsInitialized;

	memi PermanentStorageSize;
	void *PermanentStorage; 

	memi TransientStorageSize;
	void *TransientStorage; 

	debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
	debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
	debug_platform_get_file_edit_timestamp *DEBUGPlatformGetFileEditTimestamp;
};

#define GAME_UPDATE_AND_RENDER(name) void name(f32 SecondsToUpdate, thread_context *Thread,\
																							 game_memory *Memory, game_input *Input,\
																							 game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
	return;
}

// NOTE(bjorn): This has to be a fast function. A millisecond or so.
// TODO(bjorn): Reduce the pressure on this function's performance by 
//              measuring it etc.
#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *Thread,\
																							 game_sound_output_buffer *SoundBuffer,\
																							 game_memory *Memory)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
	return;
}

#define PLATFORM_H
#endif
