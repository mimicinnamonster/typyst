#include <malloc.h>
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <sys/select.h>

#include <fontconfig/fontconfig.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_thread.h>
#include <SDL2/SDL2_rotozoom.h>

#include "st.h"
#include "arg.h"
#include "main.h"
#include "config.h"

inline ushort sixd_to_16bit(int);
void drawglyph(Glyph, int, int, int);
void clear(int, int, int, int, RenderColor *);
void init();
void resize(int, int);
int loadcolor(int, const char *, RenderColor *);
int loadfont(Font *, FcPattern *);
void loadfontset(FcPattern *pattern);

#define FONTCACHESIZE 0

TermWindow win;
Animation anim;
DrawingContext dc;
double usedfontsize = 0;

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
	if (win.drawing) return;

	win.w = width;
	win.h = height;

	cols = MAX(1, (win.w-winpad*2) / win.cw);
	rows = MAX(1, (win.h-winpad*2) / win.ch);

	win.tw = cols * win.cw;
	win.th = rows * win.ch;

	#ifdef DEBUG
	printf("width: %d, height: %d, win.cw: %d, win.ch: %d, cols: %d, rows: %d\n", width, height, win.cw, win.ch, cols, rows);
	#endif

	// resize text texture
	if (win.txt) SDL_FreeSurface(win.txt);
	win.txt = SDL_CreateRGBSurface(0, width, height, 32, 0xff, 0xff00, 0xff0000, 0xff000000);

	// redraw all text on new surface
	redraw();

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
	FcPatternGetCharSet(f->match, FC_CHARSET, 0, &f->charset);

	#ifdef DEBUG
	printf("loading font file: %s, font size: %f\n", filepath, usedfontsize);
	#endif

	f->ttf = TTF_OpenFont(filepath, usedfontsize);
	if (!f->ttf) return -1;

	// TODO: hinting
	TTF_SetFontHinting(f->ttf, TTF_HINTING_LIGHT);

	f->set = NULL;

	TTF_GlyphMetrics(f->ttf, 'a', 0, 0, &f->ascent, &f->descent, &f->width);
	f->height = TTF_FontHeight(f->ttf);

	#ifdef DEBUG
	printf("glyph metrics: %dx%d\n", f->width, f->height);
	printf("allocating font cache %ld\n", FONTCACHESIZE * sizeof(SDL_Surface*));
	#endif

	f->cache = calloc(FONTCACHESIZE, sizeof(SDL_Texture*));

	return 0;
}

FcPattern *createfontpattern(const char *fontstr) {
	FcPattern *pattern = FcNameParse((const FcChar8 *)fontstr);
	return pattern;
}

void
loadfontset(FcPattern *pattern)
{
	dc.fontsets = reallocarray(dc.fontsets, ++dc.fontsetlen, sizeof(FontSet));
	FontSet *fontset = &dc.fontsets[dc.fontsetlen-1];

	double fontval;

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

	if (loadfont(&fontset->font, pattern))
		return;

	if (usedfontsize < 0) {
		FcPatternGetDouble(fontset->font.pattern, FC_PIXEL_SIZE, 0, &fontval);
		usedfontsize = fontval;
	}

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	loadfont(&fontset->ifont, pattern);

	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	loadfont(&fontset->ibfont, pattern);

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	loadfont(&fontset->bfont, pattern);
}

void
init()
{
	tnew(MAX(cols, 1), MAX(rows, 1));

	if (!FcInit()) die("could not init fontconfig.\n");
	if (TTF_Init() == -1) die("could not init sdl_ttf.\n");
	SDL_StartTextInput();

	FcPattern *pattern = createfontpattern(font);
	loadfontset(pattern);
	FcPatternDestroy(pattern);

	win.cw = ceilf(dc.fontsets->font.width);
	win.ch = ceilf(dc.fontsets->font.height);

	pattern = createfontpattern(font2);
	loadfontset(pattern);
	FcPatternDestroy(pattern);

	loadcols();

	win.ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);
	ttyresize(cols, rows); // send terminal size to the terminal

	// prepare sdl window
	if (SDL_Init(SDL_INIT_VIDEO) < 0) die("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	int w = cols * win.cw;
	int h = rows * win.ch;

	win.wnd = SDL_CreateWindow("typyst",  SDL_WINDOWPOS_CENTERED,  SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);

	resize(w, h);

	win.mode = MODE_NUMLOCK;

	if (opt_anim)
		initanim(opt_anim);
}

Font *
selectglyphfont(Glyph base)
{
	Font *f = 0;
	FontSet *fontset = &dc.fontsets[0];

	while (FcFalse == FcCharSetHasChar(fontset->font.charset, base.u) && fontset - dc.fontsets < dc.fontsetlen - 1) {
		fontset++;

		#ifdef DEBUG
		printf("trying next font %d/%d\n", fontset - dc.fontsets, dc.fontsetlen);
		#endif
	}

	if (FcFalse == FcCharSetHasChar(fontset->font.charset, base.u)) {
		FcPattern *pattern = createfontpattern(font);

		FcCharSet *charset = FcCharSetCreate();
		FcCharSetAddChar(charset, base.u);
		FcPatternAdd(pattern, FC_CHARSET, (FcValue){ .type = FcTypeCharSet, .u = { .c = charset } }, 1);

		loadfontset(pattern);
		fontset = &dc.fontsets[dc.fontsetlen-1];

		FcPatternDestroy(pattern);
		FcCharSetDestroy(charset);
	}


	f = &fontset->font;

	/* Select right font */
	if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD) {
		f = &fontset->ibfont;
	} else if (base.mode & ATTR_ITALIC) {
		f = &fontset->ifont;
	} else if (base.mode & ATTR_BOLD) {
		f = &fontset->bfont;
	}

	return f;
}

void
selectglyphcolors(Glyph base, RenderColor *ret_fg, RenderColor *ret_bg)
{
	RenderColor *fg, *bg;
	RenderColor *temp;
	RenderColor colfg, colbg, truebg;

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

	*ret_fg = *fg;
	*ret_bg = *bg;
}

int
getglyphwidth(Rune u)
{
	int width = 0;
	Font *f = selectglyphfont((Glyph){ .u = u });

	if (f->widths[u] != 0)
		return f->widths[u];

	f->widths[u] = 1;

	char text[8] = {0};
	utf8encode(u, text);

	int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
	TTF_GlyphMetrics(f->ttf, u, &minx, &maxx, &miny, &maxy, &advance);

	if (advance > win.cw)
		f->widths[u] = 2;

	#ifdef DEBUG
	printf("computed width for %s  %d: %d\n", text, u, f->widths[u]);
	#endif

	return f->widths[u];
}

void
drawglyph(Glyph base, int len, int x, int y)
{
	RenderColor bg, fg;
	selectglyphcolors(base, &fg, &bg);

	Font *f = selectglyphfont(base);

	int charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);
	int width = win.cw * charlen;

	SDL_Surface *ftxt = 0;

	if (base.u < FONTCACHESIZE && f->cache[base.u]) {
		ftxt = f->cache[base.u];
	}

	if (!ftxt) {
		char text[8] = {0};
		utf8encode(base.u, text);
		ftxt = TTF_RenderUTF8_Blended(f->ttf, text, (SDL_Color){fg.red, fg.green, fg.blue});

	}

	if (base.u < FONTCACHESIZE) {
		f->cache[base.u] = ftxt;

		#ifdef DEBUG
		printf("making cache for glyph %lc\n", base.u);
		#endif
	}

	int winx = x * win.cw;
	int winy = y * win.ch;

	clear(winx, winy, winx+width, winy+win.ch, &bg);

	if (base.mode & ATTR_UNDERLINE) {
		drawglyph((Glyph){ '_', base.mode ^ ATTR_UNDERLINE, base.fg, base.bg }, len, x, y);
	}

	if (base.mode & ATTR_STRUCK) {
		drawglyph((Glyph){ '-', base.mode ^ ATTR_STRUCK, base.fg, base.bg }, len, x, y);
	}

	if (f->width != width) {
		SDL_Surface *sftxt = shrinkSurface(ftxt, MAX(f->width/width, 1), MAX(f->height/win.ch, 1));
		SDL_BlitSurface(sftxt, 0, win.txt, &(SDL_Rect){winx, winy, width, win.ch});
		SDL_FreeSurface(sftxt);
	}
	else {
		SDL_BlitSurface(ftxt, 0, win.txt, &(SDL_Rect){winx, winy, width, win.ch});
	}

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
	// noop
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

	char *kmapbuf = kmap(ev);
	if (kmapbuf) {
		#ifdef DEBUG
		printf("sending %d %d %d %d\n", kmapbuf[0], kmapbuf[1], kmapbuf[2], kmapbuf[3]);
		#endif

		ttywrite(kmapbuf, strlen(kmapbuf), 0);
		return;
	}

	unsigned char buf[8] = { ev->key.keysym.sym };

	int isctrl = ev->key.keysym.mod & KMOD_CTRL;
	int isshift = ev->key.keysym.mod & KMOD_SHIFT;
	int isalt = ev->key.keysym.mod & KMOD_LALT;

	int isprint = !(ev->key.keysym.sym & 0x40000000);
	int isspec = !(buf[0] >= ' ' && buf[0] <= '~');
	int isletter = buf[0] >= 'A' && buf[0] <= 'z';

	if (!isprint || (!isspec && !isctrl && !isalt))
		return;

	if (!isctrl && isalt)
		return;

	// workaround for httpps://discourse.libsdl.org/t/alt-tab-in-linux-generates-another-tab/22844
	// ignore tab if it occured shortly after the window gained focus
	if (buf[0] == '	' && ev->key.timestamp - win.lastfocus < 10)
		return;

	if (isctrl && isshift && isletter)
		buf[0] -= '@';

	if (isctrl && !isshift && isletter)
		buf[0] -= '`';

	if (!isctrl && isshift && isletter) {
		printf("capitalized %d %d\n", buf[0]);
		buf[0] -= 'a' - 'A';
	}

	if (isalt) {
		buf[1] = buf[0];
		buf[0] = '\033';
	}

	#ifdef DEBUG
	printf("sending %d %d %d %d print:%d, ctrl: %d, shift: %d, alt: %d\n",
		buf[0], buf[1], buf[2], buf[3],
		isprint, isctrl, isshift, isalt);
	#endif

	ttywrite(buf, isalt ? 2 : 1, 1);
}

unsigned char *kb_state;
int kb_state_len;

void
handle_textinput(SDL_Event *ev)
{
	if (IS_SET(MODE_KBDLOCK))
		return;

	if (kb_state[SDL_SCANCODE_LCTRL])
		return;

	int isalt = kb_state[SDL_SCANCODE_LALT];

	unsigned char buf[8] = {isalt ? '\033' : 0};
	int textlen = strlen(ev->text.text);

	memcpy(buf + (isalt ? 1 : 0), ev->text.text, MIN(textlen, 8 - (isalt ? 1 : 0)));

	ttywrite(buf, strlen(buf), 1);

	#ifdef DEBUG
	printf("text input: %s\n", ev->text.text);
	#endif
}

void
read_events()
{
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
}

void
read_tty() {
	fd_set rfd;
	static struct timespec pSelectTimeout = { .tv_nsec = 10e8/60 };

	FD_ZERO(&rfd);
	FD_SET(win.ttyfd, &rfd);

	if (pselect(win.ttyfd+1, &rfd, NULL, NULL, &pSelectTimeout, NULL) < 0) {
		if (errno == EINTR) return;
		die("select failed: %s\n", strerror(errno));
	}
	if (FD_ISSET(win.ttyfd, &rfd)) {
		ttyread();
		MODBIT(win.mode, 1, MODE_VISIBLE);
		draw();
	}
}

void
run()
{
	win.rnd = SDL_CreateRenderer(win.wnd, -1, SDL_RENDERER_ACCELERATED);
	SDL_ShowWindow(win.wnd);

	SDL_Texture *tx_txt = 0;
	SDL_Texture **tx_anim = 0;
	unsigned int tx_anim_len = 0;

	int shouldDraw = 0;

	while (1) {
		read_events();
		read_tty();

		shouldDraw = 0;

		if (win.updated) {
			SDL_DestroyTexture(tx_txt);
			tx_txt = SDL_CreateTextureFromSurface(win.rnd, win.txt);
			win.updated = 0;
			shouldDraw = 1;
		}

		if (opt_anim && animate()) {
			shouldDraw = 1;
			if (anim.curr >= tx_anim_len) {
				tx_anim_len = anim.curr+1;
				tx_anim = realloc(tx_anim, sizeof(SDL_Texture*) * tx_anim_len);
				tx_anim[anim.curr] = SDL_CreateTextureFromSurface(win.rnd, anim.frame[anim.curr]);
			}
		}


		if (shouldDraw) {
			SDL_RenderClear(win.rnd);
			if (tx_anim_len > anim.curr)
				SDL_RenderCopy(win.rnd, tx_anim[anim.curr], 0, 0);
			SDL_RenderCopy(win.rnd, tx_txt, &(SDL_Rect){0,0,win.tw,win.th}, &(SDL_Rect){winpad,winpad,win.w-winpad*2,win.h-winpad*2});
			SDL_RenderPresent(win.rnd);
			shouldDraw = 0;
		}

	}
}


void
usage(void)
{
	die(
		"	-a path.gif	set animated gif backround"
	);
}

int
main(int argc, char *argv[])
{
	setlocale(LC_CTYPE, "UTF-8");

	ARGBEGIN {
	case 'a':
		opt_anim = EARGF(usage());
		break;
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
	run();

	return 0;
}
