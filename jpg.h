#define JPG_SOI  0xD8FF
#define JPG_APP0 0xE0FF
#define JPG_APP1 0xE1FF
#define JPG_COM  0xFEFF
#define JPG_DQT  0xDBFF
#define JPG_SOF0 0xC0FF
#define JPG_SOF2 0xC2FF
#define JPG_DHT  0xC4FF
#define JPG_SOS  0xDAFF
#define JPG_EOI  0xD9FF

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

#define JPG_COMPONENT_Y 1
#define JPG_COMPONENT_Cb 2
#define JPG_COMPONENT_Cr 3
#define JPG_COMPONENT_Additional 4

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
} jpg_app0;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u8 Id;
    u8 Coefficients[64];
} jpg_dqt;
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
} jpg_sof0;
#pragma pack(pop)

#define JPG_DHT_CLASS_DC 0
#define JPG_DHT_CLASS_AC 1

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
} jpg_dht;
#pragma pack(pop)

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
} jpg_sos;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    u16 Marker;
} jpg_soi;
#pragma pack(pop)

static int   DecodeJPG(void* Data, usz Size, bitmap* Bitmap);
static int   DecodeJPGfromBuffer(buffer* Buffer, bitmap* Bitmap);
static void* EncodeJPG(bitmap* Bitmap, usz* Size, u8 Quality);
static int   EncodeJPGintoBuffer(buffer* Buffer, bitmap* Bitmap, u8 Quality);