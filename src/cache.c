#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <SDL.h>
#include "cache.h"
#include "main.h"
#include "st.h"

GlyphCache gc;

void save_surface(const char *filename, SDL_Surface *sur) {
    assert(!SDL_SaveBMP(sur, filename));
}

void save_texture(const char *filename, SDL_Renderer *ren, SDL_Texture *tex) {
    SDL_Texture *ren_tex;
    SDL_Surface *sur;
    int st=0, w=0, h=0, format=SDL_PIXELFORMAT_RGBA32;
    void *pixels;

    assert(!SDL_QueryTexture(tex, NULL, NULL, &w, &h));
    assert(ren_tex = SDL_CreateTexture(ren, format, SDL_TEXTUREACCESS_TARGET, w, h));
    assert(!(st = SDL_SetRenderTarget(ren, ren_tex)));

    SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(ren);

    assert(!(st = SDL_RenderCopy(ren, tex, NULL, NULL)));
    assert(pixels = malloc(w * h * SDL_BYTESPERPIXEL(format)));
    assert(!(st = SDL_RenderReadPixels(ren, NULL, format, pixels, w * SDL_BYTESPERPIXEL(format))));
    assert(sur = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, SDL_BITSPERPIXEL(format), w * SDL_BYTESPERPIXEL(format), format));

    assert(!(st = SDL_SaveBMP(sur, filename)));

    SDL_FreeSurface(sur);
    free(pixels);
    SDL_DestroyTexture(ren_tex);
}

SDL_Texture *cache_init(SDL_Renderer *rnd, int glyph_width, int glyph_height) {
	gc.rnd = rnd;
	gc.gw = glyph_width;
	gc.gh = glyph_height;

	/* Determine max cache slots based on GPU max texture size */
	SDL_RendererInfo info;
	SDL_GetRendererInfo(rnd, &info);
	int max_tex_w = info.max_texture_width;
	int slots_by_width = max_tex_w / (2 * gc.gw);
	gc.max = slots_by_width;
	if (gc.max > CACHE_MAX_MAX) gc.max = CACHE_MAX_MAX;
	if (gc.max < CACHE_MAX_MIN) gc.max = CACHE_MAX_MIN;

	gc.head = gc.max - 1;

	if (gc.txt) {
		SDL_DestroyTexture(gc.txt);
	}

	int tex_w = gc.max * 2 * gc.gw;
	int tex_h = 4 * gc.gh;

#ifdef DEBUG
	printf("cache_init: max=%d, tex=%dx%d (GPU max tex width: %d)\n", gc.max, tex_w, tex_h, max_tex_w);
#endif

	assert(gc.txt = SDL_CreateTexture(gc.rnd, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, tex_w, tex_h));
	SDL_SetTextureBlendMode(gc.txt, SDL_BLENDMODE_BLEND);

	if (gc.items) {
		free(gc.items);
	}

	assert(gc.items = calloc(MAXGLYPHS+1, sizeof(*gc.items)));
	memset(gc.items, 0, (MAXGLYPHS+1) * sizeof(*gc.items));

	if (gc.lru) {
		free(gc.lru);
	}

	assert(gc.lru = calloc(gc.max, sizeof(*gc.lru)));
	memset(gc.lru, 0, gc.max * sizeof(*gc.lru));

	#ifdef DEBUG
	printf("glyph cache initialized. Hashmap: %d, queue: %d\n", MAXGLYPHS, gc.max);
	#endif

	return gc.txt;
}

char cache_get_mode(Glyph g) {
	char mode = 0;

	if (g.mode & CACHE_ITALIC && g.mode & CACHE_BOLD) {
		mode = CACHE_BOLDITALIC;
	} else if (g.mode & ATTR_ITALIC) {
		mode = CACHE_ITALIC;
	} else if (g.mode & ATTR_BOLD) {
		mode = CACHE_BOLD;
	} else {
		mode = CACHE_NORMAL;
	}

	if (g.mode & ATTR_WIDE) {
		mode |= CACHE_WIDE;
	}

	return mode;
}

SDL_Rect cache_rect(Glyph g) {
	assert(g.u <= MAXGLYPHS);

	char mode = cache_get_mode(g);
	int pos = gc.items[g.u].idx;

	SDL_Rect r = (SDL_Rect) {
		.x = (gc.gw * 2) * pos,
		.y = gc.gh,
		.w = ((mode & CACHE_WIDE) ? 2 : 1) * gc.gw,
		.h = gc.gh,
	};

	if (mode & CACHE_BOLDITALIC) {
		r.y *= 3;
	} else if (mode & CACHE_ITALIC) {
		r.y *= 2;
	} else if (mode & CACHE_BOLD) {
		r.y *= 1;
	} else {
		r.y *= 0;
	}

	return r;
}

int cache_get(Glyph g) {
	assert(g.u <= MAXGLYPHS);

	char mode = cache_get_mode(g);

	
	if ((gc.items[g.u].mode & mode) != mode) {
		return -1;
	}

	return gc.items[g.u].idx;
}

int cache_set(Glyph g, SDL_Surface *src) {
	assert(g.u <= MAXGLYPHS);

	int next_head = (gc.head+1) % gc.max;

	if (!gc.items[g.u].mode) {
		int prev = gc.lru[next_head];
		gc.items[prev].mode = 0;
		gc.items[prev].idx = 0;

		gc.head = next_head;
		gc.lru[gc.head] = g.u;
		gc.items[g.u].idx = gc.head;
	}

	char mode = cache_get_mode(g);
	gc.items[g.u].mode |= mode;

	SDL_Rect r = cache_rect(g);
	r.x += ((gc.gw * ((mode & CACHE_WIDE) ? 2 : 1)) - src->clip_rect.w) / 2;
	r.y += (gc.gh - src->clip_rect.h)/2;
	r.h = MIN(r.h, src->clip_rect.h);
	r.w = MIN(r.w, src->clip_rect.w);

	SDL_UpdateTexture(gc.txt, &r, src->pixels, src->pitch);

	/*
	//int counter;
	char filename[50] = {0};
	sprintf(filename, "dump_%d_%d.bmp", counter++, g.u);
	save_texture(filename, gc.rnd, gc.txt);
	*/

	return gc.items[g.u].idx;
}
