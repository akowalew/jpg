CFLAGS += -Wall -Wextra -Wpedantic -Werror -Wfatal-errors -Wundef
CFLAGS += -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable
CFLAGS += -O0 -g3

all:
	@$(CC) linux_jpeg.c -o jpeg $(CFLAGS)