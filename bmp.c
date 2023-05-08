int ParseBMP(u8* Data, usz Size, bitmap* Dst)
{
    u8* At = Data;

    bmp_file_header* FileHeader;
    if(Size < sizeof(*FileHeader))
    {
        // TODO: Logging?
        return 0;
    }

    FileHeader = (bmp_file_header*) At;
    At += sizeof(*FileHeader);
    Size -= sizeof(*FileHeader);

    if(FileHeader->Signature[0] != 'B' ||
       FileHeader->Signature[1] != 'M')
    {
        // TODO: Logging?
        return 0;
    }

    bmp_info_header* InfoHeader;
    if(Size < sizeof(*InfoHeader))
    {
        // TODO: Logging?
        return 0;
    }

    InfoHeader = (bmp_info_header*) At;
    At += sizeof(InfoHeader);
    Size -= sizeof(InfoHeader);

    if(InfoHeader->Planes != 1)
    {
        // TODO: Logging?
        return 0;
    }

    if(InfoHeader->BitCount != 24)
    {
        // TODO: Logging?
        return 0;
    }

    if(InfoHeader->Width > 0x7FFFFFFF ||
       InfoHeader->Height > 0x7FFFFFFF )
    {
        // TODO: Logging?
        return 0;
    }

    usz SrcPitch = ((InfoHeader->Width*3 + 3) / 4) * 4;
    usz SrcSize = SrcPitch * InfoHeader->Height;
    if(Size < SrcSize)
    {
        // TODO: Logging
        return 0;
    }

    u8* SrcRow = Data + FileHeader->DataOffset;
    At += SrcSize;
    Size -= SrcSize;

    Dst->Width = InfoHeader->Width;
    Dst->Height = InfoHeader->Height;
    Dst->Pitch = Dst->Width * 4;
    Dst->Size = Dst->Pitch * Dst->Height;
    Dst->At = PlatformAlloc(Dst->Size);
    if(!Dst->At)
    {
        // TODO: Logging
        return 0;
    }

    u8* DstRow = Dst->At + Dst->Size - Dst->Pitch;

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