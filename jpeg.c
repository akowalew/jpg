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

static int PopSymbol(bit_stream* BitStream, const u8* DHT, u8* Value)
{
    Assert(Refill(BitStream));

    const u8* Counts = &DHT[0];
    const u8* At = &DHT[16];

    u16 Code = 0;
    for(u8 I = 0; I < 16; I++)
    {
        u8 BitLen = I+1;

        Assert(BitStream->Len >= BitLen);

        u16 Mask = (1 << BitLen) - 1;
        u8 NextLen = (BitStream->Len - BitLen);
        u16 Compare = (BitStream->Buf >> NextLen) & Mask;

        for(u8 J = 0; J < Counts[I]; J++)
        {
            if(Compare == Code)
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
                return PushBits(BitStream, Code, BitLen);
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

            Z[Index] = (i16) CLAMP(D[Y][X], -32768, 32767);
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
    Assert(PushBits(BitStream, DC_Value, DC_Size));

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
        Assert(PushBits(BitStream, AC_Value, AC_Size));
    }

    return 1;
}

static int PopPixels(bit_stream* BitStream, i8 P[8][8], const u8* DQT, const u8* DHT_DC, const u8* DHT_AC, i16* DC)
{
    i16 Z[64] = {0};

    u8 DC_Size;
    Assert(PopSymbol(BitStream, DHT_DC, &DC_Size));

    u16 DC_Value;
    Assert(PopBits(BitStream, &DC_Value, DC_Size));

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
        int V = Z[Idx];
        V *= DQT[Idx];
        Z[Idx] = (i16) CLAMP(V, -32768, 32767);
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

            P[Y][X] = (i8) CLAMP(S, -128, 127);
        }
    }

    return 1;
}

static int PushImage(bit_stream* BitStream, bitmap* Bitmap, 
                     const u8* DQT_Y, const u8* DQT_Chroma, 
                     const u8* DHT_Y_DC, const u8* DHT_Y_AC, 
                     const u8* DHT_Chroma_DC, const u8* DHT_Chroma_AC,
                     u8 SamplingX, u8 SamplingY)
{
    Assert(SamplingX <= 2);
    Assert(SamplingY <= 2);

    // TODO: Handling of images other than mod 8
    u16 NumBlocksX = (u16)((Bitmap->Width + 7) / 8) / SamplingX;
    u16 NumBlocksY = (u16)((Bitmap->Height + 7) / 8) / SamplingY;

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
            i8  Y[2][2][8][8];
            i8 Cb[2][2][8][8];
            i8 Cr[2][2][8][8];

            u8* MidRow = Col;
            for(u8 SY = 0;
                SY < SamplingY;
                SY++)
            {
                u8* MidCol = MidRow;

                for(u8 SX = 0;
                    SX < SamplingX;
                    SX++)
                {
                    u8* SubRow = MidCol;

                    i8* MidY =  (i8*) Y[SY][SX];
                    i8* MidCb = (i8*) Cb[SY][SX];
                    i8* MidCr = (i8*) Cr[SY][SX];

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

                            float Y_Value =  (0.299f*R + 0.587f*G + 0.114f*B - 128);
                            float Cb_Value = (-0.168736f*R - 0.331264f*G + 0.5f*B);
                            float Cr_Value = (0.5f*R - 0.418688f*G - 0.081312f*B);

                            // TODO: Do we need CLAMP here or it is mathematically impossible to go out?

                            MidY [SubY*8+SubX] = (i8) CLAMP(Y_Value,  -128, 127);
                            MidCb[SubY*8+SubX] = (i8) CLAMP(Cb_Value, -128, 127);
                            MidCr[SubY*8+SubX] = (i8) CLAMP(Cr_Value, -128, 127);

                            SubCol += 4;
                        }

                        SubRow += Bitmap->Pitch;
                    }

                    MidCol += 8 * 4;
                }

                MidRow += 8 * Bitmap->Pitch;
            }

            for(u8 CY = 0;
                CY < 8;
                CY++)
            {
                for(u8 CX = 0;
                    CX < 8;
                    CX++)
                {
                    int SumCb = 0;
                    int SumCr = 0;

                    for(u8 SY = 0;
                        SY < SamplingY;
                        SY++)
                    {
                        for(u8 SX = 0;
                            SX < SamplingX;
                            SX++)
                        {
                            SumCb += Cb[SY][SX][CY][CX];
                            SumCr += Cr[SY][SX][CY][CX];
                        }
                    }

                    Cb[0][0][CY][CX] = (i8) (SumCb / (SamplingX*SamplingY)); 
                    Cr[0][0][CY][CX] = (i8) (SumCr / (SamplingX*SamplingY));
                }
            }

            for(u8 SY = 0;
                SY < SamplingY;
                SY++)
            {
                for(u8 SX = 0;
                    SX < SamplingX;
                    SX++)
                {
                    Assert(PushPixels(BitStream, Y[SY][SX], DQT_Y, DHT_Y_DC, DHT_Y_AC, &DC_Y));
                }
            }

            Assert(PushPixels(BitStream, Cb[0][0], DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cb));
            Assert(PushPixels(BitStream, Cr[0][0], DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cr));

            Col += 8 * 4 * SamplingX;
        }

        Row += Bitmap->Pitch * 8 * SamplingY;
    }

    return 1;
}

static int PopImage(bit_stream* BitStream, bitmap* Bitmap, 
                    const u8* DQT_Y, const u8* DQT_Chroma, 
                    const u8* DHT_Y_DC, const u8* DHT_Y_AC, 
                    const u8* DHT_Chroma_DC, const u8* DHT_Chroma_AC,
                    u8 SamplingX, u8 SamplingY)
{
    Assert(SamplingX <= 2);
    Assert(SamplingY <= 2);

    // TODO: Handling of images other than mod 8
    u16 NumBlocksX = (u16)((Bitmap->Width + 7) / 8) / SamplingX;
    u16 NumBlocksY = (u16)((Bitmap->Height + 7) / 8) / SamplingY;

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
            i8 Y[2][2][8][8];
            i8 Cb[8][8];
            i8 Cr[8][8];

            for(u8 SY = 0;
                SY < SamplingY;
                SY++)
            {
                for(u8 SX = 0;
                    SX < SamplingX;
                    SX++)
                {
                    Assert(PopPixels(BitStream, Y[SY][SX], DQT_Y, DHT_Y_DC, DHT_Y_AC, &DC_Y));
                }
            }

            Assert(PopPixels(BitStream, Cb, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cb));
            Assert(PopPixels(BitStream, Cr, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cr));

            u8* MidRow = Col;

            for(u8 SY = 0;
                SY < SamplingY;
                SY++)
            {
                u8* MidCol = MidRow;

                for(u8 SX = 0;
                    SX < SamplingX;
                    SX++)
                {
                    u8* SubRow = MidCol;

                    i8* MidY = (i8*) Y[SY][SX];

                    for(u8 SubY = 0;
                        SubY < 8;
                        SubY++)
                    {
                        u8* SubCol = SubRow;

                        for(u8 SubX = 0;
                            SubX < 8;
                            SubX++)
                        {
                            u8 Y_value = MidY[SubY*8+SubX] + 128;
                            i8 Cb_value = Cb[SubY][SubX];
                            i8 Cr_value = Cr[SubY][SubX];

                            f32 B = Y_value + 1.772F * Cb_value;
                            f32 G = Y_value - 0.344136F * Cb_value - 0.714136F * Cr_value;
                            f32 R = Y_value + 1.402F * Cr_value;

                            SubCol[0] = (u8) CLAMP(B, 0, 255);
                            SubCol[1] = (u8) CLAMP(G, 0, 255);
                            SubCol[2] = (u8) CLAMP(R, 0, 255);

                            SubCol += 4;
                        }

                        SubRow += Bitmap->Pitch;
                    }

                    MidCol += 8 * 4;
                }

                MidRow += 8 * Bitmap->Pitch;
            }

            Col += 8 * 4 * SamplingX;
        }

        Row += Bitmap->Pitch * 8 * SamplingY;
    }

    return 1;
}

static const u8 JPEG_STD_Y_Q50[64] = 
{
    // Coefficients (Zig-Zag order)
     16,  11,  12,  14,  12,  10,  16,  14,
     13,  14,  18,  17,  16,  19,  24,  40,
     26,  24,  22,  22,  24,  49,  35,  37,
     29,  40,  58,  51,  61,  60,  57,  51,
     56,  55,  64,  72,  92,  78,  64,  68,
     87,  69,  55,  56,  80, 109,  81,  87,
     95,  98, 103, 104, 103,  62,  77, 113,
    121, 112, 100, 120,  92, 101, 103,  99,

    // Coefficients (2D order)
    // 16,  11,  10,  16,  24,  40,  51,  61,
    // 12,  12,  14,  19,  26,  58,  60,  55,
    // 14,  13,  16,  24,  40,  57,  69,  56,
    // 14,  17,  22,  29,  51,  87,  80,  62,
    // 18,  22,  37,  56,  68, 109, 103,  77,
    // 24,  35,  55,  64,  81, 104, 113,  92,
    // 49,  64,  78,  87, 103, 121, 120, 101,
    // 72,  92,  95,  98, 112, 100, 103,  99,
};

static const u8 JPEG_STD_Chroma_Q50[64] =
{
    // Coefficients (Zig-Zag order)
     17,  18,  18,  24,  21,  24,  47,  26,
     26,  47,  99,  66,  56,  66,  99,  99,
     99,  99,  99,  99,  99,  99,  99,  99,
     99,  99,  99,  99,  99,  99,  99,  99,
     99,  99,  99,  99,  99,  99,  99,  99,
     99,  99,  99,  99,  99,  99,  99,  99,
     99,  99,  99,  99,  99,  99,  99,  99,
     99,  99,  99,  99,  99,  99,  99,  99,

    // Coefficients (2D order)
    // 17,  18,  24,  47,  99,  99,  99,  99,
    // 18,  21,  26,  66,  99,  99,  99,  99,
    // 24,  26,  56,  99,  99,  99,  99,  99,
    // 47,  66,  99,  99,  99,  99,  99,  99,
    // 99,  99,  99,  99,  99,  99,  99,  99,
    // 99,  99,  99,  99,  99,  99,  99,  99,
    // 99,  99,  99,  99,  99,  99,  99,  99,
    // 99,  99,  99,  99,  99,  99,  99,  99,
};

static const u8 JPEG_STD_DHT00[] =
{
    // Id:
    0x00, 

    // Counts
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // Codes:
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
};

static const u8 JPEG_STD_DHT01[] = 
{
    // Id:
    0x10,

    // Counts:
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,

    // Values:
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA,
};

static const u8 JPEG_STD_DHT10[] = 
{
    // Id:
    0x01,

    // Counts:
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,

    // Values:
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
};

static const u8 JPEG_STD_DHT11[] =
{
    // Id:
    0x11,

    // Counts:
    0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,

    // Values:
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA,
};

static void* EncodeJPEG(bitmap* Bitmap, usz* Size, u8 Quality)
{
    u8 SamplingX = 2;
    u8 SamplingY = 2;

    Quality = CLAMP(Quality, 1, 100);

    float Ratio = (Quality <= 50) ? (50.F / Quality) : ((100.F - Quality) / 50);

    Assert(Bitmap->Width < 65536);
    Assert(Bitmap->Height < 65536);

    buffer Buffer;
    Buffer.Elapsed = Bitmap->Size*16; // TODO: Dynamical approach, eeeh?
    Buffer.At = PlatformAlloc(Buffer.Elapsed);
    if(!Buffer.At)
    {
        return 0;
    }

    u8* BufferStart = Buffer.At;

    Assert(PushU16(&Buffer, JPEG_SOI));

    jpeg_app0* APP0;

    Assert(APP0 = PushSegment(&Buffer, JPEG_APP0, jpeg_app0));
    APP0->JFIF[0] = 'J';
    APP0->JFIF[1] = 'F';
    APP0->JFIF[2] = 'I';
    APP0->JFIF[3] = 'F';
    APP0->JFIF[4] = '\0';
    APP0->VersionMajor = 1;
    APP0->VersionMinor = 1;
    APP0->DensityUnit = 1;
    APP0->HorizontalDensity = ByteSwap16(300);
    APP0->VerticalDensity = ByteSwap16(300);
    APP0->ThumbnailWidth = 0;
    APP0->ThumbnailHeight = 0;

    jpeg_dqt* DQT[2];

    Assert(DQT[0] = PushSegmentCount(&Buffer, JPEG_DQT, sizeof(jpeg_dqt)));
    DQT[0]->Id = 0;
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        float Coeff = (JPEG_STD_Y_Q50[Idx] * Ratio);

        DQT[0]->Coefficients[Idx] = (u8) CLAMP(Coeff, 1, 255);
    }

    Assert(DQT[1] = PushSegmentCount(&Buffer, JPEG_DQT, sizeof(jpeg_dqt)));
    DQT[1]->Id = 1;
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        float Coeff = (JPEG_STD_Chroma_Q50[Idx] * Ratio);

        DQT[1]->Coefficients[Idx] = (u8) CLAMP(Coeff, 1, 255);
    }

    jpeg_sof0* SOF0;

    Assert(SOF0 = PushSegment(&Buffer, JPEG_SOF0, jpeg_sof0));
    SOF0->BitsPerSample = 8;
    SOF0->ImageHeight = ByteSwap16(Bitmap->Height);
    SOF0->ImageWidth = ByteSwap16(Bitmap->Width);
    SOF0->NumComponents = 3;
    SOF0->Components[0].ComponentId = 1;
    SOF0->Components[0].VerticalSubsamplingFactor = SamplingY;
    SOF0->Components[0].HorizontalSubsamplingFactor = SamplingX;
    SOF0->Components[0].QuantizationTableId = 0;
    SOF0->Components[1].ComponentId = 2;
    SOF0->Components[1].VerticalSubsamplingFactor = 1;
    SOF0->Components[1].HorizontalSubsamplingFactor = 1;
    SOF0->Components[1].QuantizationTableId = 1;
    SOF0->Components[2].ComponentId = 3;
    SOF0->Components[2].VerticalSubsamplingFactor = 1;
    SOF0->Components[2].HorizontalSubsamplingFactor = 1;
    SOF0->Components[2].QuantizationTableId = 1;

    jpeg_dht* DHT[2][2];

    Assert(DHT[0][0] = PushSegmentCount(&Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT00)));
    memcpy(DHT[0][0], JPEG_STD_DHT00, sizeof(JPEG_STD_DHT00));

    Assert(DHT[0][1] = PushSegmentCount(&Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT01)));
    memcpy(DHT[0][1], JPEG_STD_DHT01, sizeof(JPEG_STD_DHT01));

    Assert(DHT[1][0] = PushSegmentCount(&Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT10)));
    memcpy(DHT[1][0], JPEG_STD_DHT10, sizeof(JPEG_STD_DHT10));

    Assert(DHT[1][1] = PushSegmentCount(&Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT11)));
    memcpy(DHT[1][1], JPEG_STD_DHT11, sizeof(JPEG_STD_DHT11));

    jpeg_sos* SOS;

    Assert(SOS = PushSegment(&Buffer, JPEG_SOS, jpeg_sos));
    SOS->NumComponents = 3;
    SOS->Components[0].ComponentId = 1;
    SOS->Components[0].IndexDC = 0;
    SOS->Components[0].IndexAC = 0;
    SOS->Components[1].ComponentId = 2;
    SOS->Components[1].IndexAC = 1;
    SOS->Components[1].IndexDC = 1;
    SOS->Components[2].ComponentId = 3;
    SOS->Components[2].IndexAC = 1;
    SOS->Components[2].IndexDC = 1;
    SOS->StartOfSelection = 0;
    SOS->EndOfSelection = 63;
    SOS->ApproximationBitLow = 0;
    SOS->ApproximationBitHigh = 0;

    bit_stream BitStream;
    BitStream.At = Buffer.At;
    BitStream.Elapsed = Buffer.Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    Assert(PushImage(&BitStream, Bitmap, 
                     DQT[0]->Coefficients, DQT[1]->Coefficients,
                     &JPEG_STD_DHT00[1], &JPEG_STD_DHT01[1],
                     &JPEG_STD_DHT10[1], &JPEG_STD_DHT11[1],
                     SamplingX, SamplingY));

    u16 Zero = 0;
    u8 NextFullByte = (BitStream.Len + 7) / 8;
    u8 ZeroCount = NextFullByte * 8 - BitStream.Len;
    Assert(PushBits(&BitStream, Zero, ZeroCount));
    Assert(Flush(&BitStream));

#if 1
    bit_stream Orig = BitStream;

    BitStream.At = Buffer.At;
    BitStream.Elapsed = Buffer.Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    memset(Bitmap->At, 0xF0, Bitmap->Size);

    Assert(PopImage(&BitStream, Bitmap, 
                    DQT[0]->Coefficients, DQT[1]->Coefficients,
                    &JPEG_STD_DHT00[1], &JPEG_STD_DHT01[1],
                    &JPEG_STD_DHT10[1], &JPEG_STD_DHT11[1],
                    SamplingX, SamplingY));  

    BitStream = Orig;

    PlatformShowBitmap(Bitmap, "Bitmap");
#endif

    Buffer.At = BitStream.At;
    Buffer.Elapsed = BitStream.Elapsed;

    Assert(PushU16(&Buffer, JPEG_EOI));

    *Size = Buffer.At - BufferStart;

    return BufferStart;
}

static int DecodeJPEG(void* Data, usz Size, bitmap* Bitmap)
{
    buffer Buffer;
    Buffer.At = Data;
    Buffer.Elapsed = Size;

    u16* MarkerAt = PopU16(&Buffer);
    Assert(MarkerAt);

    u16 Marker = *MarkerAt;
    Assert(Marker == JPEG_SOI);

    jpeg_app0* APP0 = 0;
    jpeg_dqt* DQT = 0;
    jpeg_sof0* SOF0 = 0;
    jpeg_dht* DHT = 0;
    jpeg_sos* SOS = 0;

    u8* DQTs[2] = {0};
    jpeg_dht* DHTs[2][2] = {0};

    while(Buffer.Elapsed)
    {
        MarkerAt = PopU16(&Buffer);
        Assert(MarkerAt);

        Marker = *MarkerAt;

        printf("Marker: 0x%04X", Marker);

        if(Marker == JPEG_EOI)
        {
            // NOTE: JPEG can also end prematurely, and without even a picture!
            // NOTE#2: Looks like Windows can add additional bytes at the end of file,
            // so don't assume that after EOI there will be 0 bytes elapsed!!!
            break;
        }

        u16* LengthAt = PopU16(&Buffer);
        Assert(LengthAt);

        u16 Length = ByteSwap16(*LengthAt);
        Length -= sizeof(Length);

        void* Payload = PopBytes(&Buffer, Length);
        Assert(Payload);

        printf(" Length: %d\n", Length);

        switch(Marker)
        {
#if 0
            case JPEG_APP0:
            {
                Assert(Length == sizeof(*APP0));
                APP0 = Payload;
            } break;
#endif

            case JPEG_DQT:
            {
                Assert(Length == sizeof(*DQT));
                DQT = Payload;

                Assert(DQT->Id < ArrayCount(DQTs));
                DQTs[DQT->Id] = DQT->Coefficients;
            } break;

            case JPEG_SOF0:
            {
                Assert(Length == sizeof(*SOF0));
                SOF0 = Payload;

                Assert(SOF0->NumComponents <= ArrayCount(SOF0->Components));
                SOF0->ImageHeight = ByteSwap16(SOF0->ImageHeight);
                SOF0->ImageWidth = ByteSwap16(SOF0->ImageWidth);
                Assert(SOF0->NumComponents == 3);
            } break;

            case JPEG_SOF2:
            {
                Assert(!"Progressive JPEG not supported");
            } break;

            case JPEG_DHT:
            {
                Assert(Length >= sizeof(*DHT));
                DHT = Payload;
                Length -= sizeof(*DHT);

                usz Total = 0;
                for(u8 Idx = 0;
                    Idx < ArrayCount(DHT->Counts);
                    Idx++)
                {
                    // TODO: SIMD
                    Total += DHT->Counts[Idx];
                }

                Assert(Length == Total);
                u8* HuffmanCodes = DHT->Values;

                Assert(DHT->TableClass < 2);
                Assert(DHT->TableId < 2);
                DHTs[DHT->TableId][DHT->TableClass] = DHT;
            } break;

            case JPEG_SOS:
            {
                Assert(Length == sizeof(*SOS));
                SOS = Payload;

                Assert(SOS->NumComponents <= ArrayCount(SOS->Components));
                Assert(SOS->NumComponents == 3);

                Bitmap->Width = SOF0->ImageWidth;
                Bitmap->Height = SOF0->ImageHeight;
                Bitmap->Pitch = Bitmap->Width * 4;
                Bitmap->Size = Bitmap->Pitch * Bitmap->Height;
                Bitmap->At = PlatformAlloc(Bitmap->Size);
                Assert(Bitmap->At);

                bit_stream BitStream;
                BitStream.At = Buffer.At;
                BitStream.Elapsed = Buffer.Elapsed;
                BitStream.Buf = 0;
                BitStream.Len = 0;

                Assert(SOF0->Components[0].HorizontalSubsamplingFactor <= 2);
                Assert(SOF0->Components[0].VerticalSubsamplingFactor <= 2);
                Assert(SOF0->Components[1].VerticalSubsamplingFactor == 1);
                Assert(SOF0->Components[1].HorizontalSubsamplingFactor == 1);
                Assert(SOF0->Components[2].VerticalSubsamplingFactor == 1);
                Assert(SOF0->Components[2].HorizontalSubsamplingFactor == 1);

                u8 SamplingX = SOF0->Components[0].HorizontalSubsamplingFactor;
                u8 SamplingY = SOF0->Components[0].VerticalSubsamplingFactor;

                Assert(PopImage(&BitStream, Bitmap, 
                                DQTs[0], DQTs[1],
                                DHTs[0][0]->Counts, DHTs[0][1]->Counts,
                                DHTs[1][0]->Counts, DHTs[1][1]->Counts,
                                SamplingX, SamplingY));

#if 1
                PlatformShowBitmap(Bitmap, "Decoded JPEG");
#endif

                usz BackCount = (BitStream.Len & 7) ? 0 : 1;
                BackCount += (BitStream.Len + 7) / 8;
                Buffer.At = BitStream.At - BackCount;
                Buffer.Elapsed = BitStream.Elapsed - BackCount;
            } break;
        }
    }

    return 1;
}
