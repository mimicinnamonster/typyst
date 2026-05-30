/* See LICENSE.md for license details. */

static char *font = "Ubuntu Mono:pixelsize=18:antialias=true:autohint=true";
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
	// {SDLK_UP, Mod1Mask, "\033[1;3A", 0, 0},
	// {SDLK_UP, KMOD_SHIFT|Mod1Mask, "\033[1;4A", 0, 0},
	{SDLK_UP, KMOD_CTRL, "\033[1;5A", 0, 0},
	{SDLK_UP, KMOD_SHIFT|KMOD_CTRL, "\033[1;6A", 0, 0},
	// {SDLK_UP, KMOD_CTRL|Mod1Mask, "\033[1;7A", 0, 0},
	// {SDLK_UP, KMOD_SHIFT|KMOD_CTRL|Mod1Mask, "\033[1;8A", 0, 0},
	{SDLK_UP, 0xffffffff, "\033[A", 0, -1},
	{SDLK_UP, 0xffffffff, "\033OA", 0, +1},
	{SDLK_DOWN, KMOD_SHIFT, "\033[1;2B", 0, 0},
	// {SDLK_DOWN, Mod1Mask, "\033[1;3B", 0, 0},
	// {SDLK_DOWN, KMOD_SHIFT|Mod1Mask, "\033[1;4B", 0, 0},
	{SDLK_DOWN, KMOD_CTRL, "\033[1;5B", 0, 0},
	{SDLK_DOWN, KMOD_SHIFT|KMOD_CTRL, "\033[1;6B", 0, 0},
	// {SDLK_DOWN, KMOD_CTRL|Mod1Mask, "\033[1;7B", 0, 0},
	// {SDLK_DOWN, KMOD_SHIFT|KMOD_CTRL|Mod1Mask, "\033[1;8B", 0, 0},
	{SDLK_DOWN, 0xffffffff, "\033[B", 0, -1},
	{SDLK_DOWN, 0xffffffff, "\033OB", 0, +1},
	{SDLK_LEFT, KMOD_SHIFT, "\033[1;2D", 0, 0},
	// {SDLK_LEFT, Mod1Mask, "\033[1;3D", 0, 0},
	// {SDLK_LEFT, KMOD_SHIFT|Mod1Mask, "\033[1;4D", 0, 0},
	{SDLK_LEFT, KMOD_CTRL, "\033[1;5D", 0, 0},
	{SDLK_LEFT, KMOD_SHIFT|KMOD_CTRL, "\033[1;6D", 0, 0},
	// {SDLK_LEFT, KMOD_CTRL|Mod1Mask, "\033[1;7D", 0, 0},
	// {SDLK_LEFT, KMOD_SHIFT|KMOD_CTRL|Mod1Mask, "\033[1;8D", 0, 0},
	{SDLK_LEFT, 0xffffffff, "\033[D", 0, -1},
	{SDLK_LEFT, 0xffffffff, "\033OD", 0, +1},
	{SDLK_RIGHT, KMOD_SHIFT, "\033[1;2C", 0, 0},
	// {SDLK_RIGHT, Mod1Mask, "\033[1;3C", 0, 0},
	// {SDLK_RIGHT, KMOD_SHIFT|Mod1Mask, "\033[1;4C", 0, 0},
	{SDLK_RIGHT, KMOD_CTRL, "\033[1;5C", 0, 0},
	{SDLK_RIGHT, KMOD_SHIFT|KMOD_CTRL, "\033[1;6C", 0, 0},
	// {SDLK_RIGHT, KMOD_CTRL|Mod1Mask, "\033[1;7C", 0, 0},
	// {SDLK_RIGHT, KMOD_SHIFT|KMOD_CTRL|Mod1Mask, "\033[1;8C", 0, 0},
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
	
	// function keys
	{SDLK_F1, KMOD_NONE, "\033OP", 0, 0},
	{SDLK_F1, /*F13*/KMOD_SHIFT, "\033[1;2P", 0, 0},
	{SDLK_F1, /*F25*/KMOD_CTRL, "\033[1;5P", 0, 0},
	// {SDLK_F1, /*F37*/Mod4Mask, "\033[1;6P", 0, 0},
	// {SDLK_F1, /*F49*/Mod1Mask, "\033[1;3P", 0, 0},
	// {SDLK_F1, /*F61*/Mod3Mask, "\033[1;4P", 0, 0},
	{SDLK_F2, KMOD_NONE, "\033OQ", 0, 0},
	{SDLK_F2, /*F14*/KMOD_SHIFT, "\033[1;2Q", 0, 0},
	{SDLK_F2, /*F26*/KMOD_CTRL, "\033[1;5Q", 0, 0},
	// {SDLK_F2, /*F38*/Mod4Mask, "\033[1;6Q", 0, 0},
	// {SDLK_F2, /*F50*/Mod1Mask, "\033[1;3Q", 0, 0},
	// {SDLK_F2, /*F62*/Mod3Mask, "\033[1;4Q", 0, 0},
	{SDLK_F3, KMOD_NONE, "\033OR", 0, 0},
	{SDLK_F3, /*F15*/KMOD_SHIFT, "\033[1;2R", 0, 0},
	{SDLK_F3, /*F27*/KMOD_CTRL, "\033[1;5R", 0, 0},
	// {SDLK_F3, /*F39*/Mod4Mask, "\033[1;6R", 0, 0},
	// {SDLK_F3, /*F51*/Mod1Mask, "\033[1;3R", 0, 0},
	// {SDLK_F3, /*F63*/Mod3Mask, "\033[1;4R", 0, 0},
	{SDLK_F4, KMOD_NONE, "\033OS", 0, 0},
	{SDLK_F4, /*F16*/KMOD_SHIFT, "\033[1;2S", 0, 0},
	{SDLK_F4, /*F28*/KMOD_CTRL, "\033[1;5S", 0, 0},
	// {SDLK_F4, /*F40*/Mod4Mask, "\033[1;6S", 0, 0},
	// {SDLK_F4, /*F52*/Mod1Mask, "\033[1;3S", 0, 0},
	{SDLK_F5, KMOD_NONE, "\033[15~", 0, 0},
	{SDLK_F5, /*F17*/KMOD_SHIFT, "\033[15;2~", 0, 0},
	{SDLK_F5, /*F29*/KMOD_CTRL, "\033[15;5~", 0, 0},
	// {SDLK_F5, /*F41*/Mod4Mask, "\033[15;6~", 0, 0},
	// {SDLK_F5, /*F53*/Mod1Mask, "\033[15;3~", 0, 0},
	{SDLK_F6, KMOD_NONE, "\033[17~", 0, 0},
	{SDLK_F6, /*F18*/KMOD_SHIFT, "\033[17;2~", 0, 0},
	{SDLK_F6, /*F30*/KMOD_CTRL, "\033[17;5~", 0, 0},
	// {SDLK_F6, /*F42*/Mod4Mask, "\033[17;6~", 0, 0},
	// {SDLK_F6, /*F54*/Mod1Mask, "\033[17;3~", 0, 0},
	{SDLK_F7, KMOD_NONE, "\033[18~", 0, 0},
	{SDLK_F7, /*F19*/KMOD_SHIFT, "\033[18;2~", 0, 0},
	{SDLK_F7, /*F31*/KMOD_CTRL, "\033[18;5~", 0, 0},
	// {SDLK_F7, /*F43*/Mod4Mask, "\033[18;6~", 0, 0},
	// {SDLK_F7, /*F55*/Mod1Mask, "\033[18;3~", 0, 0},
	{SDLK_F8, KMOD_NONE, "\033[19~", 0, 0},
	{SDLK_F8, /*F20*/KMOD_SHIFT, "\033[19;2~", 0, 0},
	{SDLK_F8, /*F32*/KMOD_CTRL, "\033[19;5~", 0, 0},
	// {SDLK_F8, /*F44*/Mod4Mask, "\033[19;6~", 0, 0},
	// {SDLK_F8, /*F56*/Mod1Mask, "\033[19;3~", 0, 0},
	{SDLK_F9, KMOD_NONE, "\033[20~", 0, 0},
	{SDLK_F9, /*F21*/KMOD_SHIFT, "\033[20;2~", 0, 0},
	{SDLK_F9, /*F33*/KMOD_CTRL, "\033[20;5~", 0, 0},
	// {SDLK_F9, /*F45*/Mod4Mask, "\033[20;6~", 0, 0},
	// {SDLK_F9, /*F57*/Mod1Mask, "\033[20;3~", 0, 0},
	{SDLK_F10, KMOD_NONE, "\033[21~", 0, 0},
	{SDLK_F10, /*F22*/KMOD_SHIFT, "\033[21;2~", 0, 0},
	{SDLK_F10, /*F34*/KMOD_CTRL, "\033[21;5~", 0, 0},
	// {SDLK_F10, /*F46*/Mod4Mask, "\033[21;6~", 0, 0},
	// {SDLK_F10, /*F58*/Mod1Mask, "\033[21;3~", 0, 0},
	{SDLK_F11, KMOD_NONE, "\033[23~", 0, 0},
	{SDLK_F11, /*F23*/KMOD_SHIFT, "\033[23;2~", 0, 0},
	{SDLK_F11, /*F35*/KMOD_CTRL, "\033[23;5~", 0, 0},
	// {SDLK_F11, /*F47*/Mod4Mask, "\033[23;6~", 0, 0},
	// {SDLK_F11, /*F59*/Mod1Mask, "\033[23;3~", 0, 0},
	{SDLK_F12, KMOD_NONE, "\033[24~", 0, 0},
	{SDLK_F12, /*F24*/KMOD_SHIFT, "\033[24;2~", 0, 0},
	{SDLK_F12, /*F36*/KMOD_CTRL, "\033[24;5~", 0, 0},
	// {SDLK_F12, /*F48*/Mod4Mask, "\033[24;6~", 0, 0},
	// {SDLK_F12, /*F60*/Mod1Mask, "\033[24;3~", 0, 0},
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
