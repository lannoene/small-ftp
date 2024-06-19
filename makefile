# A simple Makefile for compiling small SDL projects

# set the compiler flags
PACKAGES := sdl2 SDL2_image SDL2_mixer SDL2_ttf
CFLAGS := -g -std=gnu2x -Wall -Wno-switch -mconsole -Wno-unused-variable -static
CFLAGS += `pkg-config --cflags --static $(PACKAGES)`
LDFLAGS :=-luuid -lole32 -lws2_32
LDFLAGS += `pkg-config --cflags --libs --static $(PACKAGES)` -lgraphite2 -lfreetype -lsharpyuv -lstdc++
# add header files here
HDRS :=
WINDRES = windres

# add source files here
SRCS := source/main.c source/conio.c source/dir.c source/net.c source/thread_wrap.c source/transfer.c source/queue.c source/init.c source/sdlwrp.c source/menu.c source/shell.c source/address_dbg.c

# generate names of object files
OBJS := $(SRCS:.c=.o) manifest.o

# name of executable
EXEC := a

# default recipe
all: $(EXEC)

# recipe for building the final executable
$(EXEC): $(OBJS) $(HDRS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

# recipe for building object files
#$(OBJS): $(@:.o=.c) $(HDRS) Makefile
#    $(CC) -o $@ $(@:.o=.c) -c $(CFLAGS)

manifest.o: manifest.rc utf8.xml
	$(WINDRES) -o $@ manifest.rc

# recipe to clean the workspace
clean:
	rm -f $(EXEC) $(OBJS)

.PHONY: all clean
