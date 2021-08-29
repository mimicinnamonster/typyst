#include <time.h>
#include <malloc.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL2_rotozoom.h>
#include "gifdec/gifdec.h"
#include "gifdec/gifdec.c"

typedef struct {
	struct timespec last;
	int curr;
	SDL_Texture **frame;
	int *duration;
	int frames;

} Animation;

Animation anim;

void
convert(gd_GIF *gif, unsigned char *gifpixels, SDL_Surface *frame)
{
	unsigned char *color = gifpixels;
	void *addr;
	unsigned int pixel;

	for (int i=0; i<gif->height; i++) {
		for (int j=0; j<gif->width; j++) {
				if (!gd_is_bgcolor(gif, color))
						pixel = SDL_MapRGB(frame->format, color[0], color[1], color[2]);
				else if (((i >> 2) + (j >> 2)) & 1)
						pixel = SDL_MapRGB(frame->format, 0x7F, 0x7F, 0x7F);
				else
						pixel = SDL_MapRGB(frame->format, 0x00, 0x00, 0x00);
				addr = frame->pixels + (i * frame->pitch + j * sizeof(pixel));
				memcpy(addr, &pixel, sizeof(pixel));
				color += 3;
		}
	}
}

void
initanim(char *filename)
{
	gd_GIF *gif = gd_open_gif(filename);

	#ifdef DEBUG
	printf("loaded gif %s\n", filename);
	printf("  canvas size: %ux%u\n", gif->width, gif->height);
	printf("  number of colors: %d\n", gif->palette->size);
	#endif
	
	unsigned char *gifpixels = malloc(gif->width * gif->height * 3);

	SDL_Surface *srf = SDL_CreateRGBSurface(0, gif->width, gif->height, 32, 0, 0, 0, 0);

	while (gd_get_frame(gif)) {
		++anim.frames;

		gd_render_frame(gif, gifpixels);
		convert(gif, gifpixels, srf);

		anim.duration = realloc(anim.duration, sizeof(SDL_Surface*) * anim.frames);
		anim.frame = realloc(anim.frame, sizeof(SDL_Surface*) * anim.frames);

		anim.frame[anim.frames-1] = SDL_CreateTextureFromSurface(win.rnd, srf);
		anim.duration[anim.frames-1] = gif->gce.delay;
	}

	SDL_FreeSurface(srf);

	#ifdef DEBUG
	printf("  converted frames: %d\n", anim.frames);
	#endif
}

void
animate()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	anim.curr++;
	if (anim.curr >= anim.frames)
		anim.curr = 0;
}
