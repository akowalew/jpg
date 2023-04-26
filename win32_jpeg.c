#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "jpeg.h"
#include "jpeg.c"

static void* PlatformAlloc(usz Size)
{
    return VirtualAlloc(0, Size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
}

static void* PlatformReadEntireFile(const char* Name, usz* Size)
{
    void* Result = 0;

    HANDLE FileHandle = CreateFileA(Name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD FileSize = GetFileSize(FileHandle, 0);
        if(FileSize != INVALID_FILE_SIZE)
        {
            void* FileData = VirtualAlloc((LPVOID)(0x800000000), FileSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            if(FileData)
            {
                if(ReadFile(FileHandle, FileData, FileSize, 0, 0))
                {
                    *Size = FileSize;
                    Result = FileData;
                }
                else
                {
                    VirtualFree(FileData, 0, MEM_DECOMMIT|MEM_RELEASE);
                }
            }
        }

        CloseHandle(FileHandle);
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

    char* FileName = Argv[1];

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