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

typedef struct
{
    u8* At;
    usz Elapsed;
    u32 Buf;
    u8 Len;
} bit_stream;

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
    
    BytesCount = (u8) Min(BitStream->Elapsed, BytesCount);

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

    // i16 C[8][8];

    // for(u8 Y = 0;
    //     Y < 8;
    //     Y++)
    // {
    //     for(u8 X = 0;
    //         X < 8;
    //         X++)
    //     {
    //         C[Y][X] = (i16)(D[Y][X] / DQT[Y*8+X]);
    //     }
    // }

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

            Z[Index] = (i16)D[Y][X];
        }
    }

    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        Z[Idx] /= DQT[Idx];
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

    for(Idx = 0;
        Idx < 64;
        Idx++)
    {
        Z[Idx] *= DQT[Idx];
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
                S += tDCT8x8Table[Y][K] * C[K][X];
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

            P[Y][X] = ClampI8(S);
        }
    }

    return 1;
}

static int PushImage(bit_stream* BitStream, bitmap* Bitmap, 
                     const u8* DQT_Y, const u8* DQT_Chroma, 
                     const u8* DHT_Y_DC, const u8* DHT_Y_AC, 
                     const u8* DHT_Chroma_DC, const u8* DHT_Chroma_AC)
{
    // TODO: Handling of images other than mod 8
    u16 NumBlocksX = (u16)(Bitmap->Width + 7) / 8;
    u16 NumBlocksY = (u16)(Bitmap->Height + 7) / 8;

    i16 DC_Y = 0;
    i16 DC_Cb = 0;
    i16 DC_Cr = 0;

    u8* Row = Bitmap->At;
    for(u16 BlockY = 0; 
        BlockY < NumBlocksY; 
        BlockY++)
    {
        u8* Col = Row;

        for(u16 BlockX = 0;
            BlockX < NumBlocksX;
            BlockX++)
        {
            i8 Y[8][8];
            i8 Cb[8][8];
            i8 Cr[8][8];

            u8* SubRow = Col;
            for(u8 SubY = 0;
                SubY < 8;
                SubY++)
            {
                u8* SubCol = SubRow;

                for(u8 SubX = 0;
                    SubX < 8;
                    SubX++)
                {
                    u8 B = SubCol[0];
                    u8 G = SubCol[1];
                    u8 R = SubCol[2];

                    Y[SubY][SubX]  = (i8)(0.299f*R + 0.587f*G + 0.114f*B - 128);
                    Cb[SubY][SubX] = (i8)(-0.168736f*R - 0.331264f*G + 0.5f*B);
                    Cr[SubY][SubX] = (i8)(0.5f*R - 0.418688f*G - 0.081312f*B);

                    SubCol += 4;
                }

                SubRow += Bitmap->Pitch;
            }

            // TODO: Subsampling
            Assert(PushPixels(BitStream, Y, DQT_Y, DHT_Y_DC, DHT_Y_AC, &DC_Y));
            Assert(PushPixels(BitStream, Cb, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cb));
            Assert(PushPixels(BitStream, Cr, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cr));

            Col += 8 * 4;
        }

        Row += Bitmap->Pitch * 8;
    }

    return 1;
}

static int PopImage(bit_stream* BitStream, bitmap* Bitmap, 
                    const u8* DQT_Y, const u8* DQT_Chroma, 
                    const u8* DHT_Y_DC, const u8* DHT_Y_AC, 
                    const u8* DHT_Chroma_DC, const u8* DHT_Chroma_AC)
{
    // TODO: Handling of images other than mod 8
    u16 NumBlocksX = (u16)(Bitmap->Width + 7) / 8;
    u16 NumBlocksY = (u16)(Bitmap->Height + 7) / 8;

    i16 DC_Y = 0;
    i16 DC_Cb = 0;
    i16 DC_Cr = 0;

    u8* Row = Bitmap->At;
    for(u16 BlockY = 0; 
        BlockY < NumBlocksY; 
        BlockY++)
    {
        u8* Col = Row;

        for(u16 BlockX = 0;
            BlockX < NumBlocksX;
            BlockX++)
        {
            i8 Y[8][8];
            i8 Cb[8][8];
            i8 Cr[8][8];

            // TODO: Subsampling
            Assert(PopPixels(BitStream, Y, DQT_Y, DHT_Y_DC, DHT_Y_AC, &DC_Y));
            Assert(PopPixels(BitStream, Cb, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cb));
            Assert(PopPixels(BitStream, Cr, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cr));

            u8* SubRow = Col;
            for(u8 SubY = 0;
                SubY < 8;
                SubY++)
            {
                u8* SubCol = SubRow;

                for(u8 SubX = 0;
                    SubX < 8;
                    SubX++)
                {
                    u8 Y_value = Y[SubY][SubX] + 128;
                    i8 Cb_value = Cb[SubY][SubX];
                    i8 Cr_value = Cr[SubY][SubX];

                    f32 B = Y_value + 1.772F * Cb_value;
                    f32 G = Y_value - 0.3441F * Cb_value - 0.71414F * Cr_value;
                    f32 R = Y_value + 1.402F * Cr_value;

                    SubCol[0] = (u8)ClampU8(B);
                    SubCol[1] = (u8)ClampU8(G);
                    SubCol[2] = (u8)ClampU8(R);

                    SubCol += 4;
                }

                SubRow += Bitmap->Pitch;
            }

            Col += 8 * 4;
        }

        Row += Bitmap->Pitch * 8;
    }

    return 1;
}

static int ParseJPEG(void* Data, usz Size, bitmap* Bitmap);
static void* ExportJPEG(bitmap* Bitmap, usz* Size);
