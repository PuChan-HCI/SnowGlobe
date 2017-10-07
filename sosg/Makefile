OBJS = sosg_image.o sosg_predict.o sosg_tracker.o
CC = gcc
CFLAGS = -O3 -Wall `sdl-config --cflags` -I/usr/local/include/SDL -DGL_GLEXT_PROTOTYPES
LDFLAGS = `sdl-config --libs` -lSDL_image -lSDL_net -lSDL_gfx -l SDL_ttf

ifdef USE_SOSG_VIDEO
	OBJS := sosg_video.o
	LDFLAGS := -lvlc
endif

# Mac links OpenGL differently than Linux
OS := $(shell uname)
ifeq ($(OS),Darwin)
	LDFLAGS := -framework OpenGL $(LDFLAGS)
else
	LDFLAGS := -lGL -lGLU $(LDFLAGS)
endif

.PHONY: all
all: sosg

sosg: sosg.o $(OBJS)
	$(CC) -o $@ sosg.o $(OBJS) $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) sosg.o sosg
