#ifndef JPG_PRETTY_EXTEND_EDGES
#define JPG_PRETTY_EXTEND_EDGES 0
#endif

static const ALIGNED(16) f32 DCT8x8Table[8][8] = 
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

static const ALIGNED(16) f32 tDCT8x8Table[8][8] = 
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

static const ALIGNED(16) u8 ZigZagTable[8][8] =
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

    u32 Raw = (Number < 0) ? -Number : Number;

#if 0
    u8 Idx;
    u32 Top = 1;
    for(Idx = 1;
        Idx < 16;
        Idx++)
    {
        Top <<= 1;
        if(Raw < Top)
        {
            break;        
        }  
    }
#else
    unsigned long Idx;
    _BitScanReverse(&Idx, Raw);
    Idx++;
#endif

    *Value = (u16) ((Number < 0) ? ((1 << Idx) - Raw - 1) : Raw);

    return (u8) Idx;
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

static int EncodeSymbol(bit_stream* BitStream, const u32* DHT, u8 Value)
{
#if 0
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
#else
    u32 LenCode = DHT[Value];
    u16 Code = (LenCode & 0xFFFF);
    u8 Len = (u8) (LenCode >> 16);
    Assert(Len);
    return PushBits(BitStream, Code, Len);
#endif

    return 0;
}

static void MatrixMul8x8(const f32 Left[8][8], const f32 RightT[8][8], f32 Output[8][8])
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

static int EncodePixels(bit_stream* BitStream, f32 P[8][8], const f32* DQT, const u32* DHT_DC, const u32* DHT_AC, i16* DC)
{
    ALIGNED(16) f32 Tmp[8][8];
    ALIGNED(16) f32 D[8][8];
    ALIGNED(16) f32 M[64];
    ALIGNED(16) i16 K[64];

    MatrixMul8x8(DCT8x8Table, P, Tmp);
    MatrixMul8x8(Tmp, DCT8x8Table, D);

    f32* DD = (f32*) &D[0][0];

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
        __m128 XMM0 = _mm_load_ps(&DD[Idx+0]);
        __m128 XMM1 = _mm_load_ps(&DD[Idx+4]);
        __m128 XMM2 = _mm_load_ps(&DD[Idx+8]);
        __m128 XMM3 = _mm_load_ps(&DD[Idx+12]);

        __m128 XMM4 = _mm_load_ps(&DQT[Idx+0]);
        __m128 XMM5 = _mm_load_ps(&DQT[Idx+4]);
        __m128 XMM6 = _mm_load_ps(&DQT[Idx+8]);
        __m128 XMM7 = _mm_load_ps(&DQT[Idx+12]);

        XMM0 = _mm_div_ps(XMM0, XMM4);
        XMM1 = _mm_div_ps(XMM1, XMM5);
        XMM2 = _mm_div_ps(XMM2, XMM6);
        XMM3 = _mm_div_ps(XMM3, XMM7);

        _mm_store_ps(&M[Idx+0], XMM0);
        _mm_store_ps(&M[Idx+4], XMM1);
        _mm_store_ps(&M[Idx+8], XMM2);
        _mm_store_ps(&M[Idx+12], XMM3);
    }
#endif

#if 0
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        K[ZZ[Idx]] = M[Idx];
    }
#else
    K[ 0] = (i16) M[ 0];
    K[ 1] = (i16) M[ 1];
    K[ 5] = (i16) M[ 2];
    K[ 6] = (i16) M[ 3];
    K[14] = (i16) M[ 4];
    K[15] = (i16) M[ 5];
    K[27] = (i16) M[ 6];
    K[28] = (i16) M[ 7];
    K[ 2] = (i16) M[ 8];
    K[ 4] = (i16) M[ 9];
    K[ 7] = (i16) M[10];
    K[13] = (i16) M[11];
    K[16] = (i16) M[12];
    K[26] = (i16) M[13];
    K[29] = (i16) M[14];
    K[42] = (i16) M[15];
    K[ 3] = (i16) M[16];
    K[ 8] = (i16) M[17];
    K[12] = (i16) M[18];
    K[17] = (i16) M[19];
    K[25] = (i16) M[20];
    K[30] = (i16) M[21];
    K[41] = (i16) M[22];
    K[43] = (i16) M[23];
    K[ 9] = (i16) M[24];
    K[11] = (i16) M[25];
    K[18] = (i16) M[26];
    K[24] = (i16) M[27];
    K[31] = (i16) M[28];
    K[40] = (i16) M[29];
    K[44] = (i16) M[30];
    K[53] = (i16) M[31];
    K[10] = (i16) M[32];
    K[19] = (i16) M[33];
    K[23] = (i16) M[34];
    K[32] = (i16) M[35];
    K[39] = (i16) M[36];
    K[45] = (i16) M[37];
    K[52] = (i16) M[38];
    K[54] = (i16) M[39];
    K[20] = (i16) M[40];
    K[22] = (i16) M[41];
    K[33] = (i16) M[42];
    K[38] = (i16) M[43];
    K[46] = (i16) M[44];
    K[51] = (i16) M[45];
    K[55] = (i16) M[46];
    K[60] = (i16) M[47];
    K[21] = (i16) M[48];
    K[34] = (i16) M[49];
    K[37] = (i16) M[50];
    K[47] = (i16) M[51];
    K[50] = (i16) M[52];
    K[56] = (i16) M[53];
    K[59] = (i16) M[54];
    K[61] = (i16) M[55];
    K[35] = (i16) M[56];
    K[36] = (i16) M[57];
    K[48] = (i16) M[58];
    K[49] = (i16) M[59];
    K[57] = (i16) M[60];
    K[58] = (i16) M[61];
    K[62] = (i16) M[62];
    K[63] = (i16) M[63];
#endif

    i16 PrevDC = *DC;
    *DC = K[0];
    K[0] = K[0] - PrevDC;

    u16 DC_Value;
    u8 DC_Size = EncodeNumber(K[0], &DC_Value);
    Assert(EncodeSymbol(BitStream, DHT_DC, DC_Size));
    Assert(PushBits(BitStream, DC_Value, DC_Size));

    for(u8 Idx = 1;
        Idx < 64;
        Idx++)
    {
        u8 AC_RunLength = 0;
        while(!K[Idx])
        {
            // TODO: SIMD here?

            Idx++;
            AC_RunLength++;
            if(Idx == 64)
            {
                Assert(EncodeSymbol(BitStream, DHT_AC, 0x00));

                return 1;
            }
            else if(AC_RunLength == 16)
            {
                Assert(EncodeSymbol(BitStream, DHT_AC, 0xF0));

                AC_RunLength = 0;
            }
        }

        u16 AC_Value;
        u8 AC_Size = EncodeNumber(K[Idx], &AC_Value);
        u8 AC_RS = (AC_RunLength << 4) | AC_Size;
        Assert(EncodeSymbol(BitStream, DHT_AC, AC_RS));
        Assert(PushBits(BitStream, AC_Value, AC_Size));
    }

    return 1;
}

static int DecodePixels(bit_stream* BitStream, f32 P[8][8], const u8* DQT, const u8* DHT_DC, const u8* DHT_AC, i16* DC)
{
    i16 Z[64] = {0};

    u8 DC_Size;
    Assert(PopSymbol(BitStream, DHT_DC, &DC_Size));

    u16 DC_Value;
    Assert(PopBits(BitStream, &DC_Value, DC_Size));

    i16 Diff = DecodeNumber(DC_Size, DC_Value);
    Z[0] = *DC + Diff;
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

    ALIGNED(16) f32 C[8][8];
    ALIGNED(16) f32 Tmp[8][8];

    for(u8 Y = 0;
        Y < 8;
        Y++)
    {
        for(u8 X = 0;
            X < 8;
            X++)
        {
            u8 Index = ZigZagTable[Y][X];

            C[X][Y] = Z[Index];
        }
    }

    MatrixMul8x8(tDCT8x8Table, C, Tmp);
    MatrixMul8x8(Tmp, tDCT8x8Table, P);

    return 1;
}

static int EncodeImage(bit_stream* BitStream, bitmap* Bitmap, 
                       const f32* DQT_Y, const f32* DQT_Chroma, 
                       const u32* DHT_Y_DC, const u32* DHT_Y_AC, 
                       const u32* DHT_Chroma_DC, const u32* DHT_Chroma_AC,
                       u8 SX, u8 SY)
{
    Assert(SX <= 2);
    Assert(SY <= 2);

    u16 ZX = 8;
    u16 ZY = 8;

    u16 ZXSX = ZX*SX;
    u16 ZYSY = ZY*SY;

    u16 NumBlocksX = (u16)((Bitmap->Width + ZXSX - 1) / ZXSX);
    u16 NumBlocksY = (u16)((Bitmap->Height + ZYSY - 1) / ZYSY);

    i32 FinalWidth = NumBlocksX*ZXSX;
    i32 FinalHeight = NumBlocksY*ZYSY;

    Assert(Bitmap->Pitch >= FinalWidth*4);
    Assert(Bitmap->Size >= FinalHeight*Bitmap->Pitch);

#if JPG_PRETTY_EXTEND_EDGES
    i32 WidthDiff = FinalWidth - Bitmap->Width;
    i32 HeightDiff = FinalHeight - Bitmap->Height;

    if(WidthDiff)
    {
        u8* Row = Bitmap->At + (Bitmap->Width - 1) * 4;

        for(i32 Y = 0;
            Y < Bitmap->Height;
            Y++)
        {
            u32* Col = (u32*) Row;

            u32 Value = *(Col++);

            for(i32 X = 0;
                X < WidthDiff;
                X++)
            {
                // TODO: Non-caching store
                // TODO: SIMD store multiply

                *(Col++) = Value;
            }

            Row += Bitmap->Pitch;
        }
    }

    if(HeightDiff)
    {
        u8* Col = Bitmap->At + (Bitmap->Height - 1) * Bitmap->Pitch;

        for(i32 X = 0;
            X < FinalWidth;
            X++)
        {
            // TODO: SIMD load vector

            u32 Value = *(u32*)(Col);

            u8* Row = Col + Bitmap->Pitch;

            for(i32 Y = 0;
                Y < HeightDiff;
                Y++)
            {
                // TODO: Non-caching store
                // TODO: SIMD store vector

                *(u32*)(Row) = Value;

                Row += Bitmap->Pitch;
            }

            Col += 4;
        }
    }
#endif

#if 0
    bitmap Tmp;
    Tmp.Width = FinalWidth;
    Tmp.Height = FinalHeight;
    Tmp.Pitch = Bitmap->Pitch;
    Tmp.Size = Bitmap->Size;
    Tmp.At = Bitmap->At;
    PlatformShowBitmap(&Tmp, "Extended bitmap");
#endif

    i16 DC_Y = 0;
    i16 DC_Cb = 0;
    i16 DC_Cr = 0;

    u8* Row = Bitmap->At;

    for(u16 BlockY = 0; 
        BlockY < NumBlocksY; 
        BlockY++)
    {
        u8* Col = Row;

        ALIGNED(16) f32 Lum[2][2][8][8];
        ALIGNED(16) f32 Cb[2][2][8][8];
        ALIGNED(16) f32 Cr[2][2][8][8];

#if 1
        // BB GG RR AA
        __m128 XMM2 = _mm_set_ps(0.0f, 0.299f, 0.587f, 0.114f);
        __m128 XMM3 = _mm_set_ps(0.0f, -0.168736f, -0.331264f, 0.5f);
        __m128 XMM4 = _mm_set_ps(0.0f, 0.5f, -0.418688f, -0.081312f);
        __m128 XMM12 = _mm_setzero_ps();
#endif

        for(u16 BlockX = 0;
            BlockX < NumBlocksX;
            BlockX++)
        {
            u8* MidRow = Col;
            for(u8 KY = 0;
                KY < SY;
                KY++)
            {
                u8* MidCol = MidRow;

                for(u8 KX = 0;
                    KX < SX;
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

                        SubRow += Bitmap->Pitch;
                    }

                    MidCol += 8 * 4;
                }

                MidRow += 8 * Bitmap->Pitch;
            }

            u8 SXSY = SX*SY;
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
                        KY < SY;
                        KY++)
                    {
                        for(u8 KX = 0;
                            KX < SX;
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

            for(u8 KY = 0;
                KY < SY;
                KY++)
            {
                for(u8 KX = 0;
                    KX < SX;
                    KX++)
                {
                    Assert(EncodePixels(BitStream, Lum[KY][KX], DQT_Y, DHT_Y_DC, DHT_Y_AC, &DC_Y));
                }
            }

            Assert(EncodePixels(BitStream, Cb[0][0], DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cb));
            Assert(EncodePixels(BitStream, Cr[0][0], DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cr));

            Col += 8 * 4 * SX;
        }

        Row += Bitmap->Pitch * 8 * SY;
    }

    return 1;
}

static int DecodeImage(bit_stream* BitStream, bitmap* Bitmap, 
                       const u8* DQT_Y, const u8* DQT_Chroma, 
                       const u8* DHT_Y_DC, const u8* DHT_Y_AC, 
                       const u8* DHT_Chroma_DC, const u8* DHT_Chroma_AC,
                       u8 SX, u8 SY)
{
    Assert(SX <= 2);
    Assert(SY <= 2);

    u16 ZX = 8;
    u16 ZY = 8;

    u16 ZXSX = ZX*SX;
    u16 ZYSY = ZY*SY;

    u16 NumBlocksX = (u16)((Bitmap->Width + ZXSX - 1) / ZXSX);
    u16 NumBlocksY = (u16)((Bitmap->Height + ZYSY - 1) / ZYSY);

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
            ALIGNED(16) f32 Lum[2][2][8][8];
            ALIGNED(16) f32 Cb[8][8];
            ALIGNED(16) f32 Cr[8][8];

            for(u8 KY = 0;
                KY < SY;
                KY++)
            {
                for(u8 KX = 0;
                    KX < SX;
                    KX++)
                {
                    Assert(DecodePixels(BitStream, Lum[KY][KX], DQT_Y, DHT_Y_DC, DHT_Y_AC, &DC_Y));
                }
            }

            Assert(DecodePixels(BitStream, Cb, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cb));
            Assert(DecodePixels(BitStream, Cr, DQT_Chroma, DHT_Chroma_DC, DHT_Chroma_AC, &DC_Cr));

            u8* SubRow = Col;

            u32 SubY = SY << 3;
            u32 SubX = SX << 3;

            for(u32 Y = 0;
                Y < SubY;
                Y++)
            {
                u8* SubCol = SubRow;

                for(u32 X = 0;
                    X < SubX;
                    X++)
                {
                    // TODO: SIMD!!!

                    f32 Y_value = Lum[Y/8][X/8][Y&7][X&7] + 128;
                    f32 Cb_value = Cb[Y/SY][X/SX];
                    f32 Cr_value = Cr[Y/SY][X/SX];

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

            Col += 8 * 4 * SX;
        }

        Row += Bitmap->Pitch * 8 * SY;
    }

    return 1;
}

static const u8 JPEG_STD_Y_Q50_2D[64] = 
{
    // Coefficients (2D order)
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99,
};

static const u8 JPEG_STD_Y_Q50_ZZ[64] = 
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
};

static const u8 JPEG_STD_Chroma_Q50_2D[64] =
{
    // Coefficients (2D order)
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
};

static const u8 JPEG_STD_Chroma_Q50_ZZ[64] =
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

static void GenerateJumpTableForHT(const u8* DHT, u32* JumpTable)
{
    const u8* Counts = &DHT[0];
    const u8* At = &DHT[16];

    u16 Code = 0;
    for(u8 I = 0; I < 16; I++)
    {
        u8 BitLen = I+1;
        for(u16 J = 0; J < Counts[I]; J++)
        {
            JumpTable[*At] = (BitLen << 16) | Code;

            Code++;
            At++;
        }

        Code <<= 1;
    }
}

static void* EncodeJPEG(bitmap* Bitmap, usz* Size, u8 Quality)
{
    usz BufferSize = Bitmap->Size*16; // TODO: Dynamical approach, eeeh?
    u8* BufferStart = PlatformAlloc(BufferSize);
    if(!BufferStart)
    {
        return 0;
    }

    buffer Buffer;
    Buffer.Elapsed = BufferSize;
    Buffer.At = BufferStart;
    if(!EncodeJPEGintoBuffer(&Buffer, Bitmap, Quality))
    {
        PlatformFree(Buffer.At);
        return 0;
    }

    *Size = Buffer.At - BufferStart;

    return BufferStart;
}

static int EncodeJPEGintoBuffer(buffer* Buffer, bitmap* Bitmap, u8 Quality)
{
    u8 SX = 2;
    u8 SY = 2;

    Quality = CLAMP(Quality, 1, 100);

    float Ratio = (Quality <= 50) ? (50.F / Quality) : ((100.F - Quality) / 50);

    Assert(Bitmap->Width < 65536);
    Assert(Bitmap->Height < 65536);

    Assert(PushU16(Buffer, JPEG_SOI));

    jpeg_app0* APP0;

    Assert(APP0 = PushSegment(Buffer, JPEG_APP0, jpeg_app0));
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

    u8* ZZ = (u8*) &ZigZagTable[0][0];

    jpeg_dqt* DQT[2];

    Assert(DQT[0] = PushSegmentCount(Buffer, JPEG_DQT, sizeof(jpeg_dqt)));
    DQT[0]->Id = 0;
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        float Coeff = JPEG_STD_Y_Q50_ZZ[Idx] * Ratio;
        DQT[0]->Coefficients[Idx] = (u8) CLAMP(Coeff, 1, 255);
    }

    Assert(DQT[1] = PushSegmentCount(Buffer, JPEG_DQT, sizeof(jpeg_dqt)));
    DQT[1]->Id = 1;
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        float Coeff = JPEG_STD_Chroma_Q50_ZZ[Idx] * Ratio;
        DQT[1]->Coefficients[Idx] = (u8) CLAMP(Coeff, 1, 255);
    }

    ALIGNED(16) f32 DQT_Y[64];
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        DQT_Y[Idx] = DQT[0]->Coefficients[ZZ[Idx]];
    }

    ALIGNED(16) f32 DQT_Chroma[64];
    for(u8 Idx = 0;
        Idx < 64;
        Idx++)
    {
        DQT_Chroma[Idx] = DQT[1]->Coefficients[ZZ[Idx]];
    }

    jpeg_sof0* SOF0;

    Assert(SOF0 = PushSegment(Buffer, JPEG_SOF0, jpeg_sof0));
    SOF0->BitsPerSample = 8;
    SOF0->ImageHeight = ByteSwap16(Bitmap->Height);
    SOF0->ImageWidth = ByteSwap16(Bitmap->Width);
    SOF0->NumComponents = 3;
    SOF0->Components[0].ComponentId = 1;
    SOF0->Components[0].VerticalSubsamplingFactor = SY;
    SOF0->Components[0].HorizontalSubsamplingFactor = SX;
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

    Assert(DHT[0][0] = PushSegmentCount(Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT00)));
    memcpy(DHT[0][0], JPEG_STD_DHT00, sizeof(JPEG_STD_DHT00));

    Assert(DHT[0][1] = PushSegmentCount(Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT01)));
    memcpy(DHT[0][1], JPEG_STD_DHT01, sizeof(JPEG_STD_DHT01));

    Assert(DHT[1][0] = PushSegmentCount(Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT10)));
    memcpy(DHT[1][0], JPEG_STD_DHT10, sizeof(JPEG_STD_DHT10));

    Assert(DHT[1][1] = PushSegmentCount(Buffer, JPEG_DHT, sizeof(JPEG_STD_DHT11)));
    memcpy(DHT[1][1], JPEG_STD_DHT11, sizeof(JPEG_STD_DHT11));

    u32 DHT00[256] = {0};
    u32 DHT01[256] = {0};
    u32 DHT10[256] = {0};
    u32 DHT11[256] = {0};

    GenerateJumpTableForHT(&JPEG_STD_DHT00[1], DHT00);
    GenerateJumpTableForHT(&JPEG_STD_DHT01[1], DHT01);
    GenerateJumpTableForHT(&JPEG_STD_DHT10[1], DHT10);
    GenerateJumpTableForHT(&JPEG_STD_DHT11[1], DHT11);

    jpeg_sos* SOS;

    Assert(SOS = PushSegment(Buffer, JPEG_SOS, jpeg_sos));
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
    BitStream.At = Buffer->At;
    BitStream.Elapsed = Buffer->Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    Assert(EncodeImage(&BitStream, Bitmap, 
                       DQT_Y, DQT_Chroma,
                       DHT00, DHT01,
                       DHT10, DHT11,
                       SX, SY));

    u16 Zero = 0;
    u8 NextFullByte = (BitStream.Len + 7) / 8;
    u8 ZeroCount = NextFullByte * 8 - BitStream.Len;
    Assert(PushBits(&BitStream, Zero, ZeroCount));
    Assert(Flush(&BitStream));

#if 0
    bit_stream Orig = BitStream;

    BitStream.At = Buffer->At;
    BitStream.Elapsed = Buffer->Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    memset(Bitmap->At, 0xF0, Bitmap->Size);

    Assert(DecodeImage(&BitStream, Bitmap, 
                    DQT[0]->Coefficients, DQT[1]->Coefficients,
                    &JPEG_STD_DHT00[1], &JPEG_STD_DHT01[1],
                    &JPEG_STD_DHT10[1], &JPEG_STD_DHT11[1],
                    SX, SY));

    BitStream = Orig;

    PlatformShowBitmap(Bitmap, "Bitmap");
#endif

    Buffer->At = BitStream.At;
    Buffer->Elapsed = BitStream.Elapsed;

    Assert(PushU16(Buffer, JPEG_EOI));

    return 1;
}

static int DecodeJPEG(void* Data, usz Size, bitmap* Bitmap)
{
    buffer Buffer;
    Buffer.At = Data;
    Buffer.Elapsed = Size;
    return DecodeJPEGfromBuffer(&Buffer, Bitmap);
}

static int DecodeJPEGfromBuffer(buffer* Buffer, bitmap* Bitmap)
{
    u16* MarkerAt = PopU16(Buffer);
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

    while(Buffer->Elapsed)
    {
        MarkerAt = PopU16(Buffer);
        Assert(MarkerAt);

        Marker = *MarkerAt;

        // printf("Marker: 0x%04X", Marker);

        if(Marker == JPEG_EOI)
        {
            // NOTE: JPEG can also end prematurely, and without even a picture!
            // NOTE#2: Looks like Windows can add additional bytes at the end of file,
            // so don't assume that after EOI there will be 0 bytes elapsed!!!
            break;
        }

        u16* LengthAt = PopU16(Buffer);
        Assert(LengthAt);

        u16 Length = ByteSwap16(*LengthAt);
        Length -= sizeof(Length);

        void* Payload = PopBytes(Buffer, Length);
        Assert(Payload);

        // printf(" Length: %d\n", Length);

        switch(Marker)
        {
#if 1
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

                usz Total = Sum16xU8(DHT->Counts);
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

                Assert(SOF0->Components[0].HorizontalSubsamplingFactor <= 2);
                Assert(SOF0->Components[0].VerticalSubsamplingFactor <= 2);
                Assert(SOF0->Components[1].VerticalSubsamplingFactor == 1);
                Assert(SOF0->Components[1].HorizontalSubsamplingFactor == 1);
                Assert(SOF0->Components[2].VerticalSubsamplingFactor == 1);
                Assert(SOF0->Components[2].HorizontalSubsamplingFactor == 1);

                u8 SX = SOF0->Components[0].HorizontalSubsamplingFactor;
                u8 SY = SOF0->Components[0].VerticalSubsamplingFactor;
                
                u8 ZX = 8;
                u8 ZY = 8;

                u16 ZXSX = ZX*SX;
                u16 ZYSY = ZY*SY;

                Bitmap->Width = ((SOF0->ImageWidth + ZXSX - 1) / ZXSX) * ZXSX;
                Bitmap->Height = ((SOF0->ImageHeight + ZYSY - 1) / ZYSY) * ZYSY;
                Bitmap->Pitch = Bitmap->Width * 4;
                Bitmap->Size = Bitmap->Pitch * Bitmap->Height;
                Bitmap->At = PlatformAlloc(Bitmap->Size);
                Assert(Bitmap->At);

                bit_stream BitStream;
                BitStream.At = Buffer->At;
                BitStream.Elapsed = Buffer->Elapsed;
                BitStream.Buf = 0;
                BitStream.Len = 0;

                TIMING_TICK("Decoding of image");

                Assert(DecodeImage(&BitStream, Bitmap, 
                                   DQTs[0], DQTs[1],
                                   DHTs[0][0]->Counts, DHTs[0][1]->Counts,
                                   DHTs[1][0]->Counts, DHTs[1][1]->Counts,
                                   SX, SY));

                Bitmap->Width = SOF0->ImageWidth;
                Bitmap->Height = SOF0->ImageHeight;

#if 0
                PlatformShowBitmap(Bitmap, "Decoded JPEG");
#endif

                Assert(BitStream.Elapsed >= 2);
                Assert(*(u16*)(BitStream.At) == JPEG_EOI);
                return 1;
            } break;
        }
    }

    return 1;
}
