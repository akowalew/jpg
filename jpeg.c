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
                        for(int K = BitLen-1;
                            K >= 0;
                            K--)
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
                BitStream.Bits = 0;
                BitStream.Pos = 32;

                u16 NumBlocksX = (SOF0->ImageWidth + 7) / 8;
                u16 NumBlocksY = (SOF0->ImageHeight + 7) / 8;

                i8 DCcoefficients[3] = {0};

                i8 C[64] = {0};

                u8 Value;

                for(u16 BlockY = 0;
                    BlockY < NumBlocksY;
                    BlockY++)
                {
                    for(u16 BlockX = 0;
                        BlockX < NumBlocksX;
                        BlockX++)
                    {
                        u8 DCSize;

                        Assert(DecodeValue(&BitStream, HuffmanTables[0][0], &DCSize));

                        // DCSize++;

                        printf("Num bits for DC value: %d\n", DCSize);

                        Assert(Refill(&BitStream));

                        u32 DCValue = (BitStream.Bits >> BitStream.Pos) & ((1 << DCSize) - 1);

                        printf("DC value: %d\n", DCValue);

                        BitStream.Pos += DCSize;

                        DCcoefficients[0] += DCValue;

                        C[0] = DCcoefficients[0] * QuantizationTables[0][0];

                        int Count = 1;
                        while(Count < 64)
                        {
                            u8 S1;

                            Assert(DecodeValue(&BitStream, HuffmanTables[0][1], &S1));

                            if(S1 == 0)
                            {
                                printf("End of block marker!\n");
                                break;
                            }

                            if(S1 > 15)
                            {
                                printf("Skipping %d zeros!\n", S1 >> 4);
                                Count += S1 >> 4;
                                S1 &= 0x0F;
                            }

                            if(Count < 64)
                            {
                                u8 ACsize = S1 + 1;

                                printf("Num bits for AC value: %d\n", ACsize);

                                Assert(Refill(&BitStream));

                                u8 ACvalue = (BitStream.Bits >> BitStream.Pos) & ((1 << ACsize) - 1);

                                BitStream.Pos += ACsize;

                                printf("AC value: %d\n", ACvalue);

                                C[Count] = ACvalue * QuantizationTables[0][Count];
                                Count++;
                            }
                            else
                            {
                                printf("Reached end of MCU\n");
                            }
                        }

                        printf("After Quantization:\n");
                        for(int I = 0; I < 64; I++)
                        {
                            printf("%d, ", C[I]);
                        }
                        putchar('\n');

                        fflush(stdout);
                        Assert(!"Not implemented");
                    }
                }

                return 0;
            } break;
        }
    }

    return 1;
}