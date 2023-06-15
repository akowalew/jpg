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
            // TODO: AVX2!!!

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
        // TODO: AVX2!!!

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

static void VectorMul64(const f32 Left[64], const f32 Right[64], f32 Output[64])
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
        // TODO: AVX2!!!

        __m128 XMM0 = _mm_load_ps(&Left[Idx+0]);
        __m128 XMM1 = _mm_load_ps(&Left[Idx+4]);
        __m128 XMM2 = _mm_load_ps(&Left[Idx+8]);
        __m128 XMM3 = _mm_load_ps(&Left[Idx+12]);

        __m128 XMM4 = _mm_load_ps(&Right[Idx+0]);
        __m128 XMM5 = _mm_load_ps(&Right[Idx+4]);
        __m128 XMM6 = _mm_load_ps(&Right[Idx+8]);
        __m128 XMM7 = _mm_load_ps(&Right[Idx+12]);

        XMM0 = _mm_mul_ps(XMM0, XMM4);
        XMM1 = _mm_mul_ps(XMM1, XMM5);
        XMM2 = _mm_mul_ps(XMM2, XMM6);
        XMM3 = _mm_mul_ps(XMM3, XMM7);

        _mm_store_ps(&Output[Idx+0], XMM0);
        _mm_store_ps(&Output[Idx+4], XMM1);
        _mm_store_ps(&Output[Idx+8], XMM2);
        _mm_store_ps(&Output[Idx+12], XMM3);
    }
#endif
}

static void BGRAtoYCbCr_420_8x8(void* Data, usz Pitch, f32 Lum[2][2][8][8], f32 Cb[2][2][8][8], f32 Cr[2][2][8][8])
{
    // BB GG RR AA
    __m128 XMM2 = _mm_set_ps(0.0f, 0.299f, 0.587f, 0.114f);
    __m128 XMM3 = _mm_set_ps(0.0f, -0.168736f, -0.331264f, 0.5f);
    __m128 XMM4 = _mm_set_ps(0.0f, 0.5f, -0.418688f, -0.081312f);
    __m128 XMM12 = _mm_setzero_ps();

    u8* MidRow = Data;
    for(u8 KY = 0;
        KY < 2;
        KY++)
    {
        u8* MidCol = MidRow;

        for(u8 KX = 0;
            KX < 2;
            KX++)
        {
            u8* SubRow = MidCol;

            f32* MidLum = (f32*) Lum[KY][KX];
            f32* MidCb = (f32*) Cb[KY][KX];
            f32* MidCr = (f32*) Cr[KY][KX];

            for(u8 Y = 0;
                Y < 8;
                Y++)
            {
                u8* SubCol = SubRow;

                for(u8 X = 0;
                    X < 8;
                    X++)
                {
#if 0
                    u8 B = SubCol[0];
                    u8 G = SubCol[1];
                    u8 R = SubCol[2];

                    float Lum_Value = (0.299f*R + 0.587f*G + 0.114f*B - 128);
                    float Cb_Value = (-0.168736f*R - 0.331264f*G + 0.5f*B);
                    float Cr_Value = (0.5f*R - 0.418688f*G - 0.081312f*B);

                    MidLum[Y*8+X] = (i8) CLAMP(Lum_Value, -128, 127);
                    MidCb[Y*8+X] = (i8) CLAMP(Cb_Value, -128, 127);
                    MidCr[Y*8+X] = (i8) CLAMP(Cr_Value, -128, 127);

#else
                    __m128i XMM0, XMM9, XMM10, XMM11;
                    __m128 XMM1, XMM5, XMM6, XMM7;
                    ALIGNED(16) f32 Result[4];

                    // BB, GG, RR, AA
                    XMM0 = _mm_loadu_si32(SubCol);
                    XMM0 = _mm_cvtepu8_epi32(XMM0);
                    XMM1 = _mm_cvtepi32_ps(XMM0);

                    XMM5 = _mm_mul_ps(XMM1, XMM2);
                    XMM6 = _mm_mul_ps(XMM1, XMM3);
                    XMM7 = _mm_mul_ps(XMM1, XMM4);

                    XMM5 = _mm_hadd_ps(XMM5, XMM6);
                    XMM7 = _mm_hadd_ps(XMM7, XMM12);
                    XMM5 = _mm_hadd_ps(XMM5, XMM7);

                    // TODO: No clamping?
                    _mm_store_ps(Result, XMM5);

                    u8 Idx = X*8+Y;
                    MidLum[Idx] = Result[0] - 128;
                    MidCb[Idx] = Result[1];
                    MidCr[Idx] = Result[2];
#endif

                    SubCol += 4;
                }

                SubRow += Pitch;
            }

            MidCol += 8 * 4;
        }

        MidRow += 8 * Pitch;
    }

#if 0
    for(u8 CY = 0;
        CY < 8;
        CY++)
    {
        for(u8 CX = 0;
            CX < 8;
            CX++)
        {
            f32 SumCb = 0;
            f32 SumCr = 0;

            for(u8 KY = 0;
                KY < SY;
                KY++)
            {
                for(u8 KX = 0;
                    KX < SX;
                    KX++)
                {
                    SumCb += Cb[KY][KX][CY][CX];
                    SumCr += Cr[KY][KX][CY][CX];
                }
            }

            Cb[0][0][CY][CX] = (SumCb / SXSY);
            Cr[0][0][CY][CX] = (SumCr / SXSY);
        }
    }
#else
    u8 SXSY = 4;
    __m128 XMM5 = _mm_set1_ps(SXSY);

    for(u8 CY = 0;
        CY < 8;
        CY++)
    {
        for(u8 CX = 0;
            CX < 8;
            CX += 4)
        {
            __m128 XMM0 = _mm_setzero_ps();
            __m128 XMM1 = _mm_setzero_ps();

            for(u8 KY = 0;
                KY < 2;
                KY++)
            {
                for(u8 KX = 0;
                    KX < 2;
                    KX++)
                {
                    __m128 XMM00 = _mm_load_ps(&Cb[KY][KX][CY][CX]);
                    __m128 XMM11 = _mm_load_ps(&Cr[KY][KX][CY][CX]);

                    XMM0 = _mm_add_ps(XMM0, XMM00);
                    XMM1 = _mm_add_ps(XMM1, XMM11);
                }
            }

            XMM0 = _mm_div_ps(XMM0, XMM5);
            XMM1 = _mm_div_ps(XMM1, XMM5);

            _mm_store_ps(&Cb[0][0][CY][CX], XMM0);
            _mm_store_ps(&Cr[0][0][CY][CX], XMM1);
        }
    }
#endif
}

static void YCbCr_to_BGRA_8x8(void* Data, usz Pitch, f32* Y, f32* Cb, f32* Cr)
{
    u8* Row = Data;

    for(u32 KY = 0;
        KY < 8;
        KY++)
    {
        u8* Col = Row;

        for(u32 KX = 0;
            KX < 8;
            KX++)
        {
            u32 Idx = KY*8 + KX;

            f32 Y_value  = Y[Idx] + 128;
            f32 Cb_value = Cb[Idx];
            f32 Cr_value = Cr[Idx];

            f32 B = Y_value + 1.772F    * Cb_value;
            f32 G = Y_value - 0.344136F * Cb_value - 0.714136F * Cr_value;
            f32 R = Y_value                        + 1.402F    * Cr_value;

            u32 Value = ((u8)CLAMP(B, 0, 255) << 0) |
                        ((u8)CLAMP(G, 0, 255) << 8) |
                        ((u8)CLAMP(R, 0, 255) << 16);

            *(u32*)(Col) = Value;

            Col += 4;
        }

        Row += Pitch;
    }
}

static void YCbCr_to_BGRA_420_8x8(u8* Data, usz Pitch, f32 Y[2][2][8][8], f32 Cb[8][8], f32 Cr[8][8])
{
#if 0
    u8* SubRow = Data;

    u32 SubY = 2 << 3;
    u32 SubX = 2 << 3;

    for(u32 Row = 0;
        Row < SubY;
        Row++)
    {
        u8* SubCol = SubRow;

        for(u32 Col = 0;
            Col < SubX;
            Col++)
        {
            // TODO: SIMD!!!

            f32 Y_value = Y[Row/8][Col/8][Row&7][Col&7] + 128;
            f32 Cb_value = Cb[Row/2][Col/2];
            f32 Cr_value = Cr[Row/2][Col/2];

            f32 B = Y_value + 1.772F    * Cb_value;
            f32 G = Y_value - 0.344136F * Cb_value - 0.714136F * Cr_value;
            f32 R = Y_value                        + 1.402F    * Cr_value;

            SubCol[0] = (u8) CLAMP(B, 0, 255);
            SubCol[1] = (u8) CLAMP(G, 0, 255);
            SubCol[2] = (u8) CLAMP(R, 0, 255);

            SubCol += 4;
        }

        SubRow += Pitch;
    }
#else
    YCbCr_to_BGRA_8x8(Data,                 Pitch, &Y[0][0][0][0], &Cb[0][0], &Cr[0][0]);
    YCbCr_to_BGRA_8x8(Data + 8*4,           Pitch, &Y[0][1][0][0], &Cb[0][0], &Cr[0][0]);
    YCbCr_to_BGRA_8x8(Data + 8*Pitch,       Pitch, &Y[1][0][0][0], &Cb[0][0], &Cr[0][0]);
    YCbCr_to_BGRA_8x8(Data + 8*Pitch + 8*4, Pitch, &Y[1][1][0][0], &Cb[0][0], &Cr[0][0]);
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
