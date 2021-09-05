#include <time.h>
#include <malloc.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL2_rotozoom.h>

#include "main.h"

#include "gifdec/gifdec.h"
#include "gifdec/gifdec.c"

extern Animation anim;
extern TermWindow win;

SDL_Surface *tmpsrf;
gd_GIF *gif;
unsigned char *tmppixels;
int decoded = 0;
struct timespec last;

void
convert()
{
	unsigned char *color = tmppixels;
	void *addr;
	unsigned int pixel;

	for (int i=0; i<gif->height; i++) {
		for (int j=0; j<gif->width; j++) {
				if (!gd_is_bgcolor(gif, color))
						pixel = SDL_MapRGB(tmpsrf->format, color[0], color[1], color[2]);
				else if (((i >> 2) + (j >> 2)) & 1)
						pixel = SDL_MapRGB(tmpsrf->format, 0x7F, 0x7F, 0x7F);
				else
						pixel = SDL_MapRGB(tmpsrf->format, 0x00, 0x00, 0x00);
				addr = tmpsrf->pixels + (i * tmpsrf->pitch + j * sizeof(pixel));
				memcpy(addr, &pixel, sizeof(pixel));
				color += 3;
		}
	}
}

void
initanim(char *filename)
{
	gif = gd_open_gif(filename);
	tmppixels = malloc(gif->width * gif->height * 3);
	tmpsrf = SDL_CreateRGBSurface(0, gif->width, gif->height, 32, 0, 0, 0, 0);

	#ifdef DEBUG
	printf("loaded gif %s\n", filename);
	printf("canvas size: %ux%u\n", gif->width, gif->height);
	printf("number of colors: %d\n", gif->palette->size);
	#endif
}

void
decodeframe()
{
	if (decoded || gd_get_frame(gif) <= 0) {
		decoded = 1;
		return;
	}

	++anim.frames;

	gd_render_frame(gif, tmppixels);
	convert();

	anim.duration = realloc(anim.duration, sizeof(SDL_Surface*) * anim.frames);
	anim.frame = realloc(anim.frame, sizeof(SDL_Surface*) * anim.frames);

	anim.frame[anim.frames-1] = SDL_CreateTextureFromSurface(win.rnd, tmpsrf);
	anim.duration[anim.frames-1] = gif->gce.delay;

	#ifdef DEBUG
	printf("converted gif frames: %d\n", anim.frames);
	#endif

}

void
animate()
{
	decodeframe();

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	unsigned long durr = anim.duration[anim.curr];
	unsigned long sd = (now.tv_sec - last.tv_sec) * 100;
	unsigned long nsd = (now.tv_nsec - last.tv_nsec) / 1e7;

	if (sd + nsd < durr)
		return;

	last = now;
	anim.curr++;

	if (anim.curr >= anim.frames)
		anim.curr = 0;
}
