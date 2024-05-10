# JPG

Another free time fun hobby project - JPG parsing. 

It's amazing how this format is designed! And what is even more amazing - how SIMD can speed things up, wow! 

Nowadays CPUs are so much powerful - we can do so many computations just using **single core** - the only thing we need to do is to not write obviously stupid and obviously slow code.

# What is done

![image](https://github.com/akowalew/jpg/assets/11333571/55e19fce-3f12-4775-af15-bb60405265e1)

1. JPG reading and writing for RGB888 with SIMD on single core!
2. JPG compression leveling
3. Usage of standard Quantization and Huffman tables for compression at the moment only
4. Added utilities to read from and write into BMP format for ease of test and comparison
5. Added utilities to visually show encoded/decoded image - Win32 GDI and Linux X11 backends only
6. MSVC and GCC compilers support - easy to add more
7. x86_64 SSE2/SSE3/SSE4 and AVX support only
8. Benchmarking using RTDSC via QPC on x86_64 only
9. No dependencies and super fast to build

# What needs to be fixed

As you may see there are still some artifacts after ZigZag or after Quantization - I was experimenting recently with AVX and probably left some bug there, but it's easy to fix. Just need some time.

As with everything - we can go deep with optimization into the hell - will be nice to squeeze it even more! ;)

Some cleanup is needed to enclose everything in just one file.
    
# Building

Just hit `build.bat` or `sh build.sh` depending on platform and go! 

Then type `jpg2bmp.exe test.jpg out.bmp` and then reverse the process by typing `bmp2jpg.exe out.bmp out.jpg`.
