#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "platform.h"

void PlatformFree(void* Data)
{
    VirtualFree(Data, 0, MEM_DECOMMIT|MEM_RELEASE);
}

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

    HANDLE FileHandle = CreateFileA(Name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
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
        case WM_KEYDOWN:
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

    if(Bitmap->Pitch != Bitmap->Width*4)
    {
        // FIXME: Win32 GDI API cannot handle pitch different than that...
        // maybe use OpenGL then? Instead now we need to fix it manually...

        u8* SrcRow = Bitmap->At;
        i32 SrcPitch = Bitmap->Pitch;
        u8* DstRow = Bitmap->At;
        i32 DstPitch = Bitmap->Width*4;

        for(i32 Y = 0;
            Y < Bitmap->Height;
            Y++)
        {
            u32* SrcCol = (u32*) SrcRow;
            u32* DstCol = (u32*) DstRow;

            for(i32 X = 0;
                X < Bitmap->Width;
                X++)
            {
                *(DstCol++) = *(SrcCol++);
            }

            SrcRow += SrcPitch;
            DstRow += DstPitch;
        }

        Bitmap->Pitch = DstPitch;
    }

    HMODULE ModuleHandle = GetModuleHandle(0);
    static const char* ClassName = "TestWindowClass";
    static int ClassRegistered = 0;
    if(!ClassRegistered)
    {
        WNDCLASSEXA WindowClass = {0};
        WindowClass.cbSize = sizeof(WindowClass);
        WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        WindowClass.lpfnWndProc = MainWindowProc;
        WindowClass.cbClsExtra = 0;
        WindowClass.cbWndExtra = 0;
        WindowClass.hInstance = ModuleHandle;
        WindowClass.hIcon = 0;
        WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        WindowClass.hbrBackground = 0;
        WindowClass.lpszMenuName = 0;
        WindowClass.lpszClassName = ClassName;
        WindowClass.hIconSm = 0;
        Assert(RegisterClassEx(&WindowClass));
        ClassRegistered = 1;
    }

    DWORD Style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    DWORD ExStyle = WS_EX_OVERLAPPEDWINDOW;
    RECT WindowRect;
    WindowRect.left = 0;
    WindowRect.right = Bitmap->Width;
    WindowRect.top = 0;
    WindowRect.bottom = Bitmap->Height;
    AdjustWindowRectEx(&WindowRect, Style, 0, ExStyle);
    int WindowWidth = WindowRect.right - WindowRect.left;
    int WindowHeight = WindowRect.bottom - WindowRect.top;

    HWND Window = CreateWindowEx(ExStyle,
                                 ClassName,
                                 Title,
                                 Style,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 WindowWidth, WindowHeight,
                                 0, 0, ModuleHandle, 0);
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
        BitmapInfo.bmiHeader.biSizeImage = Bitmap->Size;
        BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
        BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
        BitmapInfo.bmiHeader.biClrUsed = 0;
        BitmapInfo.bmiHeader.biClrImportant = 0;

#if 1
        Assert(SetDIBitsToDevice(DeviceContext,
                                 0, 0,
                                 Bitmap->Width, Bitmap->Height,
                                 0, 0,
                                 0, Bitmap->Height,
                                 Bitmap->At, &BitmapInfo, DIB_RGB_COLORS));
#else
        RECT ClientRect;
        Assert(GetClientRect(Window, &ClientRect));
        int ClientWidth = ClientRect.right - ClientRect.left;
        int ClientHeight = ClientRect.bottom - ClientRect.top;
        Assert(StretchDIBits(DeviceContext, 0, 0, ClientWidth, ClientHeight,
                             0, 0, Bitmap->Width, Bitmap->Height, Bitmap->At,
                             &BitmapInfo, DIB_RGB_COLORS, SRCCOPY));
#endif
    }

    return (int) Message.wParam;
}

u64 PlatformGetTicks(void)
{
    LARGE_INTEGER Ticks;
    Assert(QueryPerformanceCounter(&Ticks));
    return Ticks.QuadPart;
}