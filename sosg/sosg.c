/*
Filename:     sosg.c
Content:      Science on a Snow Globe

    An extremely basic take on supporting Science On a Sphere datasets on
    Snow Globe, a low cost DIY spherical display.
    Datasets and SOS information available at http://sos.noaa.gov
    Snow Globe information at http://eclecti.cc

    Parts are copied from some public domain code from
    Kyle Foley: http://gpwiki.org/index.php/SDL:Tutorials:Using_SDL_with_OpenGL
    John Tsiombikas: http://nuclear.mutantstargoat.com/articles/sdr_fract/

Authors:      Nirav Patel
Copyright:    Copyright (c) 2011-2017, Nirav Patel <nrp@eclecti.cc>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_ttf.h"

#include "sosg_image.h"
#ifdef USE_SOSG_VIDEO
    #include "sosg_video.h"
#endif /* USE_SOSG_VIDEO */
#include "sosg_predict.h"
#include "sosg_tracker.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // TODO: use the windows equivalent when on windows
#include <math.h>

#define TICK_INTERVAL 33
#define ROTATION_INTERVAL M_PI/(120.0*(1000.0/TICK_INTERVAL))
#define ROTATION_CONSTANT (float)30.5*ROTATION_INTERVAL
#define CLOSE_ENOUGH(a, b) (fabs(a - b) < ROTATION_INTERVAL/2)

enum sosg_mode {
    SOSG_IMAGES,
    SOSG_VIDEO,
    SOSG_PREDICT
};

typedef struct sosg_struct {
    int w;
    int h;
    int fullscreen;
    int mirror;
    int texres[2];
    float ratio;
    float radius;
    float height;
    float center[2];
    float rotation;
    float drotation;
    uint32_t time;
    int index;
    int mode;
    // TODO: use function pointers for different sources
    union {
        sosg_image_p images;
#ifdef USE_SOSG_VIDEO
        sosg_video_p video;
#endif /* USE_SOSG_VIDEO */
        sosg_predict_p predict;
    } source;
    sosg_tracker_p tracker;
    uint32_t display;
    SDL_Window *window;
    SDL_Surface *screen;
    SDL_Surface *text;
    SDL_GLContext glcontext;
    GLuint texture;
    GLuint program;
    GLuint vertex;
    GLuint fragment;
    GLuint lrotation;
    GLuint ltexres;
} sosg_t, *sosg_p;

static void load_texture(sosg_p data, SDL_Surface *surface)
{
    // Bind the texture object
    glBindTexture(GL_TEXTURE_2D, data->texture);
    
    // Edit the texture object's image data using the information SDL_Surface gives us
    glTexImage2D(GL_TEXTURE_2D, 0, 4, surface->w, surface->h, 0, 
                  GL_BGRA, GL_UNSIGNED_BYTE, surface->pixels);
}

static char *load_file(char *filename)
{
	char *buf = NULL;
    FILE *fp;
    int len;

	if(!(fp = fopen(filename, "r"))) {
		fprintf(stderr, "Error: Failed to open shader: %s\n", filename);
		return NULL;
	}
	
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buf = malloc(len + 1);

	len = fread(buf, 1, len, fp);
	buf[len] = '\0';
	fclose(fp);
	
	return buf;
}

static int load_shaders(sosg_p data)
{
    char *vbuf, *fbuf;
    
    vbuf = load_file("sosg.vert");
    if (vbuf) {
        fbuf = load_file("sosg.frag");
        if (!fbuf) {
            free(vbuf);
            return 1;
        }
    } else {
        return 1;
    }
    
    data->vertex = glCreateShader(GL_VERTEX_SHADER);
    data->fragment = glCreateShader(GL_FRAGMENT_SHADER);
    
    glShaderSource(data->vertex, 1, (const GLchar **)&vbuf, NULL);
    glShaderSource(data->fragment, 1, (const GLchar **)&fbuf, NULL);
    
    free(vbuf);
    free(fbuf);
    
    glCompileShader(data->vertex);
    glCompileShader(data->fragment);
    
    data->program = glCreateProgram();
    glAttachShader(data->program, data->vertex);
    glAttachShader(data->program, data->fragment);
    glLinkProgram(data->program);
    glUseProgram(data->program);
    
    // Set the uniforms the fragment shader will need
    GLint loc = glGetUniformLocation(data->program, "radius");
    glUniform1f(loc, data->radius);
    loc = glGetUniformLocation(data->program, "height");
    glUniform1f(loc, data->height/data->radius);
    loc = glGetUniformLocation(data->program, "center");
    glUniform2f(loc, data->center[0], data->center[1]);
    loc = glGetUniformLocation(data->program, "ratio");
    glUniform1f(loc, data->ratio);
    data->ltexres = glGetUniformLocation(data->program, "texres");
    glUniform2f(data->ltexres, 1.0/(float)data->texres[0], 1.0/(float)data->texres[1]);
    data->lrotation = glGetUniformLocation(data->program, "rotation");
    
    return 0;
}

static void setup_overlay(sosg_p data, char *text)
{
    TTF_Init();
    // TODO: pick a new font
    // TODO: make the size dynamic
    TTF_Font *font = TTF_OpenFont("orbitron-black.otf", 116);
    
    SDL_Color color = {255, 255, 255};
    data->text = TTF_RenderText_Blended(font, text, color);
    
    TTF_CloseFont(font);
    TTF_Quit();
}

static int setup(sosg_p data)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error: Unable to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    
    data->time = SDL_GetTicks();

    // Have the cursor hidden and stuck inside the window
    SDL_ShowCursor(SDL_DISABLE);
    if (SDL_SetRelativeMouseMode(1)) {
        fprintf(stderr, "Warning: Unable to capture mouse: %s\n", SDL_GetError());
    }

    uint32_t flags = SDL_WINDOW_OPENGL;

    uint32_t num_displays = SDL_GetNumVideoDisplays();
    if (data->display >= num_displays) {
        fprintf(stderr, "Error: Selected display index %d. %d displays available.\n", data->display, num_displays);
        SDL_Quit();
        return 1;
    }

    if (data->fullscreen) {
        data->window = SDL_CreateWindow("Science on a Snow Globe", SDL_WINDOWPOS_UNDEFINED_DISPLAY(data->display),
                                        SDL_WINDOWPOS_UNDEFINED_DISPLAY(data->display), 0, 0,
                                        flags | SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (data->window) SDL_GetWindowSize(data->window, &data->w, &data->h);
    } else {
        data->window = SDL_CreateWindow("Science on a Snow Globe",  SDL_WINDOWPOS_CENTERED_DISPLAY(data->display),
                                         SDL_WINDOWPOS_CENTERED_DISPLAY(data->display), data->w, data->h, flags);
    }

    if (!data->window) {
		fprintf(stderr, "Error: Unable to create window: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

    data->glcontext = SDL_GL_CreateContext(data->window);
    if (!data->glcontext) {
        fprintf(stderr, "Error: Unable to create GLContext: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
	
    // Set the OpenGL state after creating the context with SDL_SetVideoMode
	glClearColor(0, 0, 0, 0);
	glEnable(GL_TEXTURE_2D); // Need this to display a texture
    glViewport(0, 0, data->w, data->h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, data->w, data->h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Have OpenGL generate a texture object handle for us
    glGenTextures(1, &data->texture);
    
    // Bind the texture object
    glBindTexture(GL_TEXTURE_2D, data->texture);
    
    // Set the texture's stretching properties
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    return 0;
}

static void update_timer(sosg_p data)
{
    uint32_t now = SDL_GetTicks();

    if (data->time > now) {
        SDL_Delay(data->time - now);
    }

    while (data->time <= now) {
        data->time += TICK_INTERVAL;
    }
}

static void update_index(sosg_p data)
{
    switch (data->mode) {
        case SOSG_IMAGES:
            sosg_image_set_index(data->source.images, data->index);
            // The resolution can change between images, so update the shader
            sosg_image_get_resolution(data->source.images, data->texres);
            glUniform2f(data->ltexres, 1.0/(float)data->texres[0], 1.0/(float)data->texres[1]);
            break;
#ifdef USE_SOSG_VIDEO
        case SOSG_VIDEO:
            sosg_video_set_index(data->source.video, data->index);
            // Video resolution is currently fixed, but this may change in the future
            break;
#endif /* USE_SOSG_VIDEO */
        case SOSG_PREDICT:
            break;
    }
}

static int handle_events(sosg_p data)
{
    SDL_Event event;
    
    // TODO: handle key repeat interval again
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return -1;
                    case SDLK_LEFT:
                        if (event.key.keysym.mod & KMOD_SHIFT)
                            data->drotation += ROTATION_INTERVAL;
                        else
                            data->drotation = ROTATION_CONSTANT;
                        break;
                    case SDLK_RIGHT:
                        if (event.key.keysym.mod & KMOD_SHIFT)
                            data->drotation -= ROTATION_INTERVAL;
                        else
                            data->drotation = -ROTATION_CONSTANT;
                        break;
                    case SDLK_UP:
                        data->index++;
                        update_index(data);
                        break;
                    case SDLK_DOWN:
                        data->index--;
                        update_index(data);
                        break;
                    case SDLK_p:
                        data->drotation = 0.0;
                        break;
                    case SDLK_r:
                        data->rotation = M_PI;
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYUP:
                // On key up, only if we had ROTATION_CONSTANT going, stop the rotation
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:
                        if (CLOSE_ENOUGH(data->drotation,ROTATION_CONSTANT))
                            data->drotation = 0.0;
                        break;
                    case SDLK_RIGHT:
                        if (CLOSE_ENOUGH(data->drotation,-ROTATION_CONSTANT))
                            data->drotation = 0.0;
                        break;
                    default:
                        break;
                }
                break;
            case SDL_MOUSEWHEEL:
                data->index += event.wheel.y;
                update_index(data);
                break;
            case SDL_MOUSEMOTION:
                data->rotation -= (float)event.motion.xrel/(M_PI*50.0f);
                break;
            case SDL_QUIT:
                return -1;
            default:
                break;
        }
    }
    
    return 0;
}

static void update_media(sosg_p data)
{
    SDL_Surface *surface = NULL;

    switch (data->mode) {
        case SOSG_IMAGES:
            surface = sosg_image_update(data->source.images);
            break;
#ifdef USE_SOSG_VIDEO
        case SOSG_VIDEO:
            surface = sosg_video_update(data->source.video);
            break;
#endif /* USE_SOSG_VIDEO */
        case SOSG_PREDICT:
            surface = sosg_predict_update(data->source.predict);
            break;
    }

    if (surface) {
        if (data->text) {
            SDL_Rect pos;
            pos.x = 0;
            // Center the text vertically
            pos.y = surface->h/2-data->text->h/2;
            SDL_BlitSurface(data->text, NULL, surface, &pos);
        }
    
        // TODO: Support arbitrary resolution images
        // Check that the image's dimensions are a power of 2
        if ((surface->w & (surface->w - 1)) != 0 ||
            (surface->h & (surface->h - 1)) != 0) {
            fprintf(stderr, "Warning: dimensions (%d, %d) not a power of 2\n",
                surface->w, surface->h);
        }
    
        load_texture(data, surface);
    }
}

static void update_display(sosg_p data)
{
    glUniform1f(data->lrotation, data->rotation);

    // Clear the screen before drawing
	glClear(GL_COLOR_BUFFER_BIT);
    
    // Bind the texture to which subsequent calls refer to
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, data->texture);

    // Just make a full screen quad, a canvas for the shader to draw on
    glBegin(GL_QUADS);
        glTexCoord2i(data->mirror, 0);
        glVertex3f(0, 0, 0);
    
        glTexCoord2i(!data->mirror, 0);
        glVertex3f(data->w, 0, 0);
    
        glTexCoord2i(!data->mirror, 1);
        glVertex3f(data->w, data->h, 0);
    
        glTexCoord2i(data->mirror, 1);
        glVertex3f(0, data->h, 0);
    glEnd();
	
    SDL_GL_SwapWindow(data->window);
}

static void update_input(sosg_p data)
{
    if (data->tracker) {
        float rotation = data->rotation;
        int mode;
        sosg_tracker_get_rotation(data->tracker, &rotation, &mode);
        if (mode == TRACKER_ROTATE)
            data->rotation = -rotation;
        else if (mode == TRACKER_SCROLL) {
            data->index = rotation / (M_PI/3.0);
            update_index(data);
        }
    } else {
        data->rotation += data->drotation;
    }
}

static void usage(sosg_p data)
{
    printf("Usage: sosg [OPTION] [FILES]\n\n");
    printf("sosg is  a simple viewer for NOAA Science on a Sphere datasets\n");
    printf("on Snow Globe, a low cost, open source, DIY spherical display.\n");
    printf("SOS Datasets available at: http://sos.noaa.gov\n");
    printf("Snow Globe information at: http://eclecti.cc\n\n");
    printf("    Input Data\n");
    printf("        -i     Display an image or slideshow (Default)\n");
#ifdef USE_SOSG_VIDEO
    printf("        -v     Display a video or videos\n");
#endif /* USE_SOSG_VIDEO */
    printf("        -p     Satellite tracking as a PREDICT client\n");
    printf("        -s     Optional string to overlay\n\n");
    printf("    Snow Globe Configuration\n");
    printf("        -f     Fullscreen\n");
    printf("        -m     Mirror horizontally\n");
    printf("        -d     Display number to use (%d)\n", data->display);
    printf("        -w     Window width in pixels (%d)\n", data->w);
    printf("        -h     Window height in pixels (%d)\n", data->h);
    printf("        -a     Display aspect ratio (%.3f)\n", data->ratio);
    printf("        -r     Radius in ratio to height (%.3f)\n", data->radius);
    printf("        -x     X offset ratio to width (%.3f)\n", data->center[0]);
    printf("        -y     Y offset ratio to height (%.3f)\n", data->center[1]);
    printf("        -o     Lens offset ratio to height (%.3f)\n\n", data->height);
    printf("    Adjacent Reality Tracker (optional)\n");
    printf("        -t     Path to the Tracker device\n\n");
    printf("The left and right arrow keys can be used to rotate the sphere.\n");
    printf("Holding shift while using the arrows changes rotation speed.\n");
    printf("p will stop the rotation and r resets the angle.\n");
    printf("The up and down arrow keys go to the previous or next image in image mode.\n\n");
}

static void cleanup(sosg_p data)
{
    switch (data->mode) {
        case SOSG_IMAGES:
            sosg_image_destroy(data->source.images);
            break;
#ifdef USE_SOSG_VIDEO
        case SOSG_VIDEO:
            sosg_video_destroy(data->source.video);
            break;
#endif /* USE_SOSG_VIDEO */
        case SOSG_PREDICT:
            sosg_predict_destroy(data->source.predict);
            break;
    }
    
    // Now we can delete the OpenGL texture and close down SDL
    glDeleteTextures(1, &data->texture);
    if (data->text) SDL_FreeSurface(data->text);

    if (data->glcontext) SDL_GL_DeleteContext(data->glcontext);
    SDL_Quit();
}

int main(int argc, char *argv[])
{
    int c;
    char *filename = NULL;
    
    sosg_p data = calloc(1, sizeof(sosg_t));
    if (!data) {
        fprintf(stderr, "Error: Could not allocate data\n");
        return 1;
    }
    
    // Defaults are for my Snow Globe (not the only Snow Globe anymore!)
    data->w = 848;
    data->h = 480;
    data->ratio = (float)data->w/(float)data->h;
    data->radius = 378.0/(float)data->h;
    data->height = 370.0/(float)data->h;
    data->center[0] = 431.0/(float)data->w;
    data->center[1] = 210.0/(float)data->h;
    data->rotation = M_PI;
    
    while ((c = getopt(argc, argv, "ivpfma:d:s:w:h:g:r:x:y:o:t:")) != -1) {
        switch (c) {
            case 'i':
                data->mode = SOSG_IMAGES;
                break;
#ifdef USE_SOSG_VIDEO
            case 'v':
                data->mode = SOSG_VIDEO;
                break;
#endif /* USE_SOSG_VIDEO */
            case 'p':
                data->mode = SOSG_PREDICT;
                break;
            case 'f':
                data->fullscreen = 1;
                break;
            case 'm':
                data->mirror = 1;
                break;
            case 'd':
                data->display = atoi(optarg);
                break;
            case 's':
                setup_overlay(data, optarg);
                break;
            case 'w':
                data->w = atoi(optarg);
                break;
            case 'h':
                data->h = atoi(optarg);
                break;
            case 'a':
                data->ratio = atof(optarg);
                break;
            case 'r':
                data->radius = atof(optarg);
                break;
            case 'x':
                data->center[0] = atof(optarg);
                break;
            case 'y':
                data->center[1] = atof(optarg);
                break;
            case 'o':
                data->height = atof(optarg);
                break;
            case 't':
                data->tracker = sosg_tracker_init(optarg);
                if (!data->tracker)
                    return 1;
                break;
            case '?':
            default:
                usage(data);
                fprintf(stderr, "Error: Failed at option %c\n", optopt);
                return 1;
        }
    }
    
    if (optind >= argc) {
        usage(data);
        fprintf(stderr, "Error: Missing filename or path.\n");
        return 1;
    }
    
    // Pick the last non-option arg as the filename to use
    filename = argv[argc-1];
    
    if (setup(data)) {
        cleanup(data);
        return 1;
    }
    
    switch (data->mode) {
        case SOSG_IMAGES:
            // The remaining args are assumed to be filenames.  getopt
            // reorders the argv to put non option args at the end on all 
            // platforms I know of, but it is not the POSIX standard to do so.
            data->source.images = sosg_image_init(argc-optind, argv+optind);
            sosg_image_get_resolution(data->source.images, data->texres);
            break;
#ifdef USE_SOSG_VIDEO
        case SOSG_VIDEO:
            data->source.video = sosg_video_init(argc-optind, argv+optind);
            sosg_video_get_resolution(data->source.video, data->texres);
            break;
#endif /* USE_SOSG_VIDEO */
        case SOSG_PREDICT:
            data->source.predict = sosg_predict_init(filename);
            sosg_predict_get_resolution(data->source.predict, data->texres);
            break;
    }
    
    if (load_shaders(data)) {
        cleanup(data);
        return 1;
    }
    
    while (handle_events(data) != -1) {
        update_media(data);
        update_display(data);
        update_timer(data);
        update_input(data);
    }
    
    cleanup(data);
	return 0;
}
