#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <wchar.h>

#include <fontconfig/fontconfig.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_thread.h>
#include <SDL2/SDL2_rotozoom.h>


#include "st.h"
#include "arg.h"
#include "main.h"
#include "config.h"
#include "cache.h"

inline ushort sixd_to_16bit(int);
void drawglyph(Glyph, int, int);
void init(void);
void resize(int, int);
int loadcolor(int, const char *, RenderColor *);
int loadfont(Font *, FcPattern *);
int loadfontset(FcPattern *pattern);
void init_geometry(Geometry *geo);
int read_events(void);
void unloadfont(Font *f);
void resizefont(void);

#define FONTATLASSIZE 256
#define FONTCACHESIZE (1 << 16)

static char **opt_cmd = 0;
static char *opt_io	= 0;
static char *opt_line = 0;
static char *opt_anim = 0;
static int opt_fullscreen = 1;
static unsigned int opt_fps = 60;
static unsigned int tty_event_type;

TermWindow win;
Animation anim;
DrawingContext dc;
double usedfontsize = 18;
SDL_Texture **tx_anim;
int tx_anim_len;

SDL_mutex *mutex;
SDL_Texture *glyphcache = 0;
Geometry geo;

#define IS_SET(flag)	((win.mode & (flag)) != 0)

void bell(void)
{
	// TODO: bell
}

void
settermmode(int set, unsigned int flags)
{
	MODBIT(win.mode, set, flags);
	/* TODO: redraw
	int mode = win.mode;
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
		redraw();
	*/
}


void
resize(int width, int height)
{
	win.w = width;
	win.h = height;

	cols = MAX(1, (win.w-winpad*2) / win.cw);
	rows = MAX(1, (win.h-winpad*2) / win.ch);

	win.tw = cols * win.cw;
	win.th = rows * win.ch;

	#ifdef DEBUG
	printf("width: %d, height: %d, win.cw: %d, win.ch: %d, cols: %d, rows: %d\n", width, height, win.cw, win.ch, cols, rows);
	#endif

	init_geometry(&geo);

	//SDL_PIXELFORMAT_BGRA32
	// TODO: destroy old textures
	win.txt_glyphs = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_TARGET, win.tw, win.th);
	win.txt_background = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_TARGET, win.tw, win.th);
	SDL_SetTextureBlendMode(win.txt_glyphs, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(win.txt_background, SDL_BLENDMODE_BLEND);

	int newlen = cols*rows;

	win.glyphs = realloc(win.glyphs, newlen*sizeof(Glyph));
	memset(win.glyphs, 0, newlen * sizeof(Glyph));

	tresize(cols, rows);
	ttyresize(cols, rows);

	redraw();
	win.should_draw = 1;
}

ushort
sixd_to_16bit(int x)
{
	return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

int
loadcolor(int i, const char *name, RenderColor *color)
{
	*color = (RenderColor){ .alpha = 0xff, .red = 0xff, .green = 0xff, .blue = 0xff };

	if (!name) {
		if (BETWEEN(i, 16, 255)) { /* 256 color */
			if (i < 6*6*6+16) { /* same colors as xterm */
				color->red	 = sixd_to_16bit( ((i-16)/36)%6 );
				color->green = sixd_to_16bit( ((i-16)/6) %6 );
				color->blue	= sixd_to_16bit( ((i-16)/1) %6 );
			} else { /* greyscale */
				color->red = 0x0808 + 0x0a0a * (i - (6*6*6+16));
				color->green = color->blue = color->red;
			}
		} else {
			color->alpha = colorname[i].alpha;
			color->red = colorname[i].red;
			color->green = colorname[i].green;
			color->blue = colorname[i].blue;
		}
	}

	dc.col[defaultbg].alpha = 255 * alpha;
	return 1;
}

void
loadcols(void)
{
	int i;
	static int loaded;

	if (!loaded) {
		dc.collen = MAX(LEN(colorname), 256);
		dc.col = xmalloc(dc.collen * sizeof(Color));
	}

	for (i = 0; i < dc.collen; i++) {
		loadcolor(i, 0, &dc.col[i]);
	}

	loaded = 1;
}

int
setcolorname(int x, const char *name)
{
	#ifdef DEBUG
	printf("setcolorname %d, %s\n", x, name);
	#endif

	RenderColor ncolor;

	if (!BETWEEN(x, 0, dc.collen))
		return 1;

	if (!loadcolor(x, name, &ncolor))
		return 1;

	dc.col[x] = ncolor;

	return 0;
}

int
loadfont(Font *f, FcPattern *pattern)
{
	char *filepath;
	FcResult result;
	int fontindex = 0;

	FcPattern *duplicate = FcPatternDuplicate(pattern);
	f->pattern = duplicate;

	//FcPatternAddInteger(f->pattern, FC_SLANT, FC_SLANT_ROMAN);
	//FcPatternAddInteger(f->pattern, FC_WEIGHT, FC_WEIGHT_MEDIUM);
	//FcConfigSubstitute(0, f->pattern, FcMatchPattern);
	FcDefaultSubstitute(f->pattern);

	f->match = FcFontMatch(0, f->pattern, &result);
	if (result != FcResultMatch) {
		FcPatternDestroy(f->pattern);
		return 0;
	}

	FcPatternGetString(f->match, FC_FILE, 0, (FcChar8**)&filepath);
	FcPatternGetInteger(f->match, FC_INDEX, 0, &fontindex);
	FcPatternGetCharSet(f->match, FC_CHARSET, 0, &f->charset);

	f->filepath = filepath;

	#ifdef DEBUG
	printf("loading font file: %s, font size: %f, index: %d\n", filepath, usedfontsize, fontindex);
	#endif

	f->ttf = TTF_OpenFontIndex(filepath, usedfontsize, fontindex);
	if (!f->ttf) return 0;

	TTF_SetFontHinting(f->ttf, TTF_HINTING_NORMAL);
	TTF_SetFontOutline(f->ttf, 0);

	TTF_GlyphMetrics(f->ttf, 'a', 0, 0, &f->ascent, &f->descent, &f->width);
	f->height = TTF_FontHeight(f->ttf);

	#ifdef DEBUG
	printf("glyph metrics: %dx%d\n", f->width, f->height);
	printf("allocating font cache %ld\n", FONTCACHESIZE * sizeof(SDL_Surface*));
	#endif

	f->widths = calloc(MAXGLYPHS, sizeof(f->widths[0]));
	memset(f->widths, -1, MAXGLYPHS * sizeof(f->widths[0]));

	f->cache = calloc(FONTCACHESIZE, sizeof(SDL_Texture*));
	assert(f->cache);

	f->cache_widths = calloc(FONTCACHESIZE, sizeof(int));
	assert(f->cache_widths);

	f->cache_heights = calloc(FONTCACHESIZE, sizeof(int));
	assert(f->cache_heights);

	return 1;
}

void
unloadfont(Font *f)
{
	//free(f->filepath);
	FcPatternDestroy(f->pattern);
	FcPatternDestroy(f->match);
	//free(f->charset);
	TTF_CloseFont(f->ttf);
	free(f->cache);
	free(f->cache_widths);
	free(f->cache_heights);
	free(f->widths);
}

FcPattern *createfontpattern(const char *fontstr)
{
	FcPattern *pattern = FcNameParse((const FcChar8 *)fontstr);
	FcPatternDel(pattern, FC_PIXEL_SIZE);
	FcValue v = (FcValue){ .type = FcTypeDouble, .u = {.d = usedfontsize }};
	assert(FcPatternAdd(pattern, FC_PIXEL_SIZE, v, 1));
	return pattern;
}

int
loadfontset(FcPattern *pattern)
{
	dc.fontsets = realloc(dc.fontsets, ++dc.fontsetlen * sizeof(FontSet));
	FontSet *fontset = &dc.fontsets[dc.fontsetlen-1];
	*fontset = (FontSet){0};

	#ifdef DEBUG
	printf("number of fontsets: %d\n", dc.fontsetlen);
	#endif

	double fontval;

	if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) == FcResultMatch) {
		usedfontsize = fontval;
	} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) == FcResultMatch) {
		usedfontsize = -1;
	} else {
		assert(0);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12);
		usedfontsize = 12;
	}

	if (!loadfont(&fontset->font, pattern)) {
		dc.fontsetlen--;
		return 0;
	}

	if (usedfontsize < 0) {
		FcPatternGetDouble(fontset->font.pattern, FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
	}

	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	if (!loadfont(&fontset->ifont, pattern))
		fontset->ifont = fontset->font;
	FcPatternDel(pattern, FC_SLANT);

	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	if (!loadfont(&fontset->bfont, pattern))
		fontset->bfont = fontset->font;
	FcPatternDel(pattern, FC_WEIGHT);

	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	if (!loadfont(&fontset->ibfont, pattern))
		fontset->ibfont = fontset->font;
	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternDel(pattern, FC_SLANT);

	// we reallocated, so we need updated all backreferences to fontsets inside fonts
	for (int i=0; i<dc.fontsetlen; i++) {
		struct FontSetStruct *fs = &dc.fontsets[i];
		fs->font.fontset = fs;
		fs->bfont.fontset = fs;
		fs->ifont.fontset = fs;
		fs->ibfont.fontset = fs;
	}

	return 1;
}

void
init(void)
{
	tnew(MAX(cols, 1), MAX(rows, 1));

	if (!FcInit()) die("could not init fontconfig.\n");
	if (TTF_Init() == -1) die("could not init sdl_ttf.\n");

	SDL_StartTextInput();

	win.ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);

	assert(!SDL_Init(SDL_INIT_VIDEO));

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	loadcols();
	resizefont();

	win.w = cols * win.cw;
	win.h = rows * win.ch;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	win.wnd = SDL_CreateWindow("typyst",  SDL_WINDOWPOS_CENTERED,  SDL_WINDOWPOS_CENTERED, win.w, win.h, SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL|SDL_WINDOW_MAXIMIZED|SDL_WINDOW_ALLOW_HIGHDPI);
	win.rnd = SDL_CreateRenderer(win.wnd, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawBlendMode(win.rnd, SDL_BLENDMODE_BLEND);

	/* Get actual drawable size and DPI scale factor */
	int draw_w, draw_h, win_w, win_h;
	SDL_GetRendererOutputSize(win.rnd, &draw_w, &draw_h);
	SDL_GetWindowSize(win.wnd, &win_w, &win_h);
	float dpi_scale = (float)draw_w / (float)win_w;

	/* Reload fonts at scaled pixel size for HiDPI rendering */
	usedfontsize *= dpi_scale;
	resizefont();

	if (opt_fullscreen) {
		win.w = draw_w;
		win.h = draw_h;
	} else {
		win.w = draw_w;
		win.h = draw_h;
	}

	resize(win.w, win.h);
	ttyresize(cols, rows);

	win.mode = MODE_NUMLOCK;

	if (opt_anim)
		initanim(opt_anim);

	SDL_EnableScreenSaver();
	SDL_ShowWindow(win.wnd);
}


void
resizefont(void)
{
	if (dc.fontsetlen) {
		for (int i=0; i<dc.fontsetlen; i++) {
			FontSet *fs = &dc.fontsets[i];
			unloadfont(&fs->font);
			unloadfont(&fs->bfont);
			unloadfont(&fs->ifont);
			unloadfont(&fs->ibfont);
		}
		free(dc.fontsets);
		dc.fontsetlen = 0;
		dc.fontsets = 0;
	}

	FcPattern *pattern = createfontpattern(font);
	assert(pattern);
	int fsres = loadfontset(pattern);
	assert(fsres);
	FcPatternDestroy(pattern);

	win.cw = ceilf(dc.fontsets->font.width);
	win.ch = ceilf(dc.fontsets->font.height);

	pattern = createfontpattern(font2);
	assert(pattern);
	fsres = loadfontset(pattern);
	assert(fsres);
	FcPatternDestroy(pattern);
}

Font *
selectglyphfont(Glyph g)
{
	Font *f = 0;
	FontSet *fontset = dc.fontsets;

	while (FcFalse == FcCharSetHasChar(fontset->font.charset, g.u) && fontset - dc.fontsets < dc.fontsetlen - 1) {
		fontset++;
	}

	if (FcFalse == FcCharSetHasChar(fontset->font.charset, g.u)) {
		FcPattern *pattern = createfontpattern(font);

		FcCharSet *charset = FcCharSetCreate();
		#ifdef DEBUG
		printf("looking for font with char %d\n", g.u);
		#endif
		FcCharSetAddChar(charset, g.u);
		FcPatternAdd(pattern, FC_CHARSET, (FcValue){ .type = FcTypeCharSet, .u = { .c = charset } }, 1);

		int fontsetFound = loadfontset(pattern);

		FcPatternDestroy(pattern);
		FcCharSetDestroy(charset);

		if (!fontsetFound)
			return 0;

		fontset = &dc.fontsets[dc.fontsetlen-1];
		FcCharSetAddChar(fontset->font.charset, g.u);

		if (FcTrue != FcCharSetHasChar(fontset->font.charset, g.u)) {
			FcPatternDestroy(fontset->font.pattern);
			FcPatternDestroy(fontset->ifont.pattern);
			FcPatternDestroy(fontset->bfont.pattern);
			FcPatternDestroy(fontset->ibfont.pattern);

			FcCharSetDestroy(fontset->font.charset);
			FcCharSetDestroy(fontset->ifont.charset);
			FcCharSetDestroy(fontset->bfont.charset);
			FcCharSetDestroy(fontset->ibfont.charset);

			*fontset = (FontSet){0};
			dc.fontsetlen--;
			return 0;
		}
	}

	f = &(fontset->font);

	if (!f)
		return 0;

	if (FcFalse == FcCharSetHasChar(fontset->font.charset, g.u)) {
		return 0;
	}

	/* Select right font */
	if (g.mode & ATTR_ITALIC && g.mode & ATTR_BOLD) {
		f = &fontset->ibfont;
	} else if (g.mode & ATTR_ITALIC) {
		f = &fontset->ifont;
	} else if (g.mode & ATTR_BOLD) {
		f = &fontset->bfont;
	}

	return f;
}

void
selectglyphcolors(Glyph g, SDL_Color *ret_fg, SDL_Color *ret_bg)
{
	RenderColor *fg, *bg;
	RenderColor *temp;
	RenderColor colfg, colbg;

	if (IS_TRUECOL(g.fg)) {
		colfg.alpha = 0xff;
		colfg.red = TRUERED(g.fg);
		colfg.green = TRUEGREEN(g.fg);
		colfg.blue = TRUEBLUE(g.fg);
		fg = &colfg;
	} else {
		fg = &dc.col[g.fg];
	}

	if (IS_TRUECOL(g.bg)) {
		colbg.alpha = 0xff;
		colbg.red = TRUERED(g.bg);
		colbg.green = TRUEGREEN(g.bg);
		colbg.blue = TRUEBLUE(g.bg);
		bg = &colbg;
	} else {
		bg = &dc.col[g.bg];
	}

	/* Change basic system colors [0-7] to bright system colors [8-15] */
	if ((g.mode & ATTR_BOLD_FAINT) == ATTR_BOLD && g.fg <= 7)
		fg = &dc.col[g.fg + 8];

	if (IS_SET(MODE_REVERSE)) {
		if (fg == &dc.col[defaultfg]) {
			fg = &dc.col[defaultbg];
		} else {
			colfg.red = ~fg->red;
			colfg.green = ~fg->green;
			colfg.blue = ~fg->blue;
			colfg.alpha = fg->alpha;
			fg = &colfg;
		}

		if (bg == &dc.col[defaultbg]) {
			bg = &dc.col[defaultfg];
		} else {
			colbg.red = ~bg->red;
			colbg.green = ~bg->green;
			colbg.blue = ~bg->blue;
			colbg.alpha = bg->alpha;
			bg = &colbg;
		}
	}

	if ((g.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		colfg.red = fg->red / 2;
		colfg.green = fg->green / 2;
		colfg.blue = fg->blue / 2;
		colfg.alpha = fg->alpha;
		fg = &colfg;
	}

	if (g.mode & ATTR_REVERSE) {
		temp = fg;
		fg = bg;
		bg = temp;
	}

	if (g.mode & ATTR_BLINK && win.mode & MODE_BLINK)
		fg = bg;

	if (g.mode & ATTR_INVISIBLE)
		fg = bg;

	ret_fg->r = fg->red;
	ret_fg->b = fg->blue;
	ret_fg->g = fg->green;
	ret_fg->a = fg->alpha;

	ret_bg->r = bg->red;
	ret_bg->b = bg->blue;
	ret_bg->g = bg->green;
	ret_bg->a = bg->alpha;
}

int
getglyphwidth(Rune u)
{
	Font *f = selectglyphfont((Glyph){ .u = u });

	if (!f) {
		return 0;
	}

	if (f->widths[u] != -1)
		return f->widths[u];

	f->widths[u] = wcwidth(u);

	if (f->widths[u] != -1)
		return f->widths[u];

	// wcwidth returned -1 (unknown width)
	// For characters in the Supplementary Multilingual Plane (0x10000+)
	// where most emoji live, assume width 2 if the font says it's wide
	// Otherwise default to 1
	if (u >= 0x10000) {
		/* Check if this is likely an emoji by trying TTF glyph metrics */
		int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
		TTF_GlyphMetrics(f->ttf, u, &minx, &maxx, &miny, &maxy, &advance);
		if (advance > win.cw) {
			f->widths[u] = 2;
			return 2;
		}
	}

	// ignore glyph metrics for now
	return 1;
}

void
drawglyph(Glyph g, int x, int y)
{
	int w = getglyphwidth(g.u);

	if (w < 1)
		return;

	int id = (y*cols+x);
	win.glyphs[id] = g;

	if (w < 2)
		return;

	char text[8] = {0};
	utf8encode(g.u, text);

	#ifdef DEBUG
	printf("print %s %d (width %d) at %d,%d\n", text, g.u, w, x,y);
	#endif
}

int
render_glyphs(void)
{
	if (!win.updated)
		return 0;

	if (!glyphcache) {
		glyphcache = cache_init(win.rnd, win.cw, win.ch);
	}

	SDL_SetRenderDrawColor(win.rnd, 0, 0, 0, 0);
	SDL_RenderClear(win.rnd);

	for (int id = 0; id<cols*rows; id++) {
		int y = id/cols;
		int x = id - y * cols;
		Glyph g = win.glyphs[id];

		if (!g.u)
			continue;

		// sometimes we have some other character under the second cell
		// of a wide glyph, so lets check if previous glyph was wide
		// and don't render this glyph in that case
		if (x > 0 && !(g.mode & ATTR_WIDE)) {
			int prev_x = MAX(x-1, 0);
			int prev_id = y * cols + prev_x;
			if (win.glyphs[prev_id].mode & ATTR_WIDE) {
				continue;
			}
		}

		Font *f = selectglyphfont(g);
		if (!f) {
			g.u = 0xFFFD; // �
			f = selectglyphfont(g);
		}
		assert(f);

		SDL_Color fg, bg;
		selectglyphcolors(g, &fg, &bg);

		int w = win.tw/cols;
		int h = win.th/rows;
		int winx = x * w;
		int winy = y * h;
		int no = 6*id;

		FontSet *fs = f->fontset;
		assert(fs);

		int charlen = ((g.mode & ATTR_WIDE) ? 2 : 1);
		int width = win.cw * charlen;

		SDL_Rect txt_rect = {winx, winy, width, win.ch};

		/*
		//TODO: render to win.txt
		if (g.mode & ATTR_UNDERLINE) drawglyph((Glyph){ '_', g.mode ^ ATTR_UNDERLINE, g.fg, g.bg }, x, y);
		if (g.mode & ATTR_STRUCK) drawglyph((Glyph){ '-', g.mode ^ ATTR_STRUCK, g.fg, g.bg }, x, y);
		*/

		// clear
		if (bg.r > 0 || bg.g > 0 || bg.b > 0) {
			SDL_SetRenderDrawColor(win.rnd, bg.r, bg.g, bg.b, 255);
			SDL_RenderFillRect(win.rnd, &txt_rect);
		}

		int font_type = 0;
		if (g.mode & ATTR_ITALIC && g.mode & ATTR_BOLD) {
			font_type = 3;
		} else if (g.mode & ATTR_ITALIC) {
			font_type = 2;
		} else if (g.mode & ATTR_BOLD) {
			font_type = 1;
		}

		char text[8] = {0};
		utf8encode(g.u, text);

		int cache_pos = -1;
		if ((cache_pos = cache_get(g)) == -1) {
			#ifdef DEBUG
			// printf("producing glyph %s %d\n", text, g.u);
			#endif

			SDL_Surface *fsur = TTF_RenderUTF8_Blended(f->ttf, text, (SDL_Color){255, 255, 255, 255});
			if (!fsur) continue;

			if (f->width != width) {
				#ifdef DEBUG
				printf("shrinking %s %d\n", text, g.u);
				#endif

				int nwr = f->width/width;
				int nhr = f->height/win.ch;
				int scale = MAX(1, MIN(nwr, nhr));
				SDL_Surface *shrunk = shrinkSurface(fsur, scale, scale);

				if (shrunk->w != width) {
					float nwr = 1.0/((float)shrunk->w/(float)width);
					float nhr = 1.0/((float)shrunk->h/(float)win.ch);
					float scale = MIN(nwr, nhr);
					SDL_Surface *temp = zoomSurface(shrunk, scale, scale, SMOOTHING_ON);
					SDL_FreeSurface(shrunk);
					shrunk = temp;
				}

				SDL_FreeSurface(fsur);
				fsur = shrunk;
			}


			#ifdef DEBUG
			printf("font texture size: %d x %d\n", fsur->w, fsur->h);
			#endif

			#ifdef DEBUG
			printf("caching %s %d\n", text, g.u);
			#endif

			cache_pos = cache_set(g, fsur);
			SDL_FreeSurface(fsur);
		}

		// render
		int glyph_width = getglyphwidth(g.u);
		//int glyph_width_factor = 2 * (1.0 / glyph_width);

		float atlas_step = 1.0 / gc.max;
		float atlas_offset = atlas_step * cache_pos;
		float x1 = atlas_offset;
		float x2 = x1 + atlas_step / 2;

		float atlas_hstep = 1.0 / 4;
		float atlas_hoffset = atlas_hstep * font_type;
		float y1 = atlas_hoffset;
		float y2 = y1 + atlas_hstep;

		for (int i = 0; i < glyph_width; i++) {
			geo.verts[no+0+i*6].color = fg;
			geo.verts[no+1+i*6].color = fg;
			geo.verts[no+2+i*6].color = fg;
			geo.verts[no+3+i*6].color = fg;
			geo.verts[no+4+i*6].color = fg;
			geo.verts[no+5+i*6].color = fg;

			float offset = atlas_step / 2 * i;
			geo.verts[no+0+i*6].tex_coord = (SDL_FPoint){x1 + offset, y1};
			geo.verts[no+1+i*6].tex_coord = (SDL_FPoint){x2 + offset, y1};
			geo.verts[no+2+i*6].tex_coord = (SDL_FPoint){x1 + offset, y2};
			geo.verts[no+3+i*6].tex_coord = (SDL_FPoint){x1 + offset, y2};
			geo.verts[no+4+i*6].tex_coord = (SDL_FPoint){x2 + offset, y2};
			geo.verts[no+5+i*6].tex_coord = (SDL_FPoint){x2 + offset, y1};
		}
	}

	int c = 6 * cols * rows;
	SDL_RenderGeometry(win.rnd, glyphcache, geo.verts, c, geo.idxs, c);

	#ifdef DEBUG
	//save_texture("dump.bmp", win.rnd, glyphcache);
	#endif

	win.updated = 0;
	return 1;
}

int
render_animation(void)
{
	if (!opt_anim)
		return 0;

	if (animate()) {
		if (anim.curr >= tx_anim_len) {
			tx_anim_len = anim.curr+1;
			tx_anim = realloc(tx_anim, sizeof(SDL_Texture*) * tx_anim_len);
			tx_anim[anim.curr] = SDL_CreateTextureFromSurface(win.rnd, anim.frame[anim.curr]);
		}
	}

	if (tx_anim_len > 0) {
		/* Render animation frame to background texture */
		SDL_SetRenderTarget(win.rnd, win.txt_background);
		SDL_SetRenderDrawColor(win.rnd, 0, 0, 0, 0);
		SDL_RenderClear(win.rnd);
		SDL_RenderCopy(win.rnd, tx_anim[anim.curr], 0, 0);
		/* Dark overlay for readability */
		SDL_SetRenderDrawColor(win.rnd, 0, 0, 0, 255*alpha);
		SDL_RenderFillRect(win.rnd, 0);

		return 1;
	}

	return 0;
}

void
render(void)
{
	SDL_LockMutex(mutex);

	if (win.should_draw) {
		draw();
		if (!syncd_output)
			win.should_draw = 0;
	}

	int anim = render_animation();

	/* Render glyphs directly to screen (no -a) or to glyph texture (with -a) */
	if (opt_anim && anim) {
		SDL_SetRenderTarget(win.rnd, win.txt_glyphs);
	} else {
		SDL_SetRenderTarget(win.rnd, 0);
	}

	int glyp = render_glyphs();

	if (glyp || anim) {
		if (anim) {
			/* Composite: reset target to screen, draw background then glyphs on top */
			SDL_SetRenderTarget(win.rnd, 0);
			SDL_RenderCopy(win.rnd, win.txt_background, 0, 0);
			SDL_RenderCopy(win.rnd, win.txt_glyphs, 0, 0);
		}

		SDL_RenderPresent(win.rnd);
	}

	SDL_UnlockMutex(mutex);
}

void
drawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
{
	if (win.mode & MODE_HIDE) {
		return;
	}

	int tmp = g.fg;
	g.fg = g.bg;
	g.bg = tmp;
	drawglyph(g, cx, cy);

	// refresh old cursor's cell
	if (cx != ox || cy != oy)
		drawglyph(og, ox, oy);
}

void
settitle(char *p)
{
	(void)p;
}

int
startdraw(void)
{
	win.drawing = 1;
	return IS_SET(MODE_VISIBLE);
}

void
drawline(Line line, int x1, int y1, int x2)
{
	int i, x, ox;
	Glyph g, new;

	i = ox = 0;
	for (x = x1; x < x2; x++) {
		new = line[x];
		if (new.mode == ATTR_WDUMMY)
			continue;
		//if (i > 0 && ATTRCMP(g, new)) {
		if (i > 0) {
			drawglyph(g, ox, y1);
			i = 0;
		}
		if (i == 0) {
			ox = x;
			g = new;
		}
		i++;
	}
	if (i > 0)
		drawglyph(g, ox, y1);
}


void
finishdraw(void)
{
	win.drawing = 0;
	win.updated = 1;
}

void
handle_window(SDL_Event *ev)
{
	switch(ev->window.event) {
		case SDL_WINDOWEVENT_CLOSE:
			exit(0);
			break;
		case SDL_WINDOWEVENT_RESIZED: {
			int draw_w, draw_h;
			SDL_GetRendererOutputSize(win.rnd, &draw_w, &draw_h);
			resize(draw_w, draw_h);
			break;
		}
		case SDL_WINDOWEVENT_FOCUS_GAINED: {
			win.lastfocus = ev->window.timestamp;
			redraw();
			win.should_draw = 1;
			break;
		}
	}
}

char *
kmap(SDL_KeyboardEvent *ev)
{
	for (Key *kp = key; kp < key + LEN(key); kp++) {

		if (ev->keysym.sym != kp->key)
			continue;

		if (kp->mode != 0xffffffff && !(ev->keysym.mod & kp->mode))
			continue;

		#ifdef DEBUG
		printf("matched kmap %d\n", kp->key);
		#endif

		if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0)
			continue;

		if (IS_SET(MODE_NUMLOCK) && kp->appkey == 2)
			continue;

		if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < 0 : kp->appcursor > 0)
			continue;

		return kp->esc;
	}

	return 0;
}

void
handle_keypress(SDL_Event *ev)
{
	if (IS_SET(MODE_KBDLOCK))
		return;

	// workaround for httpps://discourse.libsdl.org/t/alt-tab-in-linux-generates-another-tab/22844
	// ignore tab if it occured shortly after the window gained focus
	if (ev->key.timestamp - win.lastfocus < 100) {
		return;
	}

	/* Cmd+Q — quit the application */
	if ((ev->key.keysym.mod & KMOD_GUI) && ev->key.keysym.sym == SDLK_q) {
		SDL_Event quit_event = { .type = SDL_QUIT };
		SDL_PushEvent(&quit_event);
		return;
	}

	char *kmapbuf = kmap((SDL_KeyboardEvent *)ev);
	if (kmapbuf) {
		#ifdef DEBUG
		//printf("sending %d %d %d %d\n", kmapbuf[0], kmapbuf[1], kmapbuf[2], kmapbuf[3]);
		#endif

		ttywrite(kmapbuf, strlen(kmapbuf), 0);
		return;
	}

	unsigned char keysz = 1;
	char buf[8] = { ev->key.keysym.sym };

	int isctrl = ev->key.keysym.mod & KMOD_CTRL;
	int isshift = ev->key.keysym.mod & KMOD_SHIFT;
	int isalt = ev->key.keysym.mod & KMOD_ALT;

	int isfn = (ev->key.keysym.scancode >= SDL_SCANCODE_F1 && ev->key.keysym.scancode <= SDL_SCANCODE_F12);
	int isprint = !(ev->key.keysym.sym & 1<<30);
	int isspec = !(buf[0] >= ' ' && buf[0] <= '~');
	int isletter = buf[0] >= 'A' && buf[0] <= 'z';

	if (isfn) {
		switch (ev->key.keysym.scancode) {
			case SDL_SCANCODE_F1: memcpy(buf, "\033OP", keysz = 3); break;
			case SDL_SCANCODE_F2: memcpy(buf, "\033OQ", keysz = 3); break;
			case SDL_SCANCODE_F3: memcpy(buf, "\033OR", keysz = 3); break;
			case SDL_SCANCODE_F4: memcpy(buf, "\033OS", keysz = 3); break;
			case SDL_SCANCODE_F5: memcpy(buf, "\033[15~", keysz = 5); break;
			case SDL_SCANCODE_F6: memcpy(buf, "\033[17~", keysz = 5); break;
			case SDL_SCANCODE_F7: memcpy(buf, "\033[18~", keysz = 5); break;
			case SDL_SCANCODE_F8: memcpy(buf, "\033[19~", keysz = 5); break;
			case SDL_SCANCODE_F9: memcpy(buf, "\033[20~", keysz = 5); break;
			case SDL_SCANCODE_F10: memcpy(buf, "\033[21~", keysz = 5); break;
			case SDL_SCANCODE_F11: memcpy(buf, "\033[23~", keysz = 5); break;
			case SDL_SCANCODE_F12: memcpy(buf, "\033[24~", keysz = 5); break;
			default: break;
		}
	} else {
		if (!isprint || (!isspec && !isctrl && !isalt))
			return;


		if (isctrl && buf[0] == ' ') {
			buf[0] = 0;
		}
		if (isletter) {
			if (isctrl) {
				/* Ctrl(+Shift)+letter: produce control character (same for both).
				 * SDL always sends lowercase keysym for letters regardless of shift. */
				buf[0] &= 31;
			}

			if (!isctrl && isshift) {
				#ifdef DEBUG
				printf("capitalized %d\n", buf[0]);
				#endif
				buf[0] -= 'a' - 'A';
			}
		}

		if (isalt) {
			buf[1] = buf[0];
			buf[0] = '\033';
			keysz = 2;
		}
	}

	if (isctrl && isshift && buf[0] == '=') {
		usedfontsize++;
		#ifdef DEBUG
		printf("fontsize increased: %f\n", usedfontsize);
		#endif
		resizefont();
		resize(win.w, win.h);
		glyphcache = 0;
		return;
	}

	if (isctrl && !isshift && buf[0] == '-') {
		usedfontsize--;
		#ifdef DEBUG
		printf("fontsize decreased: %f\n", usedfontsize);
		#endif
		resizefont();
		resize(win.w, win.h);
		glyphcache = 0;
		return;
	}

	#ifdef DEBUG
	printf("sending %d %d %d %d print:%d, ctrl: %d, shift: %d, alt: %d\n",
		buf[0], buf[1], buf[2], buf[3],
		isprint, isctrl, isshift, isalt);
	#endif

	ttywrite(buf, keysz, 1);// TODO: isfn ?  : isalt ? 2 : 1, 1);
}

const unsigned char * kb_state;
int kb_state_len;

void
handle_textinput(SDL_Event *ev)
{
	if (IS_SET(MODE_KBDLOCK))
		return;

	if (kb_state[SDL_SCANCODE_LCTRL])
		return;

	int isalt = kb_state[SDL_SCANCODE_LALT] || kb_state[SDL_SCANCODE_RALT];

	if (isalt)
		return;  /* Alt+letter handled by handle_keypress instead */

	ttywrite(ev->text.text, strlen(ev->text.text), 1);

	#ifdef DEBUG
	printf("text input: %s\n", ev->text.text);
	#endif
}

static int quit_requested = 0;

int
read_events(void)
{
	SDL_Event event;

	/* Block until an event arrives or the frame-interval timeout expires.
	 * This replaces the old busy-polling loop, letting the thread truly
	 * sleep when idle instead of waking 30 times per second for nothing. */
	int has_event = SDL_WaitEventTimeout(&event, 1000 / opt_fps);

	SDL_LockMutex(mutex);
	kb_state = SDL_GetKeyboardState(&kb_state_len);

	if (has_event) {
		do {
			switch (event.type) {
			case SDL_QUIT:
				quit_requested = 1;
				break;
			case SDL_TEXTINPUT:
				handle_textinput(&event);
				break;
			case SDL_KEYDOWN:
				handle_keypress(&event);
				break;
			case SDL_WINDOWEVENT:
				handle_window(&event);
				break;
			default:
				/* tty_event_type is just a wake-up signal - no action needed */
				break;
			}
		} while (SDL_PollEvent(&event));
	}

	SDL_UnlockMutex(mutex);
	return quit_requested;
}

int
read_tty(void *data) {
	(void)data;
	fd_set rfd;

	while (1) {
		FD_ZERO(&rfd);
		FD_SET(win.ttyfd, &rfd);

		if (pselect(win.ttyfd+1, &rfd, 0, 0, 0, 0) < 0) {
			if (errno == EINTR) return 0;
			die("select failed: %s\n", strerror(errno));
		}

		if (FD_ISSET(win.ttyfd, &rfd)) {
			struct timespec ts;

			SDL_LockMutex(mutex);

			/* Drain: read all buffered data with a 1ms timeout.
			 * Between two tiny writes the PTY buffer can be empty
			 * for microseconds — with a zero timeout pselect would
			 * exit the drain loop immediately and we'd render a
			 * partial frame.  1ms gives the writer time to push
			 * more data before we give up and render. */
			do {
				ttyread();
				ts.tv_sec = 0;
				ts.tv_nsec = 1000000;
				FD_ZERO(&rfd);
				FD_SET(win.ttyfd, &rfd);
			} while (pselect(win.ttyfd+1, &rfd, 0, 0, &ts, 0) > 0
			         && FD_ISSET(win.ttyfd, &rfd));

			MODBIT(win.mode, 1, MODE_VISIBLE);
			win.should_draw = 1;

			SDL_UnlockMutex(mutex);

			/* Wake up the main thread — exactly once per batch */
			SDL_Event wake = { .type = tty_event_type };
			SDL_PushEvent(&wake);
		}
	}
}

void
init_geometry(Geometry *geo) {
	if (geo->verts) free(geo->verts);
	if (geo->idxs) free(geo->idxs);

	unsigned int c = 6 * cols * rows;

	geo->verts = calloc(c, sizeof(SDL_Vertex));
	geo->idxs = calloc(c, sizeof(int));

	for (int y=0; y<rows; y++) {
		for (int x=0; x<cols; x++) {
			int no = 6 * (cols * y + x);
			float x1 = win.cw * x;
			float y1 = win.ch * y;
			float x2 = x1 + win.cw;
			float y2 = y1 + win.ch;
			(geo->verts)[no+0] = (SDL_Vertex){{x1, y1}, {0}, {0, 0}};
			(geo->verts)[no+1] = (SDL_Vertex){{x2, y1}, {0}, {1, 0}};
			(geo->verts)[no+2] = (SDL_Vertex){{x1, y2}, {0}, {0, 1}};
			(geo->verts)[no+3] = (SDL_Vertex){{x1, y2}, {0}, {0, 1}};
			(geo->verts)[no+4] = (SDL_Vertex){{x2, y2}, {0}, {1, 1}};
			(geo->verts)[no+5] = (SDL_Vertex){{x2, y1}, {0}, {1, 0}};
			(geo->idxs)[no+0] = no+0;
			(geo->idxs)[no+1] = no+1;
			(geo->idxs)[no+2] = no+2;
			(geo->idxs)[no+3] = no+3;
			(geo->idxs)[no+4] = no+4;
			(geo->idxs)[no+5] = no+5;
		}
	}
}

void
usage(void)
{
	die(
		"	-f fontconfig string\n	-a path.gif set animated gif background\n	-t transparency"
	);
}

void
randombullshitgo(void) {
	win.drawing = 1;
			unsigned char r = rand() % 255;
			unsigned char g = rand() % 255;
			unsigned char b = rand() % 255;
			unsigned char c = (rand() % (127-32)) + 32;
			Glyph gl = {.u=c, .fg=TRUECOLOR(r,g,b)};

	for (int x=0; x<cols; x++) {
		for (int y=0; y<rows; y++) {
			drawglyph(gl, x, y);
		}
	}

	win.drawing = 0;
	win.updated = 1;
}

int
main(int argc, char *argv[])
{

	setlocale(LC_ALL, "C.UTF-8");

	ARGBEGIN {
	case 'f': {
		char *s = EARGF(usage());
		font = s;
		break;
	}
	case 'p': {
		char *trans = EARGF(usage());
		opt_fps = (unsigned int)atoi(trans);
		break;
	}
	case 'a':
		opt_anim = EARGF(usage());
		break;
	case 't': {
		char *trans = EARGF(usage());
		alpha = strtof(trans, 0);
		break;
	}
	case 's': {
		char *fullscreen = EARGF(usage());
		opt_fullscreen = (unsigned int)atoi(fullscreen);
		break;
	}
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		break;
	default:
		usage();
	} ARGEND;

	/* eat all remaining arguments */
	if (argc > 0) opt_cmd = argv;

	init();

	mutex = SDL_CreateMutex();
	SDL_CreateThread(read_tty, "read_tty", 0);

	tty_event_type = SDL_RegisterEvents(1);
	if (tty_event_type == (Uint32)-1) {
		die("could not register tty event type\n");
	}

	/* Small delay prevents the main loop from starving the TTY
	 * thread of the mutex. Without it the main thread spins at
	 * max speed, making it hard for read_tty to acquire the mutex
	 * and process PTY data between renders. */
	while (!quit_requested) {
		if (read_events())
			break;
		render();
		SDL_Delay(1);
	}


	return 0;
}
