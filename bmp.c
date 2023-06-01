int ParseBMP(u8* Data, usz Size, bitmap* Dst)
{
    Assert(Size >= sizeof(bmp_headers));
    bmp_headers* BMP = (bmp_headers*) Data;
    Size -= sizeof(*BMP);

    bmp_file_header* FileHeader = &BMP->FileHeader;
    Assert(FileHeader->Signature[0] == 'B');
    Assert(FileHeader->Signature[1] == 'M');

    bmp_info_header* InfoHeader = &BMP->InfoHeader;
    Assert(InfoHeader->Planes == 1);
    Assert(InfoHeader->Width <= 0x7FFFFFFF);
    Assert(InfoHeader->Height <= 0x7FFFFFFF);

    int ZX = 8;
    int ZY = 8;
    
    int SX = 2;
    int SY = 2;

    int ZXSX = ZX*SX;
    int ZYSY = ZY*SY;

    Dst->Width = InfoHeader->Width;
    Dst->Height = InfoHeader->Height;
    Dst->Pitch = (((Dst->Width + ZXSX - 1) / ZXSX) * ZXSX) * 4;
    Dst->Size = (((Dst->Height + ZYSY - 1) / ZYSY) * ZYSY) * Dst->Pitch;
#if 0
    static u8 StaticMemory[4096 * 4096 * 4];
    Dst->At = StaticMemory;
#else
    Dst->At = PlatformAlloc(Dst->Size);
#endif
    Assert(Dst->At);

    u8* DstRow = Dst->At + Dst->Pitch * (Dst->Height - 1);

    if(InfoHeader->BitCount == 24)
    {
        usz SrcPitch = ((InfoHeader->Width*3 + 3) / 4) * 4;
        usz SrcSize = SrcPitch * InfoHeader->Height;
        Assert(Size >= SrcSize);
        Size -= SrcSize;

        u8* SrcRow = Data + FileHeader->DataOffset;

#if 0
        __m128i XMM4 = _mm_setr_epi8(0x00, 0x01, 0x02, 0xFF, 0x03, 0x04, 0x05, 0xFF, 0x06, 0x07, 0x08, 0xFF, 0x09, 0x0A, 0x0B, 0xFF);
        __m128i XMM5 = _mm_setr_epi8(0x04, 0x05, 0x06, 0xFF, 0x07, 0x08, 0x09, 0xFF, 0x0A, 0x0B, 0x0C, 0xFF, 0x0D, 0x0E, 0x0F, 0xFF);
#endif

        for(i32 Y = 0;
            Y < Dst->Height;
            Y++)
        {
            u8* SrcCol = SrcRow;
            u8* DstCol = DstRow;

#if 1
            for(i32 X = 0;
                X < Dst->Width;
                X++)
            {
                u32 Color = *(u32*)(SrcCol) & 0x00FFFFFF;

                *(u32*)(DstCol) = Color;

                DstCol += 4;
                SrcCol += 3;
            }
#else
            for(i32 X = 0;
                X < Dst->Width;
                X += 16)
            {
                __m128i XMM0 = _mm_loadu_si128((__m128i*) &SrcCol[0]);
                __m128i XMM1 = _mm_loadu_si128((__m128i*) &SrcCol[16]);
                __m128i XMM2 = _mm_loadu_si128((__m128i*) &SrcCol[32]);

                __m128i XMM3 = _mm_shuffle_epi8(XMM0, XMM4);
                _mm_storeu_si128((__m128i*) &DstCol[0], XMM3);

                XMM3 = _mm_alignr_epi8(XMM1, XMM0, 12);
                XMM3 = _mm_shuffle_epi8(XMM3, XMM4);
                _mm_storeu_si128((__m128i*) &DstCol[16], XMM3);

                XMM3 = _mm_alignr_epi8(XMM2, XMM1, 8);       
                XMM3 = _mm_shuffle_epi8(XMM3, XMM4);
                _mm_storeu_si128((__m128i*) &DstCol[32], XMM3);

                XMM3 = _mm_shuffle_epi8(XMM2, XMM5);
                _mm_storeu_si128((__m128i*) &DstCol[48], XMM3);

                DstCol += 64;
                SrcCol += 48;
            }
#endif

            SrcRow += SrcPitch;
            DstRow -= Dst->Pitch;
        }
    }
    else if(InfoHeader->BitCount == 32)
    {
        usz SrcPitch = InfoHeader->Width* 4;
        usz SrcSize = SrcPitch * InfoHeader->Height;
        Assert(Size >= SrcSize);
        Size -= SrcSize;

        u8* SrcRow = Data + FileHeader->DataOffset;

        for(i32 Y = 0;
            Y < Dst->Height;
            Y++)
        {
            u32* SrcCol = (u32*) SrcRow;
            u8* DstCol = DstRow;

            for(i32 X = 0;
                X < Dst->Width;
                X++)
            {
                u32 Value = *(SrcCol++) & 0x00FFFFFF;

                *(u32*)(DstCol) = Value;

                DstCol += 4;
            }

            SrcRow += SrcPitch;
            DstRow -= Dst->Pitch;
        }
    }
    else
    {
        Assert(!"Not supported bit count");
    }

    return 1;
}

void* ExportBMP(bitmap* Bitmap, usz* Size)
{
    bmp_file_header* FileHeader;
    bmp_info_header* InfoHeader;

    usz DstBitmapPitch = ((Bitmap->Width * 3 + 3) / 4) * 4;
    usz DstBitmapSize = DstBitmapPitch * Bitmap->Height;
    usz DstFileSize = DstBitmapSize + sizeof(*FileHeader) + sizeof(*InfoHeader);
    u8* DstBuffer = PlatformAlloc(DstFileSize);
    Assert(DstBuffer);

    u8* DstAt = DstBuffer;

    FileHeader = (bmp_file_header*) DstAt;
    DstAt += sizeof(*FileHeader);

    FileHeader->Signature[0] = 'B';
    FileHeader->Signature[1] = 'M';
    Assert(DstFileSize < 0xFFFFFFFF);
    FileHeader->FileSize = (u32)DstFileSize;
    FileHeader->Reserved = 0;
    FileHeader->DataOffset = sizeof(*FileHeader) + sizeof(*InfoHeader);

    InfoHeader = (bmp_info_header*) DstAt;
    DstAt += sizeof(*InfoHeader);

    InfoHeader->Size = sizeof(*InfoHeader);
    InfoHeader->Width = Bitmap->Width;
    InfoHeader->Height = Bitmap->Height;
    InfoHeader->Planes = 1;
    InfoHeader->BitCount = 24;
    InfoHeader->Compression = 0;
    Assert(DstBitmapSize < 0xFFFFFFFF);
    InfoHeader->ImageSize = (u32) DstBitmapSize;
    InfoHeader->XpixelsPerMeter = 0;
    InfoHeader->YpixelsPerMeter = 0;
    InfoHeader->ColorsUsed = 0;
    InfoHeader->ColorsImportant = 0;

    u8* SrcRow = Bitmap->At;
    u8* DstRow = DstAt + DstBitmapSize - DstBitmapPitch;

    for(i32 Y = 0;
        Y < Bitmap->Height;
        Y++)
    {
        u8* SrcCol = SrcRow;
        u8* DstCol = DstRow;

        for(i32 X = 0;
            X < Bitmap->Width;
            X++)
        {
            // TODO: SIMD?

            u8 SrcB = *(SrcCol++);
            u8 SrcG = *(SrcCol++);
            u8 SrcR = *(SrcCol++);
            u8 SrcA = *(SrcCol++);

            *(DstCol++) = SrcB;
            *(DstCol++) = SrcG;
            *(DstCol++) = SrcR;
        }

        SrcRow += Bitmap->Pitch;
        DstRow -= DstBitmapPitch;
    }

    *Size = DstFileSize;
    return DstBuffer;
}