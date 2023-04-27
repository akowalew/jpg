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

typedef struct
{
    u32 Width;
    u32 Height;
    u32 Pitch;
    u32 Size;
    u8* At;

    // Byte orders:
    //  u32 -> 0xAARRGGBB
    //  u8[4] -> 0xBB, 0xGG, 0xRR, 0xAA
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
static int PlatformShowBitmap(bitmap* Bitmap, const char* Title);

static bitmap* GlobalBitmap;

#define Assert(x) if(!(x)) { PlatformShowBitmap(GlobalBitmap, "bitmap"); *(int*)(0) = 0; }
