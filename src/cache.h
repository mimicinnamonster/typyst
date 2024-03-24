#ifndef CACHE_H
#define CACHE_H

#include <SDL.h>
#include "st.h"

#define CACHE_MAX ((unsigned short)512)

#define CACHE_WIDE			(1 << 0)
#define CACHE_NORMAL		(1 << 1)
#define CACHE_BOLD			(1 << 2)
#define CACHE_ITALIC		(1 << 3)
#define CACHE_BOLDITALIC	(1 << 4)

typedef struct {
	char mode;
	int idx;
} GlyphCacheItem;

typedef struct {
	SDL_Renderer *rnd;
	SDL_Texture *txt;
	GlyphCacheItem *items;
	unsigned int *lru; // LRU queue / circular buffer of glyphs
	int head; // current head position of the LRU
	int gw; // glyph width
	int gh; // glyph height
} GlyphCache;

SDL_Texture *cache_init(SDL_Renderer *rnd, int glyph_width, int glyph_height);
int cache_get(Glyph g);
int cache_set(Glyph g, SDL_Surface *src);
void save_texture(const char *filename, SDL_Renderer *ren, SDL_Texture *tex);

#endif
