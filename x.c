#include <malloc.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <SDL.h>
#include <SDL_ttf.h>

char *argv0;
#include "arg.h"
#include "st.h"
#include "win.h"

/* types used in config.h */
typedef struct {
	uint mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Shortcut;

typedef struct {
	KeySym k;
	uint mask;
	char *s;
	/* three-valued logic variables: 0 indifferent, 1 on, -1 off */
	signed char appkey;		/* application keypad */
	signed char appcursor; /* application cursor */
} Key;

/* X modifiers */
#define XK_ANY_MOD		UINT_MAX
#define XK_NO_MOD		 0
#define XK_SWITCH_MOD (1<<13)

/* function definitions used in config.h */
static void clippaste(const Arg *);
static void numlock(const Arg *);
static void selpaste(const Arg *);
static void zoom(const Arg *);
static void zoomabs(const Arg *);
static void zoomreset(const Arg *);
static void ttysend(const Arg *);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* macros */
#define IS_SET(flag)		((win.mode & (flag)) != 0)
#define TRUERED(x)		(((x) & 0xff0000) >> 8)
#define TRUEGREEN(x)		(((x) & 0xff00))
#define TRUEBLUE(x)		(((x) & 0xff) << 8)

typedef unsigned int Color;

/* Purely graphic info */
typedef struct {
	SDL_Window *wnd;
	SDL_Renderer *rnd;
	SDL_Texture *txt;

	int w, h; /* window width and height */
	int cw, ch; /* char width and height */
	int tw, th; /* tty width and height */

	int mode; /* window state/mode flags */
	int cursor; /* cursor style */
} TermWindow;

typedef struct {
	Atom xtarget;
	char *primary, *clipboard;
	struct timespec tclick1;
	struct timespec tclick2;
} XSelection;

/* Font structure */
#define Font Font_
typedef struct {
	int height;
	int width;
	int ascent;
	int descent;
	int badslant;
	int badweight;
	FcFontSet *set;
	FcPattern *pattern;
	FcPattern *match;
	FcCharSet *charset;
	TTF_Font *ttf;
	SDL_Texture **cache;
} Font;

typedef struct {
	Font font, bfont, ifont, ibfont;
} FontSet;

/* Drawing Context */
typedef struct {
	RenderColor *col;
	size_t collen;
	FontSet *fontsets;
	size_t fontsetlen;
} DC;

static inline ushort sixd_to_16bit(int);
static void _drawglyph(Glyph, int, int, int);
static void _clear(int, int, int, int, RenderColor *);
static void init();
static void resize(int, int);
static int loadcolor(int, const char *, RenderColor *);
static int loadfont(Font *, FcPattern *);
static int loadfontset(const char *, double);
static int evcol(SDL_Event *);
static int evrow(SDL_Event *);

static void handle_textinput(SDL_Event *);
static void handle_keypress(SDL_Event *);
static void handle_expose(SDL_Event *);
static void handle_visibility(SDL_Event *);
static void handle_unmap(SDL_Event *);
static void handle_window(SDL_Event *);
static void handle_focus();

static void _setsel(char *, Time);
static char *kmap(KeySym, uint);
static int match(uint, uint);

static void run(void);
static void usage(void);

/* Globals */
static DC dc;
static XSelection xsel;
static TermWindow win;

static const FONTCACHESIZE = USHRT_MAX;

static char *usedfont = NULL;
static double usedfontsize = 0;
static double defaultfontsize = 0;

static char *opt_alpha = NULL;
static char *opt_class = NULL;
static char **opt_cmd	= NULL;
static char *opt_embed = NULL;
static char *opt_font	= NULL;
static char *opt_io		= NULL;
static char *opt_line	= NULL;
static char *opt_name	= NULL;
static char *opt_title = NULL;
static char *opt_anim = NULL;

static int oldbutton = 3; /* button event on startup: 3 = release */

void
clipcopy(const Arg *dummy)
{
	// TODO: xsel.cliboard, xsel.primary
}

void
clippaste(const Arg *dummy)
{
	// TODO: xsel.xtarget, convert clipboard to selection
}

void
selpaste(const Arg *dummy)
{
	// TODO: xsel.xtarget
}

void
numlock(const Arg *dummy)
{
	win.mode ^= MODE_NUMLOCK;
}

void
zoom(const Arg *arg)
{
	Arg larg;

	larg.f = usedfontsize + arg->f;
	zoomabs(&larg);
}

void
zoomabs(const Arg *arg)
{
	loadfontset(usedfont, arg->f);
	resize(0, 0);
	redraw();
}

void
zoomreset(const Arg *arg)
{
	Arg larg;

	if (defaultfontsize > 0) {
		larg.f = defaultfontsize;
		zoomabs(&larg);
	}
}

void
ttysend(const Arg *arg)
{
	ttywrite(arg->s, strlen(arg->s), 1);
}

int
evcol(SDL_Event *e)
{
	// TODO: evcol
	return 0;
}

int
evrow(SDL_Event *e)
{
	// TODO: evrow
	return 0;
}

void
selnotify(SDL_Event *e)
{
	// TODO: selnotify
}

void
selrequest(SDL_Event *e)
{
	// TODO: selrequest
}

void
_setsel(char *str, Time t)
{
	// TODO: setsel
}

void
setsel(char *str)
{
	_setsel(str, CurrentTime);
}

void
resize(int width, int height)
{
	win.w = width;
	win.h = height;

	cols = MAX(1, (win.w) / win.cw);
	rows = MAX(1, (win.h) / win.ch);

	win.tw = cols * win.cw;
	win.th = rows * win.ch;

	#ifdef DEBUG
	printf("width: %d, height: %d, win.cw: %d, win.ch: %d, cols: %d, rows: %d\n", width, height, win.cw, win.ch, win.tw, win.th);
	#endif

	// resize text texture
	SDL_DestroyTexture(win.txt);
	win.txt = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
	SDL_SetTextureBlendMode(win.txt, SDL_BLENDMODE_BLEND);

	tresize(cols, rows);
	ttyresize(win.tw, win.th);
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

	return 1;
}

void
loadcols(void)
{
	int i;
	static int loaded;
	Color *cp;

	if (!loaded) {
		dc.collen = MAX(LEN(colorname), 256);
		dc.col = xmalloc(dc.collen * sizeof(Color));
	}

	for (i = 0; i < dc.collen; i++) {
		loadcolor(i, NULL, &dc.col[i]);
	}

	/* set alpha value of bg color */
	if (opt_alpha) alpha = strtof(opt_alpha, NULL);
	dc.col[defaultbg].alpha = 255 * alpha;
	loaded = 1;
}

int
setcolorname(int x, const char *name)
{
	RenderColor ncolor;

	if (!BETWEEN(x, 0, dc.collen))
		return 1;

	if (!loadcolor(x, name, &ncolor))
		return 1;

	dc.col[x] = ncolor;

	return 0;
}

void
_clear(int x1, int y1, int x2, int y2, RenderColor *col)
{
	SDL_SetRenderDrawColor(win.rnd, col->red, col->green, col->blue, col->alpha);
	SDL_RenderFillRect(win.rnd, &(SDL_Rect){x1, y1, x2-x1, y2-y1});
}

int
loadfont(Font *f, FcPattern *pattern)
{
	unsigned char *filepath;
	FcResult result;

	// TODO: slanted bolded
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_MEDIUM);

	f->pattern = FcPatternDuplicate(pattern);
	FcConfigSubstitute(NULL, f->pattern, FcMatchPattern);

	f->match = FcFontMatch(NULL, f->pattern, &result);

	FcPatternGetString(f->match, FC_FILE, 0, &filepath);
	FcPatternGetCharSet(f->match, FC_CHARSET, 0, 	&f->charset);

	#ifdef DEBUG
	printf("file: %s %f\n", filepath, usedfontsize);
	#endif

	f->ttf = TTF_OpenFont(filepath, usedfontsize);
	if (!f->ttf) die(TTF_GetError());

	// TODO: hinting
	TTF_SetFontHinting(f->ttf, TTF_HINTING_LIGHT);

	/*
	TODO: printable extents
	XftTextExtentsUtf8(xw.dpy, f->match,
		(const FcChar8 *) ascii_printable,
		strlen(ascii_printable), &extents);
	*/

	f->set = NULL;
	f->badslant = 0;
	f->badweight = 0;

	TTF_GlyphMetrics(f->ttf, 'a', 0, 0, &f->ascent, &f->descent, &f->width);
	f->height = TTF_FontHeight(f->ttf);

	#ifdef DEBUG
	printf("allocating %ld", FONTCACHESIZE * sizeof(SDL_Surface*));
	#endif

	f->cache = calloc(FONTCACHESIZE, sizeof(SDL_Texture*));

	return 0;
}

int
loadfontset(const char *fontstr, double fontsize)
{
	dc.fontsets = reallocarray(dc.fontsets, ++dc.fontsetlen, sizeof(FontSet));
	FontSet *fontset = &dc.fontsets[dc.fontsetlen-1];

	if (!dc.fontsets)
		die("Ran out of memory to allocate a fontset");

	FcPattern *pattern = FcNameParse((const FcChar8 *)fontstr);;
	double fontval;

	if (!pattern) die("can't open font %s\n", fontstr);

	if (fontsize > 1) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternDel(pattern, FC_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, (double)fontsize);
		usedfontsize = fontsize;
	} else {
		if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) == FcResultMatch) {
			usedfontsize = fontval;
		} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) == FcResultMatch) {
			usedfontsize = -1;
		} else {
			/*
			 * Default font size is 12, if none given. This is to
			 * have a known usedfontsize value.
			 */
			FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12);
			usedfontsize = 12;
		}
		defaultfontsize = usedfontsize;
	}

	if (loadfont(&fontset->font, pattern))
		die("can't open font %s\n", fontstr);

	if (usedfontsize < 0) {
		FcPatternGetDouble(fontset->font.pattern, FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
		if (fontsize == 0)
			defaultfontsize = fontval;
	}

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	if (loadfont(&fontset->ifont, pattern))
		die("can't open font %s\n", fontstr);

	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	if (loadfont(&fontset->ibfont, pattern))
		die("can't open font %s\n", fontstr);

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	if (loadfont(&fontset->bfont, pattern))
		die("can't open font %s\n", fontstr);

	FcPatternDestroy(pattern);

	return dc.fontsetlen-1;
}

#include "gif.c"

void
init()
{
	tnew(MAX(cols, 1), MAX(rows, 1));
	selinit();

	if (!FcInit()) die("could not init fontconfig.\n");
	if (TTF_Init() == -1) die("could not init sdl_ttf.\n");
	SDL_StartTextInput();

	usedfont = (opt_font == NULL)? font : opt_font;

	loadfontset(usedfont, 0);
	win.cw = ceilf(dc.fontsets->font.width * cwscale);
	win.ch = ceilf(dc.fontsets->font.height * chscale);

	loadfontset("noto color emoji", 0);

	loadcols();

	// prepare sdl window
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0) die("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

		int w = cols * win.cw;
		int h = rows * win.ch;

		// create window and renderer
		SDL_CreateWindowAndRenderer(w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE, &win.wnd, &win.rnd);

		// create main text surface
		win.txt = SDL_CreateTexture(win.rnd, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
		SDL_SetTextureBlendMode(win.txt, SDL_BLENDMODE_BLEND);

		// screen size based on glyph width
		resize(w, h);
	}

	{
		win.mode = MODE_NUMLOCK;
		resettitle();

		clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
		clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
		xsel.primary = NULL;
		xsel.clipboard = NULL;
	}

	if (opt_anim)
		initanim(opt_anim);
}

void
_drawglyph(Glyph base, int len, int x, int y)
{
	FontSet *fontset = &dc.fontsets[0];

	int isEmoji = FcFalse == FcCharSetHasChar(fontset->font.charset, base.u);

	if (isEmoji) {
		fontset = &dc.fontsets[1];
	}

	Font *f = &fontset->font;

	int charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);

	int winx = x * win.cw;
	int winy = y * win.ch;
	int width = charlen * win.cw;

	RenderColor *fg, *bg, *temp;
	RenderColor colfg, colbg, truebg;

	/* Fallback on color display for attributes not supported by the font */
	if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD) {
		if (fontset->ibfont.badslant || fontset->ibfont.badweight)
			base.fg = defaultattr;
	} else if ((base.mode & ATTR_ITALIC && fontset->ifont.badslant) ||
			(base.mode & ATTR_BOLD && fontset->bfont.badweight)) {
		base.fg = defaultattr;
	}

	/* Select right font */
	if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD) {
		f = &fontset->ibfont;
	} else if (base.mode & ATTR_ITALIC) {
		f = &fontset->ifont;
	} else if (base.mode & ATTR_BOLD) {
		f = &fontset->bfont;
	}

	if (IS_TRUECOL(base.fg)) {
		colfg.alpha = 0xff;
		colfg.red = TRUERED(base.fg);
		colfg.green = TRUEGREEN(base.fg);
		colfg.blue = TRUEBLUE(base.fg);
		fg = &colfg;
	} else {
		fg = &dc.col[base.fg];
	}

	if (IS_TRUECOL(base.bg)) {
		colbg.alpha = 0xff;
		colbg.green = TRUEGREEN(base.bg);
		colbg.red = TRUERED(base.bg);
		colbg.blue = TRUEBLUE(base.bg);
		bg = &truebg;
	} else {
		bg = &dc.col[base.bg];
	}

	/* Change basic system colors [0-7] to bright system colors [8-15] */
	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base.fg, 0, 7))
		fg = &dc.col[base.fg + 8];

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

	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		colfg.red = fg->red / 2;
		colfg.green = fg->green / 2;
		colfg.blue = fg->blue / 2;
		colfg.alpha = fg->alpha;
		fg = &colfg;
	}

	if (base.mode & ATTR_REVERSE) {
		temp = fg;
		fg = bg;
		bg = temp;
	}

	if (base.mode & ATTR_BLINK && win.mode & MODE_BLINK)
		fg = bg;

	if (base.mode & ATTR_INVISIBLE)
		fg = bg;

	_clear(winx, winy, winx+width, winy+win.ch, bg);


	SDL_Texture *ftxt = 0;

	if (base.u < FONTCACHESIZE && f->cache[base.u]) {
		ftxt = f->cache[base.u];
	}

	if (!ftxt) {
		SDL_Surface* bitmap;

		if (isEmoji) {
			char text[5];
			utf8encode(base.u, text);
			bitmap = TTF_RenderUTF8_Blended(f->ttf, text, (SDL_Color){fg->red, fg->blue, fg->green});
		}
		else {
			bitmap = TTF_RenderGlyph_Blended(f->ttf, base.u, (SDL_Color){fg->red, fg->blue, fg->green});
		}


		#ifdef DEBUG
		printf("making cache for glyph %d\n", base.u);
		#endif

		ftxt = SDL_CreateTextureFromSurface(win.rnd, bitmap);
		SDL_FreeSurface(bitmap);

		// TODO: underline, strikethrough
	}

	if (base.u < FONTCACHESIZE) {
		f->cache[base.u] = ftxt;
	}

	SDL_RenderCopy(win.rnd, ftxt, 0, &(SDL_Rect){winx, winy, width, win.ch});
	}

void
drawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
{
	int tmp = g.fg;
	g.fg = g.bg;
	g.bg = tmp;
	_drawglyph(g, 1, cx, cy);

	// refresh old cursor's cell
	if (cx != ox || cy != oy)
		_drawglyph(og, 1, ox, oy);
}

void
settitle(char *p)
{
	SDL_SetWindowTitle(win.wnd, p);
}

int
startdraw(void)
{
	return IS_SET(MODE_VISIBLE);
}

void
drawline(Line line, int x1, int y1, int x2)
{
	int i, x, ox;
	Glyph base, new;

	i = ox = 0;
	for (x = x1; x < x2; x++) {
		new = line[x];
		if (new.mode == ATTR_WDUMMY)
			continue;
		/*
		if (selected(x, y1))
			new.mode ^= ATTR_REVERSE;
		*/
		//if (i > 0 && ATTRCMP(base, new)) {
		if (i > 0) {
			_drawglyph(base, i, ox, y1);
			i = 0;
		}
		if (i == 0) {
			ox = x;
			base = new;
		}
		i++;
	}
	if (i > 0)
		_drawglyph(base, i, ox, y1);
}

void
finishdraw(void)
{
	#if 0
	XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, win.w,
			win.h, 0, 0);
	XSetForeground(xw.dpy, dc.gc,
			dc.col[IS_SET(MODE_REVERSE)?
				defaultfg : defaultbg].pixel);
	#endif
}

void
handle_expose(SDL_Event *ev)
{
	redraw();
}

void
handle_visibility(SDL_Event *ev)
{
	// TODO: MODBIT(win.mode, e->state != handle_visibilityFullyObscured, MODE_VISIBLE);
}

void
handle_unmap(SDL_Event *ev)
{
	win.mode &= ~MODE_VISIBLE;
}

void
setmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
		redraw();
}

int
setcursor(int cursor)
{
	if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
		return 1;
	win.cursor = cursor;
	return 0;
}

void
bell(void)
{
	// TODO: bell
}

void
handle_focus()
{
	#if 0
	if (ev->type == handle_focusIn) {
		if (xw.ime.xic)
			XSetIChandle_focus(xw.ime.xic);
		win.mode |= MODE_handle_focusED;
		if (IS_SET(MODE_handle_focus))
			ttywrite("\033[I", 3, 0);
	} else {
		if (xw.ime.xic)
			XUnsetIChandle_focus(xw.ime.xic);
		win.mode &= ~MODE_handle_focusED;
		if (IS_SET(MODE_handle_focus))
			ttywrite("\033[O", 3, 0);
	}
	#endif
}

int
match(uint mask, uint state)
{
	return mask == XK_ANY_MOD || mask == (state & ~ignoremod);
}

char*
kmap(KeySym k, uint state)
{
	Key *kp;
	int i;

	/* Check for mapped keys out of X11 function keys. */
	for (i = 0; i < LEN(mappedkeys); i++) {
		if (mappedkeys[i] == k)
			break;
	}
	if (i == LEN(mappedkeys)) {
		if ((k & 0xFFFF) < 0xFD00)
			return NULL;
	}

	for (kp = key; kp < key + LEN(key); kp++) {
		if (kp->k != k)
			continue;

		if (!match(kp->mask, state))
			continue;

		if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0)
			continue;
		if (IS_SET(MODE_NUMLOCK) && kp->appkey == 2)
			continue;

		if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < 0 : kp->appcursor > 0)
			continue;

		return kp->s;
	}

	return NULL;
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
	}
}

void
handle_keypress(SDL_Event *ev)
{
	if (IS_SET(MODE_KBDLOCK))
		return;

	char buf[6] = { ev->key.keysym.sym };

	int isctrl = ev->key.keysym.mod & KMOD_CTRL;
	int isshift = ev->key.keysym.mod & KMOD_SHIFT;
	int isalt = ev->key.keysym.mod & KMOD_ALT;

	int ismod = isctrl || isshift || isalt;
	int isprint = !(buf[0] & 0x40000000);
	int isspec = buf[0] < ' ';

	if (!isprint || (!isspec && !isctrl && !isalt))
		return;

	if (isctrl && isshift)
			buf[0] -= '@';

	if (isctrl && !isshift)
			buf[0] -= '`';

	if (isalt) {
		buf[1] = buf[0];
		buf[0] = '\033';
	}

	#ifdef DEBUG
	printf("key press: %d, mod: %d, print: %d, spec: %x\n", ev->key.keysym.sym, ismod, isprint, isspec);
	#endif

	ttywrite(buf, 1, 1);
}

void
handle_textinput(SDL_Event *ev)
{
	if (IS_SET(MODE_KBDLOCK))
		return;

	if (ev->text.text[0] <= 31)
		return;

	#ifdef DEBUG
	printf("text input: %s\n", ev->text.text);
	#endif

	ttywrite(ev->text.text, strlen(ev->text.text), 1);
}

void
run()
{

	static const struct timespec timeout = (struct timespec){ .tv_sec = 0, .tv_nsec = 1e9 / 1e3 };

	SDL_Event event;
	int w = win.w, h = win.h;
	fd_set rfd;
	int ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);

	while (1) {
		FD_ZERO(&rfd);
		FD_SET(ttyfd, &rfd);

		// tty events
		{
			if (pselect(ttyfd+1, &rfd, NULL, NULL, &timeout, NULL) < 0) {
				if (errno == EINTR) continue;
				die("select failed: %s\n", strerror(errno));
			}
			if (FD_ISSET(ttyfd, &rfd)) ttyread();
		}

		// host events
		while (SDL_PollEvent(&event)) {
			switch(event.type) {

				case SDL_TEXTINPUT:
					handle_textinput(&event);
					break;

				case SDL_WINDOWEVENT:
					handle_window(&event);
					break;

				case SDL_KEYDOWN:
					handle_keypress(&event);
					break;

				//[SDL_WindowEvent] = handle_window,
				//[SDL_WindowEvent] = handle_resize,
				//[SDL_WindowEvent] = handle_focus,
				//[SDL_WindowEvent] = handle_visibility,
				//[SDL_WindowEvent] = handle_unmap,
				//[SDL_WindowEvent] = handle_expose,
			}
		}


		MODBIT(win.mode, 1, MODE_VISIBLE);

		{
			if (opt_anim) {
				animate();
				SDL_SetRenderTarget(win.rnd, 0);
				SDL_RenderCopy(win.rnd, anim.frame[anim.curr], 0, 0);
			}

			SDL_SetRenderTarget(win.rnd, win.txt);
			draw();

			SDL_SetRenderTarget(win.rnd, 0);
			SDL_RenderCopy(win.rnd, win.txt, 0, 0);

			SDL_RenderPresent(win.rnd);
		}
	}
}


void
usage(void)
{
	die("usage: %s [-aiv] [-c class] [-f font] [-g geometry]"
			" [-n name] [-o file]\n"
			"					[-T title] [-t title] [-w windowid]"
			" [[-e] command [args ...]]\n"
			"			 %s [-aiv] [-c class] [-f font] [-g geometry]"
			" [-n name] [-o file]\n"
			"					[-T title] [-t title] [-w windowid] -l line"
			" [stty_args ...]\n", argv0, argv0);
}


int
main(int argc, char *argv[])
{
	setlocale(LC_CTYPE, "");
	setcursor(cursorshape);

	ARGBEGIN {
	case 'a':
		opt_anim = EARGF(usage());
		break;
	case 'A':
		opt_alpha = EARGF(usage());
		break;
	case 'c':
		opt_class = EARGF(usage());
		break;
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		goto run;
	case 'f':
		opt_font = EARGF(usage());
		break;
	case 'o':
		opt_io = EARGF(usage());
		break;
	case 'l':
		opt_line = EARGF(usage());
		break;
	case 'n':
		opt_name = EARGF(usage());
		break;
	case 't':
	case 'T':
		opt_title = EARGF(usage());
		break;
	case 'w':
		opt_embed = EARGF(usage());
		break;
	case 'v':
		die("%s " VERSION "\n", argv0);
		break;
	default:
		usage();
	} ARGEND;

run:
	/* eat all remaining arguments */
	if (argc > 0) opt_cmd = argv;

	if (!opt_title) opt_title = (opt_line || !opt_cmd) ? "st" : opt_cmd[0];

	init();
	run();

	return 0;
}
