/* See LICENSE.md for license details. */

/*
 * Menlo is the fallback for symbols Ubuntu Mono lacks (arrows ↑↓, blocks,
 * box-drawing, math). It is deliberately placed before the generic fallback
 * fontconfig would pick (Apple Symbols on macOS), whose arrow glyphs are thin
 * and light. Menlo's arrows have noticeably heavier shafts.
 */
static char *font = "Ubuntu Mono,Menlo:pixelsize=18:antialias=true:autohint=true";
static char *font2 = "Apple Color Emoji";
float alpha = 0.8;

static char *shell = "/bin/zsh";

char *utmp = NULL;

/* scroll program: to enable use a string like "scroll" */
char *scroll = NULL;
char *stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
char *vtiden = "\033[?6c";

/* alt screens */
int allowaltscreen = 1;

/* default TERM value */
char *termname = "typyst-256color";

/* window padding */
static int winpad = 0;

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
unsigned int tabspaces = 8;

/* Terminal colors (16 first used in escape sequence) */
// st changes colors orders 2,3 with 4,5
const RenderColor colorname[] = {
	/* 8 normal colors — Ghostty palette */
	{ .red = 85, .green = 85, .blue = 85, .alpha = 255 },         /*  0: #555555 */
	{ .red = 204, .green = 102, .blue = 102, .alpha = 255 },      /*  1: #cc6666 */
	{ .red = 181, .green = 189, .blue = 104, .alpha = 255 },      /*  2: #b5bd68 */
	{ .red = 240, .green = 198, .blue = 116, .alpha = 255 },      /*  3: #f0c674 */
	{ .red = 153, .green = 221, .blue = 255, .alpha = 255 },      /*  4: #99ddff */
	{ .red = 178, .green = 148, .blue = 187, .alpha = 255 },      /*  5: #b294bb */
	{ .red = 138, .green = 190, .blue = 183, .alpha = 255 },      /*  6: #8abeb7 */
	{ .red = 197, .green = 200, .blue = 198, .alpha = 255 },      /*  7: #c5c8c6 */

	/* 8 bright colors — Ghostty palette */
	{ .red = 170, .green = 170, .blue = 170, .alpha = 255 },      /*  8: #aaaaaa */
	{ .red = 213, .green = 78, .blue = 83, .alpha = 255 },        /*  9: #d54e53 */
	{ .red = 185, .green = 202, .blue = 74, .alpha = 255 },       /* 10: #b9ca4a */
	{ .red = 231, .green = 197, .blue = 71, .alpha = 255 },       /* 11: #e7c547 */
	{ .red = 204, .green = 238, .blue = 255, .alpha = 255 },      /* 12: #cceeff */
	{ .red = 195, .green = 151, .blue = 216, .alpha = 255 },      /* 13: #c397d8 */
	{ .red = 112, .green = 192, .blue = 177, .alpha = 255 },      /* 14: #70c0b1 */
	{ .red = 234, .green = 234, .blue = 234, .alpha = 255 },      /* 15: #eaeaea */

	/* more colors can be added after 255 to use with DefaultXX */
	[255] = {0},
	[256] = {0, 0, 0, 255},  /* pure black for background (matches Ghostty bg=#000000) */

};


/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
unsigned int defaultfg = 7;
unsigned int defaultbg = 256;

// TODO: remove this
//static unsigned int defaultrcs = 257;

/*
 * Default columns and rows numbers
 */
static int cols = 100;
static int rows = 30;

/* TODO: remove this
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
//static unsigned int defaultattr = 11;

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
// keysym, mask, string, appkey appcursor
static Key key[] = {
	// custom ctrl escapes
	{'`', KMOD_CTRL, "\0", 0, 0},
	{'~', KMOD_CTRL, "", 0, 0},
	{'2', KMOD_CTRL, "\0", 0, 0},
	{'3', KMOD_CTRL, "", 0, 0},
	{'4', KMOD_CTRL, "", 0, 0},
	{'5', KMOD_CTRL, "", 0, 0},
	{'6', KMOD_CTRL, "", 0, 0},
	{'7', KMOD_CTRL, "", 0, 0},
	{'8', KMOD_CTRL, "", 0, 0},
	{'_', KMOD_CTRL, "", 0, 0},
	{'[', KMOD_CTRL, "", 0, 0},
	{']', KMOD_CTRL, "", 0, 0},
	{'\\', KMOD_CTRL, "", 0, 0},
	{'/', KMOD_CTRL, "", 0, 0},
	
	// backspace
	{SDLK_BACKSPACE, KMOD_CTRL, "", 0, 0},
	{SDLK_BACKSPACE, KMOD_ALT, "", 0, 0},
	{SDLK_BACKSPACE, 0xffffffff, "", 0, 0},
	
	// keypad
	{SDLK_HOME, KMOD_SHIFT, "\033[2J", 0, -1},
	{SDLK_HOME, KMOD_SHIFT, "\033[1;2H", 0, +1},
	{SDLK_HOME, 0xffffffff, "\033[H", 0, -1},
	{SDLK_HOME, 0xffffffff, "\033[1~", 0, +1},
	{SDLK_PRIOR, KMOD_SHIFT, "\033[5;2~", 0, 0},
	{SDLK_PRIOR, 0xffffffff, "\033[5~", 0, 0},
	{SDLK_AUDIOPLAY, 0xffffffff, "\033[E", 0, 0},
	{SDLK_END, KMOD_CTRL, "\033[J", -1, 0},
	{SDLK_END, KMOD_CTRL, "\033[1;5F", +1, 0},
	{SDLK_END, KMOD_SHIFT, "\033[K", -1, 0},
	{SDLK_END, KMOD_SHIFT, "\033[1;2F", +1, 0},
	{SDLK_END, 0xffffffff, "\033[4~", 0, 0},
	{SDLK_AUDIONEXT, KMOD_SHIFT, "\033[6;2~", 0, 0},
	{SDLK_AUDIONEXT, 0xffffffff, "\033[6~", 0, 0},
	{SDLK_INSERT, KMOD_SHIFT, "\033[2;2~", +1, 0},
	{SDLK_INSERT, KMOD_SHIFT, "\033[4l", -1, 0},
	{SDLK_INSERT, KMOD_CTRL, "\033[L", -1, 0},
	{SDLK_INSERT, KMOD_CTRL, "\033[2;5~", +1, 0},
	{SDLK_INSERT, 0xffffffff, "\033[4h", -1, 0},
	{SDLK_INSERT, 0xffffffff, "\033[2~", +1, 0},
	{SDLK_DELETE, KMOD_CTRL, "\033[M", -1, 0},
	{SDLK_DELETE, KMOD_CTRL, "\033[3;5~", +1, 0},
	{SDLK_DELETE, KMOD_SHIFT, "\033[2K", -1, 0},
	{SDLK_DELETE, KMOD_SHIFT, "\033[3;2~", +1, 0},
	{SDLK_DELETE, 0xffffffff, "\033[P", -1, 0},
	{SDLK_DELETE, 0xffffffff, "\033[3~", +1, 0},
	{SDLK_KP_MEMMULTIPLY, 0xffffffff, "\033Oj", +2, 0},
	{SDLK_KP_MEMADD, 0xffffffff, "\033Ok", +2, 0},
	{SDLK_KP_ENTER, 0xffffffff, "\033OM", +2, 0},
	{SDLK_KP_ENTER, 0xffffffff, "\r", -1, 0},
	{SDLK_KP_MEMSUBTRACT, 0xffffffff, "\033Om", +2, 0},
	{SDLK_KP_HEXADECIMAL, 0xffffffff, "\033On", +2, 0},
	{SDLK_KP_MEMDIVIDE, 0xffffffff, "\033Oo", +2, 0},
	{SDLK_KP_0, 0xffffffff, "\033Op", +2, 0},
	{SDLK_KP_1, 0xffffffff, "\033Oq", +2, 0},
	{SDLK_KP_2, 0xffffffff, "\033Or", +2, 0},
	{SDLK_KP_3, 0xffffffff, "\033Os", +2, 0},
	{SDLK_KP_4, 0xffffffff, "\033Ot", +2, 0},
	{SDLK_KP_5, 0xffffffff, "\033Ou", +2, 0},
	{SDLK_KP_6, 0xffffffff, "\033Ov", +2, 0},
	{SDLK_KP_7, 0xffffffff, "\033Ow", +2, 0},
	{SDLK_KP_8, 0xffffffff, "\033Ox", +2, 0},
	{SDLK_KP_9, 0xffffffff, "\033Oy", +2, 0},
	
	// normal
	{SDLK_UP, KMOD_SHIFT, "\033[1;2A", 0, 0},
	{SDLK_UP, KMOD_ALT, "\033[1;3A", 0, 0},
	{SDLK_UP, KMOD_SHIFT|KMOD_ALT, "\033[1;4A", 0, 0},
	{SDLK_UP, KMOD_CTRL, "\033[1;5A", 0, 0},
	{SDLK_UP, KMOD_SHIFT|KMOD_CTRL, "\033[1;6A", 0, 0},
	{SDLK_UP, KMOD_CTRL|KMOD_ALT, "\033[1;7A", 0, 0},
	{SDLK_UP, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[1;8A", 0, 0},
	{SDLK_UP, 0xffffffff, "\033[A", 0, -1},
	{SDLK_UP, 0xffffffff, "\033OA", 0, +1},
	{SDLK_DOWN, KMOD_SHIFT, "\033[1;2B", 0, 0},
	{SDLK_DOWN, KMOD_ALT, "\033[1;3B", 0, 0},
	{SDLK_DOWN, KMOD_SHIFT|KMOD_ALT, "\033[1;4B", 0, 0},
	{SDLK_DOWN, KMOD_CTRL, "\033[1;5B", 0, 0},
	{SDLK_DOWN, KMOD_SHIFT|KMOD_CTRL, "\033[1;6B", 0, 0},
	{SDLK_DOWN, KMOD_CTRL|KMOD_ALT, "\033[1;7B", 0, 0},
	{SDLK_DOWN, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[1;8B", 0, 0},
	{SDLK_DOWN, 0xffffffff, "\033[B", 0, -1},
	{SDLK_DOWN, 0xffffffff, "\033OB", 0, +1},
	{SDLK_LEFT, KMOD_SHIFT, "\033[1;2D", 0, 0},
	{SDLK_LEFT, KMOD_ALT, "\033[1;3D", 0, 0},
	{SDLK_LEFT, KMOD_SHIFT|KMOD_ALT, "\033[1;4D", 0, 0},
	{SDLK_LEFT, KMOD_CTRL, "\033[1;5D", 0, 0},
	{SDLK_LEFT, KMOD_SHIFT|KMOD_CTRL, "\033[1;6D", 0, 0},
	{SDLK_LEFT, KMOD_CTRL|KMOD_ALT, "\033[1;7D", 0, 0},
	{SDLK_LEFT, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[1;8D", 0, 0},
	{SDLK_LEFT, 0xffffffff, "\033[D", 0, -1},
	{SDLK_LEFT, 0xffffffff, "\033OD", 0, +1},
	{SDLK_RIGHT, KMOD_SHIFT, "\033[1;2C", 0, 0},
	{SDLK_RIGHT, KMOD_ALT, "\033[1;3C", 0, 0},
	{SDLK_RIGHT, KMOD_SHIFT|KMOD_ALT, "\033[1;4C", 0, 0},
	{SDLK_RIGHT, KMOD_CTRL, "\033[1;5C", 0, 0},
	{SDLK_RIGHT, KMOD_SHIFT|KMOD_CTRL, "\033[1;6C", 0, 0},
	{SDLK_RIGHT, KMOD_CTRL|KMOD_ALT, "\033[1;7C", 0, 0},
	{SDLK_RIGHT, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[1;8C", 0, 0},
	{SDLK_RIGHT, 0xffffffff, "\033[C", 0, -1},
	{SDLK_RIGHT, 0xffffffff, "\033OC", 0, +1},
	{SDLK_TAB, KMOD_SHIFT, "\033[Z", 0, 0},
	// Modified Enter entries — Kitty keyboard protocol
	{SDLK_RETURN, KMOD_ALT, "\033[13;3u", 0, 0},
	{SDLK_RETURN, KMOD_SHIFT, "\033[13;2u", 0, 0},
	{SDLK_RETURN, KMOD_CTRL, "\033[13;5u", 0, 0},
	{SDLK_RETURN, 0xffffffff, "\r", 0, 0},
	{SDLK_INSERT, KMOD_SHIFT, "\033[4l", -1, 0},
	{SDLK_INSERT, KMOD_SHIFT, "\033[2;2~", +1, 0},
	{SDLK_INSERT, KMOD_CTRL, "\033[L", -1, 0},
	{SDLK_INSERT, KMOD_CTRL, "\033[2;5~", +1, 0},
	{SDLK_INSERT, 0xffffffff, "\033[4h", -1, 0},
	{SDLK_INSERT, 0xffffffff, "\033[2~", +1, 0},
	{SDLK_DELETE, KMOD_CTRL, "\033[M", -1, 0},
	{SDLK_DELETE, KMOD_CTRL, "\033[3;5~", +1, 0},
	{SDLK_DELETE, KMOD_SHIFT, "\033[2K", -1, 0},
	{SDLK_DELETE, KMOD_SHIFT, "\033[3;2~", +1, 0},
	{SDLK_DELETE, 0xffffffff, "\033[P", -1, 0},
	{SDLK_DELETE, 0xffffffff, "\033[3~", +1, 0},
	// {SDLK_BACKSPACE, Mod1Mask, "\033\177", 0, 0},
	{SDLK_HOME, KMOD_SHIFT, "\033[2J", 0, -1},
	{SDLK_HOME, KMOD_SHIFT, "\033[1;2H", 0, +1},
	{SDLK_HOME, 0xffffffff, "\033[H", 0, -1},
	{SDLK_HOME, 0xffffffff, "\033[1~", 0, +1},
	{SDLK_END, KMOD_CTRL, "\033[J", -1, 0},
	{SDLK_END, KMOD_CTRL, "\033[1;5F", +1, 0},
	{SDLK_END, KMOD_SHIFT, "\033[K", -1, 0},
	{SDLK_END, KMOD_SHIFT, "\033[1;2F", +1, 0},
	{SDLK_END, 0xffffffff, "\033[4~", 0, 0},
	{SDLK_PAGEDOWN, 0xffffffff, "\033[6~", 0, 0},
	{SDLK_PAGEUP, 0xffffffff, "\033[5~", 0, 0},
	{SDLK_PRIOR, KMOD_CTRL, "\033[5;5~", 0, 0},
	{SDLK_PRIOR, KMOD_SHIFT, "\033[5;2~", 0, 0},
	{SDLK_PRIOR, 0xffffffff, "\033[5~", 0, 0},
	{SDLK_AUDIONEXT, KMOD_CTRL, "\033[6;5~", 0, 0},
	{SDLK_AUDIONEXT, KMOD_SHIFT, "\033[6;2~", 0, 0},
	{SDLK_AUDIONEXT, 0xffffffff, "\033[6~", 0, 0},
	
	// function keys — modifiers use kitty CSI-u encoding (F1=57364 … F12=57375)
	//   ;2=Shift ;3=Alt ;4=Shift+Alt ;5=Ctrl ;6=Shift+Ctrl ;7=Ctrl+Alt ;8=Shift+Ctrl+Alt
	{SDLK_F1, KMOD_NONE, "\033OP", 0, 0},
	{SDLK_F1, KMOD_SHIFT, "\033[57364;2u", 0, 0},
	{SDLK_F1, KMOD_ALT, "\033[57364;3u", 0, 0},
	{SDLK_F1, KMOD_SHIFT|KMOD_ALT, "\033[57364;4u", 0, 0},
	{SDLK_F1, KMOD_CTRL, "\033[57364;5u", 0, 0},
	{SDLK_F1, KMOD_SHIFT|KMOD_CTRL, "\033[57364;6u", 0, 0},
	{SDLK_F1, KMOD_CTRL|KMOD_ALT, "\033[57364;7u", 0, 0},
	{SDLK_F1, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57364;8u", 0, 0},
	{SDLK_F2, KMOD_NONE, "\033OQ", 0, 0},
	{SDLK_F2, KMOD_SHIFT, "\033[57365;2u", 0, 0},
	{SDLK_F2, KMOD_ALT, "\033[57365;3u", 0, 0},
	{SDLK_F2, KMOD_SHIFT|KMOD_ALT, "\033[57365;4u", 0, 0},
	{SDLK_F2, KMOD_CTRL, "\033[57365;5u", 0, 0},
	{SDLK_F2, KMOD_SHIFT|KMOD_CTRL, "\033[57365;6u", 0, 0},
	{SDLK_F2, KMOD_CTRL|KMOD_ALT, "\033[57365;7u", 0, 0},
	{SDLK_F2, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57365;8u", 0, 0},
	{SDLK_F3, KMOD_NONE, "\033OR", 0, 0},
	{SDLK_F3, KMOD_SHIFT, "\033[57366;2u", 0, 0},
	{SDLK_F3, KMOD_ALT, "\033[57366;3u", 0, 0},
	{SDLK_F3, KMOD_SHIFT|KMOD_ALT, "\033[57366;4u", 0, 0},
	{SDLK_F3, KMOD_CTRL, "\033[57366;5u", 0, 0},
	{SDLK_F3, KMOD_SHIFT|KMOD_CTRL, "\033[57366;6u", 0, 0},
	{SDLK_F3, KMOD_CTRL|KMOD_ALT, "\033[57366;7u", 0, 0},
	{SDLK_F3, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57366;8u", 0, 0},
	{SDLK_F4, KMOD_NONE, "\033OS", 0, 0},
	{SDLK_F4, KMOD_SHIFT, "\033[57367;2u", 0, 0},
	{SDLK_F4, KMOD_ALT, "\033[57367;3u", 0, 0},
	{SDLK_F4, KMOD_SHIFT|KMOD_ALT, "\033[57367;4u", 0, 0},
	{SDLK_F4, KMOD_CTRL, "\033[57367;5u", 0, 0},
	{SDLK_F4, KMOD_SHIFT|KMOD_CTRL, "\033[57367;6u", 0, 0},
	{SDLK_F4, KMOD_CTRL|KMOD_ALT, "\033[57367;7u", 0, 0},
	{SDLK_F4, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57367;8u", 0, 0},
	{SDLK_F5, KMOD_NONE, "\033[15~", 0, 0},
	{SDLK_F5, KMOD_SHIFT, "\033[57368;2u", 0, 0},
	{SDLK_F5, KMOD_ALT, "\033[57368;3u", 0, 0},
	{SDLK_F5, KMOD_SHIFT|KMOD_ALT, "\033[57368;4u", 0, 0},
	{SDLK_F5, KMOD_CTRL, "\033[57368;5u", 0, 0},
	{SDLK_F5, KMOD_SHIFT|KMOD_CTRL, "\033[57368;6u", 0, 0},
	{SDLK_F5, KMOD_CTRL|KMOD_ALT, "\033[57368;7u", 0, 0},
	{SDLK_F5, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57368;8u", 0, 0},
	{SDLK_F6, KMOD_NONE, "\033[17~", 0, 0},
	{SDLK_F6, KMOD_SHIFT, "\033[57369;2u", 0, 0},
	{SDLK_F6, KMOD_ALT, "\033[57369;3u", 0, 0},
	{SDLK_F6, KMOD_SHIFT|KMOD_ALT, "\033[57369;4u", 0, 0},
	{SDLK_F6, KMOD_CTRL, "\033[57369;5u", 0, 0},
	{SDLK_F6, KMOD_SHIFT|KMOD_CTRL, "\033[57369;6u", 0, 0},
	{SDLK_F6, KMOD_CTRL|KMOD_ALT, "\033[57369;7u", 0, 0},
	{SDLK_F6, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57369;8u", 0, 0},
	{SDLK_F7, KMOD_NONE, "\033[18~", 0, 0},
	{SDLK_F7, KMOD_SHIFT, "\033[57370;2u", 0, 0},
	{SDLK_F7, KMOD_ALT, "\033[57370;3u", 0, 0},
	{SDLK_F7, KMOD_SHIFT|KMOD_ALT, "\033[57370;4u", 0, 0},
	{SDLK_F7, KMOD_CTRL, "\033[57370;5u", 0, 0},
	{SDLK_F7, KMOD_SHIFT|KMOD_CTRL, "\033[57370;6u", 0, 0},
	{SDLK_F7, KMOD_CTRL|KMOD_ALT, "\033[57370;7u", 0, 0},
	{SDLK_F7, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57370;8u", 0, 0},
	{SDLK_F8, KMOD_NONE, "\033[19~", 0, 0},
	{SDLK_F8, KMOD_SHIFT, "\033[57371;2u", 0, 0},
	{SDLK_F8, KMOD_ALT, "\033[57371;3u", 0, 0},
	{SDLK_F8, KMOD_SHIFT|KMOD_ALT, "\033[57371;4u", 0, 0},
	{SDLK_F8, KMOD_CTRL, "\033[57371;5u", 0, 0},
	{SDLK_F8, KMOD_SHIFT|KMOD_CTRL, "\033[57371;6u", 0, 0},
	{SDLK_F8, KMOD_CTRL|KMOD_ALT, "\033[57371;7u", 0, 0},
	{SDLK_F8, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57371;8u", 0, 0},
	{SDLK_F9, KMOD_NONE, "\033[20~", 0, 0},
	{SDLK_F9, KMOD_SHIFT, "\033[57372;2u", 0, 0},
	{SDLK_F9, KMOD_ALT, "\033[57372;3u", 0, 0},
	{SDLK_F9, KMOD_SHIFT|KMOD_ALT, "\033[57372;4u", 0, 0},
	{SDLK_F9, KMOD_CTRL, "\033[57372;5u", 0, 0},
	{SDLK_F9, KMOD_SHIFT|KMOD_CTRL, "\033[57372;6u", 0, 0},
	{SDLK_F9, KMOD_CTRL|KMOD_ALT, "\033[57372;7u", 0, 0},
	{SDLK_F9, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57372;8u", 0, 0},
	{SDLK_F10, KMOD_NONE, "\033[21~", 0, 0},
	{SDLK_F10, KMOD_SHIFT, "\033[57373;2u", 0, 0},
	{SDLK_F10, KMOD_ALT, "\033[57373;3u", 0, 0},
	{SDLK_F10, KMOD_SHIFT|KMOD_ALT, "\033[57373;4u", 0, 0},
	{SDLK_F10, KMOD_CTRL, "\033[57373;5u", 0, 0},
	{SDLK_F10, KMOD_SHIFT|KMOD_CTRL, "\033[57373;6u", 0, 0},
	{SDLK_F10, KMOD_CTRL|KMOD_ALT, "\033[57373;7u", 0, 0},
	{SDLK_F10, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57373;8u", 0, 0},
	{SDLK_F11, KMOD_NONE, "\033[23~", 0, 0},
	{SDLK_F11, KMOD_SHIFT, "\033[57374;2u", 0, 0},
	{SDLK_F11, KMOD_ALT, "\033[57374;3u", 0, 0},
	{SDLK_F11, KMOD_SHIFT|KMOD_ALT, "\033[57374;4u", 0, 0},
	{SDLK_F11, KMOD_CTRL, "\033[57374;5u", 0, 0},
	{SDLK_F11, KMOD_SHIFT|KMOD_CTRL, "\033[57374;6u", 0, 0},
	{SDLK_F11, KMOD_CTRL|KMOD_ALT, "\033[57374;7u", 0, 0},
	{SDLK_F11, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57374;8u", 0, 0},
	{SDLK_F12, KMOD_NONE, "\033[24~", 0, 0},
	{SDLK_F12, KMOD_SHIFT, "\033[57375;2u", 0, 0},
	{SDLK_F12, KMOD_ALT, "\033[57375;3u", 0, 0},
	{SDLK_F12, KMOD_SHIFT|KMOD_ALT, "\033[57375;4u", 0, 0},
	{SDLK_F12, KMOD_CTRL, "\033[57375;5u", 0, 0},
	{SDLK_F12, KMOD_SHIFT|KMOD_CTRL, "\033[57375;6u", 0, 0},
	{SDLK_F12, KMOD_CTRL|KMOD_ALT, "\033[57375;7u", 0, 0},
	{SDLK_F12, KMOD_SHIFT|KMOD_CTRL|KMOD_ALT, "\033[57375;8u", 0, 0},
	{SDLK_F13, KMOD_NONE, "\033[1;2P", 0, 0},
	{SDLK_F14, KMOD_NONE, "\033[1;2Q", 0, 0},
	{SDLK_F15, KMOD_NONE, "\033[1;2R", 0, 0},
	{SDLK_F16, KMOD_NONE, "\033[1;2S", 0, 0},
	{SDLK_F17, KMOD_NONE, "\033[15;2~", 0, 0},
	{SDLK_F18, KMOD_NONE, "\033[17;2~", 0, 0},
	{SDLK_F19, KMOD_NONE, "\033[18;2~", 0, 0},
	{SDLK_F20, KMOD_NONE, "\033[19;2~", 0, 0},
	{SDLK_F21, KMOD_NONE, "\033[20;2~", 0, 0},
	{SDLK_F22, KMOD_NONE, "\033[21;2~", 0, 0},
	{SDLK_F23, KMOD_NONE, "\033[23;2~", 0, 0},
	{SDLK_F24, KMOD_NONE, "\033[24;2~", 0, 0},
};
