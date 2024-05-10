#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "platform.h"
#include "jpg.h"
#include "jpg.c"

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