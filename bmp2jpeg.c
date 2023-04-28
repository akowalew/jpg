#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "bmp.h"
#include "bmp.c"
#include "jpeg.h"
#include "jpeg.c"

int main(int Argc, char** Argv)
{
    if(Argc < 3)
    {
        fprintf(stderr, "Syntax: %s <InputFile> <OutputFile>\n", Argv[0]);
        return EXIT_FAILURE;
    }

    char* InputFile = Argv[1];
    char* OutputFile = Argv[2];

    usz InputSize;
    void* InputData = PlatformReadEntireFile(InputFile, &InputSize);
    if(!InputData)
    {
        fprintf(stderr, "Failed to read input file\n");
        return EXIT_FAILURE;
    }

    bitmap Bitmap;
    if(!ParseBMP(InputData, InputSize, &Bitmap))
    {
        fprintf(stderr, "Failed to parse BMP\n");
        return EXIT_FAILURE;
    }

#if 0
    PlatformShowBitmap(&Bitmap, "BMP");
#endif
    
    usz OutputSize;
    void* OutputData = ExportJPEG(&Bitmap, &OutputSize);
    if(!OutputData)
    {
        fprintf(stderr, "Failed to export JPEG\n");
        return EXIT_FAILURE;
    }

    if(!PlatformWriteEntireFile(OutputFile, OutputData, OutputSize))
    {
        fprintf(stderr, "Failed to write output file\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}