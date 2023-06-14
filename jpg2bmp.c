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
        fprintf(stderr, "Syntax: %s <InputFile> <OutputFile>\n", Argv[0]);
        return EXIT_FAILURE;
    }

    TIMING_INIT("Reading file");

    char* InputFile = Argv[1];
    char* OutputFile = Argv[2];

    usz InputSize;
    void* InputData = PlatformReadEntireFile(InputFile, &InputSize);
    if(!InputData)
    {
        fprintf(stderr, "Failed to read input file\n");
        return EXIT_FAILURE;
    }

    TIMING_TICK("Decoding JPEG");

    bitmap Bitmap;
    if(!DecodeJPEG(InputData, InputSize, &Bitmap))
    {
        fprintf(stderr, "Failed to decode JPEG\n");
        return EXIT_FAILURE;
    }

    TIMING_TICK("Exporting BMP");
    
    usz OutputSize;
    void* OutputData = ExportBMP(&Bitmap, &OutputSize);
    if(!OutputData)
    {
        fprintf(stderr, "Failed to export BMP\n");
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
    PlatformShowBitmap(&Bitmap, "Bitmap");
#endif

    return EXIT_SUCCESS;
}