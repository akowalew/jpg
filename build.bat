@echo off
set FLAGS=/nologo /W4 /Oi /EHa- /EHc- /EHs- /GR- /GS- /Gm- /Gw- /Gy- /Zi /Zo /MT /WX /wd4100 /wd4189 /wd4101 /wd4201 /wd4200 /wd4702 /link /DEBUG:FULL /INCREMENTAL:NO /WX
cl bmp2jpg.c win32_platform.c /Fe:bmp2jpg.exe %FLAGS% user32.lib gdi32.lib
cl jpg2bmp.c win32_platform.c /Fe:jpg2bmp.exe %FLAGS% user32.lib gdi32.lib
