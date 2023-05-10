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

#define Assert(x) if((x) == 0) { *(int*)(0) = 0; }

#define ArrayCount(x) (sizeof(x)/sizeof((x)[0]))

#define ByteSwap16(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))

#define Min(x, y) ((x) <= (y) ? (x) : (y))
#define Max(x, y) ((x) >= (y) ? (x) : (y))

#define CLAMP(X, Lo, Hi) ((X) <= (Lo) ? (Lo) : ((X) >= (Hi) ? (Hi) : (X)))

typedef struct
{
    i32 Width;
    i32 Height;
    i32 Pitch;
    i32 Size;
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

static void* PushBytes(buffer* Buffer, usz Count)
{
    void* Result = 0;

    if(Buffer->Elapsed >= Count)
    {
        Result = Buffer->At;
        Buffer->Elapsed -= Count;
        Buffer->At += Count;
    }

    return Result;
}

#define PushStruct(Buffer, type) (type*)PushBytes(Buffer, sizeof(type))

static int PushU16(buffer* Buffer, u16 Value)
{
    int Result = 0;

    if(Buffer->Elapsed >= sizeof(Value))
    {
        *(u16*)(Buffer->At) = Value;

        Buffer->At += sizeof(Value);
        Buffer->Elapsed -= sizeof(Value);

        Result = 1;
    }

    return Result;
}

static void* PushSegmentCount(buffer* Buffer, u16 Marker, u16 Length)
{
    void* Result = 0;

    if(PushU16(Buffer, Marker))
    {
        // TODO: u32!
        if(PushU16(Buffer, ByteSwap16(Length+2)))
        {
            if(Buffer->Elapsed >= Length)
            {
                Result = Buffer->At;
                Buffer->At += Length;
                Buffer->Elapsed -= Length;
            }
        }
    }

    return Result;
}

#define PushSegment(Buffer, Marker, type) (type*)PushSegmentCount(Buffer, Marker, sizeof(type))

typedef struct
{
    u8* At;
    usz Elapsed;
    u32 Buf;
    u8 Len;
} bit_stream;

static int Refill(bit_stream* BitStream)
{
    u8 BytesCount = (32 - BitStream->Len) / 8;
    
    BytesCount = (u8) Min(BitStream->Elapsed, BytesCount);

    while(BytesCount)
    {
        u32 Byte = *(BitStream->At++);
        BitStream->Elapsed--;
        if(Byte == 0xFF)
        {
            u8 NextByte = *(BitStream->At++);
            BitStream->Elapsed--;
            BytesCount = (u8) Min(BitStream->Elapsed, BytesCount);

            if(NextByte != 0x00)
            {
                // FIXME: This function is vurnelable!
                // We should instead jump completely out 
                // of the world in case of meeting FFD9 marker

                if(NextByte == 0xD9)
                {
                    return 1;
                }
                else
                {
                    Assert(!"Not supported marker");
                }
            }
        }

#if 0
        BitStream->Buf |= (Byte << BitStream->Len);
#else
        BitStream->Buf <<= 8;
        BitStream->Buf |= Byte;
#endif

        BitStream->Len += 8;

        BytesCount--;
    }

    return 1;
}

static int Flush(bit_stream* BitStream)
{
    u8 BytesCount = BitStream->Len / 8;
    Assert(BitStream->Elapsed > BytesCount);

    for(u8 Idx = 0;
        Idx < BytesCount;
        Idx++)
    {
#if 0
        u8 Byte = (BitStream->Buf & 0xFF);
        BitStream->Buf >>= 8;
        BitStream->Len -= 8;
#else
        BitStream->Len -= 8;
        u8 Byte = (BitStream->Buf >> BitStream->Len) & 0xFF;
#endif

        *(BitStream->At++) = Byte;
        BitStream->Elapsed--;
        if(Byte == 0xFF)
        {
            *(BitStream->At++) = 0x00;
            BitStream->Elapsed--;
        }
    }

    return 1;
}

static int PopBits(bit_stream* BitStream, u16* Value, u8 Size)
{
    int Result = 0;

    if(!Size)
    {
        *Value = 0;
        return 1;
    }

    if(Refill(BitStream))
    {
        Assert(BitStream->Len >= Size);

        u16 Mask = (1 << Size) - 1;

#if 0
        *Value = (BitStream->Buf & Mask);
        BitStream->Buf >>= Size;
        BitStream->Len -= Size;
#else
        BitStream->Len -= Size;
        *Value = (BitStream->Buf >> BitStream->Len) & Mask;
#endif

        Result = 1;
    }

    return Result;
}

static int PushBits(bit_stream* BitStream, u16 Value, u8 Size)
{
    int Result = 0;

    if(!Size)
    {
        return 1;
    }

    Assert(Size <= sizeof(Value)*8);

    if(Flush(BitStream))
    {
        Assert(BitStream->Len + Size <= 32);

        u16 Mask = (1 << Size) - 1;
        u32 Raw = (Value & Mask);

#if 0
        BitStream->Buf |= (Raw << BitStream->Len);
#else
        BitStream->Buf <<= Size;
        BitStream->Buf |= Raw;
#endif

        BitStream->Len += Size;

        Result = 1;
    }

    return Result;
}

void* PlatformAlloc(usz Size);
void  PlatformFree(void* Data);
void* PlatformReadEntireFile(const char* Name, usz* Size);
int PlatformWriteEntireFile(const char* Name, void* Data, usz Size);
int PlatformShowBitmap(bitmap* Bitmap, const char* Title);