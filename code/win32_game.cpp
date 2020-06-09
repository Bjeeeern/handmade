#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
//NOTE(bjorn): These have to be included becase WIN32_LEAN_AND_MEAN is a blut
//tool to remove winsock.h in order to include winsock2.h
#include <mmeapi.h>
#include <timeapi.h>
// END NOTE

#include <winsock2.h>
#include <xinput.h>
#include <dsound.h>
#include <gl/gl.h>
#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

#include "platform.h"
#include "asset_streaming.h"
#include "opengl_renderer.h"

//IMPORTANT TODO(bjorn): This is for the wsprintfA and sprintf_s functions.
//Remove this in the future.
#include <stdio.h>

//NOTE(bjorn): 1080p display mode is 1920x1080 -> Half of that is 960x540.
#if 1
#define GAME_RGB_BUFFER_WIDTH 1920
#define GAME_RGB_BUFFER_HEIGHT 1080
#else
#define GAME_RGB_BUFFER_WIDTH 960
#define GAME_RGB_BUFFER_HEIGHT 540
#endif

struct win32_game_code
{
  HMODULE GameDLL;
  game_update* Update;
  game_get_sound_samples* GetSoundSamples;
  FILETIME DLLLastWriteTime;
  b32 IsValid;
};

#define WIN32_STATE_FILE_NAME_CHAR_COUNT MAX_PATH
struct win32_game
{
  game_memory Memory;
  win32_game_code Code;
  char DLLFullPath[WIN32_STATE_FILE_NAME_CHAR_COUNT];
  char TempDLLFullPath[WIN32_STATE_FILE_NAME_CHAR_COUNT];
};

struct win32_state
{
  s32 SelectedNumKey;
  HANDLE RecordHandle;
  HANDLE PlayBackHandle;

  u64 GameMemoryBlockSize;
  void *GameMemoryBlock;

  void *MemoryRecords[10];

  char EXEFileName[MAX_PATH];
  char *OnePastLastEXEFileNameSlash;
};

struct win32_debug_time_marker
{
  DWORD PlayCursor;
  DWORD WriteCursor;
  DWORD TargetCursor;
  DWORD RunningByteIndex;
};

struct win32_window_dimension
{
  s32 Width;
  s32 Height;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState) 
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  (void)dwUserIndex;
  (void)pState;
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,            \
                                                  XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  (void)dwUserIndex;
  (void)pVibration;
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, \
                                                      LPDIRECTSOUND *ppDS,  \
                                                      LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

  inline s32
StringLength(char *String)
{
  s32 Length = 0;
  while(*String++ != '\0'){ Length++; }
  return Length;
}

  internal_function void
ConcatenateStrings(size_t SourceALength, char *SourceA,
                   size_t SourceBLength, char *SourceB,
                   size_t DestinationLength, char *Destination)
{
  // TODO(bjorn): Destination bounds-checking.
  for(s32 Index = 0;
      Index < SourceALength;
      ++Index)
  {
    *Destination++ = *SourceA++;
  }
  for(s32 Index = 0;
      Index < SourceBLength;
      ++Index)
  {
    *Destination++ = *SourceB++;
  }
  *Destination = '\0';
}


#if HANDMADE_INTERNAL
//IMPORTANT(bjorn): As of 2018 6/2. GlobalPathToGameRoot is a path to the folder
//above build. Assuming that the exe is located inside a folder named build
//that is at the same level as data AKA top level. 
global_variable char GlobalPathToGameRoot[MAX_PATH] = {};

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
  if(FileMemory)
  {
    VirtualFree(FileMemory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
  debug_read_file_result Result = {};
  char FilePath[MAX_PATH] = {};
  ConcatenateStrings(StringLength(GlobalPathToGameRoot), GlobalPathToGameRoot,
                     StringLength(FileName), FileName,
                     MAX_PATH, FilePath);

  char* TestForForwardSlash = FilePath + MAX_PATH;
  while(TestForForwardSlash != FilePath) 
  { 
    if(*TestForForwardSlash == '/') { *TestForForwardSlash = '\\'; }
    --TestForForwardSlash;
  }

  HANDLE FileHandle = CreateFileA(FilePath, GENERIC_READ, FILE_SHARE_READ, 
                                  0, OPEN_EXISTING, 0, 0);
  if(FileHandle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER FileSize;
    if(GetFileSizeEx(FileHandle, &FileSize))
    {
      u32 FileSize32 = SafeTruncateU64(FileSize.QuadPart);
      Result.Content = (u8*)VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      if(Result.Content)
      {
        DWORD BytesRead;
        if(ReadFile(FileHandle, Result.Content, FileSize32, &BytesRead, 0) && 
           (FileSize32 == BytesRead))
        {
          // NOTE(bjorn): File read is successful!
          Result.ContentSize = FileSize32;
        }
        else
        {
          // TODO(bjorn): Logging.
          DEBUGPlatformFreeFileMemory(Result.Content);
          Result.Content = 0;
        }
      }
      else
      {
        // TODO(bjorn): Logging.
      }
    }
    else
    {
      // TODO(bjorn): Logging.
    }
    CloseHandle(FileHandle);
  }
  else
  {
    char TextBuffer[256];
    wsprintfA(TextBuffer, 
              "Error code:%d\n", GetLastError());
    OutputDebugStringA(TextBuffer);
    // TODO(bjorn): Logging.
  }
  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
  b32 Result = false;
  char FilePath[MAX_PATH] = {};
  ConcatenateStrings(StringLength(GlobalPathToGameRoot), GlobalPathToGameRoot,
                     StringLength(FileName), FileName,
                     MAX_PATH, FilePath);

  char* TestForForwardSlash = FilePath + MAX_PATH;
  while(TestForForwardSlash != FilePath) 
  { 
    if(*TestForForwardSlash == '/') { *TestForForwardSlash = '\\'; }
    --TestForForwardSlash;
  }

  HANDLE FileHandle = CreateFileA(FilePath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if(FileHandle != INVALID_HANDLE_VALUE)
  {
    DWORD BytesWritten;
    if(WriteFile(FileHandle, FileMemory, FileSize, &BytesWritten, 0))
    {
      // NOTE(bjorn): File is written successfuly!
      Result = (FileSize == BytesWritten);
    }
    else
    {
      // TODO(bjorn): Logging.
    }
    CloseHandle(FileHandle);
  }
  else
  {
    // TODO(bjorn): Logging.
  }
  return Result;
}

  inline FILETIME
Win32GetLastWriteTime(char *FileName)
{
  FILETIME LastWriteTime = {};

  WIN32_FILE_ATTRIBUTE_DATA Data;
  if(GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data))
  {
    LastWriteTime = Data.ftLastWriteTime;
  }

  return LastWriteTime;
}

DEBUG_PLATFORM_GET_FILE_EDIT_TIMESTAMP(DEBUGPlatformGetFileEditTimestamp)
{
  char FilePath[MAX_PATH] = {};
  ConcatenateStrings(StringLength(GlobalPathToGameRoot), GlobalPathToGameRoot,
                     StringLength(FileName), FileName,
                     MAX_PATH, FilePath);

  char* TestForForwardSlash = FilePath + MAX_PATH;
  while(TestForForwardSlash != FilePath) 
  { 
    if(*TestForForwardSlash == '/') { *TestForForwardSlash = '\\'; }
    --TestForForwardSlash;
  }

  FILETIME LastWriteTime = Win32GetLastWriteTime(FilePath);
  u64 Result = ((u64)LastWriteTime.dwHighDateTime << 32) | ((u64)LastWriteTime.dwLowDateTime);

  return Result;
}
#endif

  internal_function LPDIRECTSOUNDBUFFER
Win32InitDSound(HWND WindowHandle, s32 SamplesPerSecond, s32 BufferSize)
{
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

  if(DSoundLibrary)
  {
    direct_sound_create *DirectSoundCreate = 
      (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

    // TODO(bjorn): Double-check that this works on XP. Direct sound 8 or 7??
    LPDIRECTSOUND DirectSound;
    if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
      WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;

      if(SUCCEEDED(DirectSound->SetCooperativeLevel(WindowHandle, DSSCL_PRIORITY)))
      {
        //TODO(bjorn): Maybe DSBCAPS_GLOBALFOCUS?
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER|DSBCAPS_GETCURRENTPOSITION2;

        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
        {
          if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
          {
            // NOTE(bjorn): Finally the format is set!
            OutputDebugStringA("Primary buffer format set!\n");
          }
          else
          {
            // TODO(bjorn): Diagnostic.
          }
        }
        else
        {
          // TODO(bjorn): Diagnostic.
        }
      }
      else
      {
        // TODO(bjorn): Diagnostic.
      }

      //TODO(bjorn): DSBCAPS_GETCURRENTPOSITION2 for performance?
      DSBUFFERDESC BufferDescription = {};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      LPDIRECTSOUNDBUFFER SecondaryBuffer;
      if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0)))
      {
        OutputDebugStringA("Secondary buffer created!\n");
        return SecondaryBuffer;
      }
      else
      {
        // TODO(bjorn): Diagnostic.
      }
    }
    else
    {
      // TODO(bjorn): Diagnostic.
    }
  }
  else
  {
    // TODO(bjorn): Diagnostic.
  }
  return 0;
}

#if HANDMADE_INTERNAL
  internal_function win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
  win32_game_code GameCode = {};

  GameCode.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

  b32 FoundDLL = CopyFileA(SourceDLLName, TempDLLName, FALSE);
  GameCode.GameDLL = LoadLibraryA(TempDLLName);
  if(FoundDLL && GameCode.GameDLL)
  {
    GameCode.Update = ((game_update*)
                       GetProcAddress(GameCode.GameDLL, "GameUpdate"));
    GameCode.GetSoundSamples = ((game_get_sound_samples *)
                                GetProcAddress(GameCode.GameDLL, "GameGetSoundSamples"));
    GameCode.IsValid = (GameCode.Update && GameCode.GetSoundSamples);
  }
  if(!GameCode.IsValid)
  {
    GameCode.Update = GameUpdateStub;
    GameCode.GetSoundSamples = GameGetSoundSamplesStub;
  }
  return GameCode;
}

  internal_function void
Win32UnloadGameCode(win32_game_code *GameCode)
{
  if(GameCode->GameDLL)
  {
    FreeLibrary(GameCode->GameDLL);
    GameCode->GameDLL = 0;
  }
  GameCode->IsValid = false;
  GameCode->Update = GameUpdateStub;
  GameCode->GetSoundSamples = GameGetSoundSamplesStub;
}
#endif

  internal_function void
Win32LoadXInput()
{
  HMODULE XInputLibrary = LoadLibraryA("Xinput1_3.dll");
  if(!XInputLibrary)
  {
    XInputLibrary = LoadLibraryA("Xinput1_4.dll");
    // TODO(bjorn): Diagnostic.
  }
  if(!XInputLibrary)
  {
    XInputLibrary = LoadLibraryA("Xinput9_1_0.dll");
    // TODO(bjorn): Diagnostic.
  }
  if(XInputLibrary)
  {
    XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    // TODO(bjorn): Diagnostic.
  }
  else
  {
    // TODO(bjorn): Diagnostic.
  }
}

  internal_function win32_window_dimension
Win32GetWindowDimension(HWND WindowHandle)
{
  win32_window_dimension Dimension;
  RECT ClientRect; 
  GetClientRect(WindowHandle, &ClientRect);
  Dimension.Width = ClientRect.right - ClientRect.left;
  Dimension.Height = ClientRect.bottom - ClientRect.top;
  return Dimension;
}

  internal_function void
Win32DisplayBufferInWindow(HDC DeviceContext, 
                           s32 WindowWidth, s32 WindowHeight, 
                           s32 GameScreenLeft, s32 GameScreenTop, 
                           s32 GameScreenWidth, s32 GameScreenHeight)
{
  glViewport(GameScreenLeft, WindowHeight - (GameScreenHeight+GameScreenTop), 
             GameScreenWidth, GameScreenHeight);

  SwapBuffers(DeviceContext);
}

struct win32_window_callback_data
{
  b32 *GameIsRunning;

  u8 *WindowAlpha;

  s32 *GameScreenTop; 
  s32 *GameScreenLeft; 
  s32 *GameScreenWidth; 
  s32 *GameScreenHeight;
};

  LRESULT CALLBACK 
Win32MainWindowCallback(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
  LRESULT Result = 0;
  win32_window_callback_data *CallbackData = 
    (win32_window_callback_data*)GetWindowLongPtr(WindowHandle, GWLP_USERDATA);

  switch(Message)
  {
    case WM_SIZE:
      {
      } break;

    case WM_DESTROY:
      {
        // TODO(bjorn): Treat this as an error - log and recreate window?
        *(CallbackData->GameIsRunning) = false;
      } break;

    case WM_QUIT:
      {
        // TODO(bjorn): Notify the user?
        *(CallbackData->GameIsRunning) = false;
      } break;

    case WM_CLOSE:
      {
        // TODO(bjorn): Notify the user?
        *(CallbackData->GameIsRunning) = false;
      } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        Assert(!"A call generated outside of DispatchMessage called with a KEY message.");
      } break;

    case WM_ACTIVATEAPP:
      {
        if(CallbackData)
        {
          if(WParam == TRUE)
          {
            SetLayeredWindowAttributes(WindowHandle, RGB(0, 0, 0), 
                                       255, LWA_ALPHA);
          }
          else
          {
            SetLayeredWindowAttributes(WindowHandle, RGB(0, 0, 0), 
                                       *(CallbackData->WindowAlpha), LWA_ALPHA);
          }
        }
        else
        {
          SetLayeredWindowAttributes(WindowHandle, RGB(0, 0, 0), 255, LWA_ALPHA);
        }
      } break;

#if 0
    case WM_SETCURSOR:
      {
        SetCursor(LoadCursor(0, IDC_ARROW));
      } break;
#endif

    case WM_PAINT:
      {
        PAINTSTRUCT PaintStruct = {};
        HDC DeviceContext = BeginPaint(WindowHandle, &PaintStruct);

        win32_window_dimension Dimension = Win32GetWindowDimension(WindowHandle);
        Win32DisplayBufferInWindow(DeviceContext, 
                                   Dimension.Width, Dimension.Height,
                                   *(CallbackData->GameScreenLeft), 
                                   *(CallbackData->GameScreenTop),
                                   *(CallbackData->GameScreenWidth),
                                   *(CallbackData->GameScreenHeight));

        EndPaint(WindowHandle, &PaintStruct);
      } break;

    default:
      {
        Result = DefWindowProc(WindowHandle, Message, WParam, LParam); 
      } break;
  }
  return(Result);
}

struct win32_sound_output
{
  s32 SamplesPerSecond;
  // TODO(bjorn): Should the index be in bytes instead?
  u32 RunningSampleIndex;
  s32 BytesPerSample;
  s32 SecondaryBufferSize;
  // TODO(bjorn): Should i add bytes/second for making the math cleaer?
  s32 SafetyBytes;
};

  internal_function void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
                     game_sound_output_buffer *SourceBuffer, 
                     LPDIRECTSOUNDBUFFER SecondaryBuffer)
{
  // TODO(bjorn): More streneous checks that these are correct!
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;
  if(SUCCEEDED(SecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                     &Region1, &Region1Size,
                                     &Region2, &Region2Size,
                                     0)))
  {
    // TODO(bjorn): Assert that RegionSize is valid.
    DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
    s16 *DestinationSample = (s16 *)Region1;
    s16 *SourceSample = SourceBuffer->Samples;
    for(DWORD SampleIndex = 0;
        SampleIndex < Region1SampleCount;
        ++SampleIndex)
    {
      *DestinationSample++ = *SourceSample++;
      *DestinationSample++ = *SourceSample++;
      SoundOutput->RunningSampleIndex++;
    }
    DestinationSample = (s16 *)Region2;
    DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
    for(DWORD SampleIndex = 0;
        SampleIndex < Region2SampleCount;
        ++SampleIndex)
    {
      *DestinationSample++ = *SourceSample++;
      *DestinationSample++ = *SourceSample++;
      SoundOutput->RunningSampleIndex++;
    }
    SecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

  internal_function void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput, LPDIRECTSOUNDBUFFER SecondaryBuffer)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;
  if(SUCCEEDED(SecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                     &Region1, &Region1Size,
                                     &Region2, &Region2Size,
                                     0)))
  {
    // TODO(bjorn): Assert that RegionSize is valid.
    s8 *DestinationByte = (s8 *)Region1;
    for(DWORD ByteIndex = 0;
        ByteIndex < Region1Size;
        ++ByteIndex)
    {
      *DestinationByte++ = 0; 
    }
    DestinationByte = (s8 *)Region2;
    for(DWORD ByteIndex = 0;
        ByteIndex < Region2Size;
        ++ByteIndex)
    {
      *DestinationByte++ = 0; 
    }
    SecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

  internal_function void
Win32ProcessKeyboardButton(b32 IsDown, game_button *NewState)
{
  if(NewState->EndedDown != IsDown)
  {
    NewState->EndedDown = IsDown;
    NewState->HalfTransitionCount += 1;
  }
}

  internal_function void 
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button *OldState, 
                                DWORD ButtonBit, game_button *NewState)
{
  NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
  NewState->HalfTransitionCount += (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

  internal_function void
Win32ProcessXInputAnalogStick(game_stick *OldState, game_stick *NewState, 
                              SHORT StickX, SHORT StickY, SHORT DeadZone)
{
  // TODO(bjorn): What stick state do i want to record?
  f32 X;
  if(StickX < (-DeadZone))
  {
    X = ((f32)(-DeadZone) - (f32)StickX) / ((f32)(-DeadZone) - (-32768.0f));
    X *= -1.0f;
  }
  else if(StickX > DeadZone)
  {
    X = ((f32)StickX - (f32)DeadZone) / (32767.0f - (f32)DeadZone);
  }
  else
  {
    X = 0.0f;
  }

  f32 Y;
  if(StickY < (-DeadZone))
  {
    Y = ((f32)(-DeadZone) - (f32)StickY) / ((f32)(-DeadZone) - (-32768.0f));
    Y *= -1.0f;
  }
  else if(StickY > DeadZone)
  {
    Y = ((f32)StickY - (f32)DeadZone) / (32767.0f - (f32)DeadZone);
  }
  else
  {
    Y = 0.0f;
  }

  v2 Vector = {X, Y};
  f32 SquaredLength = LengthSquared(Vector);

  if(SquaredLength > 1.0f)
  {
    f32 InverseLength = InverseSquareRoot(SquaredLength);
    Vector *= InverseLength;
  }

  //
  // NOTE(bjorn): X-axis is - O +
  //
  //                          +
  // NOTE(bjorn): Y-axis is   O
  //                          -
  NewState->Start = OldState->End;
  NewState->End = Vector;
  NewState->Average = (0.5f * NewState->Start) + (0.5f * NewState->End); 
}

  internal_function void
Win32ProcessAnalogStickAsVirtualButton(game_stick *Input, f32 Threshold,
                                       game_stick_virtual_buttons *NewButtons,
                                       game_stick_virtual_buttons *OldButtons)
{
  if(Input->Average.Y > Threshold)
  {
    Win32ProcessXInputDigitalButton(0, &OldButtons->Up, 
                                    0, &NewButtons->Up);
  }
  if(Input->Average.Y < -Threshold)
  {
    Win32ProcessXInputDigitalButton(0, &OldButtons->Down, 
                                    0, &NewButtons->Down);
  }
  if(Input->Average.X > Threshold)
  {
    Win32ProcessXInputDigitalButton(0, &OldButtons->Right,
                                    0, &NewButtons->Right);
  }
  if(Input->Average.X < -Threshold)
  {
    Win32ProcessXInputDigitalButton(0, &OldButtons->Left, 
                                    0, &NewButtons->Left);
  }
}

#if HANDMADE_INTERNAL
  internal_function void
Win32GetInputRecordFileName(win32_state *State, s32 RecordIndex, char * BufferIn)
{
  char FileName[] = "InputRecordX.input";
  char RecordIndexChar = (char)RecordIndex + 0x30;
  for(char *Scan = FileName;
      *Scan;
      ++Scan)
  {
    if(*Scan == 'X')
    {
      *Scan = RecordIndexChar;
    }
    *BufferIn++ = *Scan;
  }
}

  internal_function void
Win32BuildPathToFileInEXEPath(win32_state *State, char *FileName, 
                              s32 DestinationCount, char *Destination)
{
  ConcatenateStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, 
                     State->EXEFileName,
                     StringLength(FileName), FileName,
                     DestinationCount, Destination);
}

  internal_function void
Win32BeginRecordingInput(win32_state *State)
{
  // TODO(bjorn): Lazily copy the memory block to RAM instead and then 
  //              use memcopy for speed?
  char FileName[WIN32_STATE_FILE_NAME_CHAR_COUNT] = {};
  Win32GetInputRecordFileName(State, State->SelectedNumKey, FileName);
  char Path[WIN32_STATE_FILE_NAME_CHAR_COUNT] = {};
  Win32BuildPathToFileInEXEPath(State, FileName, 
                                sizeof(Path), Path);

  State->RecordHandle = CreateFileA(Path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

  DWORD BytesToWrite = (DWORD)State->GameMemoryBlockSize;
  Assert(State->GameMemoryBlockSize == BytesToWrite);
  Assert(State->MemoryRecords[State->SelectedNumKey]);
  CopyMemory(State->MemoryRecords[State->SelectedNumKey], State->GameMemoryBlock,
             BytesToWrite);
}
  internal_function void
Win32RecordInput(win32_state *State, game_input *NewGameInput)
{
  DWORD BytesWritten;
  WriteFile(State->RecordHandle, NewGameInput, sizeof(*NewGameInput),
            &BytesWritten, 0);
}
  internal_function void
Win32StopRecordingInput(win32_state *State)
{
  CloseHandle(State->RecordHandle);
  State->RecordHandle = 0;
}
  internal_function void
Win32BeginInputPlayBack(win32_state *State)
{
  char FileName[WIN32_STATE_FILE_NAME_CHAR_COUNT] = {};
  Win32GetInputRecordFileName(State, State->SelectedNumKey, FileName);

  State->PlayBackHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0,
                                      OPEN_EXISTING, 0, 0);

  DWORD BytesToRead = (DWORD)State->GameMemoryBlockSize;
  Assert(State->GameMemoryBlockSize == BytesToRead);
  Assert(State->MemoryRecords[State->SelectedNumKey]);
  CopyMemory(State->GameMemoryBlock, State->MemoryRecords[State->SelectedNumKey],
             BytesToRead);
  //DWORD BytesRead;
  //ReadFile(State->PlayBackHandle, State->GameMemoryBlock, BytesToRead, &BytesRead, 0);
}
  internal_function void
Win32StopInputPlayBack(win32_state *State)
{
  CloseHandle(State->PlayBackHandle);
  State->PlayBackHandle = 0;
}
  internal_function void
Win32PlayBackInput(win32_state *State, game_input *NewGameInput)
{
  DWORD BytesRead;
  if(ReadFile(State->PlayBackHandle, NewGameInput, sizeof(*NewGameInput), &BytesRead, 0))
  {
    if(BytesRead == 0)
    {
      Win32StopInputPlayBack(State);
      Win32BeginInputPlayBack(State);
      ReadFile(State->PlayBackHandle, NewGameInput, sizeof(*NewGameInput), &BytesRead, 0);
    }
  }
}
#endif

// NOTE(bjorn): This code is from a blogpost by Raymon Chen. 
// Source: https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};

void Win32ToggleFullscreen(HWND WindowHandle, 
                           s32* GameScreenLeft, s32* GameScreenTop, 
                           s32* GameScreenWidth, s32* GameScreenHeight) 
{
  DWORD WindowStyle = GetWindowLong(WindowHandle, GWL_STYLE);

  if(WindowStyle & WS_OVERLAPPEDWINDOW) 
  {
    MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };

    if(GetWindowPlacement(WindowHandle, &GlobalWindowPosition) &&
       GetMonitorInfo(MonitorFromWindow(WindowHandle, 
                                        MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) 
    {
      s32 ScreenWidth = MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left;
      s32 ScreenHeight = MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top;

      SetWindowLong(WindowHandle, GWL_STYLE, WindowStyle & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(WindowHandle, HWND_TOP,
                   MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                   ScreenWidth, ScreenHeight,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

      *GameScreenTop = 0; 
      *GameScreenLeft = 0; 
      *GameScreenWidth = ScreenWidth;
      *GameScreenHeight = ScreenHeight; 
    }
  } 
  else 
  {
    SetWindowLong(WindowHandle, GWL_STYLE, WindowStyle | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(WindowHandle, &GlobalWindowPosition);
    SetWindowPos(WindowHandle, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    *GameScreenTop = 10; 
    *GameScreenLeft = 10; 
    *GameScreenWidth = GAME_RGB_BUFFER_WIDTH;
    *GameScreenHeight = GAME_RGB_BUFFER_HEIGHT; 
  }
}

  internal_function void
Win32ProcessWindowMessages(HWND WindowHandle, b32 *GameIsRunning, u8 *WindowAlpha, 
                           win32_state *Win32State, 
                           game_keyboard *NewKeyboard,
                           game_mouse *NewMouse,
                           win32_window_callback_data* WindowCallbackData)
{
  u32 UnicodeCodePointIndex = 0;
  u16 PrevChar = 0;
  MSG Message;
  while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&Message);

    switch(Message.message)
    {
      case WM_MOUSEWHEEL:
        {
          NewMouse->Wheel = (f32)GET_WHEEL_DELTA_WPARAM(Message.wParam) / (f32)WHEEL_DELTA;
        } break;
      case WM_CHAR:
        {
          Assert(ArrayCount(NewKeyboard->UnicodeIn) != UnicodeCodePointIndex);
          b32 IsLowEnd = ((u16)Message.wParam & 0b1101110000000000) == 0b1111110000000000;
          b32 IsHighEnd = ((u16)Message.wParam & 0b1101100000000000) == 0b1111110000000000;
          if(IsLowEnd)
          {
            PrevChar = (u16)Message.wParam;
            break;
          }

          u32 UnicodeCodePoint;
          if(PrevChar)
          {
            Assert(IsHighEnd);
            UnicodeCodePoint = ((u32)((PrevChar << 6) >> 6) << 10) + (u32)(((u16)Message.wParam << 6) >> 6);

            PrevChar = 0;
          }
          else
          {
            UnicodeCodePoint = (u32)Message.wParam;
          }

          if(UnicodeCodePoint == '\r') { UnicodeCodePoint = '\n'; }
          NewKeyboard->UnicodeIn[UnicodeCodePointIndex++] = UnicodeCodePoint;
          NewKeyboard->UnicodeInCount = UnicodeCodePointIndex;
        } break;
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP:
        {
          u32 VKCode = (u32)Message.wParam;
          b32 WasDown = ((Message.lParam & (1 << 30)) != 0);
          b32 IsDown = ((Message.lParam & (1 << 31)) == 0);

          // NOTE(bjorn): IsDown != WasDown is just there to weed out the repeating
          //              key presses.
          if(IsDown != WasDown)
          {
#if HANDMADE_INTERNAL
            b32 AltIsDown = ((GetKeyState(VK_MENU) & 0x8000) != 0);

            switch(VKCode)
            {
              case VK_F4:
                {
                  *GameIsRunning = false;
                } break;
              case '0':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
              case '8':
              case '9':
                {
                  if(IsDown)
                  {
                    Win32State->SelectedNumKey = VKCode - 0x30;
                  }
                }break;
              case 'T':
                {
                  if(IsDown && AltIsDown)
                  {
                    DWORD WindowExtendedStyle = GetWindowLong(WindowHandle, GWL_EXSTYLE);

                    b32 IsTopmostWindow = ((WindowExtendedStyle & WS_EX_TOPMOST) != 0);
                    if(IsTopmostWindow)
                    {
                      SetWindowPos(WindowHandle, HWND_NOTOPMOST,
                                   0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
                    }
                    else
                    {
                      SetWindowPos(WindowHandle, HWND_TOPMOST,
                                   0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
                    }
                  }
                }break;
              case 'A':
                {
                  if(IsDown && AltIsDown)
                  {
                    *WindowAlpha = *WindowAlpha == 255 ? 64 : 255;
                  }
                }break;
              case 'R':
                {
                  if(IsDown && AltIsDown)
                  {
                    Win32StopRecordingInput(Win32State);
                    Win32StopInputPlayBack(Win32State);
                    Win32BeginRecordingInput(Win32State);
                  }
                }break;
              case 'Q':
                {
                  if(IsDown && AltIsDown)
                  {
                    Win32StopRecordingInput(Win32State);
                    Win32StopInputPlayBack(Win32State);
                  }
                }break;
              case 'P':
                {
                  if(IsDown && AltIsDown)
                  {
                    Win32StopRecordingInput(Win32State);
                    Win32StopInputPlayBack(Win32State);
                    Win32BeginInputPlayBack(Win32State);
                  }
                }break;
              case 'F':
                {
                  if(IsDown && AltIsDown)
                  {
                    Win32ToggleFullscreen(WindowHandle, 
                                          WindowCallbackData->GameScreenLeft, 
                                          WindowCallbackData->GameScreenTop,
                                          WindowCallbackData->GameScreenWidth,
                                          WindowCallbackData->GameScreenHeight);
                  }
                }break;
            };
#endif
            if('0' <= VKCode && VKCode <= '9')
            {
              s32 NumberKey = VKCode - '0';
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Numbers[NumberKey]);
            }
            if('A' <= VKCode && VKCode <= 'Z')
            {
              s32 LetterKey = VKCode - 'A';
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Letters[LetterKey]);
            }
            if(VKCode == VK_SHIFT)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Shift);
            }
            if(VKCode == VK_CONTROL)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Control);
            }
            if(VKCode == VK_MENU)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Alt);
            }
            if(VKCode == VK_RETURN)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Enter);
            }
            if(VKCode == VK_SPACE)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Space);
            }
            if(VKCode == VK_UP)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Up);
            }
            if(VKCode == VK_RIGHT)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Right);
            }
            if(VKCode == VK_DOWN)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Down);
            }
            if(VKCode == VK_LEFT)
            {
              Win32ProcessKeyboardButton(IsDown, &NewKeyboard->Left);
            }
          }
        } break;
      default:
        {
          DispatchMessage(&Message);
        } break;
    }
  }
}

  inline LARGE_INTEGER
Win32GetWallClock()
{
  LARGE_INTEGER Counter;
  QueryPerformanceCounter(&Counter);
  return Counter;
}

  inline f32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End, s64 Frequency)
{
  return ((f32)(End.QuadPart - Start.QuadPart) / (f32)Frequency);
}

  internal_function void
Win32GetEXEFileName(win32_state *State)
{
  DWORD SizeOfFileName = GetModuleFileNameA(0, State->EXEFileName, 
                                            sizeof(State->EXEFileName));
  State->OnePastLastEXEFileNameSlash = State->EXEFileName;
  for(char *Scan = State->EXEFileName;
      *Scan;
      ++Scan)
  {
    if(*Scan == '\\')
    {
      State->OnePastLastEXEFileNameSlash = Scan + 1;
    }
  }
}

  internal_function void
HandleDebugCycleCounters(game_memory* Memory)
{
#if HANDMADE_INTERNAL
  OutputDebugStringA("DEBUG CYCLE COUNTS:\n");
  for(s32 CounterIndex = 0;
      CounterIndex < ArrayCount(Memory->Counters);
      CounterIndex++)
  {
    debug_cycle_counter* Counter = Memory->Counters + CounterIndex;

    if(Counter->HitCount)
    {
      char TextBuffer[256];
      sprintf_s(TextBuffer, 
                "%20.20s: %I64ucy %I64uh %I64ucy/h\n",
                Counter->String, Counter->CycleCount, Counter->HitCount, 
                Counter->CycleCount / Counter->HitCount);
      OutputDebugStringA(TextBuffer);

      Counter->CycleCount = 0;
      Counter->HitCount = 0;
    }
  }
#endif
}

//TODO(bjorn): Multiple producer multiple consumer.
  internal_function void
Win32PushWork(work_queue* Queue, work_queue_callback* Callback, void* Data)
{
  u32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1)% Queue->MaxEntryCount;
  Assert(Queue->NextEntryToRead != NewNextEntryToWrite);

  work_queue_entry* Entry = Queue->Entries + Queue->NextEntryToWrite;
  Entry->Callback = Callback;
  Entry->Data     = Data;

  Queue->WorkCount++;

  _WriteBarrier();
  Queue->NextEntryToWrite = NewNextEntryToWrite;

  ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

struct memory_and_size
{
  u8* Memory;
  memi Size;
};
  internal_function memory_arena*
Win32GetCurrentThreadMemoryArena(work_queue* Queue)
{
  memory_arena* Result = 0;

  for(s32 Index = 0;
      Index < Queue->ThreadCount;
      Index++)
  {
    thread_id_memory_mapping* Mapping = Queue->IdToMemory + Index;
    if(Mapping->Id == GetCurrentThreadId())
    {
      Result = &Mapping->Arena;
    }
  }

  return Result;
}

  inline b32 
Win32DoMultithreadedWork(work_queue* Queue)
{
  b32 ShouldSleep = false;

  u32 OriginalNextEntryToRead = Queue->NextEntryToRead;
  u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1)% Queue->MaxEntryCount;

  if(OriginalNextEntryToRead == Queue->NextEntryToWrite)
  {
    ShouldSleep = true;
  }
  else
  {
    u32 Test = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToRead,
                                          NewNextEntryToRead,
                                          OriginalNextEntryToRead);
    if(Test == OriginalNextEntryToRead)
    {
      work_queue_entry* Entry = Queue->Entries + OriginalNextEntryToRead;
      memory_arena* Arena = Win32GetCurrentThreadMemoryArena(Queue);
      Entry->Callback(Entry->Data, Arena);

      InterlockedIncrement((LONG volatile*)&Queue->CompletedWorkCount);
    }
  }

  return ShouldSleep;
}

  internal_function void
Win32CompleteWork(work_queue* Queue)
{
  while(Queue->CompletedWorkCount < Queue->WorkCount) 
  { 
    Win32DoMultithreadedWork(Queue); 
  }

  Queue->WorkCount = 0;
  Queue->CompletedWorkCount = 0;
}

  DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
  work_queue* Queue = (work_queue*)lpParameter;
  for(;;)
  {
    if(Win32DoMultithreadedWork(Queue))
    {
      WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
    }
  }
}

#if 0
  internal_function
WORK_QUEUE_CALLBACK(PrintStringJob)
{
  char Buffer[256];
  wsprintfA(Buffer, "Thread %u: %s\n", GetCurrentThreadId(), Data);
  OutputDebugStringA(Buffer);
}

work_queue_entry TestQueueEntries[20];
#endif
  internal_function void 
Win32UrlIteratorCallback(char const* Url, void* UserData)
{
  char* Buffer = (char*)UserData;
  if( *Buffer == '\0' )
  { 
    if(StringLength((char*)Url) < 256 )
    {
      while(*Url != '\0') { *Buffer++ = *Url++; }
    }
  }
}

struct latest_gaze_point
{
  v2 P;
  b32 Unused;
};
  internal_function void
Win32GazePointCallback(tobii_gaze_point_t const* GazePoint, void* UserData)
{
  latest_gaze_point* LatestGazePoint = (latest_gaze_point*)UserData;
  if(GazePoint->validity == TOBII_VALIDITY_VALID)
  {
    LatestGazePoint->P = {GazePoint->position_xy[0], GazePoint->position_xy[1]};
    LatestGazePoint->Unused = true;
#if 0
      {
        char TextBuffer[256];
        sprintf_s(TextBuffer, 
                  "Gaze X:%f Gaze Y:%f\n",
                  GazePoint->position_xy[0], GazePoint->position_xy[1]);
        OutputDebugStringA(TextBuffer);
      }
#endif
  }
}

internal_function void
Win32InitOpenGL(HWND Window)
{
  HDC WindowDC = GetDC(Window);

  PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
  DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
  DesiredPixelFormat.nVersion = 1;
  DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
  DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
  DesiredPixelFormat.cColorBits = 32;
  DesiredPixelFormat.cAlphaBits = 8;
  DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

  s32 SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
  PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
  DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(SuggestedPixelFormat), 
                      &SuggestedPixelFormat);
  SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

  HGLRC OpenGLRC = wglCreateContext(WindowDC);
  if(wglMakeCurrent(WindowDC, OpenGLRC))
  {
  }
  else
  {
    InvalidCodePath;
    //TODO(Bjorn): Diagnostic
  }
  ReleaseDC(Window, WindowDC);
}

#if HANDMADE_INTERNAL
game_memory* DebugGlobalMemory; 
#endif

  s32 CALLBACK 
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        s32 Show)
{
#if 0
  work_queue Queue = {};

  u32 InitialCount = 0;
  u32 ThreadCount = 4-1;
  HANDLE SemaphoreHandle = CreateSemaphoreEx(0,
                                             InitialCount,
                                             ThreadCount,
                                             0, 0, SEMAPHORE_ALL_ACCESS);
  Queue.SemaphoreHandle = SemaphoreHandle;
  Queue.Entries = TestQueueEntries;
  Queue.MaxEntryCount = ArrayCount(TestQueueEntries);

  for(u32 ThreadIndex = 0;
      ThreadIndex < ThreadCount;
      ThreadIndex++)
  {
    DWORD ThreadID;
    HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, &Queue, 0, &ThreadID);
    CloseHandle(ThreadHandle);
  }

  Win32PushWork(&Queue, PrintStringJob, "String: A0");
  Win32PushWork(&Queue, PrintStringJob, "String: A1");
  Win32PushWork(&Queue, PrintStringJob, "String: A2");
  Win32PushWork(&Queue, PrintStringJob, "String: A3");
  Win32PushWork(&Queue, PrintStringJob, "String: A4");
  Win32PushWork(&Queue, PrintStringJob, "String: A5");
  Win32PushWork(&Queue, PrintStringJob, "String: A6");
  Win32PushWork(&Queue, PrintStringJob, "String: A7");
  Win32PushWork(&Queue, PrintStringJob, "String: A8");
  Win32PushWork(&Queue, PrintStringJob, "String: A9");

  Sleep(1000);

  Win32PushWork(&Queue, PrintStringJob, "String: B0");
  Win32PushWork(&Queue, PrintStringJob, "String: B1");
  Win32PushWork(&Queue, PrintStringJob, "String: B2");
  Win32PushWork(&Queue, PrintStringJob, "String: B3");
  Win32PushWork(&Queue, PrintStringJob, "String: B4");
  Win32PushWork(&Queue, PrintStringJob, "String: B5");
  Win32PushWork(&Queue, PrintStringJob, "String: B6");
  Win32PushWork(&Queue, PrintStringJob, "String: B7");
  Win32PushWork(&Queue, PrintStringJob, "String: B8");
  Win32PushWork(&Queue, PrintStringJob, "String: B9");

  Win32CompleteWork(&Queue);
#endif

  //TODO(bjorn): This api call is intended for Vista/Win7 only. Win8.1 and
  //onwards should use SetProcessDPIAwareness instead.
  //SetProcessDPIAware();

  win32_state Win32State = {};

  Win32GetEXEFileName(&Win32State);

  win32_game Handmade = {};
  DebugGlobalMemory = &Handmade.Memory;
  Win32BuildPathToFileInEXEPath(&Win32State, "game.dll", 
                                sizeof(Handmade.DLLFullPath), Handmade.DLLFullPath);
  Win32BuildPathToFileInEXEPath(&Win32State, "game_temp.dll", 
                                sizeof(Handmade.TempDLLFullPath), Handmade.TempDLLFullPath);

#ifdef HANDMADE_INTERNAL
  {
    char* Scan = Win32State.OnePastLastEXEFileNameSlash;
    b32 Fits = false;
    while(Scan-- != Win32State.EXEFileName) 
    { 
      Fits =
        Scan[0] == 'b' && 
        Scan[1] == 'u' && 
        Scan[2] == 'i' && 
        Scan[3] == 'l' && 
        Scan[4] == 'd' ;

      if(Fits) { break; }
    }

    char* Copy = Win32State.EXEFileName;
    char* Target = GlobalPathToGameRoot;
    while(Copy != Scan) { *Target++ = *Copy++; }
  }
#endif

  LARGE_INTEGER PerformanceCountFrequencyResult;
  QueryPerformanceFrequency(&PerformanceCountFrequencyResult);
  s64 PerformanceCountFrequency = PerformanceCountFrequencyResult.QuadPart;

  // NOTE(bjorn): Sets the Windows scheduler granularity to 1 ms (make Sleep()
  // more granular)
  UINT DesiredScheduler_ms = 1;
  b32 SleepIsGranular = (timeBeginPeriod(DesiredScheduler_ms) == TIMERR_NOERROR);

  Win32LoadXInput();

  WNDCLASSW WindowClass = {};
  WindowClass.style = CS_HREDRAW|CS_VREDRAW;
  WindowClass.lpfnWndProc = &Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
  WindowClass.lpszClassName = L"HandmadeWindowClass";

  RegisterClassW(&WindowClass);
  HWND WindowHandle = CreateWindowExW(WS_EX_LAYERED|WS_EX_TOPMOST,
                                      WindowClass.lpszClassName,
                                      L"Handmade",
                                      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                      // TODO(bjorn): Use AdjustWindowRect to calculate this
                                      //              automatically.
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      0, 0, Instance, 0);
  if(WindowHandle)
  {
    Win32InitOpenGL(WindowHandle);

    b32 GameIsRunning = true;
    u8 WindowAlpha = 255;

    s32 GameScreenLeft = 10; 
    s32 GameScreenTop = 10; 
    s32 GameScreenWidth = GAME_RGB_BUFFER_WIDTH;
    s32 GameScreenHeight = GAME_RGB_BUFFER_HEIGHT; 

    win32_window_callback_data WindowCallbackData = {};
    WindowCallbackData.GameIsRunning = &GameIsRunning;
    WindowCallbackData.WindowAlpha = &WindowAlpha;
    WindowCallbackData.GameScreenLeft = &GameScreenLeft;
    WindowCallbackData.GameScreenTop = &GameScreenTop; 
    WindowCallbackData.GameScreenWidth = &GameScreenWidth; 
    WindowCallbackData.GameScreenHeight = &GameScreenHeight;
    {
      SetLastError(0);
      LONG_PTR PrevPointer = SetWindowLongPtr(WindowHandle, GWLP_USERDATA,
                                              (LONG_PTR)(&WindowCallbackData));
      if(PrevPointer == 0 && GetLastError() != 0)
      {
        // TODO(bjorn): Handle this error in the future. 
        return 0;
      }
    }

    SetWindowPos(WindowHandle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);

    // TODO(bjorn): How do we really query monitor refresh rate on windows?
    HDC RefreshDC = GetDC(WindowHandle);
    s32 MonitorRefreshHz = 60;
    s32 Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
    ReleaseDC(WindowHandle, RefreshDC);

    if(Win32RefreshRate > 1)
    {
      MonitorRefreshHz = Win32RefreshRate;
    }
    f32 GameUpdateHz = ((f32)MonitorRefreshHz / 1.0f);
    f32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

    win32_sound_output SoundOutput = {};

    // TODO(bjorn): Maybe make the soundbuffer a minute long instead of a second.
    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = sizeof(s16)*2;
    SoundOutput.SecondaryBufferSize = (SoundOutput.SamplesPerSecond * 
                                       SoundOutput.BytesPerSample);
    SoundOutput.SafetyBytes = (s32)((((f32)SoundOutput.SamplesPerSecond*
                                      (f32)SoundOutput.BytesPerSample) / 
                                     (f32)GameUpdateHz) / 2.0f);
    // TODO(bjorn): Handle sound startup.
    b32 SoundFirstTimeAround = true;

    LPDIRECTSOUNDBUFFER SecondarySoundBuffer = 
      Win32InitDSound(WindowHandle, 
                      SoundOutput.SamplesPerSecond, 
                      SoundOutput.SecondaryBufferSize);
    Win32ClearSoundBuffer(&SoundOutput, SecondarySoundBuffer);
    SecondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

    s16 *SoundSamplesBuffer = (s16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, 
                                                  MEM_RESERVE|MEM_COMMIT,
                                                  PAGE_READWRITE);

    // TODO(bjorn): As for this fix for in-game game switching, since the memrecords are
    //              shared, the size implicitly has to be shared between the games. I
    //              might want to fix this in the future.
    u64 CommonPermanentStorageSize = Megabytes(256);
    // TODO(bjorn): TransientStorage needs to be broken up into game transient
    // and cache transient. Only game transient needs to be stored for state
    // playback.
    u64 CommonTransientStorageSize = Gigabytes(2);
    u64 CommonStorageTotalSize = CommonPermanentStorageSize + CommonTransientStorageSize;
#if HANDMADE_INTERNAL
    Handmade.Memory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
    Handmade.Memory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
    Handmade.Memory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
    Handmade.Memory.DEBUGPlatformGetFileEditTimestamp = DEBUGPlatformGetFileEditTimestamp;

    for(s32 RecordIndex = 0;
        RecordIndex < 10;
        ++RecordIndex)
    {
      Win32State.MemoryRecords[RecordIndex] = VirtualAlloc(0, 
                                                           CommonPermanentStorageSize, 
                                                           MEM_RESERVE|MEM_COMMIT,
                                                           PAGE_READWRITE);
    }
#endif

#if HANDMADE_INTERNAL
    void *HandmadeBaseAddress = (void *)Terabytes(1);
    void *MyGameBaseAddress = (void *)Terabytes(2);
#else
    void *HandmadeBaseAddress = 0;
    void *MyGameBaseAddress = 0;
#endif
    {
      u64 HandmadeTotalStorageSize = (CommonPermanentStorageSize + 
                                      CommonTransientStorageSize);
      u8* MainAllocation = (u8*)VirtualAlloc(HandmadeBaseAddress, 
                                             HandmadeTotalStorageSize, 
                                             MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      Handmade.Memory.Permanent = 
        InitializeArena((memory_arena*)MainAllocation, 
                        CommonPermanentStorageSize - sizeof(memory_arena), 
                        MainAllocation + sizeof(memory_arena));
      Handmade.Memory.Transient = 
        InitializeArena((memory_arena*)(MainAllocation + CommonPermanentStorageSize), 
                        CommonTransientStorageSize - sizeof(memory_arena), 
                        MainAllocation + sizeof(memory_arena) + CommonPermanentStorageSize);
    }

    Win32State.GameMemoryBlockSize = CommonPermanentStorageSize;
    Win32State.GameMemoryBlock = (u8*)Handmade.Memory.Permanent;

    {
      u32 ThreadCount = 0;

      u32 NUMANodeCount = 0;
      u32 ProcessorCoreCount = 0;
      u32 LogicalProcessorCount = 0;
      u32 ProcessorL1CacheCount = 0;
      u32 ProcessorL2CacheCount = 0;
      u32 ProcessorL3CacheCount = 0;
      u32 ProcessorPackageCount = 0;

      DWORD ReturnedLength = 0;
      if(SUCCEEDED(GetLogicalProcessorInformation(0, &ReturnedLength)))
      {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* Buffer = 
          (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)VirtualAlloc(0, ReturnedLength, 
                                                              MEM_RESERVE|MEM_COMMIT, 
                                                              PAGE_READWRITE);
        if(SUCCEEDED(GetLogicalProcessorInformation(Buffer, &ReturnedLength)))
        {
          DWORD ByteOffset = 0;
          while ((ByteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)) <= ReturnedLength) 
          {
            switch (Buffer->Relationship) 
            {
              case RelationNumaNode
                : {
                  NUMANodeCount++;
                } break;

              case RelationProcessorCore
                : {
                  ProcessorCoreCount++;

                  //TODO(bjorn): On a system with more than 64 processors, the processor mask 
                  //identifies processors in a single processor group.
                  s32 SetBitCount = 0;
                  ULONG_PTR BitMask = 0x1;
                  for(s32 BitIndex = 0; 
                      BitIndex < sizeof(ULONG_PTR)*8;
                      BitIndex++, BitMask<<=1)
                  {
                    SetBitCount += (Buffer->ProcessorMask & BitMask) ? 1 : 0;
                  }
                  LogicalProcessorCount += SetBitCount;
                } break;

              case RelationCache
                : {
                  PCACHE_DESCRIPTOR Cache = &Buffer->Cache;
                  if (Cache->Level == 1)
                  {
                    ProcessorL1CacheCount++;
                  }
                  else if (Cache->Level == 2)
                  {
                    ProcessorL2CacheCount++;
                  }
                  else if (Cache->Level == 3)
                  {
                    ProcessorL3CacheCount++;
                  }
                } break;

              case RelationProcessorPackage
                : {
                  ProcessorPackageCount++;
                } break;

              InvalidDefaultCase;
            }

            ByteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
            Buffer++;
          }

#if 1
          char TextBuffer[256];
          sprintf_s(TextBuffer, 
                    "NUMANodeCount:%d\nProcessorCoreCount:%d\nLogicalProcessorCount:%d\nProcessorL1CacheCount:%d\nProcessorL2CacheCount:%d\nProcessorL3CacheCount:%d\nProcessorPackageCount:%d\n",
                    NUMANodeCount,
                    ProcessorCoreCount,
                    LogicalProcessorCount,
                    ProcessorL1CacheCount,
                    ProcessorL2CacheCount,
                    ProcessorL3CacheCount,
                    ProcessorPackageCount
                   );
          OutputDebugStringA(TextBuffer);
#endif
          ThreadCount = LogicalProcessorCount-1;
        }

        VirtualFree(Buffer, 0, MEM_RELEASE);
      }
      Assert(ThreadCount > 0);

      u32 NumberOfLowPriorityThreads = 1;
      u32 NumberOfHighPriorityThreads = ThreadCount-NumberOfLowPriorityThreads;

      u32 InitialCount = 0;
      HANDLE SemaphoreHandleHigh = CreateSemaphoreEx(0,
                                                     InitialCount,
                                                     NumberOfHighPriorityThreads,
                                                     0, 0, SEMAPHORE_ALL_ACCESS);
      Handmade.Memory.HighPriorityQueue.SemaphoreHandle = SemaphoreHandleHigh;
      Handmade.Memory.HighPriorityQueue.Entries = Handmade.Memory.HighPriorityQueueEntries;
      Handmade.Memory.HighPriorityQueue.MaxEntryCount = 
        ArrayCount(Handmade.Memory.HighPriorityQueueEntries);
      Handmade.Memory.HighPriorityQueue.IdToMemory = Handmade.Memory.IdToMemoryMappingEntries;
      Handmade.Memory.HighPriorityQueue.ThreadCount = ThreadCount;

      for(u32 ThreadIndex = 0;
          ThreadIndex < NumberOfHighPriorityThreads;
          ThreadIndex++)
      {
        DWORD ThreadID;
        work_queue* Queue = &Handmade.Memory.HighPriorityQueue;
        HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);
        Queue->IdToMemory[ThreadIndex].Id = ThreadID;
        CloseHandle(ThreadHandle);
      }

      HANDLE SemaphoreHandleLow = CreateSemaphoreEx(0,
                                                    InitialCount,
                                                    NumberOfLowPriorityThreads,
                                                    0, 0, SEMAPHORE_ALL_ACCESS);
      Handmade.Memory.LowPriorityQueue.SemaphoreHandle = SemaphoreHandleLow;
      Handmade.Memory.LowPriorityQueue.Entries = Handmade.Memory.LowPriorityQueueEntries;
      Handmade.Memory.LowPriorityQueue.MaxEntryCount = 
        ArrayCount(Handmade.Memory.LowPriorityQueueEntries);
      Handmade.Memory.LowPriorityQueue.IdToMemory = Handmade.Memory.IdToMemoryMappingEntries;
      Handmade.Memory.LowPriorityQueue.ThreadCount = ThreadCount;

      for(u32 ThreadIndex = NumberOfHighPriorityThreads;
          ThreadIndex < (NumberOfHighPriorityThreads + NumberOfLowPriorityThreads);
          ThreadIndex++)
      {
        DWORD ThreadID;
        work_queue* Queue = &Handmade.Memory.LowPriorityQueue;
        HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);

        Queue->IdToMemory[ThreadIndex].Id = ThreadID;
        SubArena(&Queue->IdToMemory[ThreadIndex].Arena, Handmade.Memory.Transient, Megabytes(8));

        CloseHandle(ThreadHandle);
      }
    }

    render_group* GameRenderGroup = PushStruct(Handmade.Memory.Transient, 
                                               render_group);
    GameRenderGroup->PushBufferSize = 0;
    GameRenderGroup->MaxPushBufferSize = Megabytes(64);
    GameRenderGroup->PushBufferBase = PushArray(Handmade.Memory.Transient, 
                                                GameRenderGroup->MaxPushBufferSize,
                                                u8);

    game_assets* GameAssets = PushStruct(Handmade.Memory.Transient, game_assets);
    SubArena(&GameAssets->Arena, Handmade.Memory.Transient, Gigabytes(1));

    LoadAllAssets(GameAssets);
    //TODO(bjorn): Develop the game a bit more before delving into the asset system.
    //TODO If game gets big enough:
    //some sort of 
    // if(AssetIDSetLoaded(AssetIDSet))
    // {
    //    LockAssetIDSet(AssetIDSet); //DEBUG log failed fetches?
    // }
    {
      game_bitmap* MainScreen = PushStruct(&GameAssets->Arena, game_bitmap);
      *MainScreen = {};

      GameAssets->Bitmaps[GAI_MainScreen] = MainScreen;
      GameAssets->BitmapsMeta[GAI_MainScreen].State = AssetState_Locked;
    }

    Handmade.Memory.PushWork = Win32PushWork;
    Handmade.Memory.CompleteWork = Win32CompleteWork;

    game_input OldGameInput = {};

#if HANDMADE_INTERNAL 
    s32 DebugTimeMarkerIndex = 0;
    win32_debug_time_marker DebugTimeMarker[15] = {};
#endif

    Handmade.Code = Win32LoadGameCode(Handmade.DLLFullPath, Handmade.TempDLLFullPath);
    Handmade.Code.DLLLastWriteTime = Win32GetLastWriteTime(Handmade.DLLFullPath);

    win32_game *Game = &Handmade;

    tobii_device_t* TobiiDevice = 0;
    {
      tobii_api_t* TobiiApi = 0;
      if(tobii_api_create(&TobiiApi, 0, 0) == TOBII_ERROR_NO_ERROR)
      {
        Assert(TobiiApi);

        char Url[256] = {};
        if(tobii_enumerate_local_device_urls(TobiiApi, Win32UrlIteratorCallback, Url) 
           == TOBII_ERROR_NO_ERROR)
        {
          if(tobii_device_create(TobiiApi, Url, TOBII_FIELD_OF_USE_INTERACTIVE, &TobiiDevice)
             == TOBII_ERROR_NO_ERROR)
          {
            Assert(TobiiDevice);
          }
        }
      }
    }

    LARGE_INTEGER TobiiLastSync = Win32GetWallClock();
    latest_gaze_point LatestGazePoint = {};
    if(TobiiDevice)
    {
      tobii_error_t Error;
      Error = tobii_gaze_point_subscribe(TobiiDevice, Win32GazePointCallback, &LatestGazePoint);
      Assert(Error == TOBII_ERROR_NO_ERROR);
      Error = tobii_device_clear_callback_buffers(TobiiDevice);
      Assert(Error == TOBII_ERROR_NO_ERROR);

      OutputDebugStringA("Tobii Device Subscribed\n");
    }

    LARGE_INTEGER LastCounter = Win32GetWallClock();
    u64 LastCycleCount = __rdtsc();

    s32 MonitorWidth = 0;
    s32 MonitorHeight = 0;
    {
      MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
      GetMonitorInfo(MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo);
      MonitorWidth = MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left;
      MonitorHeight = MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top;
#if 0
      {
        char TextBuffer[256];
        sprintf_s(TextBuffer, 
                  "Monitor W:%d Monitor H:%d\n",
                  MonitorWidth, MonitorHeight);
        OutputDebugStringA(TextBuffer);
      }
#endif
    }

    while(GameIsRunning)
    {
      game_input NewGameInput = {};

      if(TobiiDevice)
      {
        tobii_error_t Error;

        f32 SecondsElapsedFromTobiiSync = 
          Win32GetSecondsElapsed(TobiiLastSync, Win32GetWallClock(), PerformanceCountFrequency); 
        if(SecondsElapsedFromTobiiSync >= 30.0f)
        {
          Error = tobii_update_timesync(TobiiDevice);
          Assert(Error == TOBII_ERROR_NO_ERROR);
          TobiiLastSync = Win32GetWallClock();
        }

        Error = tobii_device_process_callbacks(TobiiDevice);
        Assert(Error != TOBII_ERROR_INVALID_PARAMETER);
        Assert(Error != TOBII_ERROR_ALLOCATION_FAILED);
        Assert(Error != TOBII_ERROR_INTERNAL);
        Assert(Error != TOBII_ERROR_CALLBACK_IN_PROGRESS);
        if(Error == TOBII_ERROR_CONNECTION_FAILED)
        {
          Error = tobii_device_reconnect(TobiiDevice);
          Assert(Error == TOBII_ERROR_NO_ERROR);
        }

        //TODO(Bjorn): Move this to everytime there's new data?
        NewGameInput.EyeTracker.IsConnected = true;
      }

      NewGameInput.EyeTracker.P = OldGameInput.EyeTracker.P;
      if(LatestGazePoint.Unused)
      {
        POINT Win32PixelPoint;
        Win32PixelPoint.x = (s32)(LatestGazePoint.P.X * MonitorWidth);
        Win32PixelPoint.y = (s32)(LatestGazePoint.P.Y * MonitorHeight);

        ScreenToClient(WindowHandle, &Win32PixelPoint);

        f32 RelativeX = (f32)(Win32PixelPoint.x - GameScreenLeft) / (f32)GameScreenWidth;
        f32 RelativeY = (f32)(Win32PixelPoint.y - GameScreenTop) / (f32)GameScreenHeight;

        //RelativeX = Clamp01(RelativeX);
        //RelativeY = Clamp01(RelativeY);
        NewGameInput.EyeTracker.P = {RelativeX, RelativeY};
#if 0
        {
          char TextBuffer[256];
          sprintf_s(TextBuffer, 
                    "Game Gaze X:%f Game Gaze Y:%f\n",
                    RelativeX, RelativeY);
          OutputDebugStringA(TextBuffer);
        }
#endif
        LatestGazePoint.Unused = false;
      }

#if HANDMADE_INTERNAL
      FILETIME LastWriteTime = Win32GetLastWriteTime(Game->DLLFullPath);
      if(CompareFileTime(&Game->Code.DLLLastWriteTime, &LastWriteTime) != 0) 
      {
        Win32UnloadGameCode(&Game->Code);
        Game->Code = Win32LoadGameCode(Game->DLLFullPath, Game->TempDLLFullPath);
        Game->Code.DLLLastWriteTime = LastWriteTime;
        NewGameInput.ExecutableReloaded = true;
      }
#endif

      // TODO(bjorn): Should this be polled more often?
      s32 MaxControllerCount = XUSER_MAX_COUNT;
      if(MaxControllerCount > ArrayCount(NewGameInput.Controllers))
      {
        MaxControllerCount = ArrayCount(NewGameInput.Controllers);
      }
      for(s32 ControllerIndex = 1;
          ControllerIndex <= MaxControllerCount;
          ControllerIndex++)
      {
        game_controller *OldController = GetController(&OldGameInput, ControllerIndex);
        game_controller *NewController = GetController(&NewGameInput, ControllerIndex);

        XINPUT_STATE ControllerState = {};
        // TODO(bjorn): Make a better check for if there is a controller connected or not.
        //              This call apparently lags on older versions if no controller is
        //              connected.
        if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
        {
          for(int ButtonIndex = 0;
              ButtonIndex < ArrayCount(OldController->Buttons);
              ButtonIndex++)
          {
            NewController->Buttons[ButtonIndex].EndedDown = 
              OldController->Buttons[ButtonIndex].EndedDown;
          }

          // TODO(bjorn): See if ControllerState.dwPacketNumber increments too rapidly.
          XINPUT_GAMEPAD *Gamepad = &ControllerState.Gamepad;

          NewController->IsConnected = true;
          NewController->LeftIsAnalog = true;
          NewController->RightIsAnalog = true;
          Win32ProcessXInputAnalogStick(&OldController->LeftStick, 
                                        &NewController->LeftStick, 
                                        Gamepad->sThumbLX, Gamepad->sThumbLY,
                                        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
          Win32ProcessAnalogStickAsVirtualButton(&NewController->LeftStick,
                                                 GAME_STICK_VIRTUAL_BUTTON_THRESHOLD,
                                                 &NewController->LeftStickVrtBtn,
                                                 &OldController->LeftStickVrtBtn);

          Win32ProcessXInputAnalogStick(&OldController->RightStick, 
                                        &NewController->RightStick, 
                                        Gamepad->sThumbRX, Gamepad->sThumbRY,
                                        XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
          Win32ProcessAnalogStickAsVirtualButton(&NewController->RightStick, 
                                                 GAME_STICK_VIRTUAL_BUTTON_THRESHOLD,
                                                 &NewController->RightStickVrtBtn,
                                                 &OldController->RightStickVrtBtn);

          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->Start, 
                                          XINPUT_GAMEPAD_START, &NewController->Start);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->Back, 
                                          XINPUT_GAMEPAD_BACK, &NewController->Back);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->LeftShoulder, 
                                          XINPUT_GAMEPAD_LEFT_SHOULDER, 
                                          &NewController->LeftShoulder);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->RightShoulder, 
                                          XINPUT_GAMEPAD_RIGHT_SHOULDER, 
                                          &NewController->RightShoulder);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->ActionDown, 
                                          XINPUT_GAMEPAD_A, &NewController->ActionDown);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->ActionRight, 
                                          XINPUT_GAMEPAD_B, &NewController->ActionRight);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->ActionLeft, 
                                          XINPUT_GAMEPAD_X, &NewController->ActionLeft);
          Win32ProcessXInputDigitalButton(Gamepad->wButtons, &OldController->ActionUp, 
                                          XINPUT_GAMEPAD_Y, &NewController->ActionUp);
        }
        else
        {
          // NOTE(bjorn): This controller is unavailable or has a problem.
        }
      }

      GetKeyboard(&NewGameInput, 1)->IsConnected = true;
      for(int KeyboardIndex = 1;
          KeyboardIndex <= ArrayCount(OldGameInput.Keyboards);
          KeyboardIndex++)
      {
        game_keyboard* NewKeyboard = GetKeyboard(&NewGameInput, KeyboardIndex);
        game_keyboard* OldKeyboard = GetKeyboard(&OldGameInput, KeyboardIndex);
        if(NewKeyboard->IsConnected)
        {
          for(int ButtonIndex = 0;
              ButtonIndex < ArrayCount(NewKeyboard->Buttons);
              ButtonIndex++)
          {
            NewKeyboard->Buttons[ButtonIndex].EndedDown = 
              OldKeyboard->Buttons[ButtonIndex].EndedDown;
          }
        }
      }

      for(int MouseIndex = 1;
          MouseIndex <= ArrayCount(OldGameInput.Mice);
          MouseIndex++)
      {
        game_mouse* NewMouse = GetMouse(&NewGameInput, MouseIndex);
        game_mouse* OldMouse = GetMouse(&OldGameInput, MouseIndex);
        if(NewMouse->IsConnected)
        {
          for(int ButtonIndex = 0;
              ButtonIndex < ArrayCount(OldMouse->Buttons);
              ButtonIndex++)
          {
            NewMouse->Buttons[ButtonIndex].EndedDown = 
              OldMouse->Buttons[ButtonIndex].EndedDown;
          }
        }
      }

      Win32ProcessWindowMessages(WindowHandle, &GameIsRunning, &WindowAlpha,
                                 &Win32State, 
                                 &NewGameInput.Keyboards[0],
                                 &NewGameInput.Mice[0],
                                 &WindowCallbackData);

      {
        POINT MousePoint;
        GetCursorPos(&MousePoint);
        ScreenToClient(WindowHandle, &MousePoint);

        game_mouse* Mouse = GetMouse(&NewGameInput, 1);
        Mouse->IsConnected = true;

        f32 RelativeMouseX = (f32)(MousePoint.x - GameScreenLeft) / (f32)GameScreenWidth;
        f32 RelativeMouseY = (f32)(MousePoint.y - GameScreenTop) / (f32)GameScreenHeight;

        Mouse->P.X = Clamp01(RelativeMouseX);
        Mouse->P.Y = Clamp01(RelativeMouseY);

        Mouse->P.Y = 1.0f - Mouse->P.Y;

        Assert(0.0f <= Mouse->P.X && 1.0f >= Mouse->P.X);
        Assert(0.0f <= Mouse->P.Y && 1.0f >= Mouse->P.Y);

        game_mouse* OldMouse = GetMouse(&OldGameInput, 1);
        Mouse->dP = OldMouse->P - Mouse->P;

        Win32ProcessKeyboardButton((GetKeyState(VK_LBUTTON) & 0x8000), &Mouse->Left); 
        Win32ProcessKeyboardButton((GetKeyState(VK_MBUTTON) & 0x8000), &Mouse->Middle); 
        Win32ProcessKeyboardButton((GetKeyState(VK_RBUTTON) & 0x8000), &Mouse->Right); 
        // TODO(bjorn): Actually test on a mouse with XBUTTON1 and XBUTTON2 which
        // one is forwards and which one is backwards.
        Win32ProcessKeyboardButton((GetKeyState(VK_XBUTTON1) & 0x8000), &Mouse->ThumbForward); 
        Win32ProcessKeyboardButton((GetKeyState(VK_XBUTTON2) & 0x8000), &Mouse->ThumbBackward); 
      }
      
      {
        game_bitmap* MainScreen = GetBitmap(GameAssets, GAI_MainScreen);
        MainScreen->Dim = {(u32)GameScreenWidth, (u32)GameScreenHeight};
        MainScreen->WidthOverHeight = GameScreenWidth/(f32)GameScreenHeight;
      }

#if 0
      {
        char TextBuffer[256];
        sprintf_s(TextBuffer, 
                  "Mouse X:%f Mouse Y:%f\n",
                  NewGameInput.Mouse[0].Pos.X, NewGameInput.Mouse[0].Pos.Y);
        OutputDebugStringA(TextBuffer);
      }
#endif

      ClearRenderGroup(GameRenderGroup);

#if HANDMADE_INTERNAL
      if(Win32State.RecordHandle)
      {
        Win32RecordInput(&Win32State, &NewGameInput);
      }
      if(Win32State.PlayBackHandle)
      {
        Win32PlayBackInput(&Win32State, &NewGameInput);
      }
#endif
      if(Game->Code.Update)
      {
        HandleDebugCycleCounters(&Game->Memory);
        Game->Code.Update(TargetSecondsPerFrame, &Game->Memory, &NewGameInput, GameRenderGroup, GameAssets);
      }

      DWORD PlayCursor;
      DWORD WriteCursor;
      if(SUCCEEDED(SecondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
      {
        // TODO(bjorn): Change this to using a lower latency offset from the
        //              play cursor when I actually start having soundeffects.
        // TODO(bjorn): Implement a Virtual Stream circle buffer as according to this article
        //              https://fgiesen.wordpress.com/2010/12/14/ring-buffers-and-queues/
        /* NOTE(bjorn):

           Here is how sound output computation works.

           We define a ceirtan amount of margin, roughly half a frame.

           We find out the byte in the sound buffer where we flip next
           in relation to the PlayCursor.

           If the write cursor plus some margin is smaller than the expected
           flip then we are in low-latency mode and just set TargetCursor to 
           equal the flip byte plus one frame's worth of bytes.

           Otherwise are we in high-latency mode and will do a best-effort 
           write where we will set the TargetCursor to equal the WriteCursor 
           plus one frame's worth of bytes plus the margin.  

           Since we just keep on adding from the RunningSampleIndex to the 
           TargetCursor, the actual bytes written each frame will auto-correct 
           to one frames worth.
           */
        if(SoundFirstTimeAround)
        {
          SoundFirstTimeAround = false;
          SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
        }
        DWORD ByteToLock = 0;
        DWORD BytesToWrite = 0;
        DWORD TargetCursor = 0;

        s32 OneFrameOfSampleBytes = (s32)(TargetSecondsPerFrame * 
                                          (f32)SoundOutput.SamplesPerSecond * 
                                          (f32)SoundOutput.BytesPerSample);

        LARGE_INTEGER Now = Win32GetWallClock();
        f32 SecondsElapsedSinceFrameFlip = Win32GetSecondsElapsed(LastCounter, Now,
                                                                  PerformanceCountFrequency); 
        f32 SecondsToNextFrame = TargetSecondsPerFrame - SecondsElapsedSinceFrameFlip;
        if(SecondsToNextFrame < 0)
        {
          // TODO(bjorn): What should we do if a frame lags hard?  
        }
        s32 SamplesToNextFrame = (s32)(SecondsToNextFrame * 
                                       (f32)SoundOutput.SamplesPerSecond); 
        DWORD SampleBytesToNextFrame = SamplesToNextFrame * SoundOutput.BytesPerSample;
        DWORD FrameFlipByte = PlayCursor + SampleBytesToNextFrame;

        DWORD UnwrappedWriteCursor = WriteCursor;
        if(UnwrappedWriteCursor < PlayCursor)
        {
          UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
        }
        DWORD SafeWriteCursor = UnwrappedWriteCursor + SoundOutput.SafetyBytes; 
        b32 LowLatency = SafeWriteCursor < FrameFlipByte;

        if((SecondsToNextFrame > 0) && LowLatency)
        {
          // NOTE(bjorn): Low-latency mode.
          TargetCursor = FrameFlipByte + OneFrameOfSampleBytes;
        }
        else
        {
          // NOTE(bjorn): High-latency mode.
          TargetCursor = WriteCursor + SoundOutput.SafetyBytes + OneFrameOfSampleBytes; 
        }
        TargetCursor %= SoundOutput.SecondaryBufferSize;

        DWORD RunningSampleByteIndex = (SoundOutput.RunningSampleIndex *
                                        SoundOutput.BytesPerSample);
        ByteToLock = (RunningSampleByteIndex % SoundOutput.SecondaryBufferSize);
        if(ByteToLock == TargetCursor)
        {
          BytesToWrite = 0;
        }
        else if(ByteToLock > TargetCursor)
        {
          BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
          BytesToWrite += TargetCursor;
        }
        else
        {
          BytesToWrite = TargetCursor - ByteToLock;
        }
#if HANDMADE_INTERNAL
        if(!NewGameInput.Controllers[0].ActionDown.EndedDown)
        {
          DebugTimeMarker[DebugTimeMarkerIndex].PlayCursor = PlayCursor;
          DebugTimeMarker[DebugTimeMarkerIndex].WriteCursor = WriteCursor;
          DebugTimeMarker[DebugTimeMarkerIndex].TargetCursor = TargetCursor;
          DebugTimeMarker[DebugTimeMarkerIndex].RunningByteIndex = 
            ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
             SoundOutput.SecondaryBufferSize);
          DebugTimeMarkerIndex++;
          if(DebugTimeMarkerIndex >= ArrayCount(DebugTimeMarker))
          {
            DebugTimeMarkerIndex = 0;
          }
        }
#endif
        game_sound_output_buffer GameSoundBuffer = {};
        GameSoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        GameSoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
        GameSoundBuffer.Samples = SoundSamplesBuffer;

        if(Game->Code.GetSoundSamples)
        {
          Game->Code.GetSoundSamples(&GameSoundBuffer, &Game->Memory);
        }
        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &GameSoundBuffer,
                             SecondarySoundBuffer);
#if 0//HANDMADE_INTERNAL
        {
          s32 WCPCDiff_ms = ((1000*((UnwrappedWriteCursor - PlayCursor) / 
                                    SoundOutput.BytesPerSample)) /
                             SoundOutput.SamplesPerSecond);
          s32 TCPCDiff_ms = ((1000*((TargetCursor - PlayCursor) / 
                                    SoundOutput.BytesPerSample)) /
                             SoundOutput.SamplesPerSecond);
          char TextBuffer[256];
          wsprintfA(TextBuffer, 
                    "WCPCdelta:%d ms TCPCdelta:%d ms\n",
                    WCPCDiff_ms, TCPCDiff_ms);
          OutputDebugStringA(TextBuffer);
        }
#endif
      }

      OldGameInput = NewGameInput;

      LARGE_INTEGER WorkCounter = Win32GetWallClock();
#if 0//HANDMADE_INTERNAL
      {
        u64 WorkCycleCount = __rdtsc();

        u64 WorkCyclesElapsed = WorkCycleCount - LastCycleCount; 
        s64 WorkCounterElapsed = WorkCounter.QuadPart - LastCounter.QuadPart;

        s32 msPerFrame = (s32)(1000*WorkCounterElapsed / 
                               PerformanceCountFrequency);
        s32 FPS = (s32)(PerformanceCountFrequency / WorkCounterElapsed);
        s32 MCPF = (s32)(WorkCyclesElapsed / (1000 * 1000));

        char TextBuffer[256];
        wsprintfA(TextBuffer, 
                  "Milliseconds Work/frame: %dms/f // %d FPS // %dMc/f ", 
                  msPerFrame, FPS, MCPF);
        OutputDebugStringA(TextBuffer);
      }
#endif

      f32 SecondsElapsedFromWork = Win32GetSecondsElapsed(LastCounter, WorkCounter,
                                                          PerformanceCountFrequency); 
      f32 SecondsElapsedForFrame = SecondsElapsedFromWork;
      if(SecondsElapsedForFrame < TargetSecondsPerFrame)
      {
        if(SleepIsGranular)
        {
          DWORD msSleep = (DWORD)(1000.0f * (TargetSecondsPerFrame - 
                                             SecondsElapsedForFrame));
          if(msSleep > 0)
          {
            Sleep(msSleep);
          }
        }

        f32 TestSecondsElapsedForFrame = 
          Win32GetSecondsElapsed(LastCounter, Win32GetWallClock(), 
                                 PerformanceCountFrequency);
        if(TestSecondsElapsedForFrame > TargetSecondsPerFrame)
        {
          // TODO(bjorn): Log frame-miss here!
        }
        while(SecondsElapsedForFrame < TargetSecondsPerFrame)
        {
          SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock(),
                                                          PerformanceCountFrequency);
        }
      }
      else
      {
        // TODO(bjorn): Missed a frame!!!
        // TODO(bjorn): Logging.
      }

#if 0//HANDMADE_INTERNAL
      {
        LARGE_INTEGER EndCounter = Win32GetWallClock();
        u64 EndCycleCount = __rdtsc();

        u64 EndCyclesElapsed = EndCycleCount - LastCycleCount; 
        s64 EndCounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;

        s32 msPerFrame = (s32)(1000*EndCounterElapsed / PerformanceCountFrequency);
        s32 FPS = (s32)(PerformanceCountFrequency / EndCounterElapsed);
        s32 MCPF = (s32)(EndCyclesElapsed / (1000 * 1000));

        char TextBuffer[256];
        wsprintfA(TextBuffer, 
                  "Milliseconds Total/frame: %dms/f // %d FPS // %dMc/f\n", 
                  msPerFrame, FPS, MCPF);
        OutputDebugStringA(TextBuffer);
      }
#endif
      LastCycleCount = __rdtsc();
      LastCounter = Win32GetWallClock();

      OpenGLRenderGroupToOutput(GameRenderGroup, GameAssets);

      {
        HDC DeviceContext = GetDC(WindowHandle);

        win32_window_dimension WindowDimension = Win32GetWindowDimension(WindowHandle);
        Win32DisplayBufferInWindow(DeviceContext, 
                                   WindowDimension.Width, WindowDimension.Height,
                                   GameScreenLeft, GameScreenTop,
                                   GameScreenWidth, GameScreenHeight);

        ReleaseDC(WindowHandle, DeviceContext);
      }

      for(s32 ControllerIndex = 1;
          ControllerIndex <= MaxControllerCount;
          ControllerIndex++)
      {
        if(GetController(&NewGameInput, ControllerIndex)->IsConnected)
        {
          // TODO(bjorn): Should this run with the other code on old input, or should it be
          //              facored out to a platform-service function. Or should this happen 
          //              here and just loop all the controllers?
          XINPUT_VIBRATION Vibration;
          Vibration.wLeftMotorSpeed = 
            (u16)(GetController(&NewGameInput, ControllerIndex)->LeftMotor * 65535);
          Vibration.wRightMotorSpeed = 
            (u16)(GetController(&NewGameInput, ControllerIndex)->RightMotor * 65535);
          XInputSetState(ControllerIndex, &Vibration);
        }
      }
    }
  }
  else
  {
    // TODO(bjorn): Handle this error in the future. 
  }

  return(0);
}
