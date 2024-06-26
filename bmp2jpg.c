#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "bmp.h"
#include "bmp.c"
#include "jpg.h"
#include "jpg.c"

int main(int Argc, char** Argv)
{
    if(Argc < 3)
    {
        fprintf(stderr, "Syntax: %s <InputFile> <OutputFile> [Quality=50]\n", Argv[0]);
        return EXIT_FAILURE;
    }

    char* InputFile = Argv[1];
    char* OutputFile = Argv[2];

    TIMING_INIT("Reading file");

    usz InputSize;
    void* InputData = PlatformReadEntireFile(InputFile, &InputSize);
    if(!InputData)
    {
        fprintf(stderr, "Failed to read input file\n");
        return EXIT_FAILURE;
    }

    TIMING_TICK("Parsing BMP");

    bitmap Bitmap;
    if(!ParseBMP(InputData, InputSize, &Bitmap))
    {
        fprintf(stderr, "Failed to parse BMP\n");
        return EXIT_FAILURE;
    }

    TIMING_TICK("Encoding JPG");

#if 0
    PlatformShowBitmap(&Bitmap, "BMP");
#endif
    
    u8 Quality = 50;
    if(Argc >= 4)
    {
        Quality = (u8) atoi(Argv[3]);
    }

    usz OutputSize;
    void* OutputData = EncodeJPG(&Bitmap, &OutputSize, Quality);
    if(!OutputData)
    {
        fprintf(stderr, "Failed to encode JPG\n");
        return EXIT_FAILURE;
    }

    TIMING_TICK("Writing file");

    if(!PlatformWriteEntireFile(OutputFile, OutputData, OutputSize))
    {
        fprintf(stderr, "Failed to write output file\n");
        return EXIT_FAILURE;
    }

    TIMING_FINI("Done");

#if 1
    bitmap JPG = {0};
    Assert(DecodeJPG(OutputData, OutputSize, &JPG));
    PlatformShowBitmap(&JPG, "JPG");
#endif

    return EXIT_SUCCESS;
}