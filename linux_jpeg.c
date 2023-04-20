#include "platform.h"
#include "jpeg.h"
#include "jpeg.c"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

static void* PlatformReadEntireFile(const char* Name, usz* Size)
{
    void* Result = 0;

    int Fd = open(Name, O_RDONLY);
    if(Fd != -1)
    {
        struct stat Stat;
        if(fstat(Fd, &Stat) != -1)
        {
            void* Data = mmap(0, Stat.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
            if(Data)
            {
                ssize_t ReadResult = read(Fd, Data, Stat.st_size);
                if(ReadResult != -1)
                {
                    *Size = (usz) ReadResult;

                    Result = Data;
                }
                else
                {
                    munmap(Data, 0);
                }
            }
        }

        close(Fd);
    }

    return Result;
}

function int ParseJPEG(void* Data, usz Size, bitmap* Bitmap)
{
    int Result = 0;

    buffer Buffer;
    Buffer.Elapsed = Size;
    Buffer.At = Data;

    u16* SOI = PopU16(&Buffer);
    if(!SOI || *SOI != JPEG_SOI)
    {
        return 0;
    }

    int QuantizationTablesCount = 0;
    int HuffmanTablesCount = 0;

    while(Buffer.Elapsed)
    {
        u16* MarkerAt = PopU16(&Buffer);
        if(!MarkerAt)
        {
            return 0;
        }

        u16 Marker = *MarkerAt;

        printf("Marker: 0x%04X ", Marker);

        if(Marker == JPEG_EOI)
        {
            printf("\n end of image\n");
            break;
        }
        else
        {
            u16* LengthAt = PopU16(&Buffer);
            if(!LengthAt)
            {
                return 0;
            }

            u16 Length = ByteSwap16(*LengthAt);

            Length -= 2;

            printf("Length: 0x%04X\n", Length);

            void* Payload = PopBytes(&Buffer, Length);
            if(!Payload)
            {
                return 0;
            }

            switch(Marker)
            {
                case JPEG_COM:
                {
                    printf(" %.*s\n", Length, (char*)Payload);
                } break;

                case JPEG_DQT:
                {
                    printf(" QuantizationTable\n");
                    QuantizationTablesCount++;

                    if(Length == 64+1)
                    {
                        u8* QuantizationTable = Payload;
                        printf("First byte: 0x%02x\n", QuantizationTable[0]);
                        for(int i = 0;
                            i < 8;
                            i++)
                        {
                            for(int j = 0;
                                j < 8;
                                j++)
                            {
                                printf("%3d ", QuantizationTable[i*8+j]);
                            }

                            putchar('\n');
                        }
                    }
                } break;

                case JPEG_DHT:
                {
                    HuffmanTablesCount++;

                    jpeg_dht* DHT;
                    if(Length >= sizeof(*DHT))
                    {
                        DHT = Payload;

                        u16 Total = 0;
                        for(usz Idx = 0;
                            Idx < ArrayCount(DHT->Counts);
                            Idx++)
                        {
                            u8 Count = DHT->Counts[Idx];

                            // TODO: This can be a single SIMD 128 bit (16x8) add! :)
                            Total += DHT->Counts[Idx];
                        }

                        if(Length == Total+sizeof(*DHT))
                        {
                            printf("HuffmanTable\n");

                            u8* At = DHT->Values;
                            for(usz Row = 0;
                                Row < ArrayCount(DHT->Counts);
                                Row++)
                            {
                                printf("%2lu: ", Row);
                                for(usz Col = 0;
                                    Col < DHT->Counts[Row];
                                    Col++)
                                {
                                    u8 Byte = *(At++);
                                    printf("%3d, ", Byte);
                                }
                                putchar('\n');
                            }
                        }
                    }
                } break;

                case JPEG_SOF0:
                {
                    jpeg_sof0* SOF0;
                    if(Length == sizeof(*SOF0))
                    {
                        SOF0 = Payload;

                        SOF0->ImageHeight = ByteSwap16(SOF0->ImageHeight);
                        SOF0->ImageWidth = ByteSwap16(SOF0->ImageWidth);

                        printf(" Start of frame baseline\n");
                    }
                } break;

                case JPEG_SOF2:
                {
                    printf(" Start of frame progressive\n");
                } break;

                case JPEG_SOS:
                {
                    jpeg_sos_pre* SOSpre;
                    if(Length >= sizeof(*SOSpre))
                    {
                        SOSpre = Payload;
                        Length -= sizeof(*SOSpre);

                        jpeg_sos_component* SOScomponent;
                        if(Length >= SOSpre->NumComponents*sizeof(*SOScomponent))
                        {
                            SOScomponent = (jpeg_sos_component*) &SOSpre[1];
                            Length -= SOSpre->NumComponents*sizeof(*SOScomponent);

                            jpeg_sos_end* SOSend;
                            if(Length >= sizeof(*SOSend))
                            {
                                SOSend = (jpeg_sos_end*) &SOScomponent[SOSpre->NumComponents];

                                printf(" Start of scan\n");
                            }
                        }
                    }
                } break;

                default:
                {
                    if((Marker & JPEG_APP0) == JPEG_APP0)
                    {
                        printf(" APP%d marker\n", (Marker >> 8) & 0x0F);

                        if(Marker == JPEG_APP0)
                        {
                            jpeg_app0* APP0;
                            if(Length == sizeof(*APP0))
                            {
                                APP0 = Payload;

                                APP0->HorizontalDensity = ByteSwap16(APP0->HorizontalDensity);
                                APP0->VerticalDensity = ByteSwap16(APP0->VerticalDensity);

                                printf("APP0\n");
                            }
                        }
                    }
                } break;
            }
        }
    }

    return Result;
}

int main(int Argc, char** Argv)
{
    if(Argc < 2)
    {
        fprintf(stderr, "Syntax: %s <FileName>\n", Argv[0]);
        return EXIT_FAILURE;
    }

    const char* FileName = Argv[1];

    usz FileSize;
    void* FileData = PlatformReadEntireFile(FileName, &FileSize);
    if(!FileData)
    {
        fprintf(stderr, "Failed to read file\n");
        return EXIT_FAILURE;
    }

    bitmap Bitmap;
    if(!ParseJPEG(FileData, FileSize, &Bitmap))
    {
        fprintf(stderr, "Failed to parse JPEG\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}