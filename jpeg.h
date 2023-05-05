#define JPEG_SOI  0xD8FF
#define JPEG_APP0 0xE0FF
#define JPEG_APP1 0xE1FF
#define JPEG_COM  0xFEFF
#define JPEG_DQT  0xDBFF
#define JPEG_SOF0 0xC0FF
#define JPEG_SOF2 0xC2FF
#define JPEG_DHT  0xC4FF
#define JPEG_SOS  0xDAFF
#define JPEG_EOI  0xD9FF

// Marker: 0xE0FF Length: 16
// Marker: 0xE1FF Length: 34
// Marker: 0xDBFF Length: 67
// Marker: 0xDBFF Length: 67
// Marker: 0xC0FF Length: 17
// Marker: 0xC4FF Length: 31
// Marker: 0xC4FF Length: 181
// Marker: 0xC4FF Length: 31
// Marker: 0xC4FF Length: 181
// Marker: 0xDAFF

// Marker: 0xE0FF Length: 0x0010
// Marker: 0xDBFF Length: 0x0043
// Marker: 0xDBFF Length: 0x0043
// Marker: 0xC0FF Length: 0x0011
// Marker: 0xC4FF Length: 0x001D
// Marker: 0xC4FF Length: 0x0042
// Marker: 0xC4FF Length: 0x001B
// Marker: 0xC4FF Length: 0x0036
// Marker: 0xDAFF Length: 0x000C
// Marker: 0xCCB2 Length: 0x506F
// Marker: 0x3FAA Length: 0x49E0
// Marker: 0xD4CD Length: 0xE128

// Marker: 0xE0FF Length: 0x0010
// Marker: 0xFEFF Length: 0x005B
// Marker: 0xDBFF Length: 0x0084
// Marker: 0xC2FF Length: 0x0011
// Marker: 0xC4FF Length: 0x001D
// Marker: 0xDAFF Length: 0x0008
// Marker: 0xA6FA Length: 0x0000

// 00 -> 0
// 010 -> 1
// 011 -> 2
// 100 -> 3
// 101 -> 4
// 110 -> 5
// 1110 -> 6
// 11110 -> 7
// 111110 -> 8
// 1111110 -> 9
// 11111110 -> 10
// 111111110 -> 11

#define JPEG_COMPONENT_Y 1
#define JPEG_COMPONENT_Cb 2
#define JPEG_COMPONENT_Cr 3
#define JPEG_COMPONENT_Additional 4

#pragma pack(push, 1)
typedef struct
{
    u8 JFIF[5]; // JFIF identifier, should be "JFIF\0"
    u8 VersionMajor; // Major version number
    u8 VersionMinor; // Minor version number
    u8 DensityUnit; // Unit of measure for X and Y density fields
    u16 HorizontalDensity; // Horizontal pixel density
    u16 VerticalDensity; // Vertical pixel density
    u8 ThumbnailWidth; // Thumbnail image width
    u8 ThumbnailHeight; // Thumbnail image height
} jpeg_app0;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u8 Id;
    u8 Coefficients[64];
} jpeg_dqt;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u8 BitsPerSample; // Number of bits per sample
    u16 ImageHeight; // Image height in pixels
    u16 ImageWidth; // Image width in pixels
    u8 NumComponents; // Number of color components in the image
    struct {
        u8 ComponentId; // Component identifier
        u8 VerticalSubsamplingFactor : 4;
        u8 HorizontalSubsamplingFactor : 4;
        u8 QuantizationTableId; // Quantization table identifier for this component
    } Components[3]; // Up to 3 components (Y, Cb, Cr)
} jpeg_sof0;
#pragma pack(pop)

#define JPEG_DHT_CLASS_DC 0
#define JPEG_DHT_CLASS_AC 1

#pragma pack(push, 1)
typedef struct
{
    union
    {
        struct
        {
            u8 TableId : 4; // Huffman table identifier (0-3)
            u8 TableClass : 4; // Huffman table class (0 for DC, 1 for AC)
        };
        u8 Header;
    };
    u8 Counts[16]; // Number of Huffman codes for each code length
    u8 Values[];
} jpeg_dht;
#pragma pack(pop)

static u16 JPEG_DHT_GetLength(const jpeg_dht* DHT)
{
    u16 Length = 0;

    // TODO: SIMD!!!

    for(u8 Idx = 0;
        Idx < ArrayCount(DHT->Counts);
        Idx++)
    {
        Length += DHT->Counts[Idx];
    }

    return Length;   
}

typedef struct
{
    u8* At;
    usz Elapsed;
    u32 Buf;
    u8 Len;
} bit_stream;

static int Refill(bit_stream* BitStream);

static int PopBits(bit_stream* BitStream, u16* Value, u8 Size);

static int PopValue(bit_stream* BitStream, i16* Value, u8 Size)
{
    int Result = 0;

    u16 Raw;
    if(PopBits(BitStream, &Raw, Size))
    {
        printf("PopBits(%d): ", Size);
        for(int I = Size-1;
            I >= 0;
            I--)
        {
            putchar((BitStream->Buf & (1 << I)) ? '1' : '0');
        }
        putchar('\n');

        u16 Thresh = (1 << (Size - 1));
        if(Raw >= Thresh)
        {
            *Value = Raw;
        }
        else
        {
            *Value = Raw - (2 * Thresh - 1);
        }

        printf("Value: %d\n", *Value);

        Result = 1;
    }

    return Result;
}

static int DecodeSymbol(bit_stream* BitStream, jpeg_dht* DHT, u8* Symbol)
{
    int Result = 0;

    if(Refill(BitStream))
    {
        u16 Code = 0;
        u8* At = DHT->Values;
        printf("DecodeSymbol:\n");
        for(u8 I = 0; I < 16; I++)
        {
            u8 BitLen = I+1;
            for(u8 J = 0; J < DHT->Counts[I]; J++)
            {
                for(int K = 0;
                    K < BitLen;
                    K++)
                {
                    putchar((Code & (1 << K)) ? '1' : '0');
                }
                printf(" -> %d\n", *At);

                Assert(BitStream->Len >= I);

                int Match;
#if 0
                u16 Mask = (1 << BitLen) - 1;
                if((BitStream->Buf & Mask) == Code)
                {
                    Match = 1;
                }
                else
                {
                    Match = 0;
                }
#else
                Match = 1;
                u32 Mask = 0x1;
                for(int K = BitLen-1;
                    K >= 0;
                    K--)
                {
                    int Left = (BitStream->Buf & Mask) != 0;
                    int Right = (Code & (1 << K)) != 0;
                    if(Left != Right)
                    {
                        Match = 0;
                    }

                    Mask <<= 1;
                }
#endif

                if(Match)
                {
                    printf("MATCH!\n");
                    BitStream->Buf >>= BitLen;
                    BitStream->Len -= BitLen;
                    *Symbol = *At;
                    return 1;
                }

                Code++;
                At++;
            }

            Code <<= 1;
        }
    }

    return Result;
}

static const f32 DCT8x8Table[8][8] = 
{
    { +0.3536f, +0.3536f, +0.3536f, +0.3536f, +0.3536f, +0.3536f, +0.3536f, +0.3536f },
    { +0.4904f, +0.4157f, +0.2778f, +0.0975f, -0.0975f, -0.2778f, -0.4157f, -0.4904f },
    { +0.4619f, +0.1913f, -0.1913f, -0.4619f, -0.4619f, -0.1913f, +0.1913f, +0.4619f },
    { +0.4157f, -0.0975f, -0.4904f, -0.2778f, +0.2778f, +0.4904f, +0.0975f, -0.4157f },
    { +0.3536f, -0.3536f, -0.3536f, +0.3536f, +0.3536f, -0.3536f, -0.3536f, +0.3536f },
    { +0.2778f, -0.4904f, +0.0975f, +0.4157f, -0.4157f, -0.0975f, +0.4904f, -0.2778f },
    { +0.1913f, -0.4619f, +0.4619f, -0.1913f, -0.1913f, +0.4619f, -0.4619f, +0.1913f },
    { +0.0975f, -0.2778f, +0.4157f, -0.4904f, +0.4904f, -0.4157f, +0.2778f, -0.0975f },
};

static const f32 tDCT8x8Table[8][8] = 
{
    { +0.3536f, +0.4904f, +0.4619f, +0.4157f, +0.3536f, +0.2778f, +0.1913f, +0.0975f },
    { +0.3536f, +0.4157f, +0.1913f, -0.0975f, -0.3536f, -0.4904f, -0.4619f, -0.2778f },
    { +0.3536f, +0.2778f, -0.1913f, -0.4904f, -0.3536f, +0.0975f, +0.4619f, +0.4157f },
    { +0.3536f, +0.0975f, -0.4619f, -0.2778f, +0.3536f, +0.4157f, -0.1913f, -0.4904f },
    { +0.3536f, -0.0975f, -0.4619f, +0.2778f, +0.3536f, -0.4157f, -0.1913f, +0.4904f },
    { +0.3536f, -0.2778f, -0.1913f, +0.4904f, -0.3536f, -0.0975f, +0.4619f, -0.4157f },
    { +0.3536f, -0.4157f, +0.1913f, +0.0975f, -0.3536f, +0.4904f, -0.4619f, +0.2778f },
    { +0.3536f, -0.4904f, +0.4619f, -0.4157f, +0.3536f, -0.2778f, +0.1913f, -0.0975f },
};

static void AddScalar8x8(u8* C, i8 K)
{
    for(u8 Y = 0; Y < 8; Y++)
    {
        for(u8 X = 0; X < 8; X++)
        {
            C[Y*8+X] += K;

            printf("%4d, ", C[Y*8+X]);
        }

        putchar('\n');
    }
}

static void IDCT8x8(i16 C[8][8])
{
    // tT * R * T

    f32 M[8][8];

    for(u8 Y = 0; Y < 8; Y++)
    {
        for(u8 X = 0; X < 8; X++)
        {
            f32 S = 0;

            for(u8 K = 0; K < 8; K++)
            {
                S += tDCT8x8Table[Y][K] * C[K][X];
            }

            M[Y][X] = S;

            printf("%10.4f, ", M[Y][X]);
        }
        putchar('\n');
    }

    putchar('\n');

    for(u8 Y = 0; Y < 8; Y++)
    {
        for(u8 X = 0; X < 8; X++)
        {
            f32 S = 0;

            for(u8 K = 0; K < 8; K++)
            {
                S += M[Y][K] * DCT8x8Table[K][X];
            }

            C[Y][X] = (i8) S;

            printf("%4d, ", C[Y][X]);
        }

        putchar('\n');
    }
}

static inline u8 ClampU8(f32 F)
{
    if(F >= 255)
    {
        return 255;
    }
    else if(F <= 0)
    {
        return 0;
    }
    else
    {
        return (u8)F;
    }
}

static inline i8 ClampI8(f32 F)
{
    if(F >= 127)
    {
        return 127;
    }
    else if(F <= -128)
    {
        return -128;
    }
    else
    {
        return (i8)F;
    }
}

static void DecodeMCU(bit_stream* BitStream, i16* C, jpeg_dht* HT_DC, jpeg_dht* HT_AC, i16* DC_Sum, u8 N)
{
    u8 DC_Size;
    Assert(DecodeSymbol(BitStream, HT_DC, &DC_Size));

    i16 DC_Coeff;
    Assert(PopValue(BitStream, &DC_Coeff, DC_Size));
    
    *DC_Sum += DC_Coeff;

    C[0] = *DC_Sum;

    u8 Idx = 1;
    while(Idx < N)
    {
        u8 AC_CountSize;
        Assert(DecodeSymbol(BitStream, HT_AC, &AC_CountSize));
        if(AC_CountSize == 0)
        {
            printf("EOB occured\n");
            goto end;
        }
        else if(AC_CountSize == 0xF0)
        {
            Idx += 16;
            Assert(Idx <= N);
            continue;
        }

        u8 AC_Count = AC_CountSize >> 4;
        u8 AC_Size = AC_CountSize & 0x0F;
        printf("AC_Count: %d\n", AC_Count);
        printf("AC_Size: %d\n", AC_Size);

        i16 AC_Coeff;
        Assert(PopValue(BitStream, &AC_Coeff, AC_Size));

        Idx += AC_Count;
        Assert(Idx <= N);
        if(Idx < N)
        {
            C[Idx] = AC_Coeff;

            Idx++;
        }
        else
        {
            Assert(AC_Size == 0);
        }
    }

    // u8 AC_CountSize;
    // Assert(DecodeSymbol(BitStream, HuffmanTables[JPEG_DHT_CLASS_AC], &AC_CountSize));
    // Assert(AC_CountSize == 0);

end:
    printf("Quantized: ");
    for(Idx = 0;
        Idx < N;
        Idx++)
    {
        printf("%d, ", C[Idx]);
    }
    putchar('\n');
}

static void ScalarMul8x8(i16 M[8][8], u8* C)
{
    for(u8 Y = 0; Y < 8; Y++)
    {
        for(u8 X = 0; X < 8; X++)
        {
            M[Y][X] *= C[Y*8+X];
            printf("%4d, ", M[Y][X]);
        }
        putchar('\n');
    }
}

static const u8 ZigZagTable[8][8] =
{
    { 0,  1,  5,  6, 14, 15, 27, 28},
    { 2,  4,  7, 13, 16, 26, 29, 42},
    { 3,  8, 12, 17, 25, 30, 41, 43},
    { 9, 11, 18, 24, 31, 40, 44, 53},
    {10, 19, 23, 32, 39, 45, 52, 54},
    {20, 22, 33, 38, 46, 51, 55, 60},
    {21, 34, 37, 47, 50, 56, 59, 61},
    {35, 36, 48, 49, 57, 58, 62, 63},
};

static void DeZigZag8x8(i16 Src[64], i16 Dst[8][8])
{
    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            u8 Index = ZigZagTable[Y][X];
            Dst[Y][X] = Src[Index];
            printf("%4d ", Dst[Y][X]);
        }
        putchar('\n');
    }
}

#pragma pack(push, 1)
typedef struct
{
    u8 NumComponents; // Number of image components in scan, should be 1, 3, or 4
    struct
    {
        u8 ComponentId; // Index of the image component (1 = Y, 2 = Cb, 3 = Cr, 4 = additional component)
        union
        {
            struct
            {
                u8 IndexDC : 4; // High 4 bits: DC entropy coding table index, Low 4 bits: AC entropy coding table index
                u8 IndexAC : 4; // High 4 bits: DC entropy coding table index, Low 4 bits: AC entropy coding table index
            };
            u8 Index;
        };
    } Components[3];
    u8 StartOfSelection; // Spectral selection start (0-63)
    u8 EndOfSelection; // Spectral selection end (0-63)
    u8 ApproximationBitLow : 4; // Approximation bit position high and low nibbles
    u8 ApproximationBitHigh : 4; // Approximation bit position high and low nibbles
} jpeg_sos;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u16 Marker;
} jpeg_soi;
#pragma pack(pop)

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
        if(PushU16(Buffer, ByteSwap16(Length)))
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

static u8 EncodeNumber(i16 Number, u16* Value)
{
    // TODO: Optimize me please...

    if(Number == 0)
    {
        return 0;
    }

    u32 Raw;
    if(Number < 0)
    {
        Raw = -Number;
    }
    else
    {
        Raw = Number;
    }

    u8 Idx;
    for(Idx = 1;
        Idx < 16;
        Idx++)
    {
        u32 Top = (1 << Idx);
        if(Raw < Top)
        {
            break;        
        }  
    }

    if(Number < 0)
    {
        u32 Top = (1 << Idx);
        *Value = (u16)(Top - Raw - 1);
    }
    else
    {
        *Value = (u16)(Raw);
    }

    return Idx;
}

static i16 DecodeNumber(u8 Size, u16 Value)
{
    if(Size == 0)
    {
        return 0;
    }

    u16 Mask = (1 << (Size - 1));
    if(Value & Mask)
    {
        return Value;
    }
    else
    {
        return -(1 << Size) + Value + 1;
    }
}

#define ReverseBits8(x) ((((x) * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32)

static u16 ReverseBits16(u16 Value)
{
    u16 Result = 0;

    u16 Mask = 0x8000;
    for(u8 Idx = 0;
        Idx < 16;
        Idx++)
    {
        if(Value & (1 << Idx))
        {
            Result |= Mask;
        }

        Mask >>= 1;
    }

    return Result;
}

static int Refill(bit_stream* BitStream)
{
    u8 BytesCount = (32 - BitStream->Len) / 8;
    Assert(BitStream->Elapsed > BytesCount);

    for(u8 ByteIdx = 0;
        ByteIdx < BytesCount;
        ByteIdx++)
    {
        u32 Byte = *(BitStream->At++);
        BitStream->Elapsed--;
        if(Byte == 0xFF)
        {
            u8 NextByte = *(BitStream->At++);
            BitStream->Elapsed--;
            Assert(NextByte == 0x00);
        }

#if 0
        BitStream->Buf |= (Byte << BitStream->Len);
#else
        BitStream->Buf <<= 8;
        BitStream->Buf |= Byte;
#endif

        BitStream->Len += 8;
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

    Assert(Size);

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

    Assert(Size);
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

static int PopSymbol(bit_stream* BitStream, const u8* DHT, u8* Value)
{
    Assert(Refill(BitStream));
    Assert(BitStream->Len > 16);

    const u8* Counts = &DHT[0];
    const u8* At = &DHT[16];

    u16 Code = 0;
    for(u8 I = 0; I < 16; I++)
    {
        u8 BitLen = I+1;

        u16 Mask = (1 << BitLen) - 1;
        u8 NextLen = (BitStream->Len - BitLen);
        u16 Compare = (BitStream->Buf >> NextLen) & Mask;

        for(u8 J = 0; J < Counts[I]; J++)
        {
#if 0
            u16 Symbol = ReverseBits16(Code) >> (16 - BitLen);
#else
            u16 Symbol = Code;
#endif

            if(Compare == Symbol)
            {
                BitStream->Len = NextLen;

                *Value = *At;
                return 1;
            }

            Code++;
            At++;
        }

        Code <<= 1;
    }

    return 0;
}

static int PushSymbol(bit_stream* BitStream, const u8* DHT, u8 Value)
{
    // TODO: Separate push/encode symbol?

    const u8* Counts = &DHT[0];
    const u8* At = &DHT[16];

    u16 Code = 0;
    for(u8 I = 0; I < 16; I++)
    {
        u8 BitLen = I+1;
        for(u16 J = 0; J < Counts[I]; J++)
        {
            if(*At == Value)
            {
#if 0
                // TODO: Isn't there really any clever way for that?!
                u16 Symbol = ReverseBits16(Code) >> (16 - BitLen);
#else
                u16 Symbol = Code;
#endif

                return PushBits(BitStream, Symbol, BitLen);
            }

            Code++;
            At++;
        }

        Code <<= 1;
    }

    return 0;
}

static int PushPixels(bit_stream* BitStream, i8 P[8][8], const u8* DQT, const u8* DHT_DC, const u8* DHT_AC, i16* DC)
{
    f32 Tmp[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            f32 S = 0;

            for(u8 K = 0;
                K < 8;
                K++)
            {
                S += DCT8x8Table[Y][K] * P[K][X];
            }

            Tmp[Y][X] = S;
        }
    }

    f32 D[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            f32 S = 0;

            for(u8 K = 0;
                K < 8;
                K++)
            {
                S += Tmp[Y][K] * tDCT8x8Table[K][X];
            }

            D[Y][X] = S;
        }
    }

    i16 C[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            C[Y][X] = (i16)(D[Y][X] / DQT[Y*8+X]);
        }
    }

    i16 Z[64];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            u8 Index = ZigZagTable[Y][X];

            Z[Index] = C[Y][X];
        }
    }

    i16 PrevDC = *DC;

    *DC = Z[0];

    Z[0] = Z[0] - PrevDC;

    u16 DC_Value;
    u8 DC_Size = EncodeNumber(Z[0], &DC_Value);

    Assert(PushSymbol(BitStream, DHT_DC, DC_Size));

    if(DC_Size)
    {
        Assert(PushBits(BitStream, DC_Value, DC_Size));
    }

    for(u8 Idx = 1;
        Idx < 64;
        Idx++)
    {
        u8 AC_RunLength = 0;
        while(!Z[Idx])
        {
            Idx++;
            AC_RunLength++;
            if(Idx == 64)
            {
                Assert(PushSymbol(BitStream, DHT_AC, 0x00));

                return 1;
            }
            else if(AC_RunLength == 16)
            {
                Assert(PushSymbol(BitStream, DHT_AC, 0xF0));

                AC_RunLength = 0;
            }
        }

        u16 AC_Value;
        u8 AC_Size = EncodeNumber(Z[Idx], &AC_Value);

        u8 AC_RS = (AC_RunLength << 4) | AC_Size;

        Assert(PushSymbol(BitStream, DHT_AC, AC_RS));

        if(AC_Size)
        {
            Assert(PushBits(BitStream, AC_Value, AC_Size));
        }
    }

    return 1;
}

static int PopPixels(bit_stream* BitStream, i8 P[8][8], const u8* DQT, const u8* DHT_DC, const u8* DHT_AC, i16* DC)
{
    i16 Z[64] = {0};

    u8 DC_Size;

    Assert(PopSymbol(BitStream, DHT_DC, &DC_Size));

    u16 DC_Value = 0;
    if(DC_Size)
    {
        Assert(PopBits(BitStream, &DC_Value, DC_Size));
    }

    Z[0] = DecodeNumber((u8)DC_Size, DC_Value);

    Z[0] += *DC;

    *DC = Z[0];

    u8 Idx = 1;
    while(Idx < 64)
    {
        u8 AC_RS = 0;
        
        Assert(PopSymbol(BitStream, DHT_AC, &AC_RS));

        if(AC_RS == 0x00)
        {
            break;
        }
        else if(AC_RS == 0xF0)
        {
            Idx += 16;
            Assert(Idx <= 64);
            continue;
        }

        u8 AC_RunLength = (AC_RS >> 4);
        u8 AC_Size = AC_RS & 0xF;
        Assert(AC_Size);

        Idx += AC_RunLength;
        Assert(Idx < 64);

        u16 AC_Value;
        Assert(PopBits(BitStream, &AC_Value, AC_Size));

        Z[Idx] = DecodeNumber(AC_Size, AC_Value);

        Idx++;
    }
    
    i16 C[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            u8 Index = ZigZagTable[Y][X];

            C[Y][X] = Z[Index];
        }
    }

    f32 D[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            D[Y][X] = (f32)(C[Y][X] * DQT[Y*8+X]);
        }
    }

    f32 Tmp[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            f32 S = 0;

            for(u8 K = 0;
                K < 8;
                K++)
            {
                S += tDCT8x8Table[Y][K] * D[K][X];
            }

            Tmp[Y][X] = S;
        }
    }

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            f32 S = 0;

            for(u8 K = 0;
                K < 8;
                K++)
            {
                S += Tmp[Y][K] * DCT8x8Table[K][X];
            }

            P[Y][X] = (i8)ClampI8(S);
        }
    }

    return 1;
}

static int ParseJPEG(void* Data, usz Size, bitmap* Bitmap);
static void* ExportJPEG(bitmap* Bitmap, usz* Size);
