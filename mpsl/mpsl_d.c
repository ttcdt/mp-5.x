/*

    MPSL - Minimum Profit Scripting Language
    Debugging functions

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

#include "mpdm.h"
#include "mpsl.h"

/** code **/

static wchar_t *dump_string(const mpdm_t v, wchar_t * ptr, int *size)
/* dumps a string, escaping special chars */
{
    wchar_t *iptr = mpdm_string(v);

    ptr = mpdm_pokews(ptr, size, L"\"");

    while (*iptr != L'\0') {
        switch (*iptr) {
        case '"':
            ptr = mpdm_pokews(ptr, size, L"\\\"");
            break;

        case '\'':
            ptr = mpdm_pokews(ptr, size, L"\\'");
            break;

        case '\r':
            ptr = mpdm_pokews(ptr, size, L"\\r");
            break;

        case '\n':
            ptr = mpdm_pokews(ptr, size, L"\\n");
            break;

        case '\t':
            ptr = mpdm_pokews(ptr, size, L"\\t");
            break;

        case '\\':
            ptr = mpdm_pokews(ptr, size, L"\\\\");
            break;

        default:
            if (*iptr >= 127) {
                char tmp[16];
                wchar_t wtmp[16];

                sprintf(tmp, "\\x{%x}", (int) *iptr);
                mbstowcs(wtmp, tmp, sizeof(wtmp));
                ptr = mpdm_pokews(ptr, size, wtmp);
            }
            else
                ptr = mpdm_pokewsn(ptr, size, iptr, 1);

            break;
        }
        iptr++;
    }

    ptr = mpdm_pokews(ptr, size, L"\"");

    return ptr;
}


wchar_t *mpsl_dump_1(const mpdm_t v, int l, wchar_t *ptr, int *size)
/* dump plugin for mpdm_dump() */
{
    int n;
    char tmp[256];
    mpdm_t w, i;
    int64_t c;
    int f;

    switch (mpdm_type(v)) {
    case MPDM_TYPE_NULL:
        ptr = mpdm_pokews(ptr, size, L"NULL");
        break;

    case MPDM_TYPE_OBJECT:
        ptr = mpdm_pokews(ptr, size, L"{");

        if (mpdm_count(v)) {
            ptr = mpdm_pokews(ptr, size, L"\n");

            c = n = 0;
            while (mpdm_iterator(v, &c, &w, &i)) {
                if (n++)
                    ptr = mpdm_pokews(ptr, size, L",\n");

                for (f = 0; f <= l; f++)
                    ptr = mpdm_pokews(ptr, size, L"  ");

                ptr = mpsl_dump_1(i, l + 1, ptr, size);
                ptr = mpdm_pokews(ptr, size, L" => ");
                ptr = mpsl_dump_1(w, l + 1, ptr, size);
            }

            ptr = mpdm_pokews(ptr, size, L"\n");

            for (f = 0; f < l; f++)
                ptr = mpdm_pokews(ptr, size, L"  ");
        }

        ptr = mpdm_pokews(ptr, size, L"}");
        break;

    case MPDM_TYPE_PROGRAM:
        if (l > 0) {
            snprintf(tmp, sizeof(tmp), "valueptr('%p') /* program */", v);
            ptr = mpdm_pokev(ptr, size, MPDM_MBS(tmp));
            break;
        }

        /* fallthrough */

    case MPDM_TYPE_ARRAY:
        ptr = mpdm_pokews(ptr, size, L"[");

        if (mpdm_count(v)) {
            ptr = mpdm_pokews(ptr, size, L"\n");

            c = n = 0;
            while (mpdm_iterator(v, &c, &w, NULL)) {
                if (n++)
                    ptr = mpdm_pokews(ptr, size, L",\n");

                for (f = 0; f <= l; f++)
                    ptr = mpdm_pokews(ptr, size, L"  ");

                ptr = mpsl_dump_1(w, l + 1, ptr, size);
            }

            ptr = mpdm_pokews(ptr, size, L"\n");

            for (f = 0; f < l; f++)
                ptr = mpdm_pokews(ptr, size, L"  ");
        }

        ptr = mpdm_pokews(ptr, size, L"]");
        break;

    case MPDM_TYPE_FUNCTION:
        snprintf(tmp, sizeof(tmp), "bincall('%p')", mpdm_data(v));
        ptr = mpdm_pokev(ptr, size, MPDM_MBS(tmp));

        break;

    case MPDM_TYPE_INTEGER:
    case MPDM_TYPE_REAL:
        ptr = mpdm_pokews(ptr, size, mpdm_string(v));
        break;

    case MPDM_TYPE_STRING:
        ptr = dump_string(v, ptr, size);
        break;

    default:
        snprintf(tmp, sizeof(tmp), "valueptr('%p') /" "* ", v);
        ptr = mpdm_pokev(ptr, size, MPDM_MBS(tmp));
        ptr = mpdm_pokews(ptr, size, mpdm_type_wcs(v));
        ptr = mpdm_pokews(ptr, size, L" */");

        break;
    }

    if (l == 0)
        ptr = mpdm_pokews(ptr, size, L";\n");

    return ptr;
}


static wchar_t *decompile_1(mpdm_t ins, wchar_t *ptr, int *z, mpdm_t op, int i)
{
    int n;
    mpdm_t o, v;

    /* indent */
    for (n = 0; n < i; n++)
        ptr = mpdm_pokews(ptr, z, L"  ");

    mpdm_ref(op);

    o = mpdm_get_i(ins, 0);

    /* search opcode name */
    v = mpdm_get_i(op, mpdm_ival(o));

    ptr = mpdm_pokev(ptr, z, v);
    ptr = mpdm_pokews(ptr, z, L"(");

    if (mpdm_ival(o) == 0) {
        /* literal */
        ptr = mpsl_dump_1(mpdm_get_i(ins, 1), i, ptr, z);
    }
    else {
        ptr = mpdm_pokews(ptr, z, L"\n");

        for (n = 1; n < mpdm_size(ins); n++) {
            if (n > 1)
                ptr = mpdm_pokews(ptr, z, L",\n");

            ptr = decompile_1(mpdm_get_i(ins, n), ptr, z, op, i + 1);
        }
    }

    ptr = mpdm_pokews(ptr, z, L")");

    mpdm_unref(op);

    return ptr;
}


mpdm_t mpsl_decompile(mpdm_t prg)
{
    wchar_t *ptr = NULL;
    int z = 0;

    if (mpdm_type(prg) == MPDM_TYPE_PROGRAM) {
        mpdm_t op = MPDM_A(0), v, i;
        int64_t n = 0;

        /* reverse the opcodes as an array */
        while (mpdm_iterator(mpsl_opcodes, &n, &v, &i))
            mpdm_set_i(op, i, mpdm_ival(v));

        ptr = decompile_1(mpdm_get_i(prg, 1), ptr, &z, op, 0);
    }

    return ptr ? MPDM_NS(ptr, z) : NULL;
}
