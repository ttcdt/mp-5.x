/*

    Minimum Profit - A Text Editor
    Curses driver.

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#ifdef CONFOPT_CURSES

#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <ncurses.h>

#define MPDM_OLD_COMPAT

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/** data **/

/* the curses attributes */
#define MAX_COLORS 100
int nc_attrs[MAX_COLORS];

/* code for the 'normal' attribute */
static int normal_attr = 0;

static int idle_msecs = 0;

/** code **/

static void nc_update_window_size(void)
{
    mpdm_t v;

    v = mpdm_hget_s(MP, L"window");
    mpdm_hset_s(v, L"tx", MPDM_I(COLS));
    mpdm_hset_s(v, L"ty", MPDM_I(LINES - 1));
}


#ifndef NCURSES_VERSION

static void nc_sigwinch(int s)
/* SIGWINCH signal handler */
{
    /* invalidate main window */
    clearok(stdscr, 1);
    refresh();

    /* re-set dimensions */
    nc_update_window_size();

    /* reattach */
    signal(SIGWINCH, nc_sigwinch);
}

#endif

#ifdef CONFOPT_WGET_WCH
int wget_wch(WINDOW * w, wint_t * ch);
#endif

static wchar_t *nc_getwch(void)
/* gets a key as a wchar_t */
{
    static wchar_t c[2];

#ifdef CONFOPT_WGET_WCH

    if (idle_msecs > 0)
        timeout(idle_msecs);

    if (wget_wch(stdscr, (wint_t *)c) == -1)
        c[0] = L'\0';

#else
    char tmp[MB_CUR_MAX + 1];
    int cc, n = 0;

    /* read one byte */
    cc = wgetch(cw);
    if (has_key(cc)) {
        c[0] = cc;
        return c;
    }

    /* set to non-blocking */
    nodelay(stdscr, 1);

    /* read all possible following characters */
    tmp[n++] = cc;
    while (n < sizeof(tmp) - 1 && (cc = getch()) != ERR)
        tmp[n++] = cc;

    /* sets input as blocking */
    nodelay(stdscr, 0);

    tmp[n] = '\0';
    mbstowcs(c, tmp, n);
#endif

    c[1] = '\0';
    return c;
}


#define ctrl(k) ((k) & 31)

static mpdm_t nc_tui_getkey(mpdm_t args, mpdm_t ctxt)
/* reads a key and converts to an action */
{
    static int shift = 0;
    wchar_t *f = NULL;
    mpdm_t k = NULL;

    f = nc_getwch();

    if (f[0] == L'\0')
        return MPDM_S(L"idle");

    /* detect shift+left, shift+right, shift+up, shift+down, shift+pageup, shift+pagedown, shift+home, shift+end */
    switch(f[0]) {
    case KEY_SLEFT:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"cursor-left");
    case KEY_SRIGHT:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"cursor-right");
    case KEY_SR:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"cursor-up");
    case KEY_SF:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"cursor-down");
    case KEY_SPREVIOUS:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"page-up");
    case KEY_SNEXT:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"page-down");
    case KEY_SEND:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"end");
    case KEY_SHOME:
        mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
        return MPDM_S(L"home");
    }


    /* shift here stands for Alt or escape sequence code */
    if (shift) {
        switch (f[0]) {
        case L'0':
            f = L"f10";
            break;
        case L'1':
            f = L"f1";
            break;
        case L'2':
            f = L"f2";
            break;
        case L'3':
            f = L"f3";
            break;
        case L'4':
            f = L"f4";
            break;
        case L'5':
            f = L"f5";
            break;
        case L'6':
            f = L"f6";
            break;
        case L'7':
            f = L"f7";
            break;
        case L'8':
            f = L"f8";
            break;
        case L'9':
            f = L"f9";
            break;
        case KEY_LEFT:
            f = L"alt-cursor-left";
            break;
        case KEY_RIGHT:
            f = L"alt-cursor-right";
            break;
        case KEY_DOWN:
            f = L"alt-cursor-down";
            break;
        case KEY_UP:
            f = L"alt-cursor-up";
            break;
        case KEY_END:
            f = L"alt-end";
            break;
        case KEY_HOME:
            f = L"alt-home";
            break;
        case L'\r':
            f = L"alt-enter";
            break;
        case L'\e':
            f = L"escape";
            break;
        case KEY_ENTER:
            f = L"alt-enter";
            break;
        case L' ':
            f = L"alt-space";
            break;
        case L'a':
            f = L"alt-a";
            break;
        case L'b':
            f = L"alt-b";
            break;
        case L'c':
            f = L"alt-c";
            break;
        case L'd':
            f = L"alt-d";
            break;
        case L'e':
            f = L"alt-e";
            break;
        case L'f':
            f = L"alt-f";
            break;
        case L'g':
            f = L"alt-g";
            break;
        case L'h':
            f = L"alt-h";
            break;
        case L'i':
            f = L"alt-i";
            break;
        case L'j':
            f = L"alt-j";
            break;
        case L'k':
            f = L"alt-k";
            break;
        case L'l':
            f = L"alt-l";
            break;
        case L'm':
            f = L"alt-m";
            break;
        case L'n':
            f = L"alt-n";
            break;
        case L'o':
            f = L"alt-o";
            break;
        case L'p':
            f = L"alt-p";
            break;
        case L'q':
            f = L"alt-q";
            break;
        case L'r':
            f = L"alt-r";
            break;
        case L's':
            f = L"alt-s";
            break;
        case L't':
            f = L"alt-t";
            break;
        case L'u':
            f = L"alt-u";
            break;
        case L'v':
            f = L"alt-v";
            break;
        case L'w':
            f = L"alt-w";
            break;
        case L'x':
            f = L"alt-x";
            break;
        case L'y':
            f = L"alt-y";
            break;
        case L'z':
            f = L"alt-z";
            break;
        case L'\'':
            f = L"alt-'";
            break;
        case L',':
            f = L"alt-,";
            break;
        case L'-':
            f = L"alt-minus";
            break;
        case L'+':
            f = L"alt-plus";
            break;
        case L'.':
            f = L"alt-.";
            break;
        case L'/':
            f = L"alt-/";
            break;
        case L'=':
            f = L"alt-=";
            break;
        case L'[':
            f = L"ansi";
            break;
        }

        shift = 0;
    }
    else {
        switch (f[0]) {
        case KEY_LEFT:
            f = L"cursor-left";
            break;
        case KEY_RIGHT:
            f = L"cursor-right";
            break;
        case KEY_UP:
            f = L"cursor-up";
            break;
        case KEY_DOWN:
            f = L"cursor-down";
            break;
        case KEY_PPAGE:
            f = L"page-up";
            break;
        case KEY_NPAGE:
            f = L"page-down";
            break;
        case KEY_HOME:
            f = L"home";
            break;
        case KEY_END:
            f = L"end";
            break;
        case KEY_LL:
            f = L"end";
            break;
        case KEY_IC:
            f = L"insert";
            break;
        case KEY_DC:
            f = L"delete";
            break;
        case 0x7f:
        case KEY_BACKSPACE:
        case L'\b':
            f = L"backspace";
            break;
        case L'\r':
        case KEY_ENTER:
            f = L"enter";
            break;
        case L'\t':
            f = L"tab";
            break;
        case KEY_BTAB:
            f = L"shift-tab";
            break;
        case L' ':
            f = L"space";
            break;
        case KEY_F(1):
            f = L"f1";
            break;
        case KEY_F(2):
            f = L"f2";
            break;
        case KEY_F(3):
            f = L"f3";
            break;
        case KEY_F(4):
            f = L"f4";
            break;
        case KEY_F(5):
            f = L"f5";
            break;
        case KEY_F(6):
            f = L"f6";
            break;
        case KEY_F(7):
            f = L"f7";
            break;
        case KEY_F(8):
            f = L"f8";
            break;
        case KEY_F(9):
            f = L"f9";
            break;
        case KEY_F(10):
            f = L"f10";
            break;
        case KEY_F(13):
            f = L"shift-f1";
            break;
        case KEY_F(14):
            f = L"shift-f2";
            break;
        case KEY_F(15):
            f = L"shift-f3";
            break;
        case KEY_F(16):
            f = L"shift-f4";
            break;
        case KEY_F(17):
            f = L"shift-f5";
            break;
        case KEY_F(18):
            f = L"shift-f6";
            break;
        case KEY_F(19):
            f = L"shift-f7";
            break;
        case KEY_F(20):
            f = L"shift-f8";
            break;
        case KEY_F(21):
            f = L"shift-f9";
            break;
        case KEY_F(22):
            f = L"shift-f10";
            break;

        case ctrl(' '):
            f = L"ctrl-space";
            break;
        case ctrl('a'):
            f = L"ctrl-a";
            break;
        case ctrl('b'):
            f = L"ctrl-b";
            break;
        case ctrl('c'):
            f = L"ctrl-c";
            break;
        case ctrl('d'):
            f = L"ctrl-d";
            break;
        case ctrl('e'):
            f = L"ctrl-e";
            break;
        case ctrl('f'):
            f = L"ctrl-f";
            break;
        case ctrl('g'):
            f = L"ctrl-g";
            break;
        case ctrl('j'):
            f = L"ctrl-j";
            break;
        case ctrl('k'):
            f = L"ctrl-k";
            break;
        case ctrl('l'):
            f = L"ctrl-l";
            break;
        case ctrl('n'):
            f = L"ctrl-n";
            break;
        case ctrl('o'):
            f = L"ctrl-o";
            break;
        case ctrl('p'):
            f = L"ctrl-p";
            break;
        case ctrl('q'):
            f = L"ctrl-q";
            break;
        case ctrl('r'):
            f = L"ctrl-r";
            break;
        case ctrl('s'):
            f = L"ctrl-s";
            break;
        case ctrl('t'):
            f = L"ctrl-t";
            break;
        case ctrl('u'):
            f = L"ctrl-u";
            break;
        case ctrl('v'):
            f = L"ctrl-v";
            break;
        case ctrl('w'):
            f = L"ctrl-w";
            break;
        case ctrl('x'):
            f = L"ctrl-x";
            break;
        case ctrl('y'):
            f = L"ctrl-y";
            break;
        case ctrl('z'):
            f = L"ctrl-z";
            break;
        case KEY_RESIZE:

            /* handle ncurses resizing */
            nc_update_window_size();
            f = NULL;

            break;
        case L'\e':
            shift = 1;
            f = NULL;
            break;

#ifdef NCURSES_MOUSE_VERSION
        case KEY_MOUSE:
            {
                MEVENT m;

                getmouse(&m);

                if ((m.bstate & BUTTON1_RELEASED) && !(m.bstate & BUTTON1_CLICKED))
                {
                    mpdm_hset_s(MP, L"mouse_to_x", MPDM_I(m.x));
                    mpdm_hset_s(MP, L"mouse_to_y", MPDM_I(m.y));

                    f = L"mouse-drag";
                    break;
                }

                mpdm_hset_s(MP, L"mouse_x", MPDM_I(m.x));
                mpdm_hset_s(MP, L"mouse_y", MPDM_I(m.y));

                if (m.y == LINES - 1)
                    f = L"mouse-menu";
                else
                if (m.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))
                    f = L"mouse-left-button";
                else
                if (m.bstate & BUTTON1_DOUBLE_CLICKED)
                    f = L"mouse-left-dblclick";
                else
                if (m.bstate & BUTTON2_PRESSED)
                    f = L"mouse-middle-button";
                else
                if (m.bstate & BUTTON3_PRESSED)
                    f = L"mouse-right-button";
                else
                if (m.bstate & BUTTON4_PRESSED)
                    f = L"mouse-wheel-up";
                else
#if NCURSES_MOUSE_VERSION == 2
                if (m.bstate & BUTTON5_PRESSED)
                    f = L"mouse-wheel-down";
                else
                    f = NULL;
#else
                {
                    static MEVENT previous;
                    /* with NCURSES_MOUSE_VERSION set to 1, there was no bit left in the state for wheel 
                       down event and most distributions still ship with such version.
                       In that case, mouse wheel down event is just an event that has not changed its position since
                       last call. That's a bit hacky, but it seems to work */
                    if (previous.x == m.x && previous.y == m.y)
                        f = L"mouse-wheel-down";
                    else
                        f = NULL;

                    previous.x = m.x; previous.y = m.y;
                }
#endif
            }
            break;
#endif /* NCURSES_MOUSE_VERSION */
        }
    }

    if (f != NULL)
        k = MPDM_S(f);

    return k;
}


static mpdm_t nc_addwstr(mpdm_t str)
/* draws a string */
{
    wchar_t *wptr = mpdm_string(str);

#ifndef CONFOPT_ADDWSTR
    char *cptr;

    cptr = mpdm_wcstombs(wptr, NULL);
    waddstr(stdscr, cptr);
    free(cptr);

#else
    waddwstr(stdscr, wptr);
#endif                          /* CONFOPT_ADDWSTR */

    return NULL;
}


static mpdm_t nc_tui_doc_draw(mpdm_t args, mpdm_t ctxt)
/* draws the document part */
{
    mpdm_t d;
    int n, m;

    werase(stdscr);

    d = mpdm_aget(args, 0);
    d = mpdm_ref(mp_draw(d, 0));

    for (n = 0; n < mpdm_size(d); n++) {
        mpdm_t l = mpdm_aget(d, n);

        wmove(stdscr, n, 0);

        for (m = 0; m < mpdm_size(l); m++) {
            int attr;
            mpdm_t s;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_aget(l, m++));
            s = mpdm_aget(l, m);

            wattrset(stdscr, nc_attrs[attr]);
            wattron(stdscr, nc_attrs[attr]);
            nc_addwstr(s);
        }
    }

    mpdm_unref(d);

    return NULL;
}


static int ncurses_detect_color_support(int rgbcolor)
/* ask the tty for color request information */
{
    mpdm_t v, w;

    v = mpdm_get_wcs(MP, L"config");
    w = mpdm_get_wcs(v, L"ansi_rgbcolor");

    if (w != NULL)
        rgbcolor = mpdm_ival(w);

    if (rgbcolor == 1) {
        rgbcolor = -1;
        if (has_colors() != FALSE && can_change_color() != FALSE) 
            rgbcolor = 1;

        mpdm_set_wcs(v, MPDM_I(rgbcolor), L"ansi_rgbcolor");
    }

    return rgbcolor;
}


static void nc_build_colors(void)
/* builds the colors */
{
    mpdm_t colors;
    mpdm_t color_names;
    mpdm_t v, i;
    int n, rgb;
    int64_t c;

#ifdef CONFOPT_TRANSPARENCY
    use_default_colors();

#define DEFAULT_INK -1
#define DEFAULT_PAPER -1

#else                           /* CONFOPT_TRANSPARENCY */

#define DEFAULT_INK COLOR_BLACK
#define DEFAULT_PAPER COLOR_WHITE

#endif

    /* gets the color definitions and attribute names */
    colors      = mpdm_hget_s(MP, L"colors");
    color_names = mpdm_hget_s(MP, L"color_names");
    rgb = ncurses_detect_color_support(0);

    /* loop the colors */
    n = c = 0;
    if (rgb > 0) {
        /* The first index that's not a default color */
        int colors_used = 8; 
        while (mpdm_iterator(colors, &c, &v, &i)) {
            mpdm_t w = mpdm_get_wcs(v, L"gui");

            int ink   = mpdm_ival(mpdm_get_i(w, 0));
            int paper = mpdm_ival(mpdm_get_i(w, 1));

            mpdm_t color_name = mpdm_hget_s(v, L"text");
            int cp, c0, c1;


            /* get color indexes */
            if ((c0 = mpdm_seek(color_names, mpdm_aget(color_name, 0), 1)) == -1 ||
                (c1 = mpdm_seek(color_names, mpdm_aget(color_name, 1), 1)) == -1)
                continue;

            /* store the 'normal' attribute */
            if (wcscmp(mpdm_string(i), L"normal") == 0)
                normal_attr = n;

            /* store the attr */
            mpdm_hset_s(v, L"attr", MPDM_I(n));

            /* extract the actual RGB color value */
            int blue  = (ink & 0x000000ff);
            int green = (ink & 0x0000ff00) >> 8;
            int red   = (ink & 0x00ff0000) >> 16;

            int fg = colors_used++;
            init_color(fg, (short)((red * 999) / 255), (short)((green * 999) / 255), (short)((blue * 999) / 255));

            int bg = c1 - 1;
            if (c1) {
                blue  = (paper & 0x000000ff);
                green = (paper & 0x0000ff00) >> 8;
                red   = (paper & 0x00ff0000) >> 16;
                bg = colors_used++;

                init_color(bg, (short)((red * 999) / 255), (short)((green * 999) / 255), (short)((blue * 999) / 255));
            }

            /* finally create the (custom) color pair */
            init_pair(n + 1, fg, bg);
            cp = COLOR_PAIR(n + 1);

            /* flags */
            w = mpdm_hget_s(v, L"flags");
            if (mpdm_seek_wcs(w, L"reverse", 1) != -1)
                cp |= A_REVERSE;
            if (mpdm_seek_wcs(w, L"underline", 1) != -1)
                cp |= A_UNDERLINE;
#ifdef A_ITALIC
            if (mpdm_seek_wcs(w, L"italic", 1) != -1)
                cp |= A_ITALIC;
#endif
            nc_attrs[n++] = cp;
        }
    } else {
        while (mpdm_iterator(colors, &c, &v, &i)) {
            mpdm_t w = mpdm_hget_s(v, L"text");
            int cp, c0, c1;

            /* get color indexes */
            if ((c0 = mpdm_seek(color_names, mpdm_aget(w, 0), 1)) == -1 ||
                (c1 = mpdm_seek(color_names, mpdm_aget(w, 1), 1)) == -1)
                continue;

            /* store the 'normal' attribute */
            if (wcscmp(mpdm_string(i), L"normal") == 0)
                normal_attr = n;

            /* store the attr */
            mpdm_hset_s(v, L"attr", MPDM_I(n));

            init_pair(n + 1, c0 - 1, c1 - 1);
            cp = COLOR_PAIR(n + 1);

            /* flags */
            w = mpdm_hget_s(v, L"flags");
            if (mpdm_seek_wcs(w, L"bright", 1) != -1)
                /* Default to old and wrong behavior to use bold instead of bright */
                cp |= A_BOLD;
            if (mpdm_seek_wcs(w, L"reverse", 1) != -1)
                cp |= A_REVERSE;
            if (mpdm_seek_wcs(w, L"underline", 1) != -1)
                cp |= A_UNDERLINE;
#ifdef A_ITALIC
            if (mpdm_seek_wcs(w, L"italic", 1) != -1)
                cp |= A_ITALIC;
#endif

            nc_attrs[n++] = cp; 
        }
    }

    /* set the background filler */
    wbkgdset(stdscr, ' ' | nc_attrs[normal_attr]);
}


/** driver functions **/

static mpdm_t ncursesw_drv_idle(mpdm_t a, mpdm_t ctxt)
{
    idle_msecs = (int) (mpdm_rval(mpdm_get_i(a, 0)) * 1000);

    return NULL;
}


static mpdm_t ncursesw_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    endwin();

    if ((v = mpdm_hget_s(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}

static mpdm_t ncursesw_drv_suspend(mpdm_t a, mpdm_t ctxt)
{
    endwin();

    printf("\nType 'fg' to return to Minimum Profit");
    fflush(stdout);

    /* Trigger suspending this process */
    kill(getpid(), SIGSTOP);

    /* Ok, we're back, let's refresh the screen */
    wrefresh(stdscr);

    return NULL;
}

/** TUI **/

static mpdm_t nc_tui_addstr(mpdm_t a, mpdm_t ctxt)
/* TUI: add a string */
{
    return nc_addwstr(mpdm_aget(a, 0));
}


static mpdm_t nc_tui_move(mpdm_t a, mpdm_t ctxt)
/* TUI: move to a screen position */
{
    /* curses' move() use y, x */
    wmove(stdscr, mpdm_ival(mpdm_aget(a, 1)), mpdm_ival(mpdm_aget(a, 0)));

    /* if third argument is not NULL, clear line */
    if (mpdm_aget(a, 2) != NULL)
        wclrtoeol(stdscr);

    return NULL;
}


static mpdm_t nc_tui_attr(mpdm_t a, mpdm_t ctxt)
/* TUI: set attribute for next string */
{
    int attr = mpdm_ival(mpdm_aget(a, 0));

    wattrset(stdscr, nc_attrs[attr]);
    wbkgdset(stdscr, ' ' | nc_attrs[attr]);

    return NULL;
}


static mpdm_t nc_tui_refresh(mpdm_t a, mpdm_t ctxt)
/* TUI: refresh the screen */
{
    wrefresh(stdscr);
    return NULL;
}


static mpdm_t nc_tui_getxy(mpdm_t a, mpdm_t ctxt)
/* TUI: returns the x and y cursor position */
{
    mpdm_t v;
    int x, y;

    getyx(stdscr, y, x);

    v = MPDM_A(2);
    mpdm_ref(v);

    mpdm_aset(v, MPDM_I(x), 0);
    mpdm_aset(v, MPDM_I(y), 1);

    mpdm_unrefnd(v);

    return v;
}


static mpdm_t nc_tui_charat(mpdm_t a, mpdm_t ctxt)
{
    wchar_t s[2];
/*    int x, y;

    x = mpdm_ival(mpdm_get_i(a, 0));
    y = mpdm_ival(mpdm_get_i(a, 1));
*/
    s[0] = L' ';
    s[1] = L'\0';

    return MPDM_S(s);
}


static void nc_register_functions(void)
{
    mpdm_t drv;
    mpdm_t tui;

    drv = mpdm_hget_s(mpdm_root(), L"mp_drv");

    mpdm_hset_s(drv, L"idle",       MPDM_X(ncursesw_drv_idle));
    mpdm_hset_s(drv, L"shutdown",   MPDM_X(ncursesw_drv_shutdown));
    mpdm_hset_s(drv, L"suspend",    MPDM_X(ncursesw_drv_suspend));

    /* execute tui */
    tui = mpsl_eval(MPDM_S(L"load('mp_tui.mpsl');"), NULL, NULL);

    mpdm_hset_s(tui, L"getkey",     MPDM_X(nc_tui_getkey));
    mpdm_hset_s(tui, L"addstr",     MPDM_X(nc_tui_addstr));
    mpdm_hset_s(tui, L"move",       MPDM_X(nc_tui_move));
    mpdm_hset_s(tui, L"attr",       MPDM_X(nc_tui_attr));
    mpdm_hset_s(tui, L"refresh",    MPDM_X(nc_tui_refresh));
    mpdm_hset_s(tui, L"getxy",      MPDM_X(nc_tui_getxy));
    mpdm_hset_s(tui, L"charat",     MPDM_X(nc_tui_charat));
    mpdm_hset_s(tui, L"doc_draw",   MPDM_X(nc_tui_doc_draw));
}


static mpdm_t ncursesw_drv_startup(mpdm_t a)
{
    signal(SIGPIPE, SIG_IGN);

    nc_register_functions();

    initscr();
    start_color();

#ifdef NCURSES_MOUSE_VERSION
    mpdm_t v = mpdm_hget_s(MP, L"config");
    mpdm_hset_s(v, L"tx", MPDM_I(COLS));

    if (mpdm_ival(mpdm_hget_s(v, L"no_text_mouse")) == 0) {
        mousemask(
            BUTTON1_PRESSED|
            BUTTON2_PRESSED|
            BUTTON3_PRESSED|
            BUTTON4_PRESSED|
            BUTTON1_RELEASED|
            BUTTON1_CLICKED|
            BUTTON1_DOUBLE_CLICKED|
            REPORT_MOUSE_POSITION,
            NULL);
    }
#endif

    keypad(stdscr, TRUE);
    nonl();
    raw();
    noecho();

    nc_build_colors();

    nc_update_window_size();

#ifndef NCURSES_VERSION
    signal(SIGWINCH, nc_sigwinch);
#endif

    return NULL;
}


int ncursesw_drv_detect(void *p)
{
    mpdm_t drv;

    drv = mpdm_hset_s(mpdm_root(), L"mp_drv", MPDM_H(0));

    mpdm_hset_s(drv, L"id",         MPDM_S(L"curses"));
    mpdm_hset_s(drv, L"startup",    MPDM_X(ncursesw_drv_startup));

    return 1;
}

#endif                          /* CONFOPT_CURSES */
