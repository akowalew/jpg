#define JPEG_SOI  0xD8FF
#define JPEG_APP0 0xE0FF
#define JPEG_APP1 0xE1FF
#define JPEG_COM  0xFEFF
#define JPEG_DQT  0xDBFF
#define JPEG_SOF0 0xC0FF
#define JPEG_SOF2 0xC2FF
#define JPEG_DHT  0xC4FF
#define JPEG_SOS  0xDAFF
#define JPEG_EOI  0xD9FF

// Marker: 0xE0FF Length: 16
// Marker: 0xE1FF Length: 34
// Marker: 0xDBFF Length: 67
// Marker: 0xDBFF Length: 67
// Marker: 0xC0FF Length: 17
// Marker: 0xC4FF Length: 31
// Marker: 0xC4FF Length: 181
// Marker: 0xC4FF Length: 31
// Marker: 0xC4FF Length: 181
// Marker: 0xDAFF

// Marker: 0xE0FF Length: 0x0010
// Marker: 0xDBFF Length: 0x0043
// Marker: 0xDBFF Length: 0x0043
// Marker: 0xC0FF Length: 0x0011
// Marker: 0xC4FF Length: 0x001D
// Marker: 0xC4FF Length: 0x0042
// Marker: 0xC4FF Length: 0x001B
// Marker: 0xC4FF Length: 0x0036
// Marker: 0xDAFF Length: 0x000C
// Marker: 0xCCB2 Length: 0x506F
// Marker: 0x3FAA Length: 0x49E0
// Marker: 0xD4CD Length: 0xE128

// Marker: 0xE0FF Length: 0x0010
// Marker: 0xFEFF Length: 0x005B
// Marker: 0xDBFF Length: 0x0084
// Marker: 0xC2FF Length: 0x0011
// Marker: 0xC4FF Length: 0x001D
// Marker: 0xDAFF Length: 0x0008
// Marker: 0xA6FA Length: 0x0000

// 00 -> 0
// 010 -> 1
// 011 -> 2
// 100 -> 3
// 101 -> 4
// 110 -> 5
// 1110 -> 6
// 11110 -> 7
// 111110 -> 8
// 1111110 -> 9
// 11111110 -> 10
// 111111110 -> 11

#define JPEG_COMPONENT_Y 1
#define JPEG_COMPONENT_Cb 2
#define JPEG_COMPONENT_Cr 3
#define JPEG_COMPONENT_Additional 4

#pragma pack(push, 1)
typedef struct
{
    u8 JFIF[5]; // JFIF identifier, should be "JFIF\0"
    u8 VersionMajor; // Major version number
    u8 VersionMinor; // Minor version number
    u8 DensityUnit; // Unit of measure for X and Y density fields
    u16 HorizontalDensity; // Horizontal pixel density
    u16 VerticalDensity; // Vertical pixel density
    u8 ThumbnailWidth; // Thumbnail image width
    u8 ThumbnailHeight; // Thumbnail image height
} jpeg_app0;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u8 Id;
    u8 Coefficients[64];
} jpeg_dqt;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u8 BitsPerSample; // Number of bits per sample
    u16 ImageHeight; // Image height in pixels
    u16 ImageWidth; // Image width in pixels
    u8 NumComponents; // Number of color components in the image
    struct {
        u8 ComponentId; // Component identifier
        u8 VerticalSubsamplingFactor : 4;
        u8 HorizontalSubsamplingFactor : 4;
        u8 QuantizationTableId; // Quantization table identifier for this component
    } Components[3]; // Up to 3 components (Y, Cb, Cr)
} jpeg_sof0;
#pragma pack(pop)

#define JPEG_DHT_CLASS_DC 0
#define JPEG_DHT_CLASS_AC 1

#pragma pack(push, 1)
typedef struct
{
    union
    {
        struct
        {
            u8 TableId : 4; // Huffman table identifier (0-3)
            u8 TableClass : 4; // Huffman table class (0 for DC, 1 for AC)
        };
        u8 Header;
    };
    u8 Counts[16]; // Number of Huffman codes for each code length
    u8 Values[];
} jpeg_dht;
#pragma pack(pop)

typedef struct
{
    u8* At;
    usz Elapsed;
    u32 Bits;
    u8 Pos;
} bit_stream;

static int Refill(bit_stream* BitStream)
{
    int Result = 0;

    u8 BytesCount = BitStream->Pos / 8;

    if(BitStream->Elapsed >= BytesCount)
    {
        printf("Refilling: ");
        for(usz ByteIdx = 0;
            ByteIdx < BytesCount;
            ByteIdx++)
        {
            u32 Byte = BitStream->At[ByteIdx];
            printf("0x%02X, ", Byte);

            BitStream->Bits >>= 8;
            BitStream->Bits |= (Byte << 24);
        }
        putchar('\n');

        BitStream->Pos -= BytesCount * 8;
        BitStream->Elapsed -= BytesCount;
        BitStream->At += BytesCount;

        printf("BitStream: ");
        for(int Idx = 31;
            Idx >= BitStream->Pos;
            Idx--)
        {
            putchar((BitStream->Bits & (1 << Idx)) ? '1' : '0');
        }
        putchar('\n');

        Result = 1;
    }

    return Result;
}

static int DecodeValue(bit_stream* BitStream, jpeg_dht* HT, u8* Value)
{
    if(!Refill(BitStream))
    {
        return 0;
    }

    u8* At = HT->Values;

    u16 Code = 0;
    printf("Tree:\n");
    for(u8 I = 0; I < 16; I++)
    {
        u8 Count = HT->Counts[I];
        u8 BitLen = I+1;
        for(u8 J = 0; J < Count; J++)
        {
#if 1
            int Matches = 1;
            u32 Mask = (1 << BitStream->Pos);
            for(int K = BitLen-1;
                K >= 0;
                K--)
            {
                int Left = (BitStream->Bits & Mask) != 0;
                int Right = (Code & (1 << K)) != 0;
                if(Left != Right)
                {
                    Matches = 0;
                }
                putchar(Right ? '1' : '0');
                Mask <<= 1;
            }
            printf(" -> %d\n", *At);
#else
            for(int K = BitLen-1;
                K >= 0;
                K--)
            {
                putchar((Code & (1 << K)) ? '1' : '0');
            }
            printf(" -> %d\n", *At);

            int Matches = 0;
            if(((BitStream->Bits >> BitStream->Pos) & ((1 << BitLen)-1)) == Code)
            {
                Matches = 1;
            }
#endif

            if(Matches)
            {
                printf("Value: %d\n", *At);
                BitStream->Pos += BitLen;
                *Value = *At;
                return 1;
            }

            At++;

            Code++;
        }

        Code <<= 1;
    }

    return 0;
}

#pragma pack(push, 1)
typedef struct
{
    u8 NumComponents; // Number of image components in scan, should be 1, 3, or 4
    struct
    {
        u8 ComponentId; // Index of the image component (1 = Y, 2 = Cb, 3 = Cr, 4 = additional component)
        union
        {
            struct
            {
                u8 IndexDC : 4; // High 4 bits: DC entropy coding table index, Low 4 bits: AC entropy coding table index
                u8 IndexAC : 4; // High 4 bits: DC entropy coding table index, Low 4 bits: AC entropy coding table index
            };
            u8 Index;
        };
    } Components[3];
    u8 StartOfSelection; // Spectral selection start (0-63)
    u8 EndOfSelection; // Spectral selection end (0-63)
    u8 ApproximationBitLow : 4; // Approximation bit position high and low nibbles
    u8 ApproximationBitHigh : 4; // Approximation bit position high and low nibbles
} jpeg_sos;
#pragma pack(pop)

function int ParseJPEG(void* Data, usz Size, bitmap* Bitmap);
