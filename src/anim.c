#include <time.h>
#include <stdio.h>

#include <SDL.h>
#include <SDL_thread.h>

#include "main.h"

#include "gifdec/gifdec.h"
#include "gifdec/gifdec.c"

extern Animation anim;
extern TermWindow win;

static SDL_Surface *tmpsrf;
static gd_GIF *gif;
static unsigned char *tmppixels;
static int decoded = 0;
static struct timespec last;
static SDL_Thread *thrd;

static
void
decodepixels()
{
	unsigned char *color = tmppixels;
	void *addr;
	unsigned int pixel;

	for (int i=0; i<gif->height; i++) {
		for (int j=0; j<gif->width; j++) {
				int alpha = 255;
				if (gif->gce.transparency && gd_is_bgcolor(gif, color))
					alpha = 0;
				pixel = SDL_MapRGBA(tmpsrf->format, color[0], color[1], color[2], alpha);
				addr = (char *)tmpsrf->pixels + (i * tmpsrf->pitch + j * sizeof(pixel));
				memcpy(addr, &pixel, sizeof(pixel));
				color += 3;
		}
	}
}


static
int
decodeframe()
{
	if (decoded || gd_get_frame(gif) <= 0) {
		decoded = 1;
		return 0;
	}

	int newframes = anim.frames + 1;

	gd_render_frame(gif, tmppixels);
	decodepixels();

	anim.duration = realloc(anim.duration, sizeof(int) * (newframes));
	anim.frame = realloc(anim.frame, sizeof(SDL_Surface*) * (newframes));

	anim.duration[anim.frames] = gif->gce.delay;
	anim.frame[anim.frames] = SDL_ConvertSurface(tmpsrf, tmpsrf->format, tmpsrf->flags);

	anim.frames = newframes;

	return 1;

}

static
int
decodeanimation()
{
	#ifdef DEBUG
	printf("starting animation decoding thread\n");
	#endif

	while (decodeframe());

	free(tmppixels);
	SDL_FreeSurface(tmpsrf);

	#ifdef DEBUG
	printf("finishing animation decoding thread\n");
	#endif

	return 0;
}

int firstRendered = 0;

int
animate()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	// frame not loaded yet
	if (!anim.duration[anim.curr]) return 0;

	if (!firstRendered) {
		firstRendered = 1;
		last = now;
		return 1;
	}

	// is it time to advance frame
	unsigned long dur = MAX(100/30, anim.duration[anim.curr]);
	unsigned long sd = (now.tv_sec - last.tv_sec) * 100;
	unsigned long nsd = (now.tv_nsec - last.tv_nsec) / 1e7;
	if (sd + nsd < dur) return 0;

	last = now;

	int updated = 0;
	int available = anim.curr+1 < anim.frames;

	if (available) {
		anim.curr++;
		updated = 1;
	}
	else if (decoded) {
		anim.curr = 0;
		updated = 1;
	}

	#ifdef DEBUG
	printf("next frame %d/%d, decoded %d\n", anim.curr, anim.frames, decoded);
	#endif

	return updated;
}

void
initanim(char *filename)
{
	gif = gd_open_gif(filename);
	assert(gif);
	tmppixels = malloc(gif->width * gif->height * 3);
	tmpsrf = SDL_CreateRGBSurface(0, gif->width, gif->height, 32, 0, 0, 0, 0);

	#ifdef DEBUG
	printf("loaded gif %s\n", filename);
	printf("canvas size: %ux%u\n", gif->width, gif->height);
	printf("number of colors: %d\n", gif->palette->size);
	#endif

	// decode first frame sync
	decodeframe();

	thrd = SDL_CreateThread(decodeanimation, "decodeanimation", filename);
}
