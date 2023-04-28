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