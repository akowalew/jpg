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

                u16 Code = 0;
                At = HuffmanCodes;
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

            } break;
        }
    }

    return 1;
}

static const u8 JPEG_STD_DQT_Y[1 + 64] = 
{
    // Id
    0,

    // Coefficients (in zig-zag order)
     3,   2,   2,   3,   2,   2,   3,   3,
     3,   3,   4,   3,   3,   4,   5,   8,
     5,   5,   4,   4,   5,  10,   7,   7,
     6,   8,  12,  10,  12,  12,  11,  10,
    11,  11,  13,  14,  18,  16,  13,  14,
    17,  14,  11,  11,  16,  22,  16,  17,
    19,  20,  21,  21,  21,  12,  15,  23,
    24,  22,  20,  24,  18,  20,  21,  20,


    // Coefficients (2D order)
    //  3,   2,   2,   3,   5,   8,  10,  12,
    //  2,   2,   3,   4,   5,  12,  12,  11,
    //  3,   3,   3,   5,   8,  11,  14,  11,
    //  3,   3,   4,   6,  10,  17,  16,  12,
    //  4,   4,   7,  11,  14,  22,  21,  15,
    //  5,   7,  11,  13,  16,  21,  23,  18,
    // 10,  13,  16,  17,  21,  24,  24,  20,
    // 14,  18,  19,  20,  22,  20,  21,  20,
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

     //  3,   4,   5,   9,  20,  20,  20,  20,
     //  4,   4,   5,  13,  20,  20,  20,  20,
     //  5,   5,  11,  20,  20,  20,  20,  20,
     //  9,  13,  20,  20,  20,  20,  20,  20,
     // 20,  20,  20,  20,  20,  20,  20,  20,
     // 20,  20,  20,  20,  20,  20,  20,  20,
     // 20,  20,  20,  20,  20,  20,  20,  20,
     // 20,  20,  20,  20,  20,  20,  20,  20,
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

    // TODO: Setting of quality factor!!!

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

    u8* ScanStart = Buffer.At;

    bit_stream BitStream;
    BitStream.At = Buffer.At;
    BitStream.Elapsed = Buffer.Elapsed;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    Assert(PushImage(&BitStream, Bitmap, 
                     &JPEG_STD_DQT_Y[1], &JPEG_STD_DQT_Chroma[1],
                     &JPEG_STD_DHT00[1], &JPEG_STD_DHT01[1],
                     &JPEG_STD_DHT10[1], &JPEG_STD_DHT11[1]));

    u16 Zero = 0;
    u8 NextFullByte = (BitStream.Len + 7) / 8;
    u8 ZeroCount = NextFullByte * 8 - BitStream.Len;
    Assert(PushBits(&BitStream, Zero, ZeroCount));
    Assert(Flush(&BitStream));

    Buffer.At = BitStream.At;
    Buffer.Elapsed = BitStream.Elapsed;

    Assert(PushU16(&Buffer, JPEG_EOI));

    usz ScanSize = Buffer.At - ScanStart;

#if 1
    BitStream.At = ScanStart;
    BitStream.Elapsed = ScanSize;
    BitStream.Buf = 0;
    BitStream.Len = 0;

    memset(Bitmap->At, 0xF0, Bitmap->Size);

    Assert(PopImage(&BitStream, Bitmap, 
                    &JPEG_STD_DQT_Y[1], &JPEG_STD_DQT_Chroma[1],
                    &JPEG_STD_DHT00[1], &JPEG_STD_DHT01[1],
                    &JPEG_STD_DHT10[1], &JPEG_STD_DHT11[1]));  

    PlatformShowBitmap(Bitmap, "Bitmap");
#endif

    *Size = Buffer.At - BufferStart;

    return BufferStart;
}