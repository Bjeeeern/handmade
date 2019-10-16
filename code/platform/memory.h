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

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
void *
PushSize_(memory_arena* Arena, memi Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	Assert(Size > 0);
	
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return Result;
}

#define ZeroArray(Array) ZeroMemory_((u8*)Array, sizeof(Array))
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
};

internal_function temporary_memory
BeginTemporaryMemory(memory_arena* Arena)
{
	temporary_memory Result = {};

	Result.Arena = Arena;
	Result.Used = Arena->Used;

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
}

internal_function void
CheckMemoryArena(memory_arena* Arena)
{
	Assert(Arena->TempMemCount == 0);
}

#define MEMORY_H
#endif
