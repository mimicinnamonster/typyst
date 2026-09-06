/*
 * Micro-test for the Ctrl+Shift(+Alt)+letter kitty CSI-u encoding added to
 * handle_keypress() in src/main.c (NOT end-to-end: runs without a window).
 *
 * The encoding block below is copied VERBATIM from src/main.c
 * (handle_keypress fallback path, from `unsigned char keysz = 1;` through the
 * final `ttywrite(buf, keysz, 1);`). GUI-only side branches (kmap table,
 * clipboard, fontsize resize) are stubbed out but the byte-encoding logic is
 * untouched. If the logic in src/main.c changes, update this copy.
 *
 * Build & run (same SDL2 linkage as the Makefile):
 *   gcc -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE -std=c11 -pedantic -g -Wall \
 *     -DDEBUG `pkg-config --cflags sdl2` -o /tmp/csiu_micro_test \
 *     tests/csiu_micro_test.c `pkg-config --libs sdl2` && /tmp/csiu_micro_test
 */
#include <stdio.h>
#include <string.h>
#include <SDL.h>

/* ---- stubs for GUI-only symbols referenced by the copied block ---- */
static char captured[256];
static size_t captured_len;

static void
ttywrite(const char *s, size_t n, int may_echo)
{
	(void)may_echo;
	if (captured_len + n < sizeof(captured)) {
		memcpy(captured + captured_len, s, n);
		captured_len += n;
	}
}
static void resizefont(void) {}
static void resize(int w, int h) { (void)w; (void)h; }
static float usedfontsize;
static unsigned int glyphcache;

/* ==== BEGIN copied from src/main.c handle_keypress (verbatim) ==== */
static void
encode_fallback(SDL_Event *ev)
{
	unsigned char keysz = 1;
	char buf[16] = { ev->key.keysym.sym };

	int isctrl = ev->key.keysym.mod & KMOD_CTRL;
	int isshift = ev->key.keysym.mod & KMOD_SHIFT;
	int isalt = ev->key.keysym.mod & KMOD_ALT;

	int isfn = (ev->key.keysym.scancode >= SDL_SCANCODE_F1 && ev->key.keysym.scancode <= SDL_SCANCODE_F12);
	int isprint = !(ev->key.keysym.sym & 1<<30);
	int isspec = !(buf[0] >= ' ' && buf[0] <= '~');
	int isletter = buf[0] >= 'A' && buf[0] <= 'z';

	if (isfn) {
		/* no F-key here in the micro-test; unreachable for these inputs */
	} else {
		if (!isprint || (!isspec && !isctrl && !isalt))
			return;


		if (isctrl && buf[0] == ' ') {
			buf[0] = 0;
		}
		if (isletter) {
			if (isctrl) {
				if (isshift) {
					/* Ctrl+Shift(+Alt)+letter: kitty keyboard protocol CSI-u encoding
					 * (ESC[<codepoint>;<mod>u, mod = 1 + shift + alt*2 + ctrl*4, so
					 * 6 for Ctrl+Shift and 8 with Alt). Legacy encoding cannot
					 * distinguish these from Ctrl+letter; sent unconditionally like
					 * Ghostty's default (disambiguate) behavior. SDL always sends
					 * lowercase keysym for letters regardless of shift. */
					int cp = (buf[0] >= 'A' && buf[0] <= 'Z') ?
					    buf[0] + ('a' - 'A') : buf[0];
					keysz = sprintf(buf, "\033[%d;%du", cp, isalt ? 8 : 6);
					ttywrite(buf, keysz, 1);
					return;
				}
				/* Ctrl+letter: produce control character.
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
		resizefont();
		resize(0, 0);
		glyphcache = 0;
		return;
	}

	if (isctrl && !isshift && buf[0] == '-') {
		usedfontsize--;
		resizefont();
		resize(0, 0);
		glyphcache = 0;
		return;
	}

	ttywrite(buf, keysz, 1);
}
/* ==== END copied from src/main.c ==== */

static SDL_KeyboardEvent
mkkey(char sym, unsigned short mod)
{
	SDL_KeyboardEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_KEYDOWN;
	ev.keysym.sym = sym;
	ev.keysym.mod = mod;
	ev.keysym.scancode = SDL_SCANCODE_P;
	return ev;
}

static int failures;

static void
expect(const char *name, const char *want, size_t wantlen)
{
	int ok = captured_len == wantlen && memcmp(captured, want, wantlen) == 0;
	printf("%-24s got %2zu bytes: ", name, captured_len);
	for (size_t i = 0; i < captured_len; i++)
		printf("%02x ", (unsigned char)captured[i]);
	printf("%s\n", ok ? "PASS" : "FAIL");
	if (!ok) {
		printf("  expected %2zu bytes: ", wantlen);
		for (size_t i = 0; i < wantlen; i++)
			printf("%02x ", (unsigned char)want[i]);
		printf("\n");
		failures++;
	}
}

int
main(void)
{
	/* 1. Ctrl+Shift+p -> ESC[112;6u */
	captured_len = 0;
	SDL_KeyboardEvent ev = mkkey('p', KMOD_CTRL | KMOD_SHIFT);
	encode_fallback((SDL_Event *)&ev);
	expect("ctrl+shift+p", "\033[112;6u", 8);

	/* 2. Ctrl+p -> 0x10 (legacy, unchanged) */
	captured_len = 0;
	ev = mkkey('p', KMOD_CTRL);
	encode_fallback((SDL_Event *)&ev);
	expect("ctrl+p", "\x10", 1);

	/* 3. Ctrl+Shift+Alt+p -> ESC[112;8u */
	captured_len = 0;
	ev = mkkey('p', KMOD_CTRL | KMOD_SHIFT | KMOD_ALT);
	encode_fallback((SDL_Event *)&ev);
	expect("ctrl+shift+alt+p", "\033[112;8u", 8);

	/* 4. Regression: plain p -> 0 bytes (printable text is delivered via
	 * SDL_TEXTINPUT, not handle_keypress; the !isspec && !isctrl && !isalt
	 * guard must return early to avoid double input) */
	captured_len = 0;
	ev = mkkey('p', 0);
	encode_fallback((SDL_Event *)&ev);
	expect("plain p (textinput)", "", 0);

	/* 5. Regression: shift+p -> 0 bytes (same TEXTINPUT path) */
	captured_len = 0;
	ev = mkkey('p', KMOD_SHIFT);
	encode_fallback((SDL_Event *)&ev);
	expect("shift+p (textinput)", "", 0);

	/* 6. Regression: alt+p -> ESC p */
	captured_len = 0;
	ev = mkkey('p', KMOD_ALT);
	encode_fallback((SDL_Event *)&ev);
	expect("alt+p", "\033p", 2);

	printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
