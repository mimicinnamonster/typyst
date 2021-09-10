#include <malloc.h>
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <sys/select.h>

#include <fontconfig/fontconfig.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "st.h"
#include "arg.h"
#include "main.h"
#include "config.h"

static inline ushort sixd_to_16bit(int);
static void drawglyph(Glyph, int, int, int);
static void clear(int, int, int, int, RenderColor *);
static void init();
static void resize(int, int);
static int loadcolor(int, const char *, RenderColor *);
static int loadfont(Font *, FcPattern *);
static int loadfontset(const char *, double);

static const size_t FONTCACHESIZE = 0; // TODO: USHRT_MAX;

TermWindow win;
Animation anim;

static DrawingContext dc;
static double usedfontsize = 0;

static char **opt_cmd	= NULL;
static char *opt_embed  = NULL;
static char *opt_io	= NULL;
static char *opt_line	= NULL;
static char *opt_anim   = NULL;

void bell()
{
	// TODO: bell
}


void
setmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	/* TODO: redraw
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
		redraw();
	*/
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
	printf("width: %d, height: %d, win.cw: %d, win.ch: %d, cols: %d, rows: %d\n", width, height, win.cw, win.ch, cols, rows);
	#endif

	// resize text texture
	if (win.txt) SDL_FreeSurface(win.txt);
	win.txt = SDL_CreateRGBSurface(0, width, height, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
	SDL_SetSurfaceBlendMode(win.txt, SDL_BLENDMODE_BLEND);

	tresize(cols, rows);
	ttyresize(cols, rows);
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
	Color *cp;

	if (!loaded) {
		dc.collen = MAX(LEN(colorname), 256);
		dc.col = xmalloc(dc.collen * sizeof(Color));
	}

	for (i = 0; i < dc.collen; i++) {
		loadcolor(i, NULL, &dc.col[i]);
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

void
clear(int x1, int y1, int x2, int y2, RenderColor *col)
{
	SDL_FillRect(win.txt, &(SDL_Rect){x1, y1, x2-x1, y2-y1}, SDL_MapRGBA(win.txt->format, col->red, col->green, col->blue, col->alpha));
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
	printf("loading font file: %s, font size: %f\n", filepath, usedfontsize);
	#endif

	f->ttf = TTF_OpenFont(filepath, usedfontsize);
	if (!f->ttf) die(TTF_GetError());

	// TODO: hinting
	TTF_SetFontHinting(f->ttf, TTF_HINTING_LIGHT);

	f->set = NULL;
	f->badslant = 0;
	f->badweight = 0;

	TTF_GlyphMetrics(f->ttf, 'a', 0, 0, &f->ascent, &f->descent, &f->width);
	f->height = TTF_FontHeight(f->ttf);

	#ifdef DEBUG
	printf("allocating font cache %ld\n", FONTCACHESIZE * sizeof(SDL_Surface*));
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
	}

	if (loadfont(&fontset->font, pattern))
		die("can't open font %s\n", fontstr);

	if (usedfontsize < 0) {
		FcPatternGetDouble(fontset->font.pattern, FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
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

void
init()
{
	tnew(MAX(cols, 1), MAX(rows, 1));

	if (!FcInit()) die("could not init fontconfig.\n");
	if (TTF_Init() == -1) die("could not init sdl_ttf.\n");
	SDL_StartTextInput();

	loadfontset(font, 0);
	win.cw = ceilf(dc.fontsets->font.width);
	win.ch = ceilf(dc.fontsets->font.height);

	loadfontset(font2, 0);
	loadcols();

	// prepare sdl window
	if (SDL_Init(SDL_INIT_VIDEO) < 0) die("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	int w = cols * win.cw;
	int h = rows * win.ch;

	// create window and renderer
	SDL_CreateWindowAndRenderer(w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE, &win.wnd, &win.rnd);

	resize(w, h);

	settitle("typyst");
	win.mode = MODE_NUMLOCK;

	if (opt_anim)
		initanim(opt_anim);
}

void
drawglyph(Glyph base, int len, int x, int y)
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

	clear(winx, winy, winx+width, winy+win.ch, bg);

	SDL_Texture *ftxt = 0;

	if (base.u < FONTCACHESIZE && f->cache[base.u]) {
		ftxt = f->cache[base.u];
	}

	if (!ftxt) {

		if (isEmoji) {
			char text[5];
			utf8encode(base.u, text);
			ftxt = TTF_RenderUTF8_Blended(f->ttf, text, (SDL_Color){fg->red, fg->green, fg->blue});
		}
		else {
			ftxt = TTF_RenderGlyph_Blended(f->ttf, base.u, (SDL_Color){fg->red, fg->green, fg->blue});
		}

		#ifdef DEBUG
		//printf("making cache for glyph %d\n", base.u);
		#endif

		if (base.mode & ATTR_UNDERLINE) {
			drawglyph((Glyph){ '_', base.mode ^ ATTR_UNDERLINE, base.fg, base.bg }, len, x, y);
		}

		if (base.mode & ATTR_STRUCK) {
			drawglyph((Glyph){ '-', base.mode ^ ATTR_STRUCK, base.fg, base.bg }, len, x, y);
		}

		// TODO: underline, strikethrough
	}

	if (base.u < FONTCACHESIZE) {
		f->cache[base.u] = ftxt;
	}

	SDL_BlitScaled(ftxt, 0, win.txt, &(SDL_Rect){winx, winy, width, win.ch});

	if (base.u >= FONTCACHESIZE) {
		SDL_FreeSurface(ftxt);
	}
}

void
drawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
{
	int tmp = g.fg;
	g.fg = g.bg;
	g.bg = tmp;
	drawglyph(g, 1, cx, cy);

	// refresh old cursor's cell
	if (cx != ox || cy != oy)
		drawglyph(og, 1, ox, oy);
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
		//if (i > 0 && ATTRCMP(base, new)) {
		if (i > 0) {
			drawglyph(base, i, ox, y1);
			i = 0;
		}
		if (i == 0) {
			ox = x;
			base = new;
		}
		i++;
	}
	if (i > 0)
		drawglyph(base, i, ox, y1);
}

void
finishdraw(void)
{
	if (win.tx_txt)
		SDL_DestroyTexture(win.tx_txt);
	win.tx_txt = SDL_CreateTextureFromSurface(win.rnd, win.txt);
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
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			win.lastfocus = ev->window.timestamp;
			break;
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

	return NULL;
}

void
handle_keypress(SDL_Event *ev)
{
	if (IS_SET(MODE_KBDLOCK))
		return;

	#ifdef DEBUG
	printf("keypress %d %d %d %d\n", ev->key.keysym.sym);
	#endif

	char *kmapbuf = kmap(ev);
	if (kmapbuf) {
		#ifdef DEBUG
		printf("sending %d %d %d %d\n", kmapbuf[0], kmapbuf[1], kmapbuf[2], kmapbuf[3]);
		#endif
		ttywrite(kmapbuf, strlen(kmapbuf), 0);
		return;
	}

	char buf[8] = { ev->key.keysym.sym };

	int isctrl = ev->key.keysym.mod & KMOD_CTRL;
	int isshift = ev->key.keysym.mod & KMOD_SHIFT;
	int isalt = ev->key.keysym.mod & KMOD_ALT;

	int ismod = isctrl || isshift || isalt;
	int isprint = !(buf[0] & 0x40000000);
	int isspec = buf[0] < ' ' || buf[0] > 'z';

	if (!isprint || (!isspec && !isctrl && !isalt)) {
		return;
	}

	// workaround for httpps://discourse.libsdl.org/t/alt-tab-in-linux-generates-another-tab/22844
	// ignore tab if it occured shortly after the window gained focus
	if (buf[0] == '	' && ev->key.timestamp - win.lastfocus < 10)
		return;

	if (isctrl && isshift && !isspec)
			buf[0] -= '@';

	if (isctrl && !isshift && !isspec)
			buf[0] -= '`';

	if (isalt) {
		buf[1] = buf[0];
		buf[0] = '\033';
	}

	#ifdef DEBUG
	printf("sending %d %d %d %d print:%d, ctrl: %d, shift: %d, alt: %d\n",
		buf[0], buf[1], buf[2], buf[3],
		isprint, isctrl, isshift, isalt);
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
	static const struct timespec timeout = (struct timespec){ .tv_sec = 0, .tv_nsec = 1e9 / 30 };

	SDL_Event event;
	fd_set rfd;
	int ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);

	// send terminal size to the terminal
	ttyresize(cols, rows);

	int shouldRender = 0;

	while (1) {
		FD_ZERO(&rfd);
		FD_SET(ttyfd, &rfd);

		shouldRender = 0;

		// tty events
		if (pselect(ttyfd+1, &rfd, NULL, NULL, &timeout, NULL) < 0) {
			if (errno == EINTR) continue;
			die("select failed: %s\n", strerror(errno));
		}
		if (FD_ISSET(ttyfd, &rfd)) {
			ttyread();
			shouldRender = 1;
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
			}
		}

		MODBIT(win.mode, 1, MODE_VISIBLE);

		if (shouldRender) {
			SDL_RenderClear(win.rnd);
			draw();
		}

		if (opt_anim) {
			if (animate()) {
				shouldRender = 1;
				if (anim.curr >= win.tx_anim_len) {
					win.tx_anim_len = anim.curr+1;
					win.tx_anim = realloc(win.tx_anim, sizeof(SDL_Texture*) * win.tx_anim_len);
					win.tx_anim[anim.curr] = SDL_CreateTextureFromSurface(win.rnd, anim.frame[anim.curr]);
				}
			}
			if (win.tx_anim_len > anim.curr)
				SDL_RenderCopy(win.rnd, win.tx_anim[anim.curr], 0, 0);

		}

		if (shouldRender) {
			SDL_RenderCopy(win.rnd, win.tx_txt, 0, 0);
			SDL_RenderPresent(win.rnd);
		}
	}
}


void
usage(void)
{
	die("");
}

int
main(int argc, char *argv[])
{
	setlocale(LC_CTYPE, "");

	ARGBEGIN {
	case 'a':
		opt_anim = EARGF(usage());
		break;
	case 'e':
		if (argc > 0)
			--argc, ++argv;
		break;
	case 'o':
		opt_io = EARGF(usage());
		break;
	case 'l':
		opt_line = EARGF(usage());
		break;
	case 'v':
		die("%s " VERSION "\n");
		break;
	default:
		usage();
	} ARGEND;

	/* eat all remaining arguments */
	if (argc > 0) opt_cmd = argv;

	init();
	run();

	return 0;
}
