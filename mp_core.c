/*

    Minimum Profit - A Text Editor

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/** data **/

/* exit requested? */
int mp_exit_requested = 0;

/** private data for drawing syntax-highlighted text **/

struct drw_1_info {
    mpdm_t txt;                 /* the document */
    mpdm_t syntax;              /* syntax highlight information */
    mpdm_t colors;              /* color definitions (for attributes) */
    mpdm_t word_color_func;     /* word color function (just for detection) */
    mpdm_t last_search;         /* last search regex */
    int normal_attr;            /* normal attr */
    int cursor_attr;            /* cursor attr */
    int n_lines;                /* number of processed lines */
    int p_lines;                /* number of prereaded lines */
    int t_lines;                /* total lines in document */
    int vx;                     /* first visible column */
    int vy;                     /* first visible line */
    int tx;                     /* horizontal window size */
    int ty;                     /* vertical window size */
    int tab_size;               /* tabulator size */
    int mod;                    /* modify count */
    int preread_lines;          /* lines to pre-read (for synhi blocks) */
    int mark_eol;               /* mark end of lines */
    int redraw;                 /* redraw trigger */
    int xoffset;                /* # of columns reserved for line numbers */
    int double_page;            /* # of columns where double page activates */
    wchar_t wc_tab;             /* char for marking tabs */
    wchar_t wc_formfeed;        /* char for marking form feeds */
    wchar_t wc_unknown;         /* char for marking unknown characters */
};

struct drw_1_info drw_1;
struct drw_1_info drw_1_o;

static struct {
    int x;                      /* cursor x */
    int y;                      /* cursor y */
    int *offsets;               /* offsets of lines */
    char *attrs;                /* attributes */
    int visible;                /* offset to the first visible character */
    int cursor;                 /* offset to cursor */
    wchar_t *ptr;               /* pointer to joined data */
    int size;                   /* size of joined data */
    int matchparen_offset;      /* offset to matched paren */
    int matchparen_o_attr;      /* original attribute there */
    int cursor_o_attr;          /* original attribute under cursor */
    mpdm_t v;                   /* the data */
    mpdm_t old;                 /* the previously generated array */
    int mark_offset;            /* offset to the marked block */
    int mark_size;              /* size of mark_o_attr */
    char *mark_o_attr;          /* saved attributes for the mark */

    wchar_t *cmap;
    int *amap;
    int *vx2x;
    int *vy2y;
    int mx;
    int my;
} drw_2;

/** code **/

#define MP_REAL_TAB_SIZE(x) (drw_1.tab_size - ((x) % drw_1.tab_size))

static int drw_wcwidth(int x, wchar_t c)
/* returns the wcwidth of c, or the tab spaces for
   the x column if it's a tab */
{
    int r;

    switch (c) {
    case L'\n':
        r = 1;
        break;

    case L'\t':
        r = MP_REAL_TAB_SIZE(x);
        break;

    default:
        r = 1;
        break;
    }

    return r < 0 ? 1 : r;
}


int drw_vx2x(mpdm_t str, int vx)
/* returns the character in str that is on column vx */
{
    const wchar_t *ptr = str->data;
    int n, x;

    for (n = x = 0; n < vx && ptr[x] != L'\0'; x++)
        n += drw_wcwidth(n, ptr[x]);

    return x;
}


int drw_x2vx(mpdm_t str, int x)
/* returns the column where the character at offset x seems to be */
{
    const wchar_t *ptr;
    int n, vx = 0;

    if (str) {
        ptr = str->data;
        for (n = vx = 0; n < x && ptr[n] != L'\0'; n++)
            vx += drw_wcwidth(vx, ptr[n]);
    }

    return vx;
}


static int drw_line_offset(int l)
/* returns the offset into v for line number l */
{
    return drw_2.offsets[l - drw_1.vy + drw_1.p_lines];
}


static int drw_adjust_y(int y, int *vy, int ty)
/* adjusts the visual y position */
{
    int t = *vy;

    if (*vy < 0)
        *vy = 0;

    /* is y above the first visible line? */
    if (y < *vy)
        *vy = y;

    /* is y below the last visible line? */
    if (y > *vy + (ty - 2))
        *vy = y - (ty - 2);

    return t != *vy;
}


static int drw_adjust_x(int x, int y, int *vx, int tx, wchar_t * ptr)
/* adjust the visual x position */
{
    int n, m;
    int t = *vx;

    /* calculate the column for the cursor position */
    for (n = m = 0; n < x; n++, ptr++)
        m += drw_wcwidth(m, *ptr);

    /* if new cursor column is nearer the leftmost column, set */
    if (m < *vx)
        *vx = m;

    /* if new cursor column is further the rightmost column, set */
    if (m > *vx + (tx - 1))
        *vx = m - (tx - 1);

    return t != *vx;
}


static int drw_get_attr(wchar_t * color_name)
/* returns the attribute number for a color */
{
    mpdm_t v;
    int attr = 0;

    if ((v = mpdm_get_wcs(drw_1.colors, color_name)) != NULL)
        attr = mpdm_ival(mpdm_get_wcs(v, L"attr"));

    return attr;
}


static int drw_prepare(mpdm_t doc)
/* prepares the document for screen drawing */
{
    mpdm_t window   = mpdm_ref(mpdm_get_wcs(MP, L"window"));
    mpdm_t config   = mpdm_ref(mpdm_get_wcs(MP, L"config"));
    mpdm_t txt      = mpdm_ref(mpdm_get_wcs(doc, L"txt"));
    mpdm_t lines    = mpdm_ref(mpdm_get_wcs(txt, L"lines"));
    int x           = mpdm_ival(mpdm_get_wcs(txt, L"x"));
    int y           = mpdm_ival(mpdm_get_wcs(txt, L"y"));
    int n;
    mpdm_t v;
    wchar_t *ptr;
    int ret = 1;

    mpdm_store(&drw_1.txt,             txt);
    mpdm_store(&drw_1.syntax,          mpdm_get_wcs(doc, L"syntax"));
    mpdm_store(&drw_1.colors,          mpdm_get_wcs(MP, L"colors"));
    mpdm_store(&drw_1.word_color_func, mpdm_get_wcs(MP, L"word_color_func"));
    mpdm_store(&drw_1.last_search,     mpdm_get_wcs(MP, L"last_search"));

    drw_1.vx            = mpdm_ival(mpdm_get_wcs(txt, L"vx"));
    drw_1.vy            = mpdm_ival(mpdm_get_wcs(txt, L"vy"));
    drw_1.tx            = mpdm_ival(mpdm_get_wcs(window, L"tx"));
    drw_1.ty            = mpdm_ival(mpdm_get_wcs(window, L"ty"));
    drw_1.tab_size      = mpdm_ival(mpdm_get_wcs(config, L"tab_size"));
    drw_1.mod           = mpdm_ival(mpdm_get_wcs(txt, L"mod"));
    drw_1.mark_eol      = mpdm_ival(mpdm_get_wcs(config, L"mark_eol"));
    drw_1.t_lines       = mpdm_size(lines);

    /* get preread_lines from the syntax definition */
    drw_1.preread_lines = mpdm_ival(mpdm_get_wcs(drw_1.syntax, L"preread_lines"));

    /* if it's 0, get it from the configuration */
    if (drw_1.preread_lines == 0)
        drw_1.preread_lines = mpdm_ival(mpdm_get_wcs(config, L"preread_lines"));

    n = mpdm_ival(mpdm_get_wcs(config, L"double_page"));

    if (n && drw_1.tx > n) {
        /* if the usable screen is wider than the
           double page setting, activate it */
        drw_1.tx /= 2;
        drw_1.ty *= 2;
        drw_1.double_page = 1;
    }
    else
        drw_1.double_page = 0;

    /* adjust the visual y coordinate */
    if (drw_adjust_y(y, &drw_1.vy, drw_1.ty))
        mpdm_set_wcs(txt, MPDM_I(drw_1.vy), L"vy");

    /* adjust the visual x coordinate */
    if (drw_adjust_x(x, y, &drw_1.vx, drw_1.tx, mpdm_string(mpdm_get_i(lines, y))))
        mpdm_set_wcs(txt, MPDM_I(drw_1.vx), L"vx");

    /* get the maximum prereadable lines */
    drw_1.p_lines = drw_1.vy > drw_1.preread_lines ? drw_1.preread_lines : drw_1.vy;

    /* maximum lines */
    drw_1.n_lines = drw_1.ty + drw_1.p_lines;

    /* get the most used attributes */
    drw_1.normal_attr = drw_get_attr(L"normal");
    drw_1.cursor_attr = drw_get_attr(L"cursor");

    drw_2.x = x;
    drw_2.y = y;

    /* redraw trigger */
    drw_1.redraw = mpdm_ival(mpdm_get_wcs(MP, L"redraw_counter"));

    /* calculate number of lines reserved for line numbers */
    n = mpdm_ival(mpdm_get_wcs(config, L"show_line_numbers"));

    if (n) {
        char tmp[32];
        sprintf(tmp, " %d ", (int) mpdm_size(lines));
        drw_1.xoffset = strlen(tmp);
    }
    else
        drw_1.xoffset = 0;

    mpdm_set_wcs(MP, MPDM_I(drw_1.xoffset), L"xoffset");

    /* placeholder Unicode characters */
    n = mpdm_ival(mpdm_get_wcs(config, L"use_unicode"));
    v = mpdm_get_i(mpdm_get_wcs(MP, L"unicode_tbl"), n);

    if ((ptr = mpdm_string(mpdm_get_wcs(v, L"tab"))) != NULL)
        drw_1.wc_tab = *ptr;
    if ((ptr = mpdm_string(mpdm_get_wcs(v, L"formfeed"))) != NULL)
        drw_1.wc_formfeed = *ptr;
    if ((ptr = mpdm_string(mpdm_get_wcs(v, L"unknown"))) != NULL)
        drw_1.wc_unknown = *ptr;

    /* compare drw_1 with drw_1_o; if they are the same,
       no more expensive calculations on drw_2 are needed */
    if (memcmp(&drw_1, &drw_1_o, sizeof(drw_1)) != 0) {

        /* different; store now */
        memcpy(&drw_1_o, &drw_1, sizeof(drw_1_o));

        /* alloc space for line offsets */
        drw_2.offsets = realloc(drw_2.offsets, drw_1.n_lines * sizeof(int));

        drw_2.ptr = NULL;
        drw_2.size = 0;

        /* add first line */
        drw_2.ptr = mpdm_pokev(drw_2.ptr, &drw_2.size,
                        mpdm_get_i(lines, drw_1.vy - drw_1.p_lines));

        /* first line start at 0 */
        drw_2.offsets[0] = 0;

        /* add the following lines and store their offsets */
        for (n = 1; n < drw_1.n_lines; n++) {
            /* add the separator */
            drw_2.ptr = mpdm_pokews(drw_2.ptr, &drw_2.size, L"\n");

            /* this line starts here */
            drw_2.offsets[n] = drw_2.size;

            /* now add it */
            drw_2.ptr = mpdm_pokev(drw_2.ptr, &drw_2.size,
                            mpdm_get_i(lines, n + drw_1.vy - drw_1.p_lines));
        }

        drw_2.ptr = mpdm_poke(drw_2.ptr, &drw_2.size, L"", 1, sizeof(wchar_t));
        drw_2.size--;

        /* now create a value */
        mpdm_store(&drw_2.v, MPDM_ENS(drw_2.ptr, drw_2.size));

        /* alloc and init space for the attributes */
        drw_2.attrs = realloc(drw_2.attrs, drw_2.size + 1);
        memset(drw_2.attrs, drw_1.normal_attr, drw_2.size + 1);

        drw_2.visible = drw_line_offset(drw_1.vy);

        /* (re)create the maps */
        n = (drw_1.tx + 1) * drw_1.ty;
        drw_2.cmap  = realloc(drw_2.cmap, n * sizeof(wchar_t));
        drw_2.amap  = realloc(drw_2.amap, n * sizeof(int));
        drw_2.vx2x  = realloc(drw_2.vx2x, n * sizeof(int));
        drw_2.vy2y  = realloc(drw_2.vy2y, n * sizeof(int));

        drw_2.mx = drw_2.my = -1;
    }
    else
        /* drw_1 didn't change */
        ret = 0;

    mpdm_unref(lines);
    mpdm_unref(txt);
    mpdm_unref(config);
    mpdm_unref(window);

    return ret;
}


static int drw_fill_attr(int attr, int offset, int size)
/* fill an attribute */
{
    if (attr != -1)
        memset(drw_2.attrs + offset, attr, size);

    return offset + size;
}


static int drw_fill_attr_regex(int attr)
/* fills with an attribute the last regex match */
{
    return drw_fill_attr(attr, mpdm_regex_offset, mpdm_regex_size);
}


static void drw_words(void)
/* fills the attributes for separate words */
{
    mpdm_t word_color = NULL;

    /* take the hash of word colors, if any */
    if ((word_color = mpdm_ref(mpdm_get_wcs(MP, L"word_color"))) != NULL) {
        mpdm_t r;

        /* get the regex for words */
        if ((r = mpdm_ref(mpdm_get_wcs(MP, L"word_regex"))) != NULL) {
            mpdm_t func, t;
            int o = drw_2.visible;

            /* get the word color function */
            func = mpdm_ref(mpdm_get_wcs(MP, L"word_color_func"));

            while ((t = mpdm_ref(mpdm_regex(drw_2.v, r, o))) != NULL) {
                int attr = -1;
                mpdm_t v;

                if ((v = mpdm_get(word_color, t)) != NULL)
                    attr = mpdm_ival(v);
                else
                if (func != NULL)
                    attr = mpdm_ival(mpdm_exec_1(func, t, NULL));

                o = drw_fill_attr_regex(attr);

                mpdm_unref(t);
            }

            mpdm_unref(func);
            mpdm_unref(r);
        }

        mpdm_unref(word_color);
    }
}


static void drw_multiline_regex(mpdm_t a, int attr)
/* sets the attribute to all matching (possibly multiline) regexes */
{
    int n;

    mpdm_ref(a);

    for (n = 0; n < mpdm_size(a); n++) {
        mpdm_t r = mpdm_get_i(a, n);
        int o = 0;

        /* if the regex is an array, it's a pair of
           'match from this' / 'match until this' */
        if (mpdm_type(r) == MPDM_TYPE_ARRAY) {
            mpdm_t rs = mpdm_get_i(r, 0);
            mpdm_t re = mpdm_get_i(r, 1);

            while (!mpdm_is_null(mpdm_regex(drw_2.v, rs, o))) {
                int s;

                /* fill the matched part */
                o = drw_fill_attr_regex(attr);

                /* try to match the end */
                if (!mpdm_is_null(mpdm_regex(drw_2.v, re, o))) {
                    /* found; fill the attribute
                       to the end of the match */
                    s = mpdm_regex_size + (mpdm_regex_offset - o);
                }
                else {
                    /* not found; fill to the end
                       of the document */
                    s = drw_2.size - o;
                }

                /* fill to there */
                o = drw_fill_attr(attr, o, s);
            }
        }
        else {
            /* must be a scalar */

            if (*mpdm_string(r) == L'%') {
                /* it's a sscanf() expression */
                mpdm_t v;

                while ((v = mpdm_ref(mpdm_sscanf(drw_2.v, r, o)))
                       && mpdm_size(v) == 2) {
                    int i = mpdm_ival(mpdm_get_i(v, 0));
                    int s = mpdm_ival(mpdm_get_i(v, 1)) - i;

                    o = drw_fill_attr(attr, i, s);

                    mpdm_unref(v);
                }
            }
            else {
                /* it's a regex */
                /* while the regex matches, fill attributes */
                while (!mpdm_is_null(mpdm_regex(drw_2.v, r, o)) && mpdm_regex_size)
                    o = drw_fill_attr_regex(attr);
            }
        }
    }

    mpdm_unref(a);
}


static void drw_blocks(void)
/* fill attributes for multiline blocks */
{
    mpdm_t defs;

    if (drw_1.syntax && (defs = mpdm_get_wcs(drw_1.syntax, L"defs"))) {
        int n;

        for (n = 0; n < mpdm_size(defs); n += 2) {
            mpdm_t attr;
            mpdm_t list;

            /* get the attribute */
            attr = mpdm_get_i(defs, n);
            attr = mpdm_get(drw_1.colors, attr);
            attr = mpdm_get_wcs(attr, L"attr");

            /* get the list for this word color */
            list = mpdm_get_i(defs, n + 1);

            drw_multiline_regex(list, mpdm_ival(attr));
        }
    }
}


static void drw_selection(void)
/* draws the selected block, if any */
{
    mpdm_t mark;
    int bx, by, ex, ey, vertical;
    int so, eo;
    int mby, mey;
    int line_offset, next_line_offset;
    int y;
    int len;
    int attr;

    /* no mark? return */
    if ((mark = mpdm_get_wcs(drw_1.txt, L"mark")) == NULL)
        return;

    bx = mpdm_ival(mpdm_get_wcs(mark, L"bx"));
    by = mpdm_ival(mpdm_get_wcs(mark, L"by"));
    ex = mpdm_ival(mpdm_get_wcs(mark, L"ex"));
    ey = mpdm_ival(mpdm_get_wcs(mark, L"ey"));
    vertical = mpdm_ival(mpdm_get_wcs(mark, L"vertical"));

    /* if block is not visible, return */
    if (ey < drw_1.vy || by >= drw_1.vy + drw_1.ty)
        return;

    so = by < drw_1.vy ? drw_2.visible : drw_line_offset(by) + bx;
    eo = ey >= drw_1.vy + drw_1.ty ? drw_2.size : drw_line_offset(ey) + ex;

    /* alloc space and save the attributes being destroyed */
    drw_2.mark_offset = so;
    drw_2.mark_size = eo - so + 1;
    drw_2.mark_o_attr = malloc(eo - so + 1);
    memcpy(drw_2.mark_o_attr, &drw_2.attrs[so], eo - so + 1);

    if (vertical == 0) {
        /* normal selection */
        drw_fill_attr(drw_get_attr(L"selection"), so, eo - so);
    }
    else {
        /* vertical selection */
        mby = by < drw_1.vy ? drw_1.vy : by;
        mey = ey >= drw_1.vy + drw_1.ty ? drw_1.vy + drw_1.ty : ey;
        line_offset = drw_line_offset(mby);
        attr = drw_get_attr(L"selection");
        for (y = mby; y <= mey; y++) {
            next_line_offset = drw_line_offset(y + 1);
            len = next_line_offset - line_offset - 1;
            so = bx > len ? -1 : bx;
            eo = ex > len ? len : ex;

            if (so >= 0 && eo >= so)
                drw_fill_attr(attr, line_offset + so, eo - so + 1);

            line_offset = next_line_offset;
        }
    }
}


static void drw_search_hit(void)
/* colorize the search hit, if any */
{
    if (drw_1.last_search != NULL) {
        mpdm_t l = MPDM_A(0);

        mpdm_set_i(l, drw_1.last_search, 0);
        drw_multiline_regex(l, drw_get_attr(L"search"));
    }
}


static void drw_cursor(void)
/* fill the attribute for the cursor */
{
    /* calculate the cursor offset */
    drw_2.cursor = drw_line_offset(drw_2.y) + drw_2.x;

    drw_2.cursor_o_attr = drw_2.attrs[drw_2.cursor];
    drw_fill_attr(drw_1.cursor_attr, drw_2.cursor, 1);
}


static void drw_matching_paren(void)
/* highlights the matching paren */
{
    int o = drw_2.cursor;
    int i = 0;
    wchar_t c;

    /* by default, no offset has been found */
    drw_2.matchparen_offset = -1;

    /* find the opposite and the increment (direction) */
    switch (drw_2.ptr[o]) {
    case L'(':
        c = L')';
        i = 1;
        break;
    case L'{':
        c = L'}';
        i = 1;
        break;
    case L'[':
        c = L']';
        i = 1;
        break;
    case L')':
        c = L'(';
        i = -1;
        break;
    case L'}':
        c = L'{';
        i = -1;
        break;
    case L']':
        c = L'[';
        i = -1;
        break;
    }

    /* if a direction is set, do the searching */
    if (i) {
        wchar_t s = drw_2.ptr[o];
        int m = 0;
        int l = i == -1 ? drw_2.visible - 1 : drw_2.size;

        while (o != l) {
            if (drw_2.ptr[o] == s) {
                /* found the same */
                m++;
            }
            else if (drw_2.ptr[o] == c) {
                /* found the opposite */
                if (--m == 0) {
                    /* found! fill and exit */
                    drw_2.matchparen_offset = o;
                    drw_2.matchparen_o_attr = drw_2.attrs[o];
                    drw_fill_attr(drw_get_attr(L"matching"), o, 1);
                    break;
                }
            }

            o += i;
        }
    }
}


static mpdm_t drw_push_pair(mpdm_t l, int i, int a, wchar_t * tmp)
/* pushes a pair of attribute / string into l */
{
    /* create the array, if doesn't exist yet */
    if (l == NULL)
        l = MPDM_A(0);

    /* finish the string */
    tmp[i] = L'\0';

    /* special magic: if the attribute is the
       one of the cursor and the string is more than
       one character, create two strings; the
       cursor is over a tab */
    if (a == drw_1.cursor_attr && i > 1) {
        mpdm_push(l, MPDM_I(a));
        mpdm_push(l, MPDM_NS(tmp, 1));

        /* the rest of the string has the original attr */
        a = drw_2.cursor_o_attr;

        /* one char less */
        tmp[i - 1] = L'\0';
    }

    /* store the attribute and the string */
    mpdm_push(l, MPDM_I(a));
    mpdm_push(l, MPDM_S(tmp));

    return l;
}


static wchar_t drw_char(wchar_t c, int n)
/* does possible conversions to the char about to be printed */
{
    /* real tab */
    if (n == 0 && c == L'\t')
        c = drw_1.wc_tab;

    /* soft hyphen */
    if (c == L'\xad')
        c = L'\xb8';        /* cedilla */

    if (drw_1.mark_eol) {
        if (c == L'\t')
            c = L'\xb7';    /* middledot */
        else
        if (c == L'\n' || c == L'\0')
            c = L'\xb6';    /* pilcrow */
    }
    else {
        if (c == L'\t' || c == L'\n' || c == L'\0')
            c = L' ';
    }

    /* replace form feed with visual representation and
       the rest of control codes with the Unicode replace char */
    if (c == L'\f')
        c = drw_1.wc_formfeed;
    else
    if (c < L' ')
        c = drw_1.wc_unknown;

    return c;
}


static void drw_map_1(int mx, int my, wchar_t c, int a, int x, int y)
{
    int o = mx + my * (drw_1.tx + 1);

    drw_2.cmap[o] = c;
    drw_2.amap[o] = a;
    drw_2.vx2x[o] = x;
    drw_2.vy2y[o] = y;

    if (a == drw_1.cursor_attr) {
        drw_2.mx = mx;
        drw_2.my = my;
    }
}


static void vpos2pos(int mx, int my, int *x, int *y)
{
    if (my < 0) {
        /* above top margin: pick previous line */
        *x = mx;
        *y = drw_1.vy - 2;
    }
    else
    if (my > drw_1.ty - 1) {
        /* below bottom margin: pick next line */
        *x = mx;
        *y = drw_1.vy + drw_1.ty;
    }
    else {
        /* in range: pick from the map */
        int o = mx + my * (drw_1.tx + 1);

        *x = drw_2.vx2x[o];
        *y = drw_2.vy2y[o];
    }
}


static void drw_remap_truncate(void)
{
    int i = drw_2.offsets[drw_1.p_lines];
    int mx, my;
    int x, y;

    x = 0;
    y = drw_1.vy;

    for (my = 0; my < drw_1.ty; my++) {
        wchar_t c;
        int ax = 0;
        mx = 0;

        do {
            c = drw_2.ptr[i];
            int t = drw_wcwidth(mx, c);
            int n;

            if (c == '\0')
                break;

            for (n = 0; n < t && mx < drw_1.tx; n++) {
                if (ax >= drw_1.vx)
                    drw_map_1(mx++, my, drw_char(c, n), drw_2.attrs[i], x, y);
                ax++;
            }

            i++;

            if (c == '\n')
                break;

            x++;

        } while (mx < drw_1.tx);

        while (mx < drw_1.tx)
            drw_map_1(mx++, my, '.', -1, x, y);

        while (c != '\n' && c != '\0')
            c = drw_2.ptr[i++];

        if (c == '\n') {
            x = 0;
            y++;
        }
    }
}


static void drw_double_page(void)
/* recompose a double page char-mapped buffer */
{
    if (drw_1.double_page) {
        int n, m;
        wchar_t *dp_cmap = drw_2.cmap;
        int *dp_amap     = drw_2.amap;
        int *dp_vx2x     = drw_2.vx2x;
        int *dp_vy2y     = drw_2.vy2y; 
        int o = 0;

        n = (drw_1.tx + 1) * drw_1.ty;
        drw_2.cmap  = calloc(n, sizeof(wchar_t));
        drw_2.amap  = calloc(n, sizeof(int));
        drw_2.vx2x  = calloc(n, sizeof(int));
        drw_2.vy2y  = calloc(n, sizeof(int));

        for (n = 0; n < drw_1.ty / 2; n++) {
            int i;

            /* copy first column */
            i = n * (drw_1.tx + 1);

            for (m = 0; m < drw_1.tx && dp_amap[i] != -1; m++) {
                drw_2.amap[o] = dp_amap[i];
                drw_2.cmap[o] = dp_cmap[i];
                drw_2.vx2x[o] = dp_vx2x[i];
                drw_2.vy2y[o] = dp_vy2y[i];

                i++;
                o++;
            }

            /* fill up to next column with spaces */
            for (; m < drw_1.tx; m++) {
                drw_2.amap[o] = drw_1.normal_attr;
                drw_2.cmap[o] = L' ';
                drw_2.vx2x[o] = dp_vx2x[i];
                drw_2.vy2y[o] = dp_vy2y[i];

                o++;
            }

            /* put the column separator */
            drw_2.amap[o] = drw_1.normal_attr;
            drw_2.cmap[o] = L'\x2502';
            o++;

            /* copy the second column */
            i = (n - 1 + drw_1.ty / 2) * (drw_1.tx + 1);

            for (m = 0; m < drw_1.tx; m++) {
                drw_2.amap[o] = dp_amap[i];
                drw_2.cmap[o] = dp_cmap[i];
                drw_2.vx2x[o] = dp_vx2x[i];
                drw_2.vy2y[o] = dp_vy2y[i];

                i++;
                o++;
            }
        }

        /* restore size */
        drw_1.tx *= 2;
        drw_1.ty /= 2;

        free(dp_cmap);
        free(dp_amap);
    }
}


static mpdm_t drw_remap_to_array(void)
/* converts the char-mapped buffer to arrays of attr, string per line */
{
    mpdm_t r = MPDM_A(0);
    wchar_t *line = malloc((drw_1.tx + 1) * sizeof(wchar_t));
    int my;
    mpdm_t fmt = NULL;

    if (drw_1.xoffset) {
        fmt = mpdm_ref(mpdm_strcat(MPDM_S(L" %"),
            mpdm_strcat_wcs(MPDM_I(drw_1.xoffset - 2), L"d ")));
    }

    for (my = 0; my < drw_1.ty; my++) {
        mpdm_t l = MPDM_A(0);
        int o = my * (drw_1.tx + 1);
        int mx = 0;

        if (drw_1.xoffset && drw_1.vy + my < drw_1.t_lines) {
            mpdm_push(l, MPDM_I(drw_1.normal_attr));
            mpdm_push(l, mpdm_fmt(fmt, MPDM_I(drw_1.vy + 1 + my)));
        }

        while (mx < drw_1.tx && drw_2.amap[o] != -1) {
            int i = 0;
            int a = drw_2.amap[o];

            while (a == drw_2.amap[o] && mx++ < drw_1.tx)
                line[i++] = drw_2.cmap[o++];

            line[i] = L'\0';

            l = drw_push_pair(l, i, a, line);
        }

        mpdm_push(r, l);
    }

    free(line);

    mpdm_unref(fmt);

    return r;
}


static mpdm_t drw_optimize_array(mpdm_t a, int optimize)
/* optimizes the array, NULLifying all lines that are the same as the last time */
{
    mpdm_t o = drw_2.old;
    mpdm_t r = a;

    mpdm_ref(a);

    if (optimize && o != NULL) {
        int n = 0;

        /* creates a copy */
        r = mpdm_clone(a);

        mpdm_ref(r);

        /* compare each array */
        while (n < mpdm_size(o) && n < mpdm_size(r)) {
            /* if both lines are equal, optimize out */
            if (mpdm_cmp(mpdm_get_i(o, n), mpdm_get_i(r, n)) == 0)
                mpdm_set_i(r, NULL, n);

            n++;
        }

        mpdm_unrefnd(r);
    }

    mpdm_store(&drw_2.old, a);

    mpdm_unref(a);

    return r;
}


static void drw_restore_attrs(void)
/* restored the patched attrs */
{
    /* matching paren, if any */
    if (drw_2.matchparen_offset != -1)
        drw_fill_attr(drw_2.matchparen_o_attr, drw_2.matchparen_offset, 1);

    /* cursor */
    drw_fill_attr(drw_2.cursor_o_attr, drw_2.cursor, 1);

    /* marked block, if any */
    if (drw_2.mark_o_attr != NULL) {
        memcpy(&drw_2.attrs[drw_2.mark_offset], drw_2.mark_o_attr,
               drw_2.mark_size);

        free(drw_2.mark_o_attr);
        drw_2.mark_o_attr = NULL;
    }
}


static mpdm_t drw_draw(mpdm_t doc, int optimize)
/* main document drawing function: takes a document and returns an array of
   arrays of attribute / string pairs */
{
    mpdm_t r = NULL, w;

    if (drw_prepare(doc)) {
        /* colorize separate words */
        drw_words();

        /* colorize multiline blocks */
        drw_blocks();
    }

    /* now set the marked block (if any) */
    drw_selection();

    /* colorize the search hit */
    drw_search_hit();

    /* the cursor */
    drw_cursor();

    /* highlight the matching paren */
    drw_matching_paren();

    /* convert to an array of string / atribute pairs */
    drw_remap_truncate();

    drw_double_page();

    r = drw_remap_to_array();

    /* optimize */
    r = drw_optimize_array(r, optimize);

    /* restore the patched attrs */
    drw_restore_attrs();

    w = mpdm_get_wcs(MP, L"window");
    mpdm_set_wcs(w, MPDM_I(drw_2.mx), L"mx");
    mpdm_set_wcs(w, MPDM_I(drw_2.my), L"my");

    return r;
}


/** interface **/

mpdm_t mp_draw(mpdm_t doc, int optimize)
/* main generic drawing function for drivers */
{
    mpdm_t f, r = NULL;
    static mpdm_t d = NULL;

    if (doc != d) {
        optimize = 0;
        mpdm_store(&d, doc);
    }

    if ((f = mpdm_get_wcs(doc, L"render")) != NULL) {
        /* create a context to contain the object itself
           (i.e. call as a method) */
        mpdm_t ctxt = MPDM_A(0);

        mpdm_push(ctxt, doc);
        r = mpdm_exec_2(f, doc, MPDM_I(optimize), ctxt);
    }

    return r;
}


#define THR_SPEED_STEP  10
#define THR_MAX_SPEED   7

int mp_keypress_throttle(int keydown)
/* processes key acceleration and throttle */
{
#ifdef USE_THROTTLE
    static int keydowns = 0;
    static int seq = 0;
    int redraw = 0;

    if (keydown) {
        int speed;

        /* as keydowns accumulate, speed increases, which is the number
           of cycles the redraw will be skipped (up to a maximum) */
        if ((speed = 1 + (++keydowns / THR_SPEED_STEP)) > THR_MAX_SPEED)
            speed = THR_MAX_SPEED;

        if (++seq % speed == 0)
            redraw = 1;
    }
    else {
        if (keydowns > 1)
            redraw = 1;

        keydowns = 0;
    }

    return redraw;

#else /* USE_THROTTLE */
    return 1;
#endif /* USE_THROTTLE */
}


mpdm_t mp_active(void)
/* interface to mp.active() */
{
    return mpdm_exec(mpdm_get_wcs(MP, L"active"), NULL, NULL);
}


void mp_process_action(mpdm_t action)
/* interface to mp.process_action() */
{
    mpdm_void(mpdm_exec_1(mpdm_get_wcs(MP, L"process_action"), action, NULL));
}


void mp_process_event(mpdm_t keycode)
/* interface to mp.process_event() */
{
    mpdm_void(mpdm_exec_1(mpdm_get_wcs(MP, L"process_event"), keycode, NULL));
}


void mp_set_y(mpdm_t doc, int y)
/* interface to mp.set_y() */
{
    mpdm_void(mpdm_exec_2(mpdm_get_wcs(doc, L"set_y"), doc, MPDM_I(y), NULL));
}


mpdm_t mp_build_status_line(void)
/* interface to mp.build_status_line() */
{
    return mpdm_exec(mpdm_get_wcs(MP, L"build_status_line"), NULL, NULL);
}


mpdm_t mp_get_history(mpdm_t key)
/* interface to mp.get_history() */
{
    return mpdm_exec_1(mpdm_get_wcs(MP, L"get_history"), key, NULL);
}


mpdm_t mp_get_doc_names(void)
/* interface to mp.get_doc_names() */
{
    return mpdm_exec(mpdm_get_wcs(MP, L"get_doc_names"), NULL, NULL);
}


mpdm_t mp_menu_label(mpdm_t action)
/* interface to mp.menu_label() */
{
    return mpdm_exec_1(mpdm_get_wcs(MP, L"menu_label"), action, NULL);
}


mpdm_t mp_c_exit(mpdm_t args, mpdm_t ctxt)
/* exit the editor (set mp_exit_requested) */
{
    mp_exit_requested = 1;

    return NULL;
}


static mpdm_t mp_c_exit_requested(mpdm_t args, mpdm_t ctxt)
/* returns the value of the mp_exit_requested variable */
{
    return MPDM_I(mp_exit_requested);
}


mpdm_t mp_c_render(mpdm_t args, mpdm_t ctxt)
{
    return drw_draw(mpdm_get_i(args, 0), mpdm_ival(mpdm_get_i(args, 1)));
}


mpdm_t mp_c_vx2x(mpdm_t args, mpdm_t ctxt)
/* interface to drw_vx2x() */
{
    return MPDM_I(drw_vx2x(mpdm_get_i(args, 0), mpdm_ival(mpdm_get_i(args, 1))));
}


mpdm_t mp_c_x2vx(mpdm_t args, mpdm_t ctxt)
/* interface to drw_x2vx() */
{
    return MPDM_I(drw_x2vx(mpdm_get_i(args, 0), mpdm_ival(mpdm_get_i(args, 1))));
}


mpdm_t mp_c_vpos2pos(mpdm_t args, mpdm_t ctxt)
{
    mpdm_t r = MPDM_A(2);
    int x = mpdm_ival(mpdm_get_i(args, 0));
    int y = mpdm_ival(mpdm_get_i(args, 1));

    vpos2pos(x, y, &x, &y);

    mpdm_set_i(r, MPDM_I(x), 0);
    mpdm_set_i(r, MPDM_I(y), 1);

    return r;
}


mpdm_t mp_c_search_hex(mpdm_t args, mpdm_t ctxt)
/* search the hex string str in the file */
{
    mpdm_t fd = mpdm_get_i(args, 0);
    mpdm_t str = mpdm_get_i(args, 1);
    FILE *f = mpdm_get_filehandle(fd);
    wchar_t *s = mpdm_string(str);
    int n = 0;
    unsigned char *ptr;
    off_t o;
    int found = 0;

    /* parse str into a binary buffer */
    ptr = malloc(wcslen(s) + 1);
    while (s[0] && s[1]) {
        char tmp[3];
        int c;

        tmp[0] = (char)s[0];
        tmp[1] = (char)s[1];
        sscanf(tmp, "%02x", &c);

        ptr[n++] = (unsigned char) c;
        s += 2;
    }
    ptr[n] = 0;

    /* start searching */
    o = ftell(f);
    while (!found && !feof(f)) {
        int c;

        fseek(f, o, 0);
        n = 0;

        while (!found && (c = fgetc(f)) != EOF) {
            if (c == ptr[n]) {
                n++;

                if (ptr[n] == '\0')
                    found = 1;
            }
            else {
                o++;
                break;
            }
        }
    }

    if (found)
        fseek(f, o, 0);

    free(ptr);

    return MPDM_I(found);
}


mpdm_t mp_c_get_offset(mpdm_t args, mpdm_t ctxt)
/* gets the character offset at lines[y][0] */
{
    mpdm_t lines = mpdm_get_i(args, 0);
    int y = mpdm_ival(mpdm_get_i(args, 1));
    int n, o = 0;

    for (n = 0; n < y; n++) {
        mpdm_t v = mpdm_get_i(lines, n);
        wchar_t *ptr = mpdm_string(v);
        int z = wcslen(ptr);

        /* count 1 less if it ends in soft-hyphen, 1 more if not */
        z += (z && ptr[z - 1] == 0xad) ? -1 : 1;

        o += z;
    }

    return MPDM_I(o);
}


mpdm_t mp_c_set_offset(mpdm_t args, mpdm_t ctxt)
/* gets the x, y position of offset */
{
    mpdm_t lines = mpdm_get_i(args, 0);
    int o = mpdm_ival(mpdm_get_i(args, 1));
    int n, y = 0;
    mpdm_t r;

    for (n = 0; n < mpdm_size(lines); n++) {
        mpdm_t v = mpdm_get_i(lines, n);
        wchar_t *ptr = mpdm_string(v);
        int z = wcslen(ptr);

        /* count 1 less if it ends in soft-hyphen, 1 more if not */
        z += (z && ptr[z - 1] == 0xad) ? -1 : 1;

        if (o <= z)
            break;

        o -= z;
        y++;
    }

    r = MPDM_A(2);
    mpdm_set_i(r, MPDM_I(o), 0);
    mpdm_set_i(r, MPDM_I(y), 1);

    return r;
}


#ifndef CONFOPT_EXTERNAL_TAR

#ifdef CONFOPT_EMBED_NOUNDER

extern const char binary_mp_tar_start;
extern const char binary_mp_tar_end;
#define TAR_START binary_mp_tar_start
#define TAR_END binary_mp_tar_end

#else /* CONFOPT_EMBED_NOUNDER */

extern const char _binary_mp_tar_start;
extern const char _binary_mp_tar_end;
#define TAR_START _binary_mp_tar_start
#define TAR_END _binary_mp_tar_end

#endif /* CONFOPT_EMBED_NOUNDER */

static mpdm_t find_in_embedded_tar(mpdm_t args, mpdm_t ctxt)
/* searches for embedded MPSL code */
{
    return mpsl_find_in_embedded_tar(mpdm_get_i(args, 0), &TAR_START, &TAR_END);
}

#endif /* CONFOPT_EXTERNAL_TAR */


mpdm_t ni_drv_startup(mpdm_t v)
{
    return NULL;
}


int ni_drv_detect(int *argc, char ***argv)
{
    int n, ret = 0;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-ni") == 0 || strcmp(argv[0][n], "-F") == 0)
            ret = 1;
    }

    if (ret) {
        mpdm_t drv;

        drv = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"mp_drv");
        mpdm_set_wcs(drv, MPDM_S(L"ni"), L"id");
        mpdm_set_wcs(drv, MPDM_X(ni_drv_startup), L"startup");
    }

    return ret;
}


void mp_startup(int argc, char *argv[])
{
    mpdm_t INC;
    char *ptr;
    mpdm_t mp_c;

    mpdm_startup();
    mpdm_set_wcs(mpdm_root(), MPDM_MBS(CONFOPT_APPNAME), L"APPID");

    mpsl_startup();

    /* reset the structures */
    memset(&drw_1, '\0', sizeof(drw_1));
    memset(&drw_1_o, '\0', sizeof(drw_1_o));

    /* set an initial value for drw_1.tab_size:
       drw_wcwidth() may be called before drw_1 being filled
       by drw_prepare() (when opening files from the command
       line and mp.config.visual_wrap == 1) */
    drw_1.tab_size = 1;

    /* new mp_c namespace (C interface) */
    mp_c = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"mp_c");

    /* version */
    mpdm_set_wcs(mp_c, MPDM_S(L"" VERSION),         L"VERSION");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_x2vx),           L"x2vx");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_vx2x),           L"vx2x");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_vpos2pos),       L"vpos2pos");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_exit),           L"exit");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_exit_requested), L"exit_requested");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_render),         L"render");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_search_hex),     L"search_hex");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_get_offset),     L"get_offset");
    mpdm_set_wcs(mp_c, MPDM_X(mp_c_set_offset),     L"set_offset");

    /* creates the INC (executable path) array */
    INC = mpdm_set_wcs(mpdm_root(), MPDM_A(0), L"INC");

    /* if the MP_LIBRARY_PATH environment variable is set,
       put it before anything else */
    if ((ptr = getenv("MP_LIBRARY_PATH")) != NULL)
        mpdm_push(INC, MPDM_MBS(ptr));

#ifdef CONFOPT_EXTERNAL_TAR
    /* add code library as externally installed tar */
    mpdm_push(INC, mpdm_strcat_wcs(
        mpdm_get_wcs(mpdm_root(), L"APPDIR"), L"/mp.tar"));

#else /* CONFOPT_EXTERNAL_TAR */

    /* add code library as embedded tar */
    mpdm_push(INC, MPDM_X(find_in_embedded_tar));

#endif /* CONFOPT_EXTERNAL_TAR */

    if (!ni_drv_detect(&argc, &argv) && !TRY_DRIVERS()) {
        printf("No usable driver found; exiting.\n");
        exit(1);
    }

    mpsl_argv(argc, argv);
}


void mp_mpsl(void)
{
    mpdm_t e;

    mpdm_void(mpsl_eval(MPDM_S(L"load('mp_core.mpsl');"), NULL, NULL));

    if ((e = mpdm_get_wcs(mpdm_root(), L"ERROR")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(e));
        printf("\n");
    }
}


void mp_shutdown(void)
{
    /* unref pending values */
    mpdm_unref(drw_1.txt);
    mpdm_unref(drw_2.v);
    mpdm_unref(drw_2.old);

#ifdef DEBUG_CLEANUP
    mpdm_unref(mpdm_root());
#endif

    mpsl_shutdown();
}


int main(int argc, char *argv[])
{
    mp_startup(argc, argv);

    mp_mpsl();

    mp_shutdown();

    return 0;
}
