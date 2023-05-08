#pragma pack(push, 1)
typedef struct
{
    u8 Signature[2];
    u32 FileSize;
    u32 Reserved;
    u32 DataOffset;
} bmp_file_header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u32 Size;
    u32 Width;
    u32 Height;
    u16 Planes;
    u16 BitCount;
    u32 Compression;
    u32 ImageSize;
    u32 XpixelsPerMeter;
    u32 YpixelsPerMeter;
    u32 ColorsUsed;
    u32 ColorsImportant;
} bmp_info_header;
#pragma pack(pop)

int ParseBMP(u8* Data, usz Size, bitmap* Dst);
void* ExportBMP(bitmap* Bitmap, usz* Size);