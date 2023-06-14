#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <intrin.h>

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

#if defined(_MSC_VER)
#define ALIGNED(x) __declspec(align(x))
#elif defined(__GNUC__)
#define ALIGNED(x) __attribute__ ((aligned(x)))
#endif

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

//
// NOTE: Following functions need to be implemented for each platform
//

void* PlatformAlloc(usz Size);
void  PlatformFree(void* Data);
void* PlatformReadEntireFile(const char* Name, usz* Size);
int PlatformWriteEntireFile(const char* Name, void* Data, usz Size);
int PlatformShowBitmap(bitmap* Bitmap, const char* Title);
u64 PlatformGetTicks(void);

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
        u32 Byte = BitStream->At[0];
        if(Byte == 0xFF)
        {
            if(BitStream->Elapsed > 2 &&
               BitStream->At[1] == 0x00)
            {
                BitStream->At++;
                BitStream->Elapsed--;
                BytesCount = (u8) Min(BitStream->Elapsed, BytesCount);
            }
            else
            {
                break;
            }
        }

        BitStream->At++;
        BitStream->Elapsed--;

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
    Assert(BitStream->Elapsed > BytesCount*2);
    BitStream->Elapsed -= BytesCount;

    while(BytesCount)
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
        }

        BytesCount--;
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

static void MatrixMul8x8T(const f32 Left[8][8], const f32 RightT[8][8], f32 Output[8][8])
{
    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        __m128 XMM0 = _mm_load_ps(&Left[Y][0]);
        __m128 XMM1 = _mm_load_ps(&Left[Y][4]);

        for(u8 X = 0;
            X < 8;
            X += 4)
        {
#if 1
            __m128 XMM2 = _mm_load_ps(&RightT[X][0]);
            __m128 XMM3 = _mm_load_ps(&RightT[X][4]);

            __m128 XMM4 = _mm_load_ps(&RightT[X+1][0]);
            __m128 XMM5 = _mm_load_ps(&RightT[X+1][4]);
            
            __m128 XMM6 = _mm_load_ps(&RightT[X+2][0]);
            __m128 XMM7 = _mm_load_ps(&RightT[X+2][4]);

            __m128 XMM8 = _mm_load_ps(&RightT[X+3][0]);
            __m128 XMM9 = _mm_load_ps(&RightT[X+3][4]);

            XMM2 = _mm_mul_ps(XMM0, XMM2);
            XMM3 = _mm_mul_ps(XMM1, XMM3);

            XMM4 = _mm_mul_ps(XMM0, XMM4);
            XMM5 = _mm_mul_ps(XMM1, XMM5);

            XMM6 = _mm_mul_ps(XMM0, XMM6);
            XMM7 = _mm_mul_ps(XMM1, XMM7);

            XMM8 = _mm_mul_ps(XMM0, XMM8);
            XMM9 = _mm_mul_ps(XMM1, XMM9);

            XMM2 = _mm_hadd_ps(XMM2, XMM3);
            XMM4 = _mm_hadd_ps(XMM4, XMM5);
            XMM6 = _mm_hadd_ps(XMM6, XMM7);
            XMM8 = _mm_hadd_ps(XMM8, XMM9);

            XMM2 = _mm_hadd_ps(XMM2, XMM4);
            XMM6 = _mm_hadd_ps(XMM6, XMM8);

            XMM2 = _mm_hadd_ps(XMM2, XMM6);

            _mm_store_ps(&Output[Y][X], XMM2);
#else
            f32 S = 0;

            for(u8 K = 0;
                K < 8;
                K++)
            {
                S += DCT8x8Table[Y][K] * P[X][K];
            }

            Tmp[Y][X] = S;
#endif
        }
    }
}

static usz Sum16xU8(u8 Values[16])
{
    usz Result = 0;

#if 0
    for(u8 Idx = 0;
        Idx < ArrayCount(DHT->Counts);
        Idx++)
    {
        // TODO: SIMD
        Result += DHT->Counts[Idx];
    }
#else
    __m128i XMM0 = _mm_loadu_si128((__m128i*) Values);
    __m256i YMM0 = _mm256_cvtepi8_epi16(XMM0);
    __m256i YMM1 = _mm256_permute2x128_si256(YMM0, YMM0, 0x01);
    YMM0 = _mm256_hadd_epi16(YMM0, YMM1);
    YMM0 = _mm256_hadd_epi16(YMM0, YMM0);
    YMM0 = _mm256_hadd_epi16(YMM0, YMM0);
    YMM0 = _mm256_hadd_epi16(YMM0, YMM0);
    Result = _mm256_extract_epi16(YMM0, 0);
#endif

    return Result;
}

static void VectorDiv64(const f32 Left[64], const f32 Right[64], f32 Output[64])
{
#if 0
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        f32 Div = K[Idx] / DQT[Idx];

        Z[Idx] = (i16)Div;
    }
#else
    for(u8 Idx = 0;
        Idx < 64;
        Idx += 16)
    {
        __m128 XMM0 = _mm_load_ps(&Left[Idx+0]);
        __m128 XMM1 = _mm_load_ps(&Left[Idx+4]);
        __m128 XMM2 = _mm_load_ps(&Left[Idx+8]);
        __m128 XMM3 = _mm_load_ps(&Left[Idx+12]);

        __m128 XMM4 = _mm_load_ps(&Right[Idx+0]);
        __m128 XMM5 = _mm_load_ps(&Right[Idx+4]);
        __m128 XMM6 = _mm_load_ps(&Right[Idx+8]);
        __m128 XMM7 = _mm_load_ps(&Right[Idx+12]);

        XMM0 = _mm_div_ps(XMM0, XMM4);
        XMM1 = _mm_div_ps(XMM1, XMM5);
        XMM2 = _mm_div_ps(XMM2, XMM6);
        XMM3 = _mm_div_ps(XMM3, XMM7);

        _mm_store_ps(&Output[Idx+0], XMM0);
        _mm_store_ps(&Output[Idx+4], XMM1);
        _mm_store_ps(&Output[Idx+8], XMM2);
        _mm_store_ps(&Output[Idx+12], XMM3);
    }
#endif
}

typedef struct
{
    const char* Description;
    u64 Ticks;
 } timing;

usz TimingIdx;
timing TimingData[1024]; 

static void TimingTick(const char* Description)
{
    u64 Ticks = PlatformGetTicks();
    
    Assert(TimingIdx < ArrayCount(TimingData));
    timing* Timing = &TimingData[TimingIdx++];
    Timing->Description = Description;
    Timing->Ticks = Ticks;
}

static void TimingInit(const char* Description)
{
    TimingIdx = 0;

    TimingTick(Description);
}

static void TimingFini(const char* Description)
{
    TimingTick(Description);

    printf("Timing report:\n");
    Assert(TimingIdx < ArrayCount(TimingData));
    Assert(TimingIdx > 1);
    usz LastIdx = TimingIdx-1;
    for(usz Idx = 0;
        Idx < LastIdx;
        Idx++)
    {
        timing* Timing = &TimingData[Idx];
        u64 Diff = TimingData[Idx+1].Ticks-TimingData[Idx].Ticks;
        printf("%s => %" PRIu64 " ticks\n", TimingData[Idx].Description, Diff);
    }
    fflush(stdout);
    getchar();
}

#ifndef TIMING_ENABLED
#define TIMING_ENABLED 1
#endif

#if TIMING_ENABLED
#define TIMING_INIT(Description) TimingInit(Description)
#define TIMING_TICK(Description) TimingTick(Description)
#define TIMING_FINI(Description) TimingFini(Description)
#else
#define TIMING_INIT(Description)
#define TIMING_TICK(Description)
#define TIMING_FINI(Description)
#endif
