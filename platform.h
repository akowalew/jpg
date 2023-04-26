#include <stdint.h>
#include <stddef.h>

#define function static

typedef size_t usz;

typedef float f32;

typedef int64_t i64;
typedef uint64_t u64;

typedef int32_t i32;
typedef uint32_t u32;

typedef int16_t i16;
typedef uint16_t u16;

typedef uint8_t u8;
typedef int8_t i8;

#define ArrayCount(x) (sizeof(x)/sizeof((x)[0]))

#define ByteSwap16(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

// static void AssertFailed()
// {
//     fprintf(stderr, "Fatal error at: %s[%d]: expression \"%s\" failed\n", __FILE__, __LINE__, #x); if(!IsDebuggerPresent()) { exit(1); } else { *(int*)(0) = 0; }
// }

#define Assert(x) if(!(x)) { *(int*)(0) = 0; }

typedef struct
{
    u32 Width;
    u32 Height;
    u32 Pitch;
    u32 Size;
    u8* At;
} bitmap;

typedef struct
{
    usz Elapsed;
    u8* At;
} buffer;

static void* PopBytes(buffer* Buffer, usz Count)
{
    void* Result = 0;

    if(Buffer->Elapsed >= Count)
    {
        Result = Buffer->At;

        Buffer->At += Count;
        Buffer->Elapsed -= Count;
    }

    return Result;
}

#define PopU16(Buffer) (u16*)PopBytes(Buffer, sizeof(u16))

static void* PlatformAlloc(usz Size);