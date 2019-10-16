#if !defined(TYPES_AND_DEFINES_H)

/* NOTE(bjorn):

   :HANDMADE_INTERNAL:
   0 - Build for public release. 
   1 - Build for developer only.

   :HANDMADE_SLOW:
   0 - No slow code allowed!
   1 - Slow code welcome.
 */

//
// NOTE(bjorn): Compilers.
//

#if !defined COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#if !defined COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM

#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
//TODO(bjorn): Moar compilerz!!
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif

#endif

#if 0
typedef uint8_t u8;
typedef int8_t s8;

typedef uint16_t u16;
typedef int16_t s16;

typedef uint32_t u32;
typedef int32_t s32;

typedef uint64_t u64;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef u32 b32;
#else
typedef unsigned char u8;
typedef signed char s8;

typedef unsigned short u16;
typedef signed short s16;

typedef unsigned int u32;
typedef signed int s32;

typedef unsigned long long u64;
typedef signed long long s64;

typedef float f32;
typedef double f64;

typedef bool b32;
#endif

typedef size_t memi;

#define internal_function static 
#define local_persist static 
#define global_variable static 

#define path_max_char_count 260 //NOTE(bjorn): Equal to MAX_PATH on Windows.

#define pi32 3.14159265359f
#define tau32 (pi32 * 2.0f)
#define inv_pi32 (1.0f / pi32)
#define inv_tau32 (1.0f / tau32)
#define root2 1.41421356237f
#define inv_root2 0.7071067811865475f

#define negative_infinity32      0b11111111100000000000000000000000  
#define positive_infinity32      0b01111111100000000000000000000000 
#define positive_zero32          0b00000000000000000000000000000000 
#define negative_zero32          0b10000000000000000000000000000000  
#define quiet_not_a_number32     0b01111111110000000000000000000000
#define signaling_not_a_number32 0b01111111101000000000000000000000

#define max_u32       0xFFFFFFFF
#define min_u32       0x00000000
#define max_s32       0x7FFFFFFF
#define min_s32 ((s32)0x80000000)

global_variable int IEEE_INF_BITS = 0x7F800000;
#define inf32 (*(float*)&IEEE_INF_BITS)

#if HANDMADE_SLOW
#define Assert(...) if(!(__VA_ARGS__)){ *(s32 *)0 = 0; }
#else
#define Assert(...)
#endif

#define InvalidCodePath Assert(!"Invalid Code Path")

#define Kilobytes(number) (number * 1024LL)
#define Megabytes(number) (Kilobytes(number) * 1024LL)
#define Gigabytes(number) (Megabytes(number) * 1024LL)
#define Terabytes(number) (Gigabytes(number) * 1024LL)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))
#define Min(a, b) ((a) < (b)?(a):(b))
#define Max(a, b) ((a) < (b)?(b):(a))
#define Swap(x, y, T) do { T SWAP = x; x = y; y = SWAP; } while (0)

#define TYPES_AND_DEFINES_H
#endif
