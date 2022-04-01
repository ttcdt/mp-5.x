/*

    Minimum Profit - A Text Editor
    Raw ANSI terminal driver.

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#ifdef CONFOPT_ANSI

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <sys/time.h>

#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"


#define MAX_COLORS 100
char ansi_attrs[MAX_COLORS][64];
int ansi_attrs_i = 0;
int normal_attr = 0;

static int idle_msecs = 0;


/** code **/

static void ansi_raw_tty(int start)
/* sets/unsets stdin in raw mode */
{
    static struct termios so;

    if (start) {
        struct termios o;

        /* save previous fd state */
        tcgetattr(0, &so);

        /* set raw */
        tcgetattr(0, &o);
        cfmakeraw(&o);
        tcsetattr(0, TCSANOW, &o);
    }
    else
        /* restore previously saved tty state */
        tcsetattr(0, TCSANOW, &so);
}


static int ansi_something_waiting(int fd, int msecs)
/* returns yes if there is something waiting on fd */
{
    fd_set ids;
    struct timeval tv;

    /* reset */
    FD_ZERO(&ids);

    /* add fd to set */
    FD_SET(fd, &ids);

    tv.tv_sec  = 0;
    tv.tv_usec = msecs * 1000;

    return select(1, &ids, NULL, NULL, &tv) > 0;
}


char *ansi_read_string(int fd)
/* reads an ansi string, waiting in the first char */
{
    static char *buf = NULL;
    static int z = 0;
    int n = 0;

    /* if there is an idle time and nothing is here, exit */
    if (idle_msecs > 0 && !ansi_something_waiting(fd, idle_msecs))
        return NULL;

    /* first char blocks, rest of possible chars not */
    do {
        char c;

        if (read(fd, &c, sizeof(c)) == -1)
            break;
        else {
            if (n == z) {
                z += 32;
                buf = realloc(buf, z + 1);
            }

            buf[n++] = c;
        }

    } while (ansi_something_waiting(fd, 10));

    buf[n] = '\0';

    return n ? buf : NULL;
}


static void ansi_db_resize(int w, int h);

static void ansi_get_tty_size(void)
/* asks the tty for its size */
{
    mpdm_t v;
    char *buffer;
    int w, h;

    /* magic line: save cursor position, move to stupid position,
       ask for current position and restore cursor position */
    printf("\0337\033[r\033[999;999H\033[6n\0338");
    fflush(stdout);

    if (ansi_something_waiting(0, 50)) {
        buffer = ansi_read_string(0);

        sscanf(buffer, "\033[%d;%dR", &h, &w);
    }
    else {
        /* terminal didn't report; let's hope it's the default */
        w = 80;
        h = 25;
    }

    v = mpdm_get_wcs(MP, L"window");
    mpdm_set_wcs(v, MPDM_I(w),     L"tx");
    mpdm_set_wcs(v, MPDM_I(h - 1), L"ty");

    ansi_db_resize(w, h + 1);
}


static int ansi_detect_color_support(int rgbcolor)
/* ask the tty for color request information */
{
    mpdm_t v, w;

    v = mpdm_get_wcs(MP, L"config");
    w = mpdm_get_wcs(v, L"ansi_rgbcolor");

    if (w != NULL)
        rgbcolor = mpdm_ival(w);

    if (rgbcolor == 1) {
        printf("\033]4;30;?\a");
        fflush(stdout);

        rgbcolor = -1;
        if (ansi_something_waiting(0, 50)) {
            char *buffer = ansi_read_string(0);

            if (buffer[0] != '\0')
                rgbcolor = 1;
        }

        mpdm_set_wcs(v, MPDM_I(rgbcolor), L"ansi_rgbcolor");
    }

    return rgbcolor;
}


static void ansi_sigwinch(int s)
/* SIGWINCH signal handler */
{
    struct sigaction sa;

    /* get new size */
    ansi_get_tty_size();

    /* (re)attach signal */
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = ansi_sigwinch;
    sigaction(SIGWINCH, &sa, NULL);
}


static void ansi_gotoxy(int x, int y)
/* positions the cursor */
{
    printf("\033[%d;%dH", y + 1, x + 1);
}


#if 0
static void ansi_getxy(int *x, int *y)
/* gets the x and y cursor position */
{
    char *buffer;

    printf("\033[6n");
    fflush(stdout);

    buffer = ansi_read_string(0);
    sscanf(buffer, "\033[%d;%dR", y, x);

    (*x)--;
    (*y)--;
}


static void ansi_clreol(void)
/* clear to end of line */
{
    printf("\033[K");
}


static void ansi_clrscr(void)
/* clears the screen */
{
    printf("\033[2J");
}


static void ansi_print_v(mpdm_t v)
/* prints an mpdm_t */
{
    char tmp[1024];

    wcstombs(tmp, mpdm_string(v), sizeof(tmp));
    printf("%s", tmp);
}


static wchar_t ansi_charat(int x, int y)
/* returns the char at the x, y position */
{
    return L' ';
}
#endif

static int ansi_set_attr(int a)
{
    printf("%s", ansi_attrs[a]);

    return a;
}


static void ansi_refresh(void)
/* refresh the screen */
{
    fflush(stdout);
}


static void ansi_build_colors(void)
{
    mpdm_t colors;
    mpdm_t color_names;
    mpdm_t v, i;
    int n;
    int64_t c;
    int rgbcolor = 1; /* default setting */

    rgbcolor = ansi_detect_color_support(rgbcolor);

    /* gets the color definitions and attribute names */
    colors      = mpdm_get_wcs(MP, L"colors");
    color_names = mpdm_get_wcs(MP, L"color_names");

    /* loop the colors */
    n = c = 0;
    while (mpdm_iterator(colors, &c, &v, &i)) {
        mpdm_t w;
        int c0, c1, cf = 0;

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(i), L"normal") == 0)
            normal_attr = n;

        /* flags */
        w = mpdm_get_wcs(v, L"flags");
        if (mpdm_seek_wcs(w, L"reverse", 1) != -1)
            cf |= 0x01;
        if (mpdm_seek_wcs(w, L"bright", 1) != -1)
            cf |= 0x02;
        if (mpdm_seek_wcs(w, L"underline", 1) != -1)
            cf |= 0x04;
        if (mpdm_seek_wcs(w, L"italic", 1) != -1)
            cf |= 0x08;

        if (rgbcolor > 0) {
            w = mpdm_get_wcs(v, L"gui");
            c0 = mpdm_ival(mpdm_get_i(w, 0));
            c1 = mpdm_ival(mpdm_get_i(w, 1));

            sprintf(ansi_attrs[n], "\033[0;%s%s%s38;2;%d;%d;%dm\033[48;2;%d;%d;%dm",
                cf & 0x1 ? "7;" : "",
                cf & 0x4 ? "4;" : "",
                cf & 0x8 ? "3;" : "",
                (c0 >> 16) & 0xff, (c0 >> 8) & 0xff, c0 & 0xff,
                (c1 >> 16) & 0xff, (c1 >> 8) & 0xff, c1 & 0xff
            );
        }
        else {
            w = mpdm_get_wcs(v, L"text");

            /* get color indexes */
            if ((c0 = mpdm_seek(color_names, mpdm_get_i(w, 0), 1)) == -1 ||
                (c1 = mpdm_seek(color_names, mpdm_get_i(w, 1), 1)) == -1)
                continue;

            if ((--c0) == -1) c0 = 9;
            if ((--c1) == -1) c1 = 9;

            sprintf(ansi_attrs[n], "\033[0;%s%s%s%d;%dm",
                cf & 0x1 ? "7;" : "",
                cf & 0x4 ? "4;" : "",
                cf & 0x8 ? "3;" : "",
                cf & 0x2 ? (c0 + 90) : (c0 + 30),
                c1 + 40
            );
        }

        /* store the attr */
        mpdm_set_wcs(v, MPDM_I(n), L"attr");

        n++;
    }

    ansi_attrs_i = n;
}


struct _str_to_code {
    char *ansi_str;
    wchar_t *code;
} str_to_code[] = {
    { "\033[A\033[A\033[A", L"mouse-wheel-up" },
    { "\033[B\033[B\033[B", L"mouse-wheel-down" },
    { "\033[A",             L"cursor-up" },
    { "\033[B",             L"cursor-down" },
    { "\033[C",             L"cursor-right" },
    { "\033[D",             L"cursor-left" },
    { "\033[5~",            L"page-up" },
    { "\033[6~",            L"page-down" },
    { "\033[H",             L"home" },
    { "\033[F",             L"end" },
    { "\033OP",             L"f1" },
    { "\033OQ",             L"f2" },
    { "\033OR",             L"f3" },
    { "\033OS",             L"f4" },
    { "\033[15~",           L"f5" },
    { "\033[17~",           L"f6" },
    { "\033[18~",           L"f7" },
    { "\033[19~",           L"f8" },
    { "\033[20~",           L"f9" },
    { "\033[21~",           L"f10" },
    { "\033[1;2P",          L"shift-f1" },
    { "\033[1;2Q",          L"shift-f2" },
    { "\033[1;2R",          L"shift-f3" },
    { "\033[1;2S",          L"shift-f4" },
    { "\033[15;2~",         L"shift-f5" },
    { "\033[17;2~",         L"shift-f6" },
    { "\033[18;2~",         L"shift-f7" },
    { "\033[19;2~",         L"shift-f8" },
    { "\033[20;2~",         L"shift-f9" },
    { "\033[21;2~",         L"shift-f10" },
    { "\033[1;5P",          L"ctrl-f1" },
    { "\033[1;5Q",          L"ctrl-f2" },
    { "\033[1;5R",          L"ctrl-f3" },
    { "\033[1;5S",          L"ctrl-f4" },
    { "\033[15;5~",         L"ctrl-f5" },
    { "\033[17;5~",         L"ctrl-f6" },
    { "\033[18;5~",         L"ctrl-f7" },
    { "\033[19;5~",         L"ctrl-f8" },
    { "\033[20;5~",         L"ctrl-f9" },
    { "\033[21;5~",         L"ctrl-f10" },
    { "\033[1;2A",          L"_shift-cursor-up" },
    { "\033[1;2B",          L"_shift-cursor-down" },
    { "\033[1;2C",          L"_shift-cursor-right" },
    { "\033[1;2D",          L"_shift-cursor-left" },
    { "\033[1;5A",          L"ctrl-cursor-up" },
    { "\033[1;5B",          L"ctrl-cursor-down" },
    { "\033[1;5C",          L"ctrl-cursor-right" },
    { "\033[1;5D",          L"ctrl-cursor-left" },
    { "\033[1;5H",          L"ctrl-home" },
    { "\033[1;5F",          L"ctrl-end" },
    { "\033[1;3A",          L"alt-cursor-up" },
    { "\033[1;3B",          L"alt-cursor-down" },
    { "\033[1;3C",          L"alt-cursor-right" },
    { "\033[1;3D",          L"alt-cursor-left" },
    { "\033[1;3H",          L"alt-home" },
    { "\033[1;3F",          L"alt-end" },
    { "\033[3~",            L"delete" },
    { "\033[2~",            L"insert" },
    { "\033[Z",             L"shift-tab" },
    { "\033\r",             L"alt-enter" },
    { "\033[1~",            L"home" },
    { "\033[4~",            L"end" },
    { "\033[5;5~",          L"ctrl-page-up" },
    { "\033[6;5~",          L"ctrl-page-down" },
    { "\033[5;3~",          L"alt-page-up" },
    { "\033[6;3~",          L"alt-page-down" },
    { "\033-",              L"alt-minus" },
    { "\033 ",              L"alt-space" },
    { "\033\033[A",         L"alt-cursor-up" },
    { "\033\033[B",         L"alt-cursor-down" },
    { "\033\033[C",         L"alt-cursor-right" },
    { "\033\033[D",         L"alt-cursor-left" },
    { "\033\033[1~",        L"alt-home" },
    { "\033\033[2~",        L"alt-insert" },
    { "\033\033[3~",        L"alt-delete" },
    { "\033\033[4~",        L"alt-end" },
    { "\033\033[5~",        L"alt-page-up" },
    { "\033\033[6~",        L"alt-page-down" },
    { "\033OA",             L"ctrl-cursor-up" },
    { "\033OB",             L"ctrl-cursor-down" },
    { "\033OD",             L"ctrl-cursor-left" },
    { "\033OC",             L"ctrl-cursor-right" },
    { "\033[11~",           L"f1" },
    { "\033[12~",           L"f2" },
    { "\033[13~",           L"f3" },
    { "\033[14~",           L"f4" },
    { "\033[E",             L"super-5" },
    { NULL,                 NULL }
};

#define ctrl(k) ((k) & 31)

static mpdm_t ansi_getkey(mpdm_t args, mpdm_t ctxt)
{
    char *str;
    wchar_t *f = NULL;
    mpdm_t k = NULL;

    str = ansi_read_string(0);

    if (str) {
        /* only one char? it's an ASCII or ctrl character */
        if (str[1] == '\0') {
            if (str[0] == ' ')
                f = L"space";
            else
            if (str[0] == '\r')
                f = L"enter";
            else
            if (str[0] == '\t')
                f = L"tab";
            else
            if (str[0] == '\033')
                f = L"escape";
            else
            if (str[0] == '\b' || str[0] == '\177')
                f = L"backspace";
            else
            if (str[0] >= ctrl('a') && str[0] <= ctrl('z')) {
                char tmp[32];
    
                sprintf(tmp, "ctrl-%c", str[0] + 'a' - ctrl('a'));
                k = MPDM_MBS(tmp);
            }
            else
            if (str[0] == ctrl(' '))
                f = L"ctrl-space";
            else
                k = MPDM_MBS(str);
        }

        /* esc+letter? alt-letter */
        if (str[0] == '\033' && str[1] >= '!' &&
            str[1] <= '~' && str[2] == '\0') {
            char tmp[16];

            if (str[1] == '-')
                strcpy(tmp, "alt-minus");
            else
            if (str[1] == '+')
                strcpy(tmp, "alt-plus");
            else
                sprintf(tmp, "alt-%c", str[1]);

            k = MPDM_MBS(tmp);
        }

        /* if it's a string that starts with a backspace,
           it's probably a sequence of them, so treat it as only one */
        if (str[0] == '\b' || str[0] == '\177')
            f = L"backspace";

        /* still nothing? search the table of keys */
        if (k == NULL && f == NULL) {
            int n;
            char *ptr;
    
            for (n = 0; (ptr = str_to_code[n].ansi_str) != NULL; n++) {
                if (strncmp(ptr, str, strlen(ptr)) == 0) {
                    f = str_to_code[n].code;
                    break;
                }
            }
    
            /* if a found key starts with _shift-, set shift_pressed flag */
            if (f && wcsncmp(f, L"_shift-", 7) == 0) {
                mpdm_set_wcs(MP, MPDM_I(1), L"shift_pressed");
                f += 7;
            }
        }
    
        /* if there is still no recognized ANSI string, return string as is */
        if (k == NULL && f == NULL) {
            int n;
            mpdm_t v;

            /* convert carriage returns to new lines */
            for (n = 0; str[n]; n++) {
                if (str[n] == '\r')
                    str[n] = '\n';
            }

            v = MPDM_MBS(str);

            if (mpdm_size(v) == 1) {
                /* return as is */
                k = v;
            }
            else {
                /* more than one char */
                mpdm_set_wcs(MP, v, L"raw_string");
                f = L"insert-raw-string";
            }
        }

        /* if something, create a value */
        if (k == NULL && f != NULL)
            k = MPDM_S(f);
    }
    else
        k = MPDM_S(L"idle");

    return k;
}


static mpdm_t ansi_drv_idle(mpdm_t a, mpdm_t ctxt)
{
    idle_msecs = (int) (mpdm_rval(mpdm_get_i(a, 0)) * 1000);

    return NULL;
}


static mpdm_t ansi_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    ansi_raw_tty(0);

    /* set default attribute */
    printf("\033[0;39;49m\n");

    /* leave alternate screen */
    printf("\033[?1049l\n");

    if ((v = mpdm_get_wcs(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static mpdm_t ansi_drv_suspend(mpdm_t a, mpdm_t ctxt)
{
    ansi_raw_tty(0);

    printf("\nType 'fg' to return to Minimum Profit");
    fflush(stdout);

    /* Trigger suspending this process */
    kill(getpid(), SIGSTOP);

    ansi_raw_tty(1);

    return NULL;
}


static mpdm_t ansi_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
/* driver-dependent mp to system clipboard */
{
    mpdm_t d = mpdm_ref(mpdm_join_wcs(mpdm_get_wcs(MP, L"clipboard"), L"\n"));
    char *ptr = mpdm_wcstombs(mpdm_string(d), NULL);
    char *b64;

    /* if text is longer than 74994 (hard limit), truncate it */
    if (strlen(ptr) > 74994) {
        char *msg = "\n[Clipboard too big for ANSI copying]";
        strcpy(ptr + 74994 - strlen(msg) - 1, msg);
    }

    b64 = mpdm_base64enc_mbs((unsigned char *)ptr, strlen(ptr));

    /* use the OSC 52 ANSI code */

    /* empty clipboard */
    printf("\033]52;c;!\a");

    /* copy clipboard in base64 format */
    printf("\033]52;c;%s\a", b64);

    fflush(stdout);

    mpdm_unref(d);
    free(b64);
    free(ptr);

    return NULL;
}


/** double buffer **/

/* cursor position */
static int db_x      = 0;
static int db_y      = 0;

/* buffer size */
static int db_width  = 0;
static int db_height = 0;

/* buffers */
static unsigned int *db_scr = NULL;
static unsigned int *db_scr_o = NULL;

/* last attr set */
static unsigned int db_attr = 0;


static void ansi_db_resize(int w, int h)
/* initialize a new double buffer with a new size */
{
    /* alloc new space */
    free(db_scr);
    db_scr = (unsigned int *)calloc(w * h, sizeof(unsigned int));
    free(db_scr_o);
    db_scr_o = (unsigned int *)calloc(w * h, sizeof(unsigned int));

    db_x      = 0;
    db_y      = 0;
    db_width  = w;
    db_height = h;
}


static void ansi_db_print_v(mpdm_t v)
/* print a value */
{
    int i = db_y * db_width;
    wchar_t *p = mpdm_string(v);

    if (db_y < db_height - 1) {
        /* copy the string to current position */
        while (db_x < db_width && *p) {
            db_scr[i + db_x++] = ((unsigned int)*p * ansi_attrs_i) + db_attr;
            p++;
        }
    }
}


static void ansi_db_gotoxy(int x, int y)
/* set cursor position */
{
    db_x = x;
    db_y = y;
}


static void ansi_db_clreol(void)
/* clear to end of line */
{
    int i = db_y * db_width;
    unsigned int c = ' ' * ansi_attrs_i + db_attr;
    int n = db_x;

    /* fill the rest of the line with blanks */
    while (n < db_width)
        db_scr[i + n++] = c;
}


static void ansi_db_set_attr(int a)
/* set attribute for next characters */
{
    db_attr = a;
}


static void ansi_db_refresh(void)
/* dump the buffer to the screen */
{
    int n, m;
    int p_attr = -1;

    p_attr = ansi_set_attr(normal_attr);

    for (n = 0; n < db_height; n++) {
        int i = n * db_width;

        for (m = 0; m < db_width; m++) {
            /* different content? */
            if (db_scr[i + m] != db_scr_o[i + m]) {
                wchar_t w;
                int a;
                char tmp[64];

                ansi_gotoxy(m, n);

                /* unpack character and attr */
                w = db_scr[i + m] / ansi_attrs_i;
                a = db_scr[i + m] % ansi_attrs_i;

                /* write attr if different from the already set */
                if (a != p_attr)
                    p_attr = ansi_set_attr(a);

                tmp[wctomb(tmp, w)] = '\0';
                printf("%s", tmp);
            }
        }
    }

    ansi_gotoxy(db_x, db_y);

    ansi_refresh();

    /* save buffer */
    memcpy(db_scr_o, db_scr, db_width * db_height * sizeof(wchar_t));
}


static void ansi_db_getxy(int *x, int *y)
/* get cursor position */
{
    *x = db_x;
    *y = db_y;
}


static wchar_t ansi_db_charat(int x, int y)
/* get character at position */
{
    int i = y * db_width + x;

    return db_scr[i] / ansi_attrs_i;
}


/** TUI **/

static mpdm_t ansi_tui_addstr(mpdm_t a, mpdm_t ctxt)
/* TUI: add a string */
{
    ansi_db_print_v(mpdm_get_i(a, 0));

    return NULL;
}


static mpdm_t ansi_tui_move(mpdm_t a, mpdm_t ctxt)
/* TUI: move to a screen position */
{
    ansi_db_gotoxy(mpdm_ival(mpdm_get_i(a, 0)), mpdm_ival(mpdm_get_i(a, 1)));

    /* if third argument is not NULL, clear line */
    if (mpdm_get_i(a, 2) != NULL)
        ansi_db_clreol();

    return NULL;
}


static mpdm_t ansi_tui_attr(mpdm_t a, mpdm_t ctxt)
/* TUI: set attribute for next string */
{
    ansi_db_set_attr(mpdm_ival(mpdm_get_i(a, 0)));

    return NULL;
}


static mpdm_t ansi_tui_refresh(mpdm_t a, mpdm_t ctxt)
/* TUI: refresh the screen */
{
    ansi_db_refresh();

    return NULL;
}


static mpdm_t ansi_doc_draw(mpdm_t args, mpdm_t ctxt)
/* draw the document */
{
    mpdm_t d;
    int n, m;

    d = mpdm_get_i(args, 0);
    d = mpdm_ref(mp_draw(d, 0));

    for (n = 0; n < mpdm_size(d) && n < db_height - 1; n++) {
        mpdm_t l = mpdm_get_i(d, n);

        if (l != NULL) {
            ansi_db_gotoxy(0, n);

            for (m = 0; m < mpdm_size(l); m++) {
                int attr;
                mpdm_t s;

                /* get the attribute and the string */
                attr = mpdm_ival(mpdm_get_i(l, m++));
                s = mpdm_get_i(l, m);

                ansi_db_set_attr(attr);
                ansi_db_print_v(s);
            }

            ansi_db_set_attr(normal_attr);

            /* delete to end of line */
            ansi_db_clreol();
        }
    }

    mpdm_unref(d);

    return NULL;
}


static mpdm_t ansi_tui_getxy(mpdm_t a, mpdm_t ctxt)
/* TUI: returns the x and y cursor position */
{
    mpdm_t v;
    int x, y;

    ansi_db_getxy(&x, &y);

    v = MPDM_A(2);
    mpdm_ref(v);

    mpdm_set_i(v, MPDM_I(x), 0);
    mpdm_set_i(v, MPDM_I(y), 1);

    mpdm_unrefnd(v);

    return v;
}


static mpdm_t ansi_tui_charat(mpdm_t a, mpdm_t ctxt)
/* TUI: returns the character at x, y position */
{
    wchar_t s[2];
    int x, y;

    x = mpdm_ival(mpdm_get_i(a, 0));
    y = mpdm_ival(mpdm_get_i(a, 1));

    s[0] = ansi_db_charat(x, y);
    s[1] = L'\0';

    return MPDM_S(s);
}


static void ansi_register_functions(void)
{
    mpdm_t drv;
    mpdm_t tui;

    drv = mpdm_get_wcs(mpdm_root(), L"mp_drv");

    mpdm_set_wcs(drv, MPDM_X(ansi_drv_idle),        L"idle");
    mpdm_set_wcs(drv, MPDM_X(ansi_drv_shutdown),    L"shutdown");
    mpdm_set_wcs(drv, MPDM_X(ansi_drv_suspend),     L"suspend");
    mpdm_set_wcs(drv, MPDM_X(ansi_drv_clip_to_sys), L"clip_to_sys");

    /* execute tui */
    tui = mpsl_eval(MPDM_S(L"load('mp_tui.mpsl');"), NULL, NULL);

    mpdm_set_wcs(tui, MPDM_X(ansi_getkey),      L"getkey");
    mpdm_set_wcs(tui, MPDM_X(ansi_tui_addstr),  L"addstr");
    mpdm_set_wcs(tui, MPDM_X(ansi_tui_move),    L"move");
    mpdm_set_wcs(tui, MPDM_X(ansi_tui_attr),    L"attr");
    mpdm_set_wcs(tui, MPDM_X(ansi_tui_refresh), L"refresh");
    mpdm_set_wcs(tui, MPDM_X(ansi_tui_getxy),   L"getxy");
    mpdm_set_wcs(tui, MPDM_X(ansi_tui_charat),  L"charat");
    mpdm_set_wcs(tui, MPDM_X(ansi_doc_draw),    L"doc_draw");
}


static mpdm_t ansi_drv_startup(mpdm_t a)
{
    signal(SIGPIPE, SIG_IGN);

    ansi_register_functions();

    ansi_raw_tty(1);

    ansi_sigwinch(0);

    ansi_build_colors();

    /* enter alternate screen */
    printf("\033[?1049h");

    return NULL;
}


int ansi_drv_detect(int *argc, char ***argv)
{
    mpdm_t drv;

    drv = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"mp_drv");

    mpdm_set_wcs(drv, MPDM_S(L"ansi"),          L"id");
    mpdm_set_wcs(drv, MPDM_X(ansi_drv_startup), L"startup");

    return 1;
}


#endif /* CONFOPT_ANSI */
