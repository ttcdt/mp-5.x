/*

    Minimum Profit - A Text Editor
    Win32 console driver.

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#ifdef CONFOPT_WIN32

#include <stdio.h>
#include <windows.h>
#include <wchar.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

HANDLE s_in;
HANDLE s_out;

int bx;
int by;
int cx;
int cy;
int tx;
int ty;

#define MAX_COLORS 100
WORD win32c_attrs[MAX_COLORS];
int normal_attr = 0;
int last_attr;

CHAR_INFO *buf = NULL;


/** code **/

static void win32c_clrscr(void);

static void update_window_size(void)
{
    mpdm_t v;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(s_out, &csbi);

    bx = 0;
    by = csbi.srWindow.Top;
    tx = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
    ty = (csbi.srWindow.Bottom - csbi.srWindow.Top) + 1;

    v = mpdm_get_wcs(MP, L"window");
    mpdm_set_wcs(v, MPDM_I(tx),     L"tx");
    mpdm_set_wcs(v, MPDM_I(ty - 1), L"ty");

    buf = realloc(buf, tx * ty * sizeof(CHAR_INFO));
    win32c_clrscr();
}

static void build_colors(void)
{
    mpdm_t colors;
    mpdm_t color_names;
    mpdm_t v, i;
    int n;
    int64_t c;

    /* gets the color definitions and attribute names */
    colors      = mpdm_get_wcs(MP, L"colors");
    color_names = mpdm_get_wcs(MP, L"color_names");

    /* loop the colors */
    n = c = 0;
    while (mpdm_iterator(colors, &c, &v, &i)) {
        mpdm_t w = mpdm_get_wcs(v, L"text");
        int c0, c1;
    	WORD cp = 0;

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(i), L"normal") == 0)
            normal_attr = n;

        /* store the attr */
        mpdm_set_wcs(v, MPDM_I(n), L"attr");

        /* get color indexes */
        if ((c0 = mpdm_seek(color_names, mpdm_get_i(w, 0), 1)) == -1 ||
            (c1 = mpdm_seek(color_names, mpdm_get_i(w, 1), 1)) == -1)
            continue;

        /* color names match the color bits; 0 is 'default' */
        if (c0 == 0) c0 = 8;
        if (c1 == 0) c1 = 1;

        c0--;
        c1--;

        /* flags */
        w = mpdm_get_wcs(v, L"flags");

        if (mpdm_seek_wcs(w, L"reverse", 1) != -1) {
            int t = c0;
            c0 = c1;
            c1 = t;
        }
        if (mpdm_seek_wcs(w, L"bright", 1) != -1)
            cp |= FOREGROUND_INTENSITY;

        if ((c0 & 1)) cp |= FOREGROUND_RED;
        if ((c1 & 1)) cp |= BACKGROUND_RED;
        if ((c0 & 2)) cp |= FOREGROUND_GREEN;
        if ((c1 & 2)) cp |= BACKGROUND_GREEN;
        if ((c0 & 4)) cp |= FOREGROUND_BLUE;
        if ((c1 & 4)) cp |= BACKGROUND_BLUE;

        win32c_attrs[n] = cp;
        n++;
    }
}


#define ctrl(c) ((c) & 31)

int mouse_down = 0;

static mpdm_t win32c_getkey(mpdm_t args, mpdm_t ctxt)
/* reads a key and converts to an action */
{
    wchar_t *f = NULL;
    wchar_t *p = L"";
    mpdm_t k = NULL;
    DWORD ne;
    INPUT_RECORD ev;
    wchar_t sc[2];
    wchar_t action[64];

    WaitForSingleObject(s_in, INFINITE);

    ReadConsoleInputW(s_in, &ev, 1, &ne);

    if (ne) {
        if (ev.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            update_window_size();
        }
        else
        if (ev.EventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD *e = &ev.Event.MouseEvent;

            switch (e->dwEventFlags) {
            case 0:

                mpdm_set_wcs(MP, MPDM_I(e->dwMousePosition.X), L"mouse_x");
                mpdm_set_wcs(MP, MPDM_I(e->dwMousePosition.Y - by), L"mouse_y");

                mouse_down = 0;
                if (e->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                    f = L"mouse-left-button";
                    mouse_down = 1;
                }
                else
                if (e->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED)
                    f = L"mouse-middle-button";
                else
                if (e->dwButtonState & RIGHTMOST_BUTTON_PRESSED)
                    f = L"mouse-right-button";

                if (e->dwMousePosition.Y - by == ty - 1)
                    f = L"mouse-menu";

                break;

            case MOUSE_MOVED:
                if (mouse_down) {
                    mpdm_set_wcs(MP, MPDM_I(e->dwMousePosition.X), L"mouse_to_x");
                    mpdm_set_wcs(MP, MPDM_I(e->dwMousePosition.Y - by), L"mouse_to_y");

                    f = L"mouse-drag";
                }

                break;

            case MOUSE_WHEELED:
                f = (HIWORD(e->dwButtonState) > 0) ? L"mouse-wheel-up" : L"mouse-wheel-down";
                break;
            }
        }
        else
        if (ev.EventType == KEY_EVENT) {
            KEY_EVENT_RECORD *e = &ev.Event.KeyEvent;

            if (e->bKeyDown) {
                wchar_t c = e->uChar.UnicodeChar;

                if (e->dwControlKeyState & (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))
                    p = L"ctrl-";
                else
                if (e->dwControlKeyState & (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED))
                    p = L"alt-";

                mpdm_set_wcs(MP, MPDM_I(e->dwControlKeyState & SHIFT_PRESSED), L"shift_pressed");

                if (c) {
                    if (c > 32) {
                        sc[0] = c;
                        sc[1] = L'\0';
                        f = sc;
                    }
                    else {
                        switch (c) {
                        case ctrl(' '):
                            f = L"space";
                            break;
                        case ctrl('h'):
                            f = L"backspace";
                            break;
                        case ctrl('i'):
                            f = (e->dwControlKeyState & SHIFT_PRESSED) ? L"shift-tab" : L"tab";
                            break;
                        case ctrl('m'):
                            f = L"enter";
                            break;
                        case ' ':
                            f = L"space";
                            break;
                        case 27:
                            f = L"escape";
                            break;
                        default:
                            sc[0] = c + 96;
                            sc[1] = L'\0';
                            f = sc;
                            break;
                        }
                    }
                }
                else {
                    switch (e->wVirtualKeyCode) {
                    case VK_UP:
                        f = L"cursor-up";
                        break;
                    case VK_DOWN:
                        f = L"cursor-down";
                        break;
                    case VK_LEFT:
                        f = L"cursor-left";
                        break;
                    case VK_RIGHT:
                        f = L"cursor-right";
                        break;
                    case VK_PRIOR:
                        f = L"page-up";
                        break;
                    case VK_NEXT:
                        f = L"page-down";
                        break;
                    case VK_HOME:
                        f = L"home";
                        break;
                    case VK_END:
                        f = L"end";
                        break;
                    case VK_RETURN:
                        f = L"enter";
                        break;
                    case VK_BACK:
                        f = L"backspace";
                        break;
                    case VK_DELETE:
                        f = L"delete";
                        break;
                    case VK_INSERT:
                        f = L"insert";
                        break;
                    case VK_DIVIDE:
                        f = L"kp-divide";
                        break;
                    case VK_MULTIPLY:
                        f = L"kp-multiply";
                        break;
                    case VK_SUBTRACT:
                        f = L"kp-minus";
                        break;
                    case VK_ADD:
                        f = L"kp-plus";
                        break;
                    case VK_F1:
                        f = L"f1";
                        break;
                    case VK_F2:
                        f = L"f2";
                        break;
                    case VK_F3:
                        f = L"f3";
                        break;
                    case VK_F4:
                        f = L"f4";
                        break;
                    case VK_F5:
                        f = L"f5";
                        break;
                    case VK_F6:
                        f = L"f6";
                        break;
                    case VK_F7:
                        f = L"f7";
                        break;
                    case VK_F8:
                        f = L"f8";
                        break;
                    case VK_F9:
                        f = L"f9";
                        break;
                    case VK_F10:
                        f = L"f10";
                        break;
                    case VK_F11:
                        f = L"f11";
                        break;
                    case VK_F12:
                        f = L"f12";
                        break;
                    }
                }
            }
        }
    }

    if (f != NULL) {
        action[0] = '\0';
        wcscat(action, p);
        wcscat(action, f);

        k = MPDM_S(action);
    }

    return k;
}


static void win32c_move(int x, int y, int clr)
{
    cx = x;
    cy = y;

    if (clr) {
        int n, o;

        o = cy * tx;
        for (n = cx; n < tx; n++) {
            buf[n + o].Attributes = win32c_attrs[last_attr];
            buf[n + o].Char.UnicodeChar = L' ';
        }
    }

    COORD bs = { bx + cx, by + cy };
    SetConsoleCursorPosition(s_out, bs);
}


static void win32c_clrscr(void)
{
    int n;

    for (n = 0; n < tx * ty; n++) {
        buf[n].Attributes = win32c_attrs[normal_attr];
        buf[n].Char.UnicodeChar = L' ';
    }
}


static void win32c_draw(void)
{
    COORD sz = { tx, ty };
    COORD pos = { 0, 0 };
    SMALL_RECT r = { 0, by, tx - 1, by + ty - 1 };

    WriteConsoleOutputW(s_out, buf, sz,  pos, &r);
}


static void win32c_addstr(mpdm_t s)
{
    wchar_t *str = mpdm_string(s);
    int o = cx + cy * tx;

    while (*str && cx < tx) {
        buf[o].Attributes = win32c_attrs[last_attr];
        buf[o].Char.UnicodeChar = *str;
        str++;
        o++;
        cx++;
    }
}


static void win32c_attr(int attr)
{
    last_attr = attr;
}


static wchar_t win32c_charat(int x, int y)
{
    return (wchar_t) buf[y * tx + x].Char.UnicodeChar;
}


/** TUI **/

static mpdm_t tui_addstr(mpdm_t a, mpdm_t ctxt)
/* TUI: add a string */
{
    win32c_addstr(mpdm_get_i(a, 0));
    return NULL;
}


static mpdm_t tui_move(mpdm_t a, mpdm_t ctxt)
/* TUI: move to a screen position */
{
    int cx = mpdm_ival(mpdm_get_i(a, 0));
    int cy = mpdm_ival(mpdm_get_i(a, 1));
    int dl = mpdm_get_i(a, 2) != NULL;

    win32c_move(cx, cy, dl);

    return NULL;
}


static mpdm_t tui_attr(mpdm_t a, mpdm_t ctxt)
/* TUI: set attribute for next string */
{
    win32c_attr(mpdm_ival(mpdm_get_i(a, 0)));
    return NULL;
}


static mpdm_t tui_refresh(mpdm_t a, mpdm_t ctxt)
/* TUI: refresh the screen */
{
    win32c_draw();
    return NULL;
}


static mpdm_t tui_getxy(mpdm_t a, mpdm_t ctxt)
/* TUI: returns the x and y cursor position */
{
    mpdm_t v;

    v = MPDM_A(2);
    mpdm_ref(v);

    mpdm_set_i(v, MPDM_I(cx), 0);
    mpdm_set_i(v, MPDM_I(cy), 1);

    mpdm_unrefnd(v);

    return v;
}


static mpdm_t tui_charat(mpdm_t a, mpdm_t ctxt)
{
    wchar_t s[2];
    int x, y;

    x = mpdm_ival(mpdm_get_i(a, 0));
    y = mpdm_ival(mpdm_get_i(a, 1));

    s[0] = win32c_charat(x, y);
    s[1] = L'\0';

    return MPDM_S(s);
}


static mpdm_t win32c_doc_draw(mpdm_t args, mpdm_t ctxt)
/* draws the document part */
{
    mpdm_t d;
    int n, m;

    win32c_clrscr();

    d = mpdm_get_i(args, 0);
    d = mpdm_ref(mp_draw(d, 0));

    for (n = 0; n < mpdm_size(d); n++) {
        mpdm_t l = mpdm_get_i(d, n);

        win32c_move(0, n, 0);

        for (m = 0; m < mpdm_size(l); m++) {
            int attr;
            mpdm_t s;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_get_i(l, m++));
            s = mpdm_get_i(l, m);

            win32c_attr(attr);
            win32c_addstr(s);
        }
    }

    mpdm_unref(d);

    return NULL;
}


static mpdm_t win32c_drv_shutdown(mpdm_t a)
{
    mpdm_t v;

    if ((v = mpdm_get_wcs(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static void register_functions(void)
{
    mpdm_t drv;
    mpdm_t tui;

    drv = mpdm_get_wcs(mpdm_root(), L"mp_drv");
    mpdm_set_wcs(drv, MPDM_X(win32c_drv_shutdown), L"shutdown");

    /* execute tui */
    tui = mpsl_eval(MPDM_S(L"load('mp_tui.mpsl');"), NULL, NULL);

    mpdm_set_wcs(tui, MPDM_X(win32c_getkey),    L"getkey");
    mpdm_set_wcs(tui, MPDM_X(tui_addstr),       L"addstr");
    mpdm_set_wcs(tui, MPDM_X(tui_move),         L"move");
    mpdm_set_wcs(tui, MPDM_X(tui_attr),         L"attr");
    mpdm_set_wcs(tui, MPDM_X(tui_refresh),      L"refresh");
    mpdm_set_wcs(tui, MPDM_X(tui_getxy),        L"getxy");
    mpdm_set_wcs(tui, MPDM_X(tui_charat),       L"charat");
    mpdm_set_wcs(tui, MPDM_X(win32c_doc_draw),  L"doc_draw");
}


static mpdm_t win32c_drv_startup(mpdm_t a, mpdm_t ctxt)
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    SetConsoleTitleW(L"mp " VERSION);

    register_functions();
    build_colors();
    update_window_size();

    return NULL;
}


int win32_drv_detect(int *argc, char ***argv)
{
    int n, ret = 0;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-h") == 0)
            return 0;
    }

    s_in  = GetStdHandle(STD_INPUT_HANDLE);
    s_out = GetStdHandle(STD_OUTPUT_HANDLE);

    if (SetConsoleMode(s_in, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT) &&
        SetConsoleMode(s_out, ENABLE_PROCESSED_OUTPUT)) {

        mpdm_t drv = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"mp_drv");

        mpdm_set_wcs(drv, MPDM_S(sizeof(char *) == 8 ? L"win64c" : L"win32c"), L"id");
        mpdm_set_wcs(drv, MPDM_X(win32c_drv_startup), L"startup");

        ret = 1;
    }

    return ret;
}

#endif                          /* CONFOPT_WIN32 */
