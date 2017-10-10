OBJS = sosg_image.o sosg_predict.o sosg_tracker.o
CFLAGS = -O3 -Wall `sdl2-config --cflags` -DGL_GLEXT_PROTOTYPES
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_net -lSDL2_gfx -lSDL2_ttf -lm

ifdef USE_SOSG_VIDEO
	OBJS := sosg_video.o
	LDFLAGS := -lvlc
endif

# Mac links OpenGL differently than Linux
OS := $(shell uname)
ifeq ($(OS),Darwin)
	CC = clang
	LDFLAGS := -framework OpenGL $(LDFLAGS)
else
	CC = gcc
	LDFLAGS := -lGL -lGLU $(LDFLAGS)
endif

.PHONY: all
all: sosg

sosg: sosg.o $(OBJS)
	$(CC) -o $@ sosg.o $(OBJS) $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) sosg.o sosg
