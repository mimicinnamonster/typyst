/*
 * Headless regression test for cursor rendering in drawcursor()
 * (src/main.c). NOT end-to-end: runs without a window.
 *
 * Unlike csiu_micro_test.c this does NOT copy code verbatim — it compiles
 * the REAL src/main.c (with main() renamed) and calls the production
 * drawcursor()/drawglyph()/getglyphwidth() directly.
 *
 * Behavior under test:
 *
 * 1. Ghost cursor: drawcursor() must restore the cell under the previous
 *    cursor position even when MODE_HIDE (DECTCEM) is set, and must not
 *    guard the restore with "if the cursor moved". drawglyph() only stamps
 *    into the persistent win.glyphs[] array (rendered later by
 *    render_glyphs()), so a fg/bg-swapped glyph left behind stays on
 *    screen as a ghost until that row is fully redrawn, and a hidden
 *    cursor would never visually disappear.
 *
 * 2. Inverted cursor (rendered-colors invariant): selectglyphcolors()
 *    swaps fg/bg AT RENDER TIME for cells carrying ATTR_REVERSE and
 *    collapses fg/bg for ATTR_INVISIBLE/ATTR_BLINK. The cursor stamp
 *    must invert the RENDERED colors: those attrs cleared, the stored
 *    pair swapped once on normal cells — but stamped AS-IS (no swap) on
 *    reverse cells, where the renderer already swapped. Re-swapping
 *    there cancels the highlight's inversion and the cursor renders as
 *    plain text (no visible blink on vim MatchParen/Error cells).
 *
 * 3. Cursor blink: with win.cursoron == 0 (blink-off phase) the cursor
 *    cell is stamped with its own glyph VERBATIM (attrs and colors
 *    untouched) so the cursor vanishes without disturbing content. With
 *    win.cursoron == 1 the stamp is the inverted cursor from (2).
 *    MODE_HIDE always suppresses the cursor stamp.
 *
 * A fake single-entry FontSet (charset containing 'X', widths[] preset to
 * -1 so wcwidth() resolves) lets getglyphwidth() work without loading real
 * fonts or opening a display.
 *
 * Expected result against the current drawcursor() (strips attrs, then
 * ALWAYS swaps the stored pair): the reverse-cell no-swap checks (cases
 * 1, 4, 7) FAIL — that stamp double-inverts and renders identical to
 * the cell's own content. Against the rendered-colors-invert
 * drawcursor(): all checks pass.
 *
 * Build & run (same flags/linkage as the Makefile, all src objects except
 * main.o which is compiled into this TU):
 *   make _build/st.o _build/cache.o _build/anim.o
 *   gcc -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE -std=c11 -g -Wall \
 *     `pkg-config --cflags fontconfig sdl2 SDL2_ttf SDL2_gfx` \
 *     -o /tmp/test_cursor_ghost tests/test_cursor_ghost.c \
 *     _build/st.o _build/cache.o _build/anim.o \
 *     `pkg-config --libs fontconfig sdl2 SDL2_ttf SDL2_gfx` -lutil \
 *     -Wl,-rpath,`brew --prefix sdl3`/lib \
 *   && /tmp/test_cursor_ghost
 */
/* Rename main.c's entry point so this TU can define its own main(). */
#define main typyst_main
#include "../src/main.c"
#undef main

#include <stdio.h>

static int failures = 0;
static int checks = 0;

static void
expect(int cond, const char *msg, int line)
{
	checks++;
	if (cond) {
		printf("ok %d - %s\n", checks, msg);
	} else {
		failures++;
		printf("FAIL %d - %s (tests/test_cursor_ghost.c:%d)\n",
		       checks, msg, line);
	}
}

static int
glyph_eq(Glyph a, Glyph b)
{
	return a.u == b.u && a.mode == b.mode && a.fg == b.fg && a.bg == b.bg;
}

/* Classic inverted cursor stamp: exactly one fg/bg swap and no
 * render-time double-inversion attrs left on the glyph. */
static void
expect_cursor_invert(int id, Glyph cell, const char *msg, int line)
{
	Glyph s = win.glyphs[id];
	expect(!(s.mode & (ATTR_REVERSE | ATTR_INVISIBLE | ATTR_BLINK)) &&
	       s.fg == cell.bg && s.bg == cell.fg, msg, line);
}

/* Cursor stamp on a REVERSE-video cell: the renderer swaps colors once
 * for ATTR_REVERSE, so the stamp must carry the stored pair with the
 * attrs cleared and NO additional swap — the two swaps would cancel and
 * render the cursor identical to the cell's own content. */
static void
expect_cursor_noswap(int id, Glyph cell, const char *msg, int line)
{
	Glyph s = win.glyphs[id];
	expect(!(s.mode & (ATTR_REVERSE | ATTR_INVISIBLE | ATTR_BLINK)) &&
	       s.fg == cell.fg && s.bg == cell.bg, msg, line);
}

/* Blink-off stamp: the cell's own glyph, attrs and colors untouched. */
static void
expect_cursor_off(int id, Glyph cell, const char *msg, int line)
{
	expect(glyph_eq(win.glyphs[id], cell), msg, line);
}

static FontSet test_fs;
static int test_widths[128];

static void
setup_fake_font(void)
{
	memset(&test_fs, 0, sizeof(test_fs));
	memset(test_widths, 0xff, sizeof(test_widths)); /* all = -1 */
	test_fs.font.charset = FcCharSetCreate();
	FcCharSetAddChar(test_fs.font.charset, 'X');
	test_fs.font.widths = test_widths;
	dc.fontsets = &test_fs;
	dc.fontsetlen = 1;
}

int
main(void)
{
	int A_id, B_id, R_id, I_id, S_id, E_id, RB_id, TC_id, RR_id;
	Glyph content, moved, rev, inv, same, errrev, errbg, tcred, revred;

	setup_fake_font();

	win.glyphs = calloc((size_t)cols * rows, sizeof(Glyph));
	assert(win.glyphs);
	win.mode = MODE_VISIBLE; /* ensure MODE_HIDE is clear */
	win.cw = 10;
	win.ch = 20;
	win.cursoron = 1; /* blink-on phase */
	win.lastblink = 0;
	printf("grid %dx%d, ghost cell A=(10,5), cursor cell B=(20,5)\n",
	       cols, rows);

	/* Underlying cell content (also the glyph under the frame-1 cursor). */
	content = (Glyph){ .u = 'X', .mode = 0, .fg = 7, .bg = 0 };
	/* Content at B after the cursor moved there. */
	moved = (Glyph){ .u = 'X', .mode = 0, .fg = 15, .bg = 8 };

	A_id = 5 * cols + 10;
	B_id = 5 * cols + 20;

	/* Frame 1: cursor visible at A over `content` (no movement yet). */
	drawcursor(10, 5, content, 10, 5, content);
	expect_cursor_invert(A_id, content,
	       "frame 1: cursor painted as classic invert at A", __LINE__);

	/* Frame 2: app sends DECTCEM off (\e[?25l) and the cursor logically
	 * moves to B. draw() calls drawcursor(B..., old A), then advances
	 * term.ocx/ocy to B. The cell at A must be restored even though the
	 * cursor is now hidden. */
	win.mode |= MODE_HIDE;
	drawcursor(20, 5, moved, 10, 5, content);
	expect(glyph_eq(win.glyphs[A_id], content),
	       "frame 2: hidden cursor restores cell A (no ghost / "
	       "not-stuck-visible bug)", __LINE__);

	/* Frame 3: DECTCEM on (\e[?25h). drawcursor(B, old B): the previous
	 * "if (cx != ox || cy != oy)" guard skipped the restore entirely, so
	 * any ghost at A would persist until the row is fully dirtied. */
	win.mode &= ~MODE_HIDE;
	drawcursor(20, 5, moved, 20, 5, moved);
	expect(glyph_eq(win.glyphs[A_id], content),
	       "frame 3: ghost at A gone after cursor shown at B", __LINE__);
	expect_cursor_invert(B_id, moved,
	       "frame 3: cursor painted as classic invert at B", __LINE__);

	/* ---- Inverted cursor on special cells (blink-on phase). The stamp
	 * must carry exactly one swap with the offending attrs cleared —
	 * selectglyphcolors() swaps again at render time otherwise and the
	 * cursor vanishes on these cells. ---- */

	/* Case 1: reverse-video cell (vim Visual selection, tmux/htop
	 * headers, man pages...). */
	rev = (Glyph){ .u = 'X', .mode = ATTR_REVERSE, .fg = 7, .bg = 0 };
	R_id = 6 * cols + 11;
	drawcursor(11, 6, rev, 11, 6, rev);
	expect(!(win.glyphs[R_id].mode & ATTR_REVERSE),
	       "case 1: ATTR_REVERSE cleared in cursor stamp", __LINE__);
	expect_cursor_noswap(R_id, rev,
	       "case 1: reverse cell stamps stored pair (render swaps already)",
	       __LINE__);

	/* Case 2: SGR 8 cell (hidden text). ATTR_INVISIBLE collapses fg to
	 * bg at render time — the cursor would vanish without clearing. */
	inv = (Glyph){ .u = 'X', .mode = ATTR_INVISIBLE | ATTR_BLINK,
	               .fg = 7, .bg = 0 };
	I_id = 7 * cols + 12;
	drawcursor(12, 7, inv, 12, 7, inv);
	expect(!(win.glyphs[I_id].mode & (ATTR_INVISIBLE | ATTR_BLINK)),
	       "case 2: ATTR_INVISIBLE/ATTR_BLINK cleared in cursor stamp",
	       __LINE__);
	expect_cursor_invert(I_id, inv,
	       "case 2: exactly one swap on hidden-text cell", __LINE__);

	/* Case 3: cell with fg == bg — the swap is a no-op; assert the
	 * swapped values only (accepted: block color == text color). */
	same = (Glyph){ .u = 'X', .mode = 0, .fg = 4, .bg = 4 };
	S_id = 8 * cols + 13;
	drawcursor(13, 8, same, 13, 8, same);
	expect_cursor_invert(S_id, same,
	       "case 3: fg==bg cell stamps swapped (equal) colors", __LINE__);

	/* ---- Red/error-highlighted cells (vim Error, SpellBad): the
	 * cursor keeps the classic invert instead of vanishing. ---- */

	/* Case 4: reverse-video cell with red fg (SGR 7 + red fg — classic
	 * `hi Error cterm=reverse`). */
	errrev = (Glyph){ .u = 'X', .mode = ATTR_REVERSE, .fg = 1, .bg = 0 };
	E_id = 9 * cols + 14;
	drawcursor(14, 9, errrev, 14, 9, errrev);
	expect(!(win.glyphs[E_id].mode & ATTR_REVERSE),
	       "case 4: ATTR_REVERSE cleared on red error cell", __LINE__);
	expect_cursor_noswap(E_id, errrev,
	       "case 4: reverse-red cell stamps stored pair (no swap)",
	       __LINE__);

	/* Case 7: reverse-red on a light background (SGR 7 + red fg, stored
	 * bg = 7) — same rendered-colors invariant as cases 1/4. */
	revred = (Glyph){ .u = 'X', .mode = ATTR_REVERSE, .fg = 1, .bg = 7 };
	RR_id = 12 * cols + 17;
	drawcursor(17, 12, revred, 17, 12, revred);
	expect(!(win.glyphs[RR_id].mode & ATTR_REVERSE),
	       "case 7: ATTR_REVERSE cleared on red-reverse cell", __LINE__);
	expect_cursor_noswap(RR_id, revred,
	       "case 7: red-reverse cell stamps stored pair (no swap)",
	       __LINE__);

	/* Case 5: red background cell (SGR 41 error highlight). */
	errbg = (Glyph){ .u = 'X', .mode = 0, .fg = 7, .bg = 1 };
	RB_id = 10 * cols + 15;
	drawcursor(15, 10, errbg, 15, 10, errbg);
	expect_cursor_invert(RB_id, errbg,
	       "case 5: swapped colors on red-background cell", __LINE__);

	/* Case 6: truecolor red fg (SGR 38;2;255;0;0). TRUECOLOR() packs the
	 * RGB into the fg index itself (st.h), so the glyph is constructible
	 * without any runtime state. */
	tcred = (Glyph){ .u = 'X', .mode = 0,
	                 .fg = TRUECOLOR(255, 0, 0), .bg = 0 };
	TC_id = 11 * cols + 16;
	drawcursor(16, 11, tcred, 16, 11, tcred);
	expect_cursor_invert(TC_id, tcred,
	       "case 6: swapped colors on truecolor-red cell", __LINE__);

	/* ---- Blink-off phase: the cursor vanishes; the cell is stamped
	 * with its own glyph VERBATIM (attrs and colors untouched). ---- */
	win.cursoron = 0;
	drawcursor(20, 5, moved, 20, 5, moved);
	expect_cursor_off(B_id, moved,
	       "blink-off: normal cell stamped verbatim", __LINE__);

	win.cursoron = 1;
	drawcursor(11, 6, rev, 11, 6, rev); /* back to on-phase first */
	win.cursoron = 0;
	drawcursor(11, 6, rev, 11, 6, rev);
	expect_cursor_off(R_id, rev,
	       "blink-off: reverse-video cell stamped verbatim (attrs kept)",
	       __LINE__);

	win.cursoron = 1;
	drawcursor(15, 10, errbg, 15, 10, errbg);
	win.cursoron = 0;
	drawcursor(15, 10, errbg, 15, 10, errbg);
	expect_cursor_off(RB_id, errbg,
	       "blink-off: red-background cell stamped verbatim", __LINE__);

	win.cursoron = 1;
	drawcursor(16, 11, tcred, 16, 11, tcred);
	win.cursoron = 0;
	drawcursor(16, 11, tcred, 16, 11, tcred);
	expect_cursor_off(TC_id, tcred,
	       "blink-off: truecolor-red cell stamped verbatim", __LINE__);

	/* MODE_HIDE suppresses the cursor stamp in any blink phase. */
	win.cursoron = 1;
	win.mode |= MODE_HIDE;
	win.glyphs[B_id] = moved; /* sentinel: must survive untouched */
	drawcursor(20, 5, moved, 20, 5, moved);
	expect_cursor_off(B_id, moved,
	       "MODE_HIDE: nothing stamped at the cursor position", __LINE__);

	if (failures) {
		printf("\n%d/%d checks FAILED — cursor rendering bugs reproduced\n",
		       failures, checks);
		return 1;
	}
	printf("\nall %d checks passed\n", checks);
	return 0;
}
