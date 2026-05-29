#ifndef MAIN_H
#define MAIN_H

#include <stddef.h>
#include <fontconfig/fontconfig.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "st.h"

#define MAXGLYPHS 1114112

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	#define RMASK (0xff000000)
	#define GMASK (0x00ff0000)
	#define BMASK (0x0000ff00)
	#define AMASK (0x000000ff)
#else
	#define RMASK (0x000000ff)
	#define GMASK (0x0000ff00)
	#define BMASK (0x00ff0000)
	#define AMASK (0xff00000)
#endif

#define TRUERED(x)		(((x) & 0xff0000) >> 16)
#define TRUEGREEN(x)	(((x) & 0x00ff00) >> 8)
#define TRUEBLUE(x)		(((x) & 0x0000ff) >> 0)

typedef unsigned int Color;

typedef struct {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char alpha;
} RenderColor;

typedef struct {
	SDL_Window *wnd;
	SDL_Renderer *rnd;
	SDL_Texture *txt_glyphs;
	SDL_Texture *txt_background;
	int drawing;
	int updated;
	int should_draw;
	int w, h; /* window width and height */
	int cw, ch; /* char width and height */
	int tw, th; /* tty width and height */
	int mode; /* window state/mode flags */
	int cursor; /* cursor style */
	unsigned int lastfocus;
	int ttyfd;
	Glyph *glyphs;
} TermWindow;

struct FontSetStruct;

typedef struct {
	int height;
	int width;
	int ascent;
	int descent;
	char *filepath;
	FcPattern *pattern;
	FcPattern *match;
	FcCharSet *charset;
	TTF_Font *ttf;
	SDL_Texture **cache;
	int *cache_widths;
	int *cache_heights;
	int *widths;
	struct FontSetStruct *fontset;
} Font;

typedef struct {
	SDL_Vertex *verts;
	int *idxs;
} Geometry;

typedef struct FontSetStruct {
	Font font, bfont, ifont, ibfont;
	Geometry geo;
	SDL_Texture *atlas;
} FontSet;

typedef struct {
	RenderColor *col;
	int collen;
	FontSet *fontsets;
	int fontsetlen;
} DrawingContext;

typedef struct {
	int curr;
	SDL_Surface **frame;
	int *duration;
	int frames;
} Animation;

enum win_mode {
	MODE_VISIBLE     = 1 << 0,
	MODE_FOCUSED     = 1 << 1,
	MODE_APPKEYPAD   = 1 << 2,
	MODE_MOUSEBTN    = 1 << 3,
	MODE_MOUSEMOTION = 1 << 4,
	MODE_REVERSE     = 1 << 5,
	MODE_KBDLOCK     = 1 << 6,
	MODE_HIDE        = 1 << 7,
	MODE_APPCURSOR   = 1 << 8,
	MODE_MOUSESGR    = 1 << 9,
	MODE_8BIT        = 1 << 10,
	MODE_BLINK       = 1 << 11,
	MODE_FBLINK      = 1 << 12,
	MODE_FOCUS       = 1 << 13,
	MODE_MOUSEX10    = 1 << 14,
	MODE_MOUSEMANY   = 1 << 15,
	MODE_BRCKTPASTE  = 1 << 16,
	MODE_NUMLOCK     = 1 << 17,
	MODE_MOUSE       = MODE_MOUSEBTN|MODE_MOUSEMOTION|MODE_MOUSEX10\
	                  |MODE_MOUSEMANY,
};

typedef struct {
	SDL_Keycode key;
	SDL_Keymod mode;
	char *esc;
	int appkey;
	int appcursor;
} Key;

void bell(void);
void clipcopy(const Arg *dummy);
void drawcursor(int, int, Glyph, int, int, Glyph);
void drawline(Line, int, int, int);
void finishdraw(void);
void loadcols(void);
int setcolorname(int, const char *);
extern const RenderColor colorname[];
void settitle(char *);
int setcursor(int);
void settermmode(int, unsigned int);
void setpointermotion(int);
void setsel(char *);
int startdraw(void);
int getglyphwidth(Rune u);

extern void initanim(char *);
extern int animate();

#endif
