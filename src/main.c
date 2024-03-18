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
//# include <SDL_rotozoom.h>
#include <SDL2/SDL2_rotozoom.h>


#include "st.h"
#include "arg.h"
#include "main.h"
#include "config.h"

inline ushort sixd_to_16bit(int);
void drawglyph(Glyph, int, int);
void init();
void resize(int, int);
int loadcolor(int, const char *, RenderColor *);
int loadfont(Font *, FcPattern *);
int loadfontset(FcPattern *pattern);
void init_geometry(Geometry *geo);
void fps();
int read_events();

#define FONTATLASSIZE 256
#define FONTCACHESIZE (1 << 16)

static char **opt_cmd = 0;
static char *opt_io	= 0;
static char *opt_line = 0;
static char *opt_anim = 0;
static int opt_fullscreen = 1;
static unsigned int opt_fps = 30;

TermWindow win;
Animation anim;
DrawingContext dc;
double usedfontsize;
SDL_Texture **tx_anim;
int tx_anim_len;
unsigned int lasttick;
unsigned int framecount;
SDL_mutex* mutex;

#define IS_SET(flag)	((win.mode & (flag)) != 0)

void bell()
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

	for (int i=0; i<dc.fontsetlen; i++) {
		init_geometry(&(dc.fontsets[i].geo));
	}

	//SDL_PIXELFORMAT_BGRA32
	win.txt_glyphs = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_TARGET, win.tw, win.th);
	win.txt_background = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_TARGET, win.tw, win.th);
	SDL_SetTextureBlendMode(win.txt_glyphs, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(win.txt_background, SDL_BLENDMODE_BLEND);

	int newlen = cols*rows;

	win.glyphs = realloc(win.glyphs, newlen*sizeof(Glyph));
	memset(win.glyphs, 0, newlen); // TODO: figure out how to zero only new bits

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
	FcPatternGetCharSet(f->match, FC_CHARSET, 0, &f->charset);

	f->filepath = filepath;

	#ifdef DEBUG
	printf("loading font file: %s, font size: %f\n", filepath, usedfontsize);
	#endif

	f->ttf = TTF_OpenFont(filepath, usedfontsize);
	if (!f->ttf) return 0;

	// TODO: hinting
	TTF_SetFontHinting(f->ttf, TTF_HINTING_LIGHT);

	f->set = 0;

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

FcPattern *createfontpattern(const char *fontstr)
{
	FcPattern *pattern = FcNameParse((const FcChar8 *)fontstr);
	return pattern;
}

int
loadfontset(FcPattern *pattern)
{
	dc.fontsets = realloc(dc.fontsets, ++dc.fontsetlen * sizeof(FontSet));
	FontSet *fontset = &dc.fontsets[dc.fontsetlen-1];
	*fontset = (FontSet){0};

	double fontval;

	if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) == FcResultMatch) {
		usedfontsize = fontval;
	} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) == FcResultMatch) {
		usedfontsize = -1;
	} else {
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

	init_geometry(&(fontset->geo));

	return 1;
}

void
init()
{
	tnew(MAX(cols, 1), MAX(rows, 1));

	if (!FcInit()) die("could not init fontconfig.\n");
	if (TTF_Init() == -1) die("could not init sdl_ttf.\n");
	SDL_StartTextInput();

	// loadfontset calls init_geometry which needs win.cw and win.ch
	// so lets fill it with something so it doesn't SIGFPE
	win.cw = 1;
	win.ch = 1;

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

	loadcols();

	win.ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);
	ttyresize(cols, rows); // send terminal size to the terminal

	#ifdef DEBUG
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
	#endif

	assert(!SDL_Init(SDL_INIT_VIDEO));

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	int w = cols * win.cw;
	int h = rows * win.ch;

	if (opt_fullscreen) {
		SDL_DisplayMode DM;
		SDL_GetCurrentDisplayMode(0, &DM);
		w = DM.w;
		h = DM.h;
	}

	resize(w, h);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	win.wnd = SDL_CreateWindow("typyst",  SDL_WINDOWPOS_CENTERED,  SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL|SDL_WINDOW_MAXIMIZED);
	win.rnd = SDL_CreateRenderer(win.wnd, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawBlendMode(win.rnd, SDL_BLENDMODE_BLEND);


	win.mode = MODE_NUMLOCK;

	if (opt_anim)
		initanim(opt_anim);

	SDL_EnableScreenSaver();
	SDL_ShowWindow(win.wnd);
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

	// ignore glyph metrics for now
	return 1;

	char text[8] = {0};
	utf8encode(u, text);
	int before = f->widths[u];

	int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
	TTF_GlyphMetrics(f->ttf, u, &minx, &maxx, &miny, &maxy, &advance);

	if (advance > win.cw)
		f->widths[u] = 2;
	else
		f->widths[u] = 0;

	#ifdef DEBUG
	printf("width %s %x: %d before %d\n", text, u, f->widths[u], before);
	#endif

	return f->widths[u];
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
render_glyphs()
{
	if (!win.updated)
		return 0;

	SDL_SetRenderDrawColor(win.rnd, 0, 0, 0, 0);
	SDL_RenderClear(win.rnd);

	for (int id = cols*rows-1; id >= 0; id--) {
		int y = id/cols;
		int x = id - y * cols;
		Glyph g = win.glyphs[id];

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

		// dynamically create atlas if it wasn't created yet
		if (!fs->atlas) {
			int w = FONTATLASSIZE * win.cw * 2;
			int h = win.ch * 4;
			fs->atlas = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, w, h);
			assert(fs->atlas);
			SDL_SetTextureBlendMode(fs->atlas, SDL_BLENDMODE_BLEND);
			#ifdef DEBUG
			printf("creating atlas for fontset %s: %d x %d\n", fs->font.filepath, w, h);
			#endif
		}
		assert(fs->atlas);

		int charlen = ((g.mode & ATTR_WIDE) ? 2 : 1);
		int width = win.cw * charlen;

		SDL_Texture *ftxt = 0;
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

		for (int i=0; i<dc.fontsetlen; i++) {
			FontSet *fs = &dc.fontsets[i];
			for (int j=0; j<charlen; j++) {
				int no2 = no + j*6;
				fs->geo.verts[no2+0].tex_coord = (SDL_FPoint){0, 0};
				fs->geo.verts[no2+1].tex_coord = (SDL_FPoint){0, 0};
				fs->geo.verts[no2+2].tex_coord = (SDL_FPoint){0, 0};
				fs->geo.verts[no2+3].tex_coord = (SDL_FPoint){0, 0};
				fs->geo.verts[no2+4].tex_coord = (SDL_FPoint){0, 0};
				fs->geo.verts[no2+5].tex_coord = (SDL_FPoint){0, 0};
			}
		}

		if (!g.u)
			continue;

		if (g.u < FONTCACHESIZE && f->cache[g.u]) {
			ftxt = f->cache[g.u];
		}

		int font_type = 0;
		if (g.mode & ATTR_ITALIC && g.mode & ATTR_BOLD) {
			font_type = 3;
		} else if (g.mode & ATTR_ITALIC) {
			font_type = 2;
		} else if (g.mode & ATTR_BOLD) {
			font_type = 1;
		}

		SDL_Rect ftxt_rect = txt_rect;

		char text[8] = {0};
		utf8encode(g.u, text);

		if (ftxt) {
			ftxt_rect.w = f->cache_widths[g.u];
			ftxt_rect.h = f->cache_heights[g.u];
		} else {
			#ifdef DEBUG
			printf("producing glyph %s %d\n", text, g.u);
			#endif

			SDL_Surface *fsur = TTF_RenderUTF8_Blended(f->ttf, text, (SDL_Color){255, 255, 255, 255});
			if (!fsur) continue;

			if (f->width != width) {
				#ifdef DEBUG
				printf("shrinking %s %d\n", text, g.u);
				#endif

				float nwr = 1/((float)f->width/(float)width);
				float nhr = 1/((float)f->height/(float)win.ch);
				float scale = MIN(nwr, nhr);
				//SDL_Surface *shrunk = shrinkSurface(fsur, scale, scale);
				SDL_Surface *shrunk = zoomSurface(fsur, scale, scale, SMOOTHING_OFF);
				SDL_FreeSurface(fsur);
				fsur = shrunk;
			}

			#ifdef DEBUG
			printf("font texture size: %d x %d\n", fsur->w, fsur->h);
			#endif

			ftxt = SDL_CreateTextureFromSurface(win.rnd, fsur);

			assert(f);
			assert(f->cache);

			int is_cached = g.u < FONTCACHESIZE && f->cache[g.u] != 0;

			if (g.u < FONTATLASSIZE && !is_cached) {
				SDL_Rect atlasrect = {
					g.u * win.cw * 2,
					font_type * win.ch,
					win.cw,
					win.ch
				};
				#ifdef DEBUG
				printf("atlasing glyph %lc (%d) at pos: %d %d\n", g.u, g.u, atlasrect.x, atlasrect.y);
				#endif
				SDL_UpdateTexture(fs->atlas, &atlasrect, fsur->pixels, fsur->pitch);
			}

			if (g.u < FONTCACHESIZE && !is_cached) {
				#ifdef DEBUG
				printf("caching glyph %lc\n", g.u);
				#endif
				f->cache[g.u] = ftxt;
				f->cache_widths[g.u] = fsur->w;
				f->cache_heights[g.u] = fsur->h;
			}

			ftxt_rect.w = fsur->w;
			ftxt_rect.h = fsur->h;
			SDL_FreeSurface(fsur);
		}

		// center
		ftxt_rect.x += (width - ftxt_rect.w) / 2;
		ftxt_rect.y += (win.ch - ftxt_rect.h) / 2;

		// render
		fs->geo.verts[no+0].color = fg;
		fs->geo.verts[no+1].color = fg;
		fs->geo.verts[no+2].color = fg;
		fs->geo.verts[no+3].color = fg;
		fs->geo.verts[no+4].color = fg;
		fs->geo.verts[no+5].color = fg;

		int glyph_width = getglyphwidth(g.u);
		if (g.u < FONTATLASSIZE) {
			float atlas_step = 1.0 / FONTATLASSIZE;
			float atlas_offset = atlas_step * g.u;
			int glyph_width_factor = 2 * (1 / glyph_width);
			float x1 = atlas_offset;
			float x2 = x1 + atlas_step / glyph_width_factor;

			float atlas_hstep = 1.0 / 4;
			float atlas_hoffset = atlas_hstep * font_type;
			float y1 = atlas_hoffset;
			float y2 = y1 + atlas_hstep;

			fs->geo.verts[no+0].tex_coord = (SDL_FPoint){x1, y1};
			fs->geo.verts[no+1].tex_coord = (SDL_FPoint){x2, y1};
			fs->geo.verts[no+2].tex_coord = (SDL_FPoint){x1, y2};
			fs->geo.verts[no+3].tex_coord = (SDL_FPoint){x1, y2};
			fs->geo.verts[no+4].tex_coord = (SDL_FPoint){x2, y2};
			fs->geo.verts[no+5].tex_coord = (SDL_FPoint){x2, y1};
		} else {
			SDL_SetTextureColorMod(ftxt, fg.r, fg.g, fg.b);
			SDL_RenderCopy(win.rnd, ftxt, 0, &ftxt_rect);
		}

		if (g.u >= FONTCACHESIZE) {
			SDL_DestroyTexture(ftxt);
		}
	}

	int c = 6 * cols * rows;
	for (int dci=0; dci<dc.fontsetlen; dci++) {
		FontSet *fs = &dc.fontsets[dci];
		if (fs->atlas){
			SDL_RenderGeometry(win.rnd, fs->atlas, fs->geo.verts, c, fs->geo.idxs, c);
		}
	}

	win.updated = 0;
	return 1;
}

int
render_animation()
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
		SDL_SetRenderTarget(win.rnd, 0/*win.txt_background*/);

		SDL_RenderCopy(win.rnd, tx_anim[anim.curr], 0, 0);
		SDL_SetRenderDrawColor(win.rnd, 0, 0, 0, 255*alpha);
		SDL_RenderFillRect(win.rnd, 0);

		return 1;
	}

	return 0;
}

void
render()
{
	SDL_LockMutex(mutex);

	if (win.should_draw) {
		draw();
		win.should_draw = 0;
	}

	int anim = render_animation();

	if (opt_anim && anim){
		SDL_SetRenderTarget(win.rnd, win.txt_glyphs);
	}

	int glyp = render_glyphs();

	if (glyp || anim) {
		if (anim) {
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
	char title[100] = {0};
	if (p == 0 || p[0] == 0) {
		SDL_SetWindowTitle(win.wnd, "typyst");
	} else {
		snprintf(title, 100, "typyst: %s", p);
		SDL_SetWindowTitle(win.wnd, title);
	}
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
		case SDL_WINDOWEVENT_RESIZED:
			resize(ev->window.data1, ev->window.data2);
			break;
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

		if (!(ev->keysym.mod & kp->mode))
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
	int isalt = ev->key.keysym.mod & KMOD_LALT;

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

		if (!isctrl && isalt)
			return;

		if (isctrl && buf[0] == ' ') {
			buf[0] = 0;
		}
		if (isletter) {
			if (isctrl && isshift)
				buf[0] -= '@';

			if (isctrl && !isshift)
				buf[0] -= '`';

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

	int isalt = kb_state[SDL_SCANCODE_LALT];

	char buf[8] = {isalt ? '\033' : 0};
	int textlen = strlen(ev->text.text);

	memcpy(buf + (isalt ? 1 : 0), ev->text.text, MIN(textlen, 8 - (isalt ? 1 : 0)));

	ttywrite(buf, strlen(buf), 1);

	#ifdef DEBUG
	printf("text input: %s\n", ev->text.text);
	#endif
}

int
read_events()
{
	SDL_LockMutex(mutex);
	kb_state = SDL_GetKeyboardState(&kb_state_len);

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch(event.type) {
			case SDL_TEXTINPUT:
				handle_textinput(&event);
				break;
			case SDL_KEYDOWN:
				handle_keypress(&event);
				break;
			case SDL_WINDOWEVENT:
				handle_window(&event);
				break;
		}
	}
	SDL_UnlockMutex(mutex);
	return 0;
}

int
read_tty() {
	fd_set rfd;

	while (1) {
		FD_ZERO(&rfd);
		FD_SET(win.ttyfd, &rfd);

		if (pselect(win.ttyfd+1, &rfd, 0, 0, 0, 0) < 0) {
			if (errno == EINTR) return 0;
			die("select failed: %s\n", strerror(errno));
		}

		if (FD_ISSET(win.ttyfd, &rfd)) {
			SDL_LockMutex(mutex);

			ttyread();
			MODBIT(win.mode, 1, MODE_VISIBLE);
			//draw();
			win.should_draw = 1;

			SDL_UnlockMutex(mutex);
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
fps()
{
	unsigned int currtick = SDL_GetTicks();

	if (!lasttick) {
		lasttick = currtick;
		return;
	}

	framecount++;

	unsigned int dt = currtick - lasttick;

	if (dt > 1000) {
		char title[30] = {0};
		snprintf(title, 30, "%d fps", framecount);
		settitle(title);

		framecount = 0;
		lasttick = currtick;
	}
}

void
randombullshitgo() {
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

	unsigned int lastframe = 0;

	while (1) {
		//randombullshitgo();
		read_events();

		unsigned int currframe = SDL_GetTicks();
		unsigned int dt = currframe - lastframe;

		if (dt < 1000/opt_fps) {
			SDL_Delay((1000/opt_fps)-dt);
		}

		fps();

		render();

		lastframe = SDL_GetTicks();
	}


	return 0;
}
