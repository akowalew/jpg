@echo off
set FLAGS=/nologo /W4 /Oi /EHa- /EHc- /EHs- /GR- /GS- /Gm- /Gw- /Gy- /Zi /Zo /MT /WX /wd4100 /wd4189 /wd4101 /wd4201 /wd4200 /link /DEBUG:FULL /INCREMENTAL:NO /WX
cl win32_jpeg.c /Fe:jpeg.exe %FLAGS% user32.lib gdi32.lib
