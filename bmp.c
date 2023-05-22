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

        for(i32 Y = 0;
            Y < Dst->Height;
            Y++)
        {
            u8* SrcCol = SrcRow;
            u8* DstCol = DstRow;

            for(i32 X = 0;
                X < Dst->Width;
                X++)
            {
                u8 SrcB = *(SrcCol++);
                u8 SrcG = *(SrcCol++);
                u8 SrcR = *(SrcCol++);
                u8 SrcA = 0;

                *(u32*)(DstCol) = (SrcA << 24) | (SrcR << 16) | (SrcG << 8) | (SrcB << 0);

                DstCol += 4;
            }

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