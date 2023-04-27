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
                    printf(" %2d: %d codes\n", Idx+1, DHT->Counts[Idx]);
                }

                Assert(Length == Total);
                u8* HuffmanCodes = DHT->Values;

                printf("Codes:\n");
                u8* At = HuffmanCodes;
                for(u8 I = 0;
                    I < ArrayCount(DHT->Counts);
                    I++)
                {
                    u8 Count = DHT->Counts[I];
                    printf(" %2d: ", I+1);
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