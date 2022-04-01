/*

    MPDM - Minimum Profit Data Manager
    mpdm_r.c - Regular expressions

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "mpdm.h"

#ifdef CONFOPT_PCRE
#include <pcreposix.h>
#endif

#ifdef CONFOPT_SYSTEM_REGEX
#include <regex.h>
#endif

#ifdef CONFOPT_INCLUDED_REGEX
#include "gnu_regex.h"
#endif


/** data **/

/* matching of the last regex */

int mpdm_regex_offset = -1;
int mpdm_regex_size = 0;

/* number of substitutions in last sregex */

int mpdm_sregex_count = 0;


/** code **/

static wchar_t *regex_flags(const mpdm_t r)
{
    wchar_t *ptr = mpdm_string(r);
    return wcsrchr(ptr, *ptr);
}


static mpdm_t vc_regex_destroy(mpdm_t v)
{
    regfree((regex_t *) mpdm_data(v));
    v->data = NULL;

    return v;
}


mpdm_t mpdm_regcomp(mpdm_t r)
{
    mpdm_t c = NULL;
    mpdm_t regex_cache = NULL;

    mpdm_ref(r);

    /* if cache does not exist, create it */
    if ((regex_cache = mpdm_get_wcs(mpdm_root(), L"__REGEX_CACHE__")) == NULL)
        regex_cache = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"__REGEX_CACHE__");

    /* search the regex in the cache */
    if ((c = mpdm_get(regex_cache, r)) == NULL) {
        mpdm_t rmb;
        regex_t re;
        char *regex;
        char *flags;
        int f = REG_EXTENDED;

        /* not found; regex must be compiled */

        /* convert to mbs */
        rmb = mpdm_ref(MPDM_2MBS(mpdm_data(r)));
        regex = (char *) mpdm_data(rmb);

        if ((flags = strrchr(regex, *regex)) != NULL) {

            if (strchr(flags, 'i') != NULL)
                f |= REG_ICASE;
            if (strchr(flags, 'm') != NULL)
                f |= REG_NEWLINE;

            regex++;
            *flags = '\0';

            if (!regcomp(&re, regex, f)) {
                c = MPDM_C(MPDM_TYPE_REGEX, &re, sizeof(regex_t));

                mpdm_set(regex_cache, c, r);
            }
        }

        mpdm_unref(rmb);
    }

    mpdm_unref(r);

    return c;
}


static mpdm_t regex1(mpdm_t cr, const mpdm_t v, int offset)
/* test for one regex */
{
    mpdm_t w = NULL;
    regmatch_t rm;
    char *ptr;

    mpdm_ref(cr);
    mpdm_ref(v);

    /* no matching yet */
    mpdm_regex_offset = -1;

    /* convert to mbs */
    ptr = mpdm_wcstombs((wchar_t *) mpdm_string(v) + offset, NULL);

    /* match? */
    if (regexec((regex_t *) mpdm_data(cr), ptr, 1, &rm, offset > 0 ? REG_NOTBOL : 0) == 0) {
        /* converts to mbs the string from the beginning
           to the start of the match, just to know
           the size (and immediately frees it) */
        free(mpdm_mbstowcs(ptr, &mpdm_regex_offset, rm.rm_so));

        /* add the offset */
        mpdm_regex_offset += offset;

        /* create now the matching string */
        w = MPDM_NMBS(ptr + rm.rm_so, rm.rm_eo - rm.rm_so);

        /* and store the size */
        mpdm_regex_size = mpdm_size(w);
    }

    free(ptr);

    mpdm_unref(v);
    mpdm_unref(cr);

    return w;
}


/**
 * mpdm_regex - Matches a regular expression.
 * @v: the value to be matched
 * @r: the regular expression
 * @offset: offset from the start of mpdm_data(v)
 *
 * Matches a regular expression against a value. Valid flags are 'i',
 * for case-insensitive matching, or 'm', to treat the string as a
 * multiline string (i.e., one containing newline characters), so
 * that ^ and $ match the boundaries of each line instead of the
 * whole string.
 *
 * If @r is a string, an ordinary regular expression matching is tried
 * over the @v string. If the matching is possible, the match result
 * is returned, or NULL otherwise.
 *
 * If @r is an array (of strings), each element is tried sequentially
 * as an individual regular expression over the @v string, each one using
 * the offset returned by the previous match. All regular expressions
 * must match to be successful. If this is the case, an array (with
 * the same number of arguments) is returned containing the matched
 * strings, or NULL otherwise.
 *
 * If @r is NULL, the result of the previous regex matching
 * is returned as a two element array. The first element will contain
 * the character offset of the matching and the second the number of
 * characters matched. If the previous regex was unsuccessful, NULL
 * is returned.
 * [Regular Expressions]
 */
mpdm_t mpdm_regex(const mpdm_t v, const mpdm_t r, int offset)
{
    mpdm_t t, w = NULL;
    int n;

    mpdm_ref(r);
    mpdm_ref(v);

    /* special case: if v or r is NULL, return previous match */
    if (v == NULL || r == NULL) {
        /* if previous regex was successful... */
        if (mpdm_regex_offset != -1) {
            w = MPDM_A(2);

            mpdm_set_i(w, MPDM_I(mpdm_regex_offset), 0);
            mpdm_set_i(w, MPDM_I(mpdm_regex_size), 1);
        }
    }
    else {
        switch (mpdm_type(r)) {
        case MPDM_TYPE_ARRAY:
            /* multiple value; try sequentially all regexes,
               moving the offset forward */

            w = MPDM_A(0);

            for (n = 0; n < mpdm_size(r); n++) {
                if ((t = mpdm_regex(v, mpdm_get_i(r, n), offset)) == NULL)
                    break;

                /* found; store and move forward */
                mpdm_push(w, t);
                offset = mpdm_regex_offset + mpdm_regex_size;
            }

            break;

        case MPDM_TYPE_STRING:
            w = mpdm_regex(v, mpdm_regcomp(r), offset);
            break;

        case MPDM_TYPE_REGEX:
            w = regex1(r, v, offset);
            break;

        default:
            w = NULL;
            break;
        }
    }

    mpdm_unref(v);
    mpdm_unref(r);

    return w;
}


static mpdm_t expand_ampersands(const mpdm_t s, const mpdm_t t)
/* substitutes all unescaped ampersands in s with t */
{
    const wchar_t *sptr = mpdm_string(s);
    wchar_t *wptr;
    wchar_t *optr = NULL;
    int osize = 0;
    mpdm_t r = NULL;

    mpdm_ref(s);
    mpdm_ref(t);

    if (s != NULL) {
        while ((wptr = wcschr(sptr, L'\\')) != NULL ||
               (wptr = wcschr(sptr, L'&')) != NULL) {
            int n = wptr - sptr;

            /* add the leading part */
            optr = mpdm_pokewsn(optr, &osize, sptr, n);

            if (*wptr == L'\\') {
                if (*(wptr + 1) == L'&' || *(wptr + 1) == L'\\')
                    wptr++;

                optr = mpdm_pokewsn(optr, &osize, wptr, 1);
            }
            else
            if (*wptr == '&')
                optr = mpdm_pokev(optr, &osize, t);

            sptr = wptr + 1;
        }

        /* add the rest of the string */
        optr = mpdm_pokews(optr, &osize, sptr);
        optr = mpdm_pokewsn(optr, &osize, L"", 1);
        r = MPDM_ENS(optr, osize - 1);
    }

    mpdm_unref(t);
    mpdm_unref(s);

    return r;
}


/**
 * mpdm_sregex - Matches and substitutes a regular expression.
 * @v: the value to be matched
 * @r: the regular expression
 * @s: the substitution string, hash or code
 * @offset: offset from the start of mpdm_data(v)
 *
 * Matches a regular expression against a value, and substitutes the
 * found substring with @s. Valid flags are 'i', for case-insensitive
 * matching, and 'g', for global replacements (all ocurrences in @v
 * will be replaced, instead of just the first found one).
 *
 * If @s is executable, it's executed with the matched part as
 * the only argument and its return value is used as the
 * substitution string.
 *
 * If @s is a hash, the matched string is used as a key to it and
 * its value used as the substitution. If this value itself is
 * executable, it's executed with the matched string as its only
 * argument and its return value used as the substitution.
 *
 * If @r is NULL, returns the number of substitutions made in the
 * previous call to mpdm_sregex() (can be zero if none was done).
 *
 * The global variables @mpdm_regex_offset and @mpdm_regex_size are
 * set to the offset of the matched string and the size of the
 * replaced string, respectively.
 *
 * Always returns a new string (either modified or an exact copy).
 * [Regular Expressions]
 */
mpdm_t mpdm_sregex(mpdm_t v, const mpdm_t r, const mpdm_t s, int offset)
{
    mpdm_t o = NULL;

    mpdm_ref(v);
    mpdm_ref(r);
    mpdm_ref(s);

    if (r == NULL) {
        /* return last count */
        o = MPDM_I(mpdm_sregex_count);
    }
    else
    if (v != NULL) {
        wchar_t *global;
        mpdm_t m = NULL;

        mpdm_sregex_count = 0;

        /* take pointer to global flag */
        if ((global = regex_flags(r)) != NULL)
            global = wcschr(global, 'g');

        o = mpdm_ref(MPDM_S(mpdm_string(v)));

        while ((m = mpdm_regex(o, r, offset)) != NULL) {
            mpdm_t w, no = NULL;
            int del, add;

            /* get match information before it gets possibly
               destroyed by mpdm_exec_1() or others */
            offset = mpdm_regex_offset;
            del    = mpdm_regex_size;

            mpdm_ref(m);
            w = s;

            /* loop while it's executable or an object */
            for (;;) {
                if (mpdm_can_exec(w))
                    w = mpdm_exec_1(w, m, NULL);
                else
                if (mpdm_type(w) == MPDM_TYPE_OBJECT)
                    w = mpdm_get(w, m);
                else {
                    w = expand_ampersands(w, m);
                    break;
                }
            }

            mpdm_unref(m);

            add = mpdm_size(w);

            /* splice */
            mpdm_splice(o, w, offset, del, &no, NULL);
            mpdm_store(&o, no);

            /* next iteration shall be after the insertion */
            offset += add;

            /* one more substitution */
            mpdm_sregex_count++;

            if (!global)
                break;
        }

        mpdm_unrefnd(o);
    }

    mpdm_unref(s);
    mpdm_unref(r);
    mpdm_unref(v);

    return o;
}


/** data vc **/

struct mpdm_type_vc mpdm_vc_regex = { /* VC */
    L"regex",               /* name */
    vc_regex_destroy,       /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_default_get,         /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_default_set,         /* set */
    vc_default_exec,        /* exec */
    vc_default_iterator,    /* iterator */
    vc_default_map,         /* map */
    vc_default_cannot_exec, /* can_exec */
    vc_default_clone        /* clone */
};
