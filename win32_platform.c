#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "platform.h"

void* PlatformAlloc(usz Size)
{
    void* Result = VirtualAlloc(0, Size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    return Result;
}

void* PlatformReadEntireFile(const char* Name, usz* Size)
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

int PlatformWriteEntireFile(const char* Name, void* Data, usz Size)
{
    int Result = 0;

    Assert(Size < 0xFFFFFFFF);
    DWORD BytesToWrite = (DWORD) Size;

    HANDLE FileHandle = CreateFileA(Name, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        if(WriteFile(FileHandle, Data, BytesToWrite, 0, 0))
        {
            Result = 1;
        }

        CloseHandle(FileHandle);
    }

    return Result;
}

LRESULT MainWindowProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Msg)
    {
        case WM_CLOSE:
        {
            // TODO: Message to user?
            PostQuitMessage(0);
        } break;

        default:
        {
            Result = DefWindowProc(Window, Msg, WParam, LParam);
        } break;
    }

    return Result;
}

int PlatformShowBitmap(bitmap* Bitmap, const char* Title)
{
    if(!Bitmap)
    {
        return 0;
    }

    WNDCLASSEXA WindowClass = {0};
    WindowClass.cbSize = sizeof(WindowClass);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = MainWindowProc;
    WindowClass.cbClsExtra = 0;
    WindowClass.cbWndExtra = 0;
    WindowClass.hInstance = GetModuleHandle(0);
    WindowClass.hIcon = 0;
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.hbrBackground = 0;
    WindowClass.lpszMenuName = 0;
    WindowClass.lpszClassName = "TestWindowClass";
    WindowClass.hIconSm = 0;
    Assert(RegisterClassEx(&WindowClass));

    HWND Window = CreateWindowEx(0,
                                 WindowClass.lpszClassName,
                                 Title,
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 0, 0, GetModuleHandle(0), 0);
    Assert(Window);

    HDC DeviceContext = GetDC(Window);
    Assert(DeviceContext);

    MSG Message;
    while(GetMessage(&Message, 0, 0, 0))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);

        BITMAPINFO BitmapInfo = {0};
        BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
        BitmapInfo.bmiHeader.biWidth = Bitmap->Width;
        BitmapInfo.bmiHeader.biHeight = -(int)(Bitmap->Height);
        BitmapInfo.bmiHeader.biPlanes = 1;
        BitmapInfo.bmiHeader.biBitCount = 32;
        BitmapInfo.bmiHeader.biCompression = BI_RGB;
        BitmapInfo.bmiHeader.biSizeImage = 0;
        BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
        BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
        BitmapInfo.bmiHeader.biClrUsed = 0;
        BitmapInfo.bmiHeader.biClrImportant = 0;

        Assert(SetDIBitsToDevice(DeviceContext,
                                 0, 0,
                                 Bitmap->Width, Bitmap->Height,
                                 0, 0,
                                 0, Bitmap->Height,
                                 Bitmap->At, &BitmapInfo, DIB_RGB_COLORS));
    }

    return (int) Message.wParam;
}

// static void FillRectangle(bitmap* Bitmap, int X1, int Y1, int X2, int Y2, u32 Color)
// {
//     u8* Row = Bitmap->At + Y1*Bitmap->Pitch + X1*4;
//     for(int Y = Y1;
//         Y <= Y2;
//         Y++)
//     {
//         u8* Col = Row;
        
//         for(int X = X1;
//             X <= X2;
//             X++)
//         {
//             *(u32*)(Col) = Color;

//             Col += 4;
//         }   

//         Row += Bitmap->Pitch;
//     }
// }

// int main(int Argc, char** Argv)
// {
// #if 0
//     bitmap Example;
//     Example.Width = 100;
//     Example.Height = 100;
//     Example.Pitch = Example.Width * 4;
//     Example.Size = Example.Pitch * Example.Height;
//     Example.At = PlatformAlloc(Example.Size);
//     FillRectangle(&Example, 0, 0, 31, 31, 0x000000FF);
//     PlatformShowBitmap(&Example, "example");
// #endif

//     if(Argc < 2)
//     {
//         fprintf(stderr, "Syntax: %s <FileName>\n", Argv[0]);
//         return EXIT_FAILURE;
//     }

//     char* FileName = Argv[1];

//     usz FileSize;
//     void* FileData = PlatformReadEntireFile(FileName, &FileSize);
//     if(!FileData)
//     {
//         fprintf(stderr, "Failed to read file\n");
//         return EXIT_FAILURE;
//     }

//     bitmap Bitmap;
//     if(!ParseJPEG(FileData, FileSize, &Bitmap))
//     {
//         fprintf(stderr, "Failed to parse JPEG\n");
//         return EXIT_FAILURE;
//     }

//     return EXIT_SUCCESS;
// }