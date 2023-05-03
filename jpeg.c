static int ParseJPEG(void* Data, usz Size, bitmap* Bitmap)
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

    u8* QuantizationTables[2] = {0};
    jpeg_dht* HuffmanTables[2][2] = {0};

    while(Buffer.Elapsed)
    {
        MarkerAt = PopU16(&Buffer);
        Assert(MarkerAt);

        Marker = *MarkerAt;

        printf("[[Marker: 0x%04X", Marker);

        if(Marker == JPEG_EOI)
        {
            // NOTE: JPEG can also end prematurely, and without even a picture!
            break;
        }

        u16* LengthAt = PopU16(&Buffer);
        Assert(LengthAt);

        u16 Length = ByteSwap16(*LengthAt);
        Length -= sizeof(Length);

        void* Payload = PopBytes(&Buffer, Length);
        Assert(Payload);

        printf(" Length: %d]]\n", Length);

        switch(Marker)
        {
            case JPEG_APP0:
            {
                Assert(Length == sizeof(*APP0));
                APP0 = Payload;

                printf("APP0 version %d.%d\n", APP0->VersionMajor, APP0->VersionMinor);
            } break;

            case JPEG_APP1:
            {
                printf("APP1\n");
            } break;

            case JPEG_DQT:
            {
                Assert(Length == sizeof(*DQT));
                DQT = Payload;

                Assert(DQT->Id < ArrayCount(QuantizationTables));
                QuantizationTables[DQT->Id] = DQT->Coefficients;

                printf("Quantization table #%d\n", DQT->Id);
                for(int I = 0; I < 8; I++)
                {
                    for(int J = 0; J < 8; J++)
                    {
                        printf("%3d, ", DQT->Coefficients[I*8+J]);
                    }

                    putchar('\n');
                }
            } break;

            case JPEG_SOF0:
            {
                Assert(Length == sizeof(*SOF0));
                SOF0 = Payload;

                Assert(SOF0->NumComponents <= ArrayCount(SOF0->Components));

                SOF0->ImageHeight = ByteSwap16(SOF0->ImageHeight);
                SOF0->ImageWidth = ByteSwap16(SOF0->ImageWidth);

                printf("Start of frame %dx%d %dbpp\n", SOF0->ImageWidth, SOF0->ImageHeight, SOF0->BitsPerSample);
                for(int I = 0; I < SOF0->NumComponents; I++)
                {
                    printf(" Component#%d: sampling %d:%d table %d\n", I, 
                        SOF0->Components[I].VerticalSubsamplingFactor, 
                        SOF0->Components[I].HorizontalSubsamplingFactor,
                        SOF0->Components[I].QuantizationTableId);
                }

                Assert(SOF0->NumComponents == 3);
            } break;

            case JPEG_DHT:
            {
                Assert(Length >= sizeof(*DHT));
                DHT = Payload;
                Length -= sizeof(*DHT);

                printf("Huffman table header %d class %d (%s) id %d\n", DHT->Header, DHT->TableClass, DHT->TableClass ? "AC" : "DC", DHT->TableId);
                printf("Counts:\n");
                usz Total = 0;
                for(u8 Idx = 0;
                    Idx < ArrayCount(DHT->Counts);
                    Idx++)
                {
                    Total += DHT->Counts[Idx];
                    // printf(" %2d: %d codes\n", Idx+1, DHT->Counts[Idx]);
                    printf(" %d, ", DHT->Counts[Idx]);
                }
                putchar('\n');

                Assert(Length == Total);
                u8* HuffmanCodes = DHT->Values;

                printf("Values:\n");
                u8* At = HuffmanCodes;
                for(u8 I = 0;
                    I < ArrayCount(DHT->Counts);
                    I++)
                {
                    u8 Count = DHT->Counts[I];
                    // printf(" %2d: ", I+1);
                    for(u8 J = 0;
                        J < Count;
                        J++)
                    {
                        printf("%d, ", *(At++));
                    }
                    putchar('\n');
                }

                Assert(DHT->TableClass < 2);
                Assert(DHT->TableId < 2);
                HuffmanTables[DHT->TableId][DHT->TableClass] = DHT;

                At = HuffmanCodes;
                u16 Code = 0;
                printf("Tree:\n");
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

                        Code++;
                        At++;
                    }

                    Code <<= 1;
                }
            } break;

            case JPEG_SOS:
            {
                Assert(Length == sizeof(*SOS));
                SOS = Payload;

                Assert(SOS->NumComponents <= ArrayCount(SOS->Components));
                printf("Start of scan %d components\n", SOS->NumComponents);
                for(u8 I = 0;
                    I < SOS->NumComponents;
                    I++)
                {
                    printf(" Component#%d: id %d index %d dc table %d ac table %d\n", I,
                        SOS->Components[I].ComponentId,
                        SOS->Components[I].Index,
                        SOS->Components[I].IndexDC,
                        SOS->Components[I].IndexAC);
                }

                Assert(SOS->NumComponents == 3);

                bit_stream BitStream;
                BitStream.At = Buffer.At;
                BitStream.Elapsed = Buffer.Elapsed;
                BitStream.Buf = 0;
                BitStream.Len = 0;

                u16 NumBlocksX = (SOF0->ImageWidth + 7) / 8;
                u16 NumBlocksY = (SOF0->ImageHeight + 7) / 8;

                i16 DC_Y = 0;
                i16 DC_Cb = 0;
                i16 DC_Cr = 0;

                Bitmap->Width = SOF0->ImageWidth;
                Bitmap->Height = SOF0->ImageHeight;
                Bitmap->Pitch = Bitmap->Width * 4;
                Bitmap->Size = Bitmap->Pitch * Bitmap->Height;
                Bitmap->At = PlatformAlloc(Bitmap->Size);
                Assert(Bitmap->At);
                GlobalBitmap = Bitmap;

                for(u16 BlockY = 0;
                    BlockY < NumBlocksY;
                    BlockY++)
                {
                    for(u16 BlockX = 0;
                        BlockX < NumBlocksX;
                        BlockX++)
                    {
                        i16 ZZ_Y[64] = {0};
                        i16 ZZ_Cb[64] = {0};
                        i16 ZZ_Cr[64] = {0};

                        DecodeMCU(&BitStream, ZZ_Y, HuffmanTables[0][0], HuffmanTables[0][1], &DC_Y, 64);
                        // DecodeMCU(&BitStream, ZZ_Y, HuffmanTables[0], &DC_Y, 64);
                        // DecodeMCU(&BitStream, ZZ_Y, HuffmanTables[0], &DC_Y, 64);
                        // DecodeMCU(&BitStream, ZZ_Y, HuffmanTables[0], &DC_Y, 64);
                        DecodeMCU(&BitStream, ZZ_Cb, HuffmanTables[1][0], HuffmanTables[1][1], &DC_Cb, 64);
                        DecodeMCU(&BitStream, ZZ_Cr, HuffmanTables[1][0], HuffmanTables[1][1], &DC_Cr, 64);

                        i16 C_Y[8][8];
                        i16 C_Cb[8][8];
                        i16 C_Cr[8][8];

                        printf("Y:\n");
                        DeZigZag8x8(ZZ_Y, C_Y);
                        printf("Cb:\n");
                        DeZigZag8x8(ZZ_Cb, C_Cb);
                        printf("Cr:\n");
                        DeZigZag8x8(ZZ_Cr, C_Cr);

                        printf("Y:\n");
                        ScalarMul8x8(C_Y, QuantizationTables[0]);
                        printf("Cb:\n");
                        ScalarMul8x8(C_Cr, QuantizationTables[1]);
                        printf("Cr:\n");
                        ScalarMul8x8(C_Cr, QuantizationTables[1]);

                        printf("Y:\n");
                        IDCT8x8(C_Y);
                        printf("Cb:\n");
                        IDCT8x8(C_Cb);
                        printf("Cr:\n");
                        IDCT8x8(C_Cr);

                        printf("RGB:\n");
                        u8* Row = Bitmap->At + (BlockY*8)*Bitmap->Pitch + (BlockX*8)*4;
                        for(u8 Y = 0; Y < 8; Y++)
                        {
                            u8* Col = Row;

                            for(u8 X = 0; X < 8; X++)
                            {
                                f32 R = (C_Y[Y][X] + 1.4020F*C_Cr[Y][X] + 128);
                                f32 G = (C_Y[Y][X] - 0.3441F*C_Cb[Y][X] - 0.71414F*C_Cr[Y][X] + 128);
                                f32 B = (C_Y[Y][X] + 1.7720F*C_Cb[Y][X] + 128);

                                Col[0] = ClampU8(B);
                                Col[1] = ClampU8(G);
                                Col[2] = ClampU8(R);
                                Col[3] = 0;

                                Col += 4;

                                printf("(%4d,%4d,%4d), ", (int)R, (int)G, (int)B);
                                // printf("(%4d,%4d,%4d), ", RGB[Y][X][0], RGB[Y][X][1], RGB[Y][X][2]);
                            }
                            putchar('\n');

                            Row += Bitmap->Pitch;
                        }
                    }
                }

                Assert(!"Not implemented");
            } break;
        }
    }

    return 1;
}

static const u8 JPEG_STD_DQT_Y[1 + 64] = 
{
    // Id
    0,

    // Coefficients
     3,   2,   2,   3,   2,   2,   3,   3,
     3,   3,   4,   3,   3,   4,   5,   8,
     5,   5,   4,   4,   5,  10,   7,   7,
     6,   8,  12,  10,  12,  12,  11,  10,
    11,  11,  13,  14,  18,  16,  13,  14,
    17,  14,  11,  11,  16,  22,  16,  17,
    19,  20,  21,  21,  21,  12,  15,  23,
    24,  22,  20,  24,  18,  20,  21,  20,
};

static const u8 JPEG_STD_DQT_Chroma[1 + 64] =
{
    // Id,
    1,

    // Coefficients
      3,   4,   4,   5,   4,   5,   9,   5,
      5,   9,  20,  13,  11,  13,  20,  20,
     20,  20,  20,  20,  20,  20,  20,  20,
     20,  20,  20,  20,  20,  20,  20,  20,
     20,  20,  20,  20,  20,  20,  20,  20,
     20,  20,  20,  20,  20,  20,  20,  20,
     20,  20,  20,  20,  20,  20,  20,  20,
     20,  20,  20,  20,  20,  20,  20,  20,
};

static const u8 JPEG_STD_DHT00[] =
{
    // Id
    0x00, 

    // Counts:
    /*  1: */ 0,
    /*  2: */ 2,
    /*  3: */ 2,
    /*  4: */ 3,
    /*  5: */ 1,
    /*  6: */ 1,
    /*  7: */ 1,
    /*  8: */ 0,
    /*  9: */ 0,
    /* 10: */ 0,
    /* 11: */ 0,
    /* 12: */ 0,
    /* 13: */ 0,
    /* 14: */ 0,
    /* 15: */ 0,
    /* 16: */ 0,

    // Codes:
    /*  1: */
    /*  2: */ 5, 6,
    /*  3: */ 3, 4,
    /*  4: */ 2, 7, 8,
    /*  5: */ 1,
    /*  6: */ 0,
    /*  7: */ 9,
    /*  8: */
    /*  9: */
    /* 10: */
    /* 11: */
    /* 12: */
    /* 13: */
    /* 14: */
    /* 15: */
    /* 16: */
};

static const u8 JPEG_STD_DHT01[] = 
{
    // Id
    0x10,

    // Counts:
    /*  1: */ 0,
    /*  2: */ 2,
    /*  3: */ 1,
    /*  4: */ 3,
    /*  5: */ 2,
    /*  6: */ 4,
    /*  7: */ 5,
    /*  8: */ 2,
    /*  9: */ 4,
    /* 10: */ 4,
    /* 11: */ 3,
    /* 12: */ 4,
    /* 13: */ 8,
    /* 14: */ 5,
    /* 15: */ 5,
    /* 16: */ 1,

    // Values
    /*  1: */
    /*  2: */ 1, 2,
    /*  3: */ 3,
    /*  4: */ 0, 4, 17,
    /*  5: */ 5, 33,
    /*  6: */ 6, 18, 49, 65,
    /*  7: */ 7, 19, 34, 81, 97,
    /*  8: */ 20, 113,
    /*  9: */ 8, 50, 129, 145,
    /* 10: */ 21, 35, 66, 161,
    /* 11: */ 82, 177, 193,
    /* 12: */ 51, 98, 209, 225,
    /* 13: */ 9, 22, 23, 36, 114, 146, 240, 241,
    /* 14: */ 37, 52, 67, 130, 178,
    /* 15: */ 24, 39, 68, 83, 162,
    /* 16: */ 115,
};

static const u8 JPEG_STD_DHT10[] = 
{
    // Id
    0x01,

    // Counts
    /*  1: */ 0,
    /*  2: */ 2,
    /*  3: */ 3,
    /*  4: */ 1,
    /*  5: */ 1,
    /*  6: */ 1,
    /*  7: */ 0,
    /*  8: */ 0,
    /*  9: */ 0,
    /* 10: */ 0,
    /* 11: */ 0,
    /* 12: */ 0,
    /* 13: */ 0,
    /* 14: */ 0,
    /* 15: */ 0,
    /* 16: */ 0,

    // Values
    /*  1: */
    /*  2: */ 1, 2,
    /*  3: */ 0, 3, 4,
    /*  4: */ 5,
    /*  5: */ 6,
    /*  6: */ 7,
    /*  7: */
    /*  8: */
    /*  9: */
    /* 10: */
    /* 11: */
    /* 12: */
    /* 13: */
    /* 14: */
    /* 15: */
    /* 16: */
};

static const u8 JPEG_STD_DHT11[] =
{
    // Counts:
    /*  1: */ 0,
    /*  2: */ 2,
    /*  3: */ 2,
    /*  4: */ 2,
    /*  5: */ 2,
    /*  6: */ 2,
    /*  7: */ 1,
    /*  8: */ 3,
    /*  9: */ 3,
    /* 10: */ 1,
    /* 11: */ 7,
    /* 12: */ 4,
    /* 13: */ 2,
    /* 14: */ 3,
    /* 15: */ 0,
    /* 16: */ 0,

    // Codes:
    /*  1: */
    /*  2: */ 0, 1,
    /*  3: */ 2, 17,
    /*  4: */ 3, 33,
    /*  5: */ 18, 49,
    /*  6: */ 4, 65,
    /*  7: */ 81,
    /*  8: */ 19, 34, 97,
    /*  9: */ 5, 50, 113,
    /* 10: */ 145,
    /* 11: */ 20, 35, 66, 129, 161, 177, 209,
    /* 12: */ 6, 21, 193, 240,
    /* 13: */ 36, 241,
    /* 14: */ 51, 82, 162,
    /* 15: */
    /* 16: */
};

static void* ExportJPEG(bitmap* Bitmap, usz* Size)
{
    Assert(Bitmap->Width < 65536);
    Assert(Bitmap->Height < 65536);

    buffer Buffer;
    Buffer.Elapsed = Bitmap->Size*16; // TODO: Dynamical approach, eeeh?
    Buffer.At = PlatformAlloc(Buffer.Elapsed);
    if(!Buffer.At)
    {
        return 0;
    }

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

    Assert(DQT[0] = PushSegmentCount(&Buffer, JPEG_DQT, sizeof(JPEG_STD_DQT_Y)));
    memcpy(DQT[0], JPEG_STD_DQT_Y, sizeof(JPEG_STD_DQT_Y));

    Assert(DQT[1] = PushSegmentCount(&Buffer, JPEG_DQT, sizeof(JPEG_STD_DQT_Chroma)));
    memcpy(DQT[1], JPEG_STD_DQT_Chroma, sizeof(JPEG_STD_DQT_Chroma));

    jpeg_sof0* SOF0;

    Assert(SOF0 = PushSegment(&Buffer, JPEG_SOF0, jpeg_sof0));
    SOF0->BitsPerSample = 8;
    SOF0->ImageHeight = ByteSwap16(Bitmap->Height);
    SOF0->ImageWidth = ByteSwap16(Bitmap->Width);
    SOF0->NumComponents = 3;
    SOF0->Components[0].ComponentId = 1;
    SOF0->Components[0].VerticalSubsamplingFactor = 1;
    SOF0->Components[0].HorizontalSubsamplingFactor = 1;
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

    u16 NumBlocksX = (u16)(Bitmap->Width + 7) / 8;
    u16 NumBlocksY = (u16)(Bitmap->Height + 7) / 8;

    bit_stream BitStream;
    BitStream.At = Buffer.At;
    BitStream.Elapsed = Buffer.Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

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

            Assert(PushPixels(&BitStream, Y, &JPEG_STD_DQT_Y[1], JPEG_STD_DHT00, JPEG_STD_DHT01, &DC_Y));
            Assert(PushPixels(&BitStream, Cb, &JPEG_STD_DQT_Chroma[1], JPEG_STD_DHT10, JPEG_STD_DHT11, &DC_Cb));
            Assert(PushPixels(&BitStream, Cr, &JPEG_STD_DQT_Chroma[1], JPEG_STD_DHT10, JPEG_STD_DHT11, &DC_Cr));

            Col += 8 * 4;
        }

        Row += Bitmap->Pitch * 8;
    }

#if 1
    BitStream.At = Buffer.At;
    BitStream.Elapsed = Buffer.Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    DC_Y = 0;
    DC_Cb = 0;
    DC_Cr = 0;

    Row = Bitmap->At;
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

            Assert(PopPixels(&BitStream, Y, &JPEG_STD_DQT_Y[1], JPEG_STD_DHT00, JPEG_STD_DHT01, &DC_Y));
            Assert(PopPixels(&BitStream, Cb, &JPEG_STD_DQT_Chroma[1], JPEG_STD_DHT10, JPEG_STD_DHT11, &DC_Cb));
            Assert(PopPixels(&BitStream, Cr, &JPEG_STD_DQT_Chroma[1], JPEG_STD_DHT10, JPEG_STD_DHT11, &DC_Cr));

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

    PlatformShowBitmap(Bitmap, "Bitmap");
#endif

    Assert(PushU16(&Buffer, JPEG_EOI));

    return 0;
}