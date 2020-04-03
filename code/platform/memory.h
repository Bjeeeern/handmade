#if !defined(MEMORY_H)
#include "types_and_defines.h"
#include "intrinsics.h"

struct memory_arena
{
	memi Size;
	u8 *Base;
	memi Used;
	s32 TempMemCount;
};

internal_function void
InitializeArena(memory_arena* Arena, memi Size, u8 *Base)
{
	Arena->Size = Size;
	Arena->Base = Base;
	Arena->Used = 0;
	Arena->TempMemCount = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_((Arena), sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_((Arena), (Count)*sizeof(type))
#define PushSize(Arena, Size) PushSize_((Arena), (Size))
void *
PushSize_(memory_arena* Arena, memi Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	Assert(Size > 0);
	
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return Result;
}

internal_function void
SubArena(memory_arena* New, memory_arena* Base, memi Size)
{
  void* BasePointer = PushSize(Base, Size);
	InitializeArena(New, Size, (u8*)BasePointer);
}

#define ZeroArray(Array) ZeroMemory_((u8*)(Array), sizeof(Array))
#define ZeroMemory(Pointer, Size) ZeroMemory_((u8*)(Pointer), (Size))
void
ZeroMemory_(u8* ClearBits, memi Size)
{
	Assert(ClearBits);
	Assert(Size > 0);

	memi BitsToClean = Size;
	while(BitsToClean--){ *ClearBits++ = 0; }
}

struct temporary_memory
{
	memory_arena* Arena;
	memi Used;
  s32 ID;
};

internal_function temporary_memory
BeginTemporaryMemory(memory_arena* Arena)
{
	temporary_memory Result = {};

	Result.Arena = Arena;
	Result.Used = Arena->Used;
	Result.ID = Arena->TempMemCount;
	Arena->TempMemCount++;

	return Result;
}

internal_function void
EndTemporaryMemory(temporary_memory TempMem)
{
	memory_arena* Arena = TempMem.Arena;

	Assert(Arena->Used >= TempMem.Used);
	Arena->Used = TempMem.Used;

	Assert(Arena->TempMemCount > 0);
	Arena->TempMemCount--;
  Assert(TempMem.ID == Arena->TempMemCount);
}

internal_function void
CheckMemoryArena(memory_arena* Arena)
{
	Assert(Arena->TempMemCount == 0);
}

struct task_with_memory
{
  b32 _BeingUsed;
  temporary_memory _TempMem;
  memory_arena Arena;
};

internal_function task_with_memory*
BeginTaskWithMemory(task_with_memory* Tasks, u32 TaskCount)
{
  task_with_memory* Result = 0;

  for(u32 TaskIndex = 0;
      TaskIndex < TaskCount;
      TaskCount++)
  {
    task_with_memory* Task = Tasks + TaskIndex;

    if(!Task->_BeingUsed)
    {
      //TODO IMPORTANT(bjorn): Not threadsafe. If called linearly work cannot be stacked though.
      Task->_BeingUsed = true;
      Task->_TempMem = BeginTemporaryMemory(&Task->Arena);

      Result = Task;
      break;
    }
  }

  return Result;
}

internal_function void
EndTaskWithMemory(task_with_memory* Task)
{
  EndTemporaryMemory(Task->_TempMem);
  Task->_BeingUsed = false;
}

#define MEMORY_H
#endif
