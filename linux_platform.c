#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "platform.h"

void* PlatformAlloc(usz Size)
{
    return malloc(Size);//mmap(0, Size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
}

void PlatformFree(void* Data)
{
    munmap(Data, 0);
}

void* PlatformReadEntireFile(const char* Name, usz* Size)
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

int PlatformWriteEntireFile(const char* Name, void* Data, usz Size)
{
    int Result = 0;

    int Fd = open(Name, O_RDWR);
    if(Fd != -1)
    {
        ssize_t WriteResult = write(Fd, Data, Size);
        if(WriteResult != -1)
        {
            size_t BytesWritten = (size_t) WriteResult;
            if(BytesWritten == Size)
            {
                Result = 1;
            }
        }

        close(Fd);
    }

    return Result;
}

int PlatformShowBitmap(bitmap* Bitmap, const char* Title)
{
    static Display* xDisplay;
    if(!xDisplay)
    {
        xDisplay = XOpenDisplay(0);
    }

    int xRootWindow = DefaultRootWindow(xDisplay);
    int xDefaultScreen = DefaultScreen(xDisplay);

    XVisualInfo xVisualInfo = {0};
    XMatchVisualInfo(xDisplay, xDefaultScreen, 24, TrueColor, &xVisualInfo);

    XSetWindowAttributes xWindowAttributes = {0};
    xWindowAttributes.background_pixel = 0;
    xWindowAttributes.colormap = XCreateColormap(xDisplay, xRootWindow, xVisualInfo.visual, AllocNone);
    xWindowAttributes.event_mask = StructureNotifyMask;
    Window xWindow = XCreateWindow(xDisplay, xRootWindow, 0, 0,
                                   Bitmap->Width, Bitmap->Height, 0,
                                   xVisualInfo.depth, InputOutput, xVisualInfo.visual,
                                   CWBackPixel | CWColormap | CWEventMask,
                                   &xWindowAttributes);
    XStoreName(xDisplay, xWindow, Title);
    XMapWindow(xDisplay, xWindow);
    XFlush(xDisplay);

    Atom WM_DELETE_WINDOW = XInternAtom(xDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(xDisplay, xWindow, &WM_DELETE_WINDOW, 1);

    XImage* xWindowBuffer = XCreateImage(xDisplay, xVisualInfo.visual, xVisualInfo.depth,
                                 ZPixmap, 0, (char*)Bitmap->At, Bitmap->Width, Bitmap->Height,
                                 32, 0);

    GC xDefaultGC = DefaultGC(xDisplay, xDefaultScreen);

    while(1)
    {
        XEvent xEvent = {0};
        XNextEvent(xDisplay, &xEvent);
        switch(xEvent.type)
        {
            case ClientMessage:
            {
                XClientMessageEvent* xE = (XClientMessageEvent*) &xEvent;
                if((Atom)xE->data.l[0] == WM_DELETE_WINDOW)
                {
                    XDestroyWindow(xDisplay, xWindow);
                    return 0;
                }
            } break;
        }

        XPutImage(xDisplay, xWindow, xDefaultGC, xWindowBuffer, 0, 0, 0, 0, Bitmap->Width, Bitmap->Height);
    }

    return 0;
}