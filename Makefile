CFLAGS += -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -Wundef
CFLAGS += -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable
CFLAGS += -O0 -g3

all:
	@$(CC) bmp2jpeg.c linux_platform.c -o bmp2jpeg $(CFLAGS) -lX11
	@$(CC) jpg2bmp.c linux_platform.c -o jpg2bmp $(CFLAGS) -lX11