/*

    MPDM - Minimum Profit Data Manager
    mpdm_f.c - File management

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#ifdef CONFOPT_CANONICALIZE_FILE_NAME
#define _GNU_SOURCE
#endif

#ifdef CONFOPT_WIN32
/* include this first to avoid clashing with standard select() */
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef CONFOPT_WIN32

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>

#undef UNICODE

#include <ws2tcpip.h>

#else /* CONFOPT_WIN32 */

#ifdef CONFOPT_GLOB_H
#include <glob.h>
#endif

#ifdef CONFOPT_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef CONFOPT_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef CONFOPT_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef CONFOPT_NETDB_H
#include <netdb.h>
#endif

#ifdef CONFOPT_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <fcntl.h>

#endif /* CONFOPT_WIN32 */

#ifdef CONFOPT_UNISTD_H
#include <unistd.h>
#endif

#ifdef CONFOPT_PWD_H
#include <pwd.h>
#endif

#ifdef CONFOPT_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef CONFOPT_SYS_FILE_H
#include <sys/file.h>
#endif

#include "mpdm.h"

#ifdef CONFOPT_ICONV
#include <iconv.h>
#endif

#define MAX_EOL 2

/* file structure */
struct mpdm_file {
    FILE *in;
    FILE *out;

    int sock;
    int is_pipe;
    int skip_wait;

    wchar_t eol[MAX_EOL + 1];
    int auto_chomp;

    wchar_t *(*f_read) (struct mpdm_file *, int *, int *);
    int (*f_write)  (struct mpdm_file *, const wchar_t *);

#ifdef CONFOPT_ICONV
    iconv_t ic_enc;
    iconv_t ic_dec;
#endif                          /* CONFOPT_ICONV */

#ifdef CONFOPT_WIN32
    HANDLE hin;
    HANDLE hout;
    HANDLE process;
#endif                          /* CONFOPT_WIN32 */
};

#include <errno.h>


/** code **/


static void store_syserr(void)
/* stores the system error inside the global ERRNO */
{
    char tmp[1024];

    sprintf(tmp, "%d %s", errno, strerror(errno));
    mpdm_set_wcs(mpdm_root(), MPDM_MBS(tmp), L"ERRNO");
}


static int get_byte(struct mpdm_file *f)
/* reads a byte from a file structure */
{
    int c = EOF;

#ifdef CONFOPT_WIN32

    if (f->hin != NULL) {
        char tmp;
        DWORD n;

        if (ReadFile(f->hin, &tmp, 1, &n, NULL) && n > 0)
            c = (int) tmp;
    }

#endif /* CONFOPT_WIN32 */

    if (f->in != NULL) {
        /* read (converting to positive if needed) */
        if ((c = fgetc(f->in)) < 0 && !feof(f->in))
            c += 256;
    }

    if (f->sock != -1) {
        char b;

        if (recv(f->sock, &b, sizeof(b), 0) == sizeof(b))
            c = b;
    }

    return c;
}


static int put_buf(const char *ptr, int s, struct mpdm_file *f)
/* writes s bytes in the buffer in ptr to f */
{
#ifdef CONFOPT_WIN32

    if (f->hout != NULL) {
        DWORD n;

        if (WriteFile(f->hout, ptr, s, &n, NULL) && n > 0)
            s = n;
    }
    else
#endif                          /* CONFOPT_WIN32 */

    if (f->out != NULL)
        s = fwrite(ptr, s, 1, f->out);

    if (f->sock != -1)
        s = send(f->sock, ptr, s, 0);

    return s;
}


static int put_byte(int c, struct mpdm_file *f)
/* writes a character in a file structure */
{
    char tmp = c;

    if (put_buf(&tmp, 1, f) != 1)
        c = EOF;

    return c;
}


static int store_in_line(wchar_t **ptr, int *s, int *eol, wchar_t wc)
/* store the c in the line, keeping track for EOLs */
{
    int done = 0;

    /* track EOL sequence position */
    if (eol && *eol == -1) {
        if (wc == L'\r' || wc == L'\n')
            *eol = *s;
    }

    /* store */
    *ptr = mpdm_pokewsn(*ptr, s, &wc, 1);

    /* end of line? finish */
    if (wc == L'\n')
        done = 1;

    return done;
}


static wchar_t *read_mbs(struct mpdm_file *f, int *s, int *eol)
/* reads a multibyte string from a mpdm_file into a dynamic string */
{
    char tmp[128];
    wchar_t *ptr = NULL;
    int c, i = 0;
    wchar_t wc;
    mbstate_t ps;

    while ((c = get_byte(f)) != EOF) {
        int r = -1;

        if (i < sizeof(tmp)) {
            tmp[i++] = c;

            /* try to convert what is read by now */
            memset(&ps, '\0', sizeof(ps));
            r = mbrtowc(&wc, tmp, i, &ps);

            /* incomplete sequence? keep trying */
            if (r == -2)
                continue;
        }

        if (r == -1) {
            /* too many failing bytes or invalid sequence;
               use the Unicode replacement char */
            wc = L'\xfffd';
        }

        i = 0;

        if (store_in_line(&ptr, s, eol, wc))
            break;
    }

    return ptr;
}


static int write_wcs(struct mpdm_file *f, const wchar_t *str)
/* writes a wide string to an struct mpdm_file */
{
    int s;
    char *ptr;

    ptr = mpdm_wcstombs(str, &s);
    s = put_buf(ptr, s, f);
    free(ptr);

    return s;
}


#ifdef CONFOPT_ICONV

static wchar_t *read_iconv(struct mpdm_file *f, int *s, int *eol)
/* reads a multibyte string transforming with iconv */
{
    char tmp[128];
    wchar_t *ptr = NULL;
    int c, i = 0;
    wchar_t wc;

    /* resets the decoder */
    iconv(f->ic_dec, NULL, NULL, NULL, NULL);

    while ((c = get_byte(f)) != EOF) {
        size_t il, ol;
        char *iptr, *optr;

        tmp[i++] = c;

        /* too big? shouldn't happen */
        if (i == sizeof(tmp))
            break;

        il = i;
        iptr = tmp;
        ol = sizeof(wchar_t);
        optr = (char *) &wc;

        /* write to file */
        if (iconv(f->ic_dec, &iptr, &il, &optr, &ol) == (size_t) - 1) {
            /* found incomplete multibyte character */
            if (errno == EINVAL)
                continue;

            /* otherwise, return '?' */
            wc = L'\xfffd';
        }

        i = 0;

        if (store_in_line(&ptr, s, eol, wc))
            break;
    }

    return ptr;
}


static int write_iconv(struct mpdm_file *f, const wchar_t *str)
/* writes a wide string to a stream using iconv */
{
    char tmp[128];
    int cnt = 0;

    /* resets the encoder */
    iconv(f->ic_enc, NULL, NULL, NULL, NULL);

    /* convert char by char */
    for (; *str != L'\0'; str++) {
        size_t il, ol;
        char *iptr, *optr;
        int n;

        il = sizeof(wchar_t);
        iptr = (char *) str;
        ol = sizeof(tmp);
        optr = tmp;

        /* write to file */
        if (iconv(f->ic_enc, &iptr, &il, &optr, &ol) == (size_t) - 1) {
            /* error converting; convert a '?' instead */
            wchar_t q = L'?';

            il = sizeof(wchar_t);
            iptr = (char *) &q;
            ol = sizeof(tmp);
            optr = tmp;

            iconv(f->ic_enc, &iptr, &il, &optr, &ol);
        }

        for (n = 0; n < (int) (sizeof(tmp) - ol); n++, cnt++) {
            if (put_byte(tmp[n], f) == EOF)
                return -1;
        }
    }

    return cnt;
}


#endif /* CONFOPT_ICONV */

#define BYTE_OR_BREAK(c, f) if((c = get_byte(f)) == EOF) break

static wchar_t *read_utf8(struct mpdm_file *f, int *s, int *eol)
/* utf8 reader */
{
    wchar_t *ptr = NULL;
    int c, st = 0;

    while ((c = get_byte(f)) != EOF) {
        wchar_t wc;

        if (mpdm_utf8_to_wc(&wc, &st, c) == 0)
            if (store_in_line(&ptr, s, eol, wc))
                break;
    }

    return ptr;
}


static int write_utf8(struct mpdm_file *f, const wchar_t *str)
/* utf8 writer */
{
    int cnt = 0;
    wchar_t wc;

    /* convert char by char */
    for (; (wc = *str) != L'\0'; str++) {
        if (wc < 0x80)
            put_byte((int) wc, f);
        else
        if (wc < 0x800) {
            put_byte((int) (0xc0 | (wc >> 6)), f);
            put_byte((int) (0x80 | (wc & 0x3f)), f);
            cnt++;
        }
        else
        if (wc < 0x10000) {
            put_byte((int) (0xe0 | (wc >> 12)), f);
            put_byte((int) (0x80 | ((wc >> 6) & 0x3f)), f);
            put_byte((int) (0x80 | (wc & 0x3f)), f);
            cnt += 2;
        }
        else
        if (wc < 0x200000) {
            put_byte((int) (0xf0 | (wc >> 18)), f);
            put_byte((int) (0x80 | ((wc >> 12) & 0x3f)), f);
            put_byte((int) (0x80 | ((wc >> 6) & 0x3f)), f);
            put_byte((int) (0x80 | (wc & 0x3f)), f);
            cnt += 3;
        }

        cnt++;
    }

    return cnt;
}


static wchar_t *read_utf8_bom(struct mpdm_file *f, int *s, int *eol)
/* utf-8 reader with BOM detection */
{
    wchar_t *enc = L"";

    f->f_read = NULL;

    /* autodetection */
    if (get_byte(f) == 0xef && get_byte(f) == 0xbb && get_byte(f) == 0xbf)
        enc = L"utf-8bom";
    else {
        enc = L"utf-8";
        fseek(f->in, 0, SEEK_SET);
    }

    mpdm_set_wcs(mpdm_root(), MPDM_S(enc), L"DETECTED_ENCODING");

    /* we're utf-8 from now on */
    f->f_read = read_utf8;

    return f->f_read(f, s, eol);
}


static int write_utf8_bom(struct mpdm_file *f, const wchar_t *str)
/* utf-8 writer with BOM */
{
    /* store the BOM */
    put_byte(0xef, f);
    put_byte(0xbb, f);
    put_byte(0xbf, f);

    /* we're utf-8 from now on */
    f->f_write = write_utf8;

    return f->f_write(f, str);
}


static wchar_t *read_iso8859_1(struct mpdm_file *f, int *s, int *eol)
/* iso8859-1 reader */
{
    wchar_t *ptr = NULL;
    wchar_t wc;
    int c;

    while ((c = get_byte(f)) != EOF) {
        wc = c;

        if (store_in_line(&ptr, s, eol, wc))
            break;
    }

    return ptr;
}


static int write_iso8859_1(struct mpdm_file *f, const wchar_t *str)
/* iso8859-1 writer */
{
    int cnt = 0;
    wchar_t wc;

    /* convert char by char */
    for (; (wc = *str) != L'\0'; str++)
        put_byte(wc <= 0xff ? (int) wc : '?', f);

    return cnt;
}


static wchar_t *read_utf16ae(struct mpdm_file *f, int *s, int *eol, int le)
/* utf16 reader, ANY ending */
{
    wchar_t *ptr = NULL;
    wchar_t wc;
    int c1, c2;

    for (;;) {
        wc = L'\0';

        BYTE_OR_BREAK(c1, f);
        BYTE_OR_BREAK(c2, f);

        if (le)
            wc = c1 | (c2 << 8);
        else
            wc = c2 | (c1 << 8);

        if (store_in_line(&ptr, s, eol, wc))
            break;
    }

    return ptr;
}


static int write_utf16ae(struct mpdm_file *f, const wchar_t *str, int le)
/* utf16 writer, ANY ending */
{
    int cnt = 0;
    wchar_t wc;

    /* convert char by char */
    for (; (wc = *str) != L'\0'; str++) {

        if (le) {
            put_byte(wc & 0xff, f);
            put_byte((wc & 0xff00) >> 8, f);
        }
        else {
            put_byte((wc & 0xff00) >> 8, f);
            put_byte(wc & 0xff, f);
        }
    }

    return cnt;
}


static wchar_t *read_utf16le(struct mpdm_file *f, int *s, int *eol)
{
    return read_utf16ae(f, s, eol, 1);
}


static int write_utf16le(struct mpdm_file *f, const wchar_t *str)
{
    return write_utf16ae(f, str, 1);
}


static wchar_t *read_utf16be(struct mpdm_file *f, int *s, int *eol)
{
    return read_utf16ae(f, s, eol, 0);
}


static int write_utf16be(struct mpdm_file *f, const wchar_t *str)
{
    return write_utf16ae(f, str, 0);
}


static wchar_t *read_utf16(struct mpdm_file *f, int *s, int *eol)
{
    int c1, c2;
    wchar_t *enc = L"utf-16le";

    /* assume little-endian */
    f->f_read = read_utf16le;

    /* autodetection */
    c1 = get_byte(f);
    c2 = get_byte(f);

    if (c1 == 0xfe && c2 == 0xff) {
        enc = L"utf-16be";
        f->f_read = read_utf16be;
    }
    else
    if (c1 != 0xff || c2 != 0xfe) {
        /* no BOM; rewind and hope */
        fseek(f->in, 0, SEEK_SET);
    }

    mpdm_set_wcs(mpdm_root(), MPDM_S(enc), L"DETECTED_ENCODING");

    return f->f_read(f, s, eol);
}


static int write_utf16le_bom(struct mpdm_file *f, const wchar_t *str)
{
    /* store the LE signature */
    put_byte(0xff, f);
    put_byte(0xfe, f);

    /* we're 16le from now on */
    f->f_write = write_utf16le;

    return f->f_write(f, str);
}


static int write_utf16be_bom(struct mpdm_file *f, const wchar_t *str)
{
    /* store the BE signature */
    put_byte(0xfe, f);
    put_byte(0xff, f);

    /* we're 16be from now on */
    f->f_write = write_utf16be;

    return f->f_write(f, str);
}


static wchar_t *read_utf32ae(struct mpdm_file *f, int *s, int *eol, int le)
/* utf32 reader, ANY ending */
{
    wchar_t *ptr = NULL;
    wchar_t wc;
    int c1, c2, c3, c4;

    for (;;) {
        wc = L'\0';

        BYTE_OR_BREAK(c1, f);
        BYTE_OR_BREAK(c2, f);
        BYTE_OR_BREAK(c3, f);
        BYTE_OR_BREAK(c4, f);

        if (le)
            wc = c1 | (c2 << 8) | (c3 << 16) | (c4 << 24);
        else
            wc = c4 | (c3 << 8) | (c2 << 16) | (c1 << 24);

        if (store_in_line(&ptr, s, eol, wc))
            break;
    }

    return ptr;
}


static int write_utf32ae(struct mpdm_file *f, const wchar_t *str, int le)
/* utf32 writer, ANY ending */
{
    int cnt = 0;
    wchar_t wc;

    /* convert char by char */
    for (; (wc = *str) != L'\0'; str++) {

        if (le) {
            put_byte((wc & 0x000000ff), f);
            put_byte((wc & 0x0000ff00) >> 8, f);
            put_byte((wc & 0x00ff0000) >> 16, f);
            put_byte((wc & 0xff000000) >> 24, f);
        }
        else {
            put_byte((wc & 0xff000000) >> 24, f);
            put_byte((wc & 0x00ff0000) >> 16, f);
            put_byte((wc & 0x0000ff00) >> 8, f);
            put_byte((wc & 0x000000ff), f);
        }
    }

    return cnt;
}


static wchar_t *read_utf32le(struct mpdm_file *f, int *s, int *eol)
{
    return read_utf32ae(f, s, eol, 1);
}


static int write_utf32le(struct mpdm_file *f, const wchar_t *str)
{
    return write_utf32ae(f, str, 1);
}


static wchar_t *read_utf32be(struct mpdm_file *f, int *s, int *eol)
{
    return read_utf32ae(f, s, eol, 0);
}


static int write_utf32be(struct mpdm_file *f, const wchar_t *str)
{
    return write_utf32ae(f, str, 0);
}


static wchar_t *read_utf32(struct mpdm_file *f, int *s, int *eol)
{
    int c1, c2, c3, c4;
    wchar_t *enc = L"utf-32le";

    f->f_read = read_utf32le;

    /* autodetection */
    c1 = get_byte(f);
    c2 = get_byte(f);
    c3 = get_byte(f);
    c4 = get_byte(f);

    if (c1 == 0 && c2 == 0 && c3 == 0xfe && c4 == 0xff) {
        enc = L"utf-32be";
        f->f_read = read_utf32be;
    }
    if (c1 != 0xff || c2 != 0xfe || c3 != 0 || c4 != 0) {
        /* no BOM; assume le and hope */
        fseek(f->in, 0, SEEK_SET);
    }

    mpdm_set_wcs(mpdm_root(), MPDM_S(enc), L"DETECTED_ENCODING");

    return f->f_read(f, s, eol);
}


static int write_utf32le_bom(struct mpdm_file *f, const wchar_t *str)
{
    /* store the LE signature */
    put_byte(0xff, f);
    put_byte(0xfe, f);
    put_byte(0, f);
    put_byte(0, f);

    /* we're 32le from now on */
    f->f_write = write_utf32le;

    return f->f_write(f, str);
}


static int write_utf32be_bom(struct mpdm_file *f, const wchar_t *str)
{
    /* store the BE signature */
    put_byte(0, f);
    put_byte(0, f);
    put_byte(0xfe, f);
    put_byte(0xff, f);

    /* we're 32be from now on */
    f->f_write = write_utf32be;

    return f->f_write(f, str);
}


static wchar_t *msdos_437 = L"\x2302"
    "\x00C7\x00FC\x00E9\x00E2\x00E4\x00E0\x00E5\x00E7"
    "\x00EA\x00EB\x00E8\x00EF\x00EE\x00EC\x00C4\x00C5"
    "\x00C9\x00E6\x00C6\x00F4\x00F6\x00F2\x00FB\x00F9"
    "\x00FF\x00D6\x00DC\x00A2\x00A3\x00A5\x20A7\x0192"
    "\x00E1\x00ED\x00F3\x00FA\x00F1\x00D1\x00AA\x00BA"
    "\x00BF\x2310\x00AC\x00BD\x00BC\x00A1\x00AB\x00BB"
    "\x2591\x2592\x2593\x2502\x2524\x2561\x2562\x2556"
    "\x2555\x2563\x2551\x2557\x255D\x255C\x255B\x2510"
    "\x2514\x2534\x252C\x251C\x2500\x253C\x255E\x255F"
    "\x255A\x2554\x2569\x2566\x2560\x2550\x256C\x2567"
    "\x2568\x2564\x2565\x2559\x2558\x2552\x2553\x256B"
    "\x256A\x2518\x250C\x2588\x2584\x258C\x2590\x2580"
    "\x03B1\x03B2\x0393\x03C0\x03A3\x03C3\x00B5\x03C4"
    "\x03A6\x0398\x03A9\x03B4\x221E\x2205\x2208\x2229"
    "\x2261\x00B1\x2265\x2264\x2320\x2321\x00F7\x2248"
    "\x00B0\x2219\x00B7\x221A\x207F\x00B2\x25A0\x00A0";

static wchar_t *msdos_850 = L"\x2302"
    "\x00C7\x00FC\x00E9\x00E2\x00E4\x00E0\x00E5\x00E7"
    "\x00EA\x00EB\x00E8\x00EF\x00EE\x00EC\x00C4\x00C5"
    "\x00C9\x00E6\x00C6\x00F4\x00F6\x00F2\x00FB\x00F9"
    "\x00FF\x00D6\x00DC\x00F8\x00A3\x00D8\x00D7\x0192"
    "\x00E1\x00ED\x00F3\x00FA\x00F1\x00D1\x00AA\x00BA"
    "\x00BF\x00AE\x00AC\x00BD\x00BC\x00A1\x00AB\x00BB"
    "\x2591\x2592\x2593\x2502\x2524\x00C1\x00C2\x00C0"
    "\x00A9\x2563\x2551\x2557\x255D\x00A2\x00A5\x2510"
    "\x2514\x2534\x252C\x251C\x2500\x253C\x00E3\x00C3"
    "\x255A\x2554\x2569\x2566\x2560\x2550\x256C\x00A4"
    "\x00F0\x00D0\x00CA\x00CB\x00C8\x0131\x00CD\x00CE"
    "\x00CF\x2518\x250C\x2588\x2584\x00A6\x00CC\x2580"
    "\x00D3\x00DF\x00D4\x00D2\x00F5\x00D5\x00B5\x00FE"
    "\x00DE\x00DA\x00DB\x00D9\x00FD\x00DD\x00AF\x00B4"
    "\x00AD\x00B1\x2017\x00BE\x00B6\x00A7\x00F7\x00B8"
    "\x00B0\x00A8\x00B7\x00B9\x00B3\x00B2\x25A0\x00A0";

static wchar_t *windows_1252 = L"\x2302"
    "\x20AC\x0081\x201A\x0192\x201E\x2026\x2020\x2021"
    "\x02C6\x2030\x0160\x2039\x0152\x008D\x017D\x008F"
    "\x0090\x2018\x2019\x201C\x201D\x2022\x2013\x2014"
    "\x02DC\x2122\x0161\x203A\x0153\x009D\x017E\x0178"
    "\x00A0\x00A1\x00A2\x00A3\x00A4\x00A5\x00A6\x00A7"
    "\x00A8\x00A9\x00AA\x00AB\x00AC\x00AD\x00AE\x00AF"
    "\x00B0\x00B1\x00B2\x00B3\x00B4\x00B5\x00B6\x00B7"
    "\x00B8\x00B9\x00BA\x00BB\x00BC\x00BD\x00BE\x00BF"
    "\x00C0\x00C1\x00C2\x00C3\x00C4\x00C5\x00C6\x00C7"
    "\x00C8\x00C9\x00CA\x00CB\x00CC\x00CD\x00CE\x00CF"
    "\x00D0\x00D1\x00D2\x00D3\x00D4\x00D5\x00D6\x00D7"
    "\x00D8\x00D9\x00DA\x00DB\x00DC\x00DD\x00DE\x00DF"
    "\x00E0\x00E1\x00E2\x00E3\x00E4\x00E5\x00E6\x00E7"
    "\x00E8\x00E9\x00EA\x00EB\x00EC\x00ED\x00EE\x00EF"
    "\x00F0\x00F1\x00F2\x00F3\x00F4\x00F5\x00F6\x00F7"
    "\x00F8\x00F9\x00FA\x00FB\x00FC\x00FD\x00FE\x00FF";

static wchar_t *read_msdos(struct mpdm_file *f, int *s, int *eol, wchar_t *cp)
/* generic MSDOS reader */
{
    wchar_t *ptr = NULL;
    wchar_t wc;
    int c;

    while ((c = get_byte(f)) != EOF) {

        if (c > 127)
            wc = cp[c - 127];
        else
            wc = c;

        if (store_in_line(&ptr, s, eol, wc))
            break;
    }

    return ptr;
}


static int write_msdos(struct mpdm_file *f, const wchar_t *str, wchar_t *cp)
/* generic MSDOS writer */
{
    int cnt = 0;
    wchar_t wc;

    /* convert char by char */
    for (; (wc = *str) != L'\0'; str++) {
        int c = wc;

        if (c > 0xff)
            c = '?';
        else
        if (c >= 127) {
            int n;

            c = '?';
            for (n = 0; c == '?' && cp[n]; n++) {
                if (wc == cp[n])
                    c = 127 + n;
            }
        }

        put_byte(c, f);
    }

    return cnt;
}


static wchar_t *read_msdos_437(struct mpdm_file *f, int *s, int *eol)
{
    return read_msdos(f, s, eol, msdos_437);
}


static int write_msdos_437(struct mpdm_file *f, const wchar_t *str)
{
    return write_msdos(f, str, msdos_437);
}


static wchar_t *read_msdos_850(struct mpdm_file *f, int *s, int *eol)
{
    return read_msdos(f, s, eol, msdos_850);
}


static int write_msdos_850(struct mpdm_file *f, const wchar_t *str)
{
    return write_msdos(f, str, msdos_850);
}


static wchar_t *read_windows_1252(struct mpdm_file *f, int *s, int *eol)
{
    return read_msdos(f, s, eol, windows_1252);
}


static int write_windows_1252(struct mpdm_file *f, const wchar_t *str)
{
    return write_msdos(f, str, windows_1252);
}


static wchar_t *read_auto(struct mpdm_file *f, int *s, int *eol)
/* autodetects different encodings based on the BOM */
{
    wchar_t *enc = L"";

    /* by default, multibyte reading */
    f->f_read = read_mbs;

    /* ensure seeking is possible */
    if (f->in != NULL && fseek(f->in, 0, SEEK_CUR) != -1) {
        int c;

        c = get_byte(f);

        if (c == 0xff) {
            /* can be utf32le or utf16le */
            if (get_byte(f) == 0xfe) {
                /* if next 2 chars are 0x00, it's utf32; otherwise utf16 */
                if (get_byte(f) == 0x00 && get_byte(f) == 0x00) {
                    enc = L"utf-32le";
                    f->f_read = read_utf32le;
                    goto got_encoding;
                }
                else {
                    /* rewind to 3rd character */
                    fseek(f->in, 2, SEEK_SET);

                    enc = L"utf-16le";
                    f->f_read = read_utf16le;
                    goto got_encoding;
                }
            }
        }
        else
        if (c == 0x00) {
            /* can be utf32be */
            if (get_byte(f) == 0x00 && get_byte(f) == 0xfe
                && get_byte(f) == 0xff) {
                enc = L"utf-32be";
                f->f_read = read_utf32be;
                goto got_encoding;
            }
        }
        else
        if (c == 0xfe) {
            /* can be utf16be */
            if (get_byte(f) == 0xff) {
                enc = L"utf-16be";
                f->f_read = read_utf16be;
                goto got_encoding;
            }
        }
        else
        if (c == 0xef) {
            /* can be utf8 with BOM */
            if (get_byte(f) == 0xbb && get_byte(f) == 0xbf) {
                enc = L"utf-8bom";
                f->f_read = read_utf8;
                goto got_encoding;
            }
        }
        else {
            /* try if a first bunch of chars are valid UTF-8 */
            int p = c;
            int n = 10000;
            int u = 0;

            while (--n && (c = get_byte(f)) != EOF) {
                if ((c & 0xc0) == 0x80) {
                    if ((p & 0xc0) == 0xc0)
                        u++;
                    else
                    if ((p & 0x80) == 0x00) {
                        u = -1;
                        break;
                    }
                }
                else
                if ((p & 0xc0) == 0xc0) {
                    u = -1;
                    break;
                }

                p = c;
            }

            if (u < 0) {
                /* invalid utf-8; fall back to 8bit */
                enc = L"8bit";
                f->f_read = read_iso8859_1;
            }
            else
            if (u > 0) {
                /* utf-8 sequences found */
                enc = L"utf-8";
                f->f_read = read_utf8;
            }

            /* 7 bit ASCII: do nothing */
        }

        /* none of the above; restart */
        fseek(f->in, 0, SEEK_SET);
    }

got_encoding:
    mpdm_set_wcs(mpdm_root(), MPDM_S(enc), L"DETECTED_ENCODING");

    return f->f_read(f, s, eol);
}


/** interface **/

wchar_t *mpdm_read_mbs(FILE *f, int *s)
/* reads a multibyte string from a stream into a dynamic string */
{
    struct mpdm_file fs;

    /* reset the structure */
    memset(&fs, '\0', sizeof(fs));
    fs.in = f;

    *s = 0;
    return read_mbs(&fs, s, NULL);
}


int mpdm_write_wcs(FILE *f, const wchar_t *str)
/* writes a wide string to a stream */
{
    struct mpdm_file fs;

    /* reset the structure */
    memset(&fs, '\0', sizeof(fs));
    fs.out = f;

    return write_wcs(&fs, str);
}


/**
 * mpdm_open - Opens a file.
 * @filename: the file name
 * @mode: an fopen-like mode string
 *
 * Opens a file. If @filename can be open in the specified @mode, an
 * mpdm_t value will be returned containing the file descriptor, or NULL
 * otherwise. If @mode is NULL, "r" is assumed.
 *
 * If the file is open for reading, some charset detection methods are
 * used. If any of them is successful, its name is stored in the
 * DETECTED_ENCODING element of the mpdm_root() hash. This value is
 * suitable to be copied over ENCODING or TEMP_ENCODING.
 *
 * If the file is open for writing, the encoding to be used is read from
 * the ENCODING element of mpdm_root() and, if not set, from the
 * TEMP_ENCODING one. The latter will always be deleted afterwards.
 * [File Management]
 */
mpdm_t mpdm_open(mpdm_t filename, mpdm_t mode)
{
    FILE *f = NULL;
    mpdm_t fn;
    mpdm_t fm;

    /* extreme lazyness */
    if (mode == NULL)
        mode = MPDM_S(L"r");

    mpdm_ref(filename);
    mpdm_ref(mode);

    if (filename != NULL && mode != NULL) {
        /* convert to mbs,s */
        fn = mpdm_ref(MPDM_2MBS(mpdm_data(filename)));
        fm = mpdm_ref(MPDM_2MBS(mpdm_data(mode)));

        if ((f = fopen((char *) mpdm_data(fn), (char *) mpdm_data(fm))) == NULL)
            store_syserr();
        else {
#if defined(CONFOPT_SYS_STAT_H) && defined(S_ISDIR) && defined(EISDIR)
            struct stat s;

            /* test if the open file is a directory */
            if (fstat(fileno(f), &s) != -1 && S_ISDIR(s.st_mode)) {
                /* it's a directory; fail */
                errno = EISDIR;
                store_syserr();
                fclose(f);
                f = NULL;
            }
#endif
        }

        mpdm_unref(fm);
        mpdm_unref(fn);
    }

    mpdm_unref(mode);
    mpdm_unref(filename);

    return f ? MPDM_F(f) : NULL;
}


/**
 * mpdm_read - Reads a line from a file descriptor.
 * @fd: the value containing the file descriptor
 *
 * Reads a line from @fd. Returns the line, or NULL on EOF.
 * [File Management]
 * [Character Set Conversion]
 */
mpdm_t mpdm_read(const mpdm_t fd)
{
    mpdm_t v = NULL;

    if (mpdm_type(fd) == MPDM_TYPE_FILE) {
        wchar_t *ptr;
        int s = 0;
        int eol = -1;
        struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);

        ptr = fs->f_read(fs, &s, &eol);

        if (ptr != NULL) {
            /* something read; does it have an eol? */
            if (eol != -1) {
                /* store */
                wcsncpy(fs->eol, &ptr[eol], MAX_EOL);

                /* if auto_chomp is set, delete the eol */
                if (fs->auto_chomp) {
                    s -= wcslen(fs->eol);
                    ptr[s] = L'\0';
                }
            }
            else
                fs->eol[0] = L'\0';

            /* return the line */
            v = MPDM_ENS(ptr, s);
        }
        else {
            /* nothing read; if last read had an eol,
               return an empty string as the last read */
            if (fs->eol[0]) {
                v = MPDM_S(L"");
                fs->eol[0] = L'\0';
            }
        }
    }

    return v;
}


wchar_t *mpdm_eol(mpdm_t fd)
{
    wchar_t *r = NULL;

    if (mpdm_type(fd) == MPDM_TYPE_FILE) {
        struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);

        r = fs->eol;
    }

    return r;
}


mpdm_t mpdm_getchar(const mpdm_t fd)
{
    mpdm_t r = NULL;

    if (mpdm_type(fd) == MPDM_TYPE_FILE) {
        int c;
        wchar_t tmp[2];
        struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);

        if ((c = get_byte(fs)) != EOF) {
            /* get the char as-is */
            tmp[0] = (wchar_t) c;
            tmp[1] = L'\0';

            r = MPDM_S(tmp);
        }
    }

    return r;
}


int mpdm_putchar(const mpdm_t fd, const mpdm_t c)
{
    int r = 1;

    mpdm_ref(c);

    if (mpdm_type(fd) == MPDM_TYPE_FILE) {
        struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);
        const wchar_t *ptr = mpdm_string(c);

        if (put_byte(*ptr, fs) == -1)
            r = 0;
    }

    mpdm_unref(c);

    return r;
}


/**
 * mpdm_write - Writes a value into a file.
 * @fd: the file descriptor.
 * @v: the value to be written.
 *
 * Writes the @v value into @fd, using the current encoding. If @v is
 * an array or file, it's iterated and its elements written into @fd.
 * [File Management]
 * [Character Set Conversion]
 */
int mpdm_write(const mpdm_t fd, const mpdm_t v)
{
    int ret = -1;

    mpdm_ref(v);

    if (mpdm_type(fd) == MPDM_TYPE_FILE) {
        if (mpdm_type(v) == MPDM_TYPE_ARRAY || mpdm_type(v) == MPDM_TYPE_FILE) {
            int64_t n = 0;
            mpdm_t w;

            while (mpdm_iterator(v, &n, &w, NULL))
                ret = mpdm_write(fd, w);
        }
        else {
            struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);
            ret = fs->f_write(fs, mpdm_string(v));
        }
    }

    mpdm_unref(v);

    return ret;
}


int mpdm_fseek(const mpdm_t fd, long offset, int whence)
{
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);

    return fseek(fs->in, offset, whence);
}


long mpdm_ftell(const mpdm_t fd)
{
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);

    return ftell(fs->in);
}


int mpdm_feof(const mpdm_t fd)
{
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);

    return feof(fs->in);
}


int mpdm_flock(mpdm_t fd, int operation)
{
    int ret = 0;

#ifndef WIN32

    int i = -1;
    FILE *f = mpdm_get_filehandle(fd);

    if (f != NULL)
        i = fileno(f);

    ret = flock(i, operation);

#endif /* WIN32 */

    return ret;
}


FILE *mpdm_get_filehandle(const mpdm_t fd)
{
    FILE *f = NULL;

    if (mpdm_type(fd) == MPDM_TYPE_FILE) {
        struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(fd);
        f = fs->in;
    }

    return f;
}


/*
mpdm_t mpdm_bread(mpdm_t fd, int size)
{
}


int mpdm_bwrite(mpdm_tfd, mpdm_t v, int size)
{
}
*/


static mpdm_t embedded_encodings(void)
{
    mpdm_t e;
    wchar_t *e2e[] = {
        L"utf-8",      L"utf-8",
        L"utf8",       NULL,
        L"iso8859-1",  L"iso8859-1",
        L"iso-8859-1", NULL,
        L"8bit",       NULL,
        L"latin1",     NULL,
        L"latin-1",    NULL,
        L"utf-16le",   L"utf-16le",
        L"utf16le",    NULL,
        L"ucs-2le",    NULL,
        L"utf-16be",   L"utf-16be",
        L"utf16be",    NULL,
        L"ucs-2be",    NULL,
        L"utf-16",     L"utf-16",
        L"utf16",      NULL,
        L"ucs-2",      NULL,
        L"ucs2",       NULL,
        L"utf-32le",   L"utf-32le",
        L"utf32le",    NULL,
        L"ucs-4le",    NULL,
        L"utf-32be",   L"utf-32be",
        L"utf32be",    NULL,
        L"ucs-4be",    NULL,
        L"utf-32",     L"utf-32",
        L"utf32",      NULL,
        L"ucs-4",      NULL,
        L"ucs4",       NULL,
        L"utf-8bom",   L"utf-8bom",
        L"utf8bom",    NULL,
        L"msdos-437",  L"msdos-437",
        L"437",        NULL,
        L"cp-437",     NULL,
        L"cp437",      NULL,
        L"msdos-850",  L"msdos-850",
        L"850",        NULL,
        L"cp-850",     NULL,
        L"cp850",      NULL,
        L"windows-1252", L"windows-1252",
        NULL,          NULL
    };

    if ((e = mpdm_get_wcs(mpdm_root(), L"EMBEDDED_ENCODINGS")) == NULL) {
        int n;
        mpdm_t p = NULL;

        e = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"EMBEDDED_ENCODINGS");

        for (n = 0; e2e[n] != NULL; n += 2) {
            mpdm_t v = MPDM_S(e2e[n]);

            if (e2e[n + 1] != NULL)
                p = MPDM_S(e2e[n + 1]);

            mpdm_set(e, p, v);
            mpdm_set(e, p, mpdm_ulc(v, 1));
        }

    }

    return e;
}


/**
 * mpdm_encoding - Sets the current charset encoding for files.
 * @charset: the charset name.
 *
 * Sets the current charset encoding for files. Future opened
 * files will be assumed to be encoded with @charset, which can
 * be any of the supported charset names (utf-8, iso-8859-1, etc.),
 * and converted on each read / write. If charset is NULL, it
 * is reverted to default charset conversion (i.e. the one defined
 * in the locale).
 *
 * This function stores the @charset value into the ENCODING item
 * of the mpdm_root() hash.
 *
 * Returns a negative number if @charset is unsupported, or zero
 * if no errors were found.
 * [File Management]
 * [Character Set Conversion]
 */
int mpdm_encoding(mpdm_t charset)
{
    int ret = -1;
    mpdm_t e = embedded_encodings();
    mpdm_t v = NULL;

    mpdm_ref(charset);

    /* NULL encoding? done */
    if (mpdm_size(charset) == 0) {
        mpdm_set_wcs(mpdm_root(), NULL, L"ENCODING");
        ret = 0;
    }

#ifdef CONFOPT_ICONV
    else {
        iconv_t ic;
        mpdm_t cs = mpdm_ref(MPDM_2MBS(mpdm_data(charset)));

        /* tries to create an iconv encoder and decoder for this charset */

        if ((ic = iconv_open("WCHAR_T", (char *) mpdm_data(cs))) == (iconv_t) - 1)
            ret = -1;
        else {
            iconv_close(ic);

            if ((ic = iconv_open((char *) mpdm_data(cs), "WCHAR_T")) == (iconv_t) - 1)
                ret = -2;
            else {
                iconv_close(ic);

                /* got a valid encoding */
                v = charset;
                ret = 0;
            }
        }

        mpdm_unref(cs);
    }
#endif                          /* CONFOPT_ICONV */

    if (ret != 0 && (v = mpdm_get(e, charset)) != NULL)
        ret = 0;

    if (ret == 0)
        mpdm_set_wcs(mpdm_root(), v, L"ENCODING");

    mpdm_unref(charset);

    return ret;
}


/**
 * mpdm_unlink - Deletes a file.
 * @filename: file name to be deleted
 *
 * Deletes a file.
 * [File Management]
 */
int mpdm_unlink(const mpdm_t filename)
{
    int ret;
    mpdm_t fn;

    mpdm_ref(filename);

    /* convert to mbs */
    fn = mpdm_ref(MPDM_2MBS(mpdm_data(filename)));

    if ((ret = unlink((char *) mpdm_data(fn))) == -1)
        store_syserr();

    mpdm_unref(fn);
    mpdm_unref(filename);

    return ret;
}


/**
 * mpdm_rename - Renames a file.
 * @o: old path
 * @n: new path
 *
 * Renames a file.
 * [File Management]
 */
int mpdm_rename(const mpdm_t o, const mpdm_t n)
{
    int ret;
    mpdm_t om, nm;

    mpdm_ref(o);
    mpdm_ref(n);

    om = mpdm_ref(MPDM_2MBS(mpdm_data(o)));
    nm = mpdm_ref(MPDM_2MBS(mpdm_data(n)));

    if ((ret = rename((char *)mpdm_data(om), (char *)mpdm_data(nm))) == -1)
        store_syserr();

    mpdm_unref(nm);
    mpdm_unref(om);
    mpdm_unref(n);
    mpdm_unref(o);

    return ret;
}


/**
 * mpdm_stat - Gives status from a file.
 * @filename: file name to get the status from
 *
 * Returns a 14 element array of the status (permissions, onwer, etc.)
 * from the desired @filename, or NULL if the file cannot be accessed.
 * (man 2 stat).
 *
 * The values are: 0, device number of filesystem; 1, inode number;
 * 2, file mode; 3, number of hard links to the file; 4, uid; 5, gid;
 * 6, device identifier; 7, total size of file in bytes; 8, atime;
 * 9, mtime; 10, ctime; 11, preferred block size for system I/O;
 * 12, number of blocks allocated and 13, canonicalized file name.
 * Not all elements have necesarily meaningful values, as most are
 * system-dependent.
 * [File Management]
 */
mpdm_t mpdm_stat(const mpdm_t filename)
{
    mpdm_t r = NULL;

    mpdm_ref(filename);

#ifdef CONFOPT_SYS_STAT_H
    struct stat s;
    mpdm_t fn;

    fn = mpdm_ref(MPDM_2MBS(mpdm_data(filename)));

    if (stat((char *) mpdm_data(fn), &s) != -1) {
        r = MPDM_A(14);

        mpdm_ref(r);

        mpdm_set_i(r, MPDM_I(s.st_dev), 0);
        mpdm_set_i(r, MPDM_I(s.st_ino), 1);
        mpdm_set_i(r, MPDM_I(s.st_mode), 2);
        mpdm_set_i(r, MPDM_I(s.st_nlink), 3);
        mpdm_set_i(r, MPDM_I(s.st_uid), 4);
        mpdm_set_i(r, MPDM_I(s.st_gid), 5);
        mpdm_set_i(r, MPDM_I(s.st_rdev), 6);
        mpdm_set_i(r, MPDM_I(s.st_size), 7);
        mpdm_set_i(r, MPDM_R((double)s.st_atime), 8);
        mpdm_set_i(r, MPDM_R((double)s.st_mtime), 9);
        mpdm_set_i(r, MPDM_R((double)s.st_ctime), 10);
        mpdm_set_i(r, MPDM_I(0), 11);    /* s.st_blksize */
        mpdm_set_i(r, MPDM_I(0), 12);    /* s.st_blocks */

#ifdef CONFOPT_CANONICALIZE_FILE_NAME

        {
            char *ptr;

            if ((ptr = canonicalize_file_name((char *) mpdm_data(fn))) != NULL) {
                mpdm_set_i(r, MPDM_MBS(ptr), 13);
                free(ptr);
            }
        }
#endif

#ifdef CONFOPT_REALPATH
        {
            char tmp[2048];

            if (realpath((char *) mpdm_data(fn), tmp) != NULL)
                mpdm_set_i(r, MPDM_MBS(tmp), 13);
        }
#endif

#ifdef CONFOPT_FULLPATH
        {
            char tmp[_MAX_PATH + 1];

            if (_fullpath(tmp, (char *) mpdm_data(fn), _MAX_PATH) != NULL)
                mpdm_set_i(r, MPDM_MBS(tmp), 13);
        }
#endif

        mpdm_unrefnd(r);
    }
    else
        store_syserr();

    mpdm_unref(fn);

#endif                          /* CONFOPT_SYS_STAT_H */

    mpdm_unref(filename);

    return r;
}


/**
 * mpdm_chmod - Changes a file's permissions.
 * @filename: the file name
 * @perms: permissions (element 2 from mpdm_stat())
 *
 * Changes the permissions for a file.
 * [File Management]
 */
int mpdm_chmod(const mpdm_t filename, mpdm_t perms)
{
    int r = -1;

    mpdm_ref(filename);
    mpdm_ref(perms);

    mpdm_t fn = mpdm_ref(MPDM_2MBS(mpdm_data(filename)));

    if ((r = chmod((char *) mpdm_data(fn), mpdm_ival(perms))) == -1)
        store_syserr();

    mpdm_unref(fn);
    mpdm_unref(perms);
    mpdm_unref(filename);

    return r;
}


/**
 * mpdm_chdir - Changes the working directory
 * @dir: the new path
 *
 * Changes the working directory
 * [File Management]
 */
int mpdm_chdir(const mpdm_t dir)
{
    int r = -1;

    mpdm_ref(dir);
    mpdm_t fn = mpdm_ref(MPDM_2MBS(mpdm_data(dir)));

    if ((r = chdir((char *) mpdm_data(fn))) == -1)
        store_syserr();

    mpdm_unref(fn);
    mpdm_unref(dir);

    return r;
}


/**
 * mpdm_mkdir - Creates a directory
 * @dir: the new path
 * @mode: permissions
 *
 * Creates a directory.
 * [File Management]
 */
int mpdm_mkdir(const mpdm_t dir, const mpdm_t mode)
{
    int r = -1;
    mode_t m;

    mpdm_ref(dir);
    mpdm_t fn = mpdm_ref(MPDM_2MBS(mpdm_data(dir)));

    m = (mode_t) mpdm_ival(mode);

    /* default permissions */
    if (m == 0)
        m = 0755;

#ifdef CONFOPT_MKDIR_MODE
    if ((r = mkdir((char *) mpdm_data(fn), m)) == -1)
#else
    if ((r = mkdir((char *) mpdm_data(fn))) == -1)
#endif
        store_syserr();

    mpdm_unref(fn);
    mpdm_unref(dir);

    return r;
}


/**
 * mpdm_getcwd - Get current working directory
 *
 * Returns the current working directory.
 * [File Management]
 */
mpdm_t mpdm_getcwd(void)
{
    char tmp[4096];

    getcwd(tmp, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    return MPDM_MBS(tmp);
}


/**
 * mpdm_chown - Changes a file's owner.
 * @filename: the file name
 * @uid: user id (element 4 from mpdm_stat())
 * @gid: group id (element 5 from mpdm_stat())
 *
 * Changes the owner and group id's for a file.
 * [File Management]
 */
int mpdm_chown(const mpdm_t filename, mpdm_t uid, mpdm_t gid)
{
    int r = -1;

    mpdm_ref(filename);
    mpdm_ref(uid);
    mpdm_ref(gid);

#ifdef CONFOPT_CHOWN

    mpdm_t fn = mpdm_ref(MPDM_2MBS(mpdm_data(filename)));

    if ((r = chown((char *) mpdm_data(fn), mpdm_ival(uid), mpdm_ival(gid))) == -1)
        store_syserr();

    mpdm_unref(fn);

#endif /* CONFOPT_CHOWN */

    mpdm_unref(gid);
    mpdm_unref(uid);
    mpdm_unref(filename);

    return r;
}


/**
 * mpdm_glob - Executes a file globbing.
 * @spec: Globbing spec
 * @base: Optional base directory
 *
 * Executes a file globbing. @spec is system-dependent, but usually
 * the * and ? metacharacters work everywhere. @base can contain a
 * directory; if that's the case, the output strings will include it.
 * In any case, each returned value will be suitable for a call to
 * mpdm_open().
 *
 * Returns an array of files that match the globbing (can be an empty
 * array if no file matches), or NULL if globbing is unsupported.
 * [File Management]
 */
mpdm_t mpdm_glob(mpdm_t spec, mpdm_t base)
{
    mpdm_t d, f;
    char *ptr;

#ifdef CONFOPT_WIN32
    WIN32_FIND_DATA fd;
    HANDLE h;
    const wchar_t *def_spec = L"*.*";
#endif

#if CONFOPT_GLOB_H
    glob_t globbuf;
    const wchar_t *def_spec = L"*";
#endif

    /* build full path */
    if (base != NULL) {
        base = mpdm_strcat_wcs(base, L"/");

        /* escape expandable chars */
        base = mpdm_sregex(base, MPDM_S(L"@[]\\[]@g"), MPDM_S(L"\\\\&"), 0);
    }

    if (spec == NULL)
        spec = mpdm_strcat_wcs(base, def_spec);
    else
        spec = mpdm_strcat(base, spec);

    /* delete repeated directory delimiters */
    spec = mpdm_sregex(spec, MPDM_S(L"@[\\/]{2,}@g"), MPDM_S(L"/"), 0);

    mpdm_ref(spec);
    ptr = mpdm_wcstombs(mpdm_string(spec), NULL);
    mpdm_unref(spec);

    d = MPDM_A(0);
    f = MPDM_A(0);

#ifdef CONFOPT_WIN32
    if ((h = FindFirstFile(ptr, &fd)) != INVALID_HANDLE_VALUE) {
        mpdm_t p;
        char *b;

        /* if spec includes a directory, store in s */
        if ((b = strrchr(ptr, '/')) != NULL) {
            *(b + 1) = '\0';
            p = MPDM_MBS(ptr);
        }
        else
            p = NULL;

        mpdm_ref(p);

        do {
            mpdm_t t;

            /* ignore . and .. */
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;

            /* concat base directory and file names */
            t = mpdm_strcat(p, MPDM_MBS(fd.cFileName));

            /* if it's a directory, add a / */
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                mpdm_push(d, mpdm_strcat_wcs(t, L"/"));
            else
                mpdm_push(f, t);
        }
        while (FindNextFile(h, &fd));

        FindClose(h);

        mpdm_unref(p);
    }
#endif

#if CONFOPT_GLOB_H
    globbuf.gl_offs = 1;

    if (glob(ptr, GLOB_MARK, NULL, &globbuf) == 0) {
        int n;

        for (n = 0; globbuf.gl_pathv[n] != NULL; n++) {
            char *p = globbuf.gl_pathv[n];
            mpdm_t t = MPDM_MBS(p);

            /* if last char is /, add to directories */
            if (p[strlen(p) - 1] == '/')
                mpdm_push(d, t);
            else
                mpdm_push(f, t);
        }
    }

    globfree(&globbuf);
#endif

    free(ptr);

    mpdm_sort(d, 1);
    mpdm_sort(f, 1);

    return mpdm_join(d, f);
}


/** pipes **/


#ifdef CONFOPT_WIN32

static void win32_pipe(HANDLE *h, int n)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE cp, t;

    memset(&sa, '\0', sizeof(sa));
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = NULL;

    cp = GetCurrentProcess();

    CreatePipe(&h[0], &h[1], &sa, 0);
    DuplicateHandle(cp, h[n], cp, &t, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(h[n]);
    h[n] = t;
}


static int sysdep_popen(mpdm_t v, char *prg, int rw)
/* win32-style pipe */
{
    HANDLE pr[2];
    HANDLE pw[2];
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    int ret;
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(v);

    fs->is_pipe = 1;

    /* init all */
    pr[0] = pr[1] = pw[0] = pw[1] = NULL;

    if (rw & 0x01)
        win32_pipe(pr, 0);
    if (rw & 0x02)
        win32_pipe(pw, 1);

    /* spawn new process */
    memset(&pi, '\0', sizeof(pi));
    memset(&si, '\0', sizeof(si));

    si.cb = sizeof(STARTUPINFO);
    si.hStdError = pr[1];
    si.hStdOutput = pr[1];
    si.hStdInput = pw[0];
    si.dwFlags |= STARTF_USESTDHANDLES;

    ret = CreateProcess(NULL, prg, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    if (rw & 0x01)
        CloseHandle(pr[1]);
    if (rw & 0x02)
        CloseHandle(pw[0]);

    fs->hin     = pr[0];
    fs->hout    = pw[1];
    fs->process = pi.hProcess;

    return ret;
}


static int sysdep_pclose(const mpdm_t v)
{
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(v);
    DWORD out = 0;

    if (fs->hin != NULL)
        CloseHandle(fs->hin);

    if (fs->hout != fs->hin && fs->hout != NULL)
        CloseHandle(fs->hout);

    fs->hin = NULL;
    fs->hout = NULL;

    if (!fs->skip_wait) {
        /* waits until the process terminates */
        WaitForSingleObject(fs->process, 1000);
        GetExitCodeProcess(fs->process, &out);
    } else fs->skip_wait = 0;
    return (int) out;
}


#else /* CONFOPT_WIN32 */

extern char **environ;

static int sysdep_popen(mpdm_t v, char *prg, int rw)
/* unix-style pipe open */
{
    int pr[2], pw[2];
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(v);

    fs->is_pipe = 1;

    /* init all */
    pr[0] = pr[1] = pw[0] = pw[1] = -1;

    if (rw & 0x01)
        pipe(pr);
    if (rw & 0x02)
        pipe(pw);

    if (fork() == 0) {
        /* child process */
        mpdm_t v;
        int n;

        setsid();

        if (rw & 0x01) {
            close(1);
            dup(pr[1]);
            close(pr[0]);
        }
        if (rw & 0x02) {
            close(0);
            dup(pw[0]);
            close(pw[1]);
        }

        /* redirect stderr to stdout */
        close(2);
        dup(1);

        /* build the environment for the subprocess */
        v = mpdm_join(mpdm_get_wcs(mpdm_root(), L"ENV"), MPDM_S(L"="));
        environ = (char **) calloc(sizeof(char *), mpdm_size(v) + 1);

        mpdm_ref(v);
        for (n = 0; n < mpdm_size(v); n++) {
            mpdm_t w = MPDM_2MBS(mpdm_string(mpdm_get_i(v, n)));
            environ[n] = (char *)mpdm_data(w);
        }
        mpdm_unref(v);

        /* run the program */
        execlp("/bin/sh", "/bin/sh", "-c", prg, NULL);
        execlp(prg, prg, NULL);

        /* still here? exec failed; close pipes and exit */
        close(0);
        close(1);
        exit(0);
    }

    /* create the pipes as non-buffered streams */
    if (rw & 0x01) {
        fs->in = fdopen(pr[0], "r");
        setvbuf(fs->in, NULL, _IONBF, 0);
        close(pr[1]);
    }

    if (rw & 0x02) {
        fs->out = fdopen(pw[1], "w");
        setvbuf(fs->out, NULL, _IONBF, 0);
        close(pw[0]);
    }

    return 1;
}


static int sysdep_pclose(const mpdm_t v)
/* unix-style pipe close */
{
    int s = 0;
    struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(v);

    if (!fs->skip_wait)
        wait(&s);
    else fs->skip_wait = 0;

    return s;
}


#endif /* CONFOPT_WIN32 */


/**
 * mpdm_popen - Opens a pipe.
 * @prg: the program to pipe
 * @mode: an fopen-like mode string
 *
 * Opens a pipe to a program. If @prg can be open in the specified @mode, an
 * mpdm_t value will be returned containing the file descriptor, or NULL
 * otherwise.
 * [File Management]
 */
mpdm_t mpdm_popen(const mpdm_t prg, const mpdm_t mode)
{
    mpdm_t v = NULL;

    mpdm_ref(prg);
    mpdm_ref(mode);

    if (prg != NULL && mode != NULL) {
        mpdm_t pr, md;
        char *m;
        int rw = 0;

        v = MPDM_F(NULL);

        /* convert to mbs,s */
        pr = mpdm_ref(MPDM_2MBS(mpdm_data(prg)));
        md = mpdm_ref(MPDM_2MBS(mpdm_data(mode)));

        /* get the mode */
        m = (char *) mpdm_data(md);

        /* set the mode */
        if (m[0] == 'r')
            rw = 0x01;
        if (m[0] == 'w')
            rw = 0x02;
        if (m[1] == '+')
            rw = 0x03;          /* r+ or w+ */

        if (!sysdep_popen(v, (char *) mpdm_data(pr), rw)) {
            mpdm_void(v);
            v = NULL;
        } else {
            /* need to wait on first pclose call */
            struct mpdm_file *fs = (struct mpdm_file *) mpdm_data(v);
            fs->skip_wait = 0;
        }

        mpdm_unref(md);
        mpdm_unref(pr);
    }

    mpdm_unref(mode);
    mpdm_unref(prg);

    return v;
}


/**
 * mpdm_popen2 - Opens a pipe and returns 2 descriptors.
 * @prg: the program to pipe
 *
 * Opens a read-write pipe and returns an array of two descriptors,
 * one for reading and one for writing. If @prg could not be piped to,
 * returns NULL.
 * [File Management]
 */
mpdm_t mpdm_popen2(const mpdm_t prg)
{
    mpdm_t i, o;
    mpdm_t p = NULL;

    if ((i = mpdm_popen(prg, MPDM_S(L"r+"))) != NULL) {
        struct mpdm_file *ifs;
        struct mpdm_file *ofs;

        o = MPDM_C(i->type, (void *)mpdm_data(i), i->size);

        ifs = (struct mpdm_file *)mpdm_data(i);
        ofs = (struct mpdm_file *)mpdm_data(o);

        ofs->in = ifs->out;
        ifs->out = NULL;
	/* make sure that when closing write pipe, it does not wait for the child's end */ 
	ofs->skip_wait = 1;

#ifdef CONFOPT_WIN32
        ofs->hin = ifs->hout;
        ifs->hout = NULL;
#endif

        p = mpdm_ref(MPDM_A(2));
        mpdm_set_i(p, i, 0);
        mpdm_set_i(p, o, 1);
        mpdm_unrefnd(p);
    }

    return p;
}


/**
 * mpdm_pclose - Closes a pipe.
 * @fd: the value containing the file descriptor
 *
 * Closes a pipe.
 * [File Management]
 */
int mpdm_pclose(mpdm_t fd)
{
    return mpdm_close(fd);
}


/**
 * mpdm_conf_dir - Returns a directory where configuration can be stored.
 *
 * Returns a system-dependent directory where the user can write
 * configuration files and create subdirectories.
 * [File Management]
 */
mpdm_t mpdm_conf_dir(void)
{
    mpdm_t r = NULL;
    char *ptr = "";
    char tmp[512] = "";

    memset(tmp, '\0', sizeof(tmp));

#ifdef CONFOPT_WIN32

    LPITEMIDLIST pidl;

    /* get the 'My Documents' folder */
    SHGetSpecialFolderLocation(NULL, CSIDL_LOCAL_APPDATA, &pidl);
    SHGetPathFromIDList(pidl, tmp);
    strcat(tmp, "\\");

#endif

    /* XDG_CONFIG_HOME defined? */
    if (tmp[0] == '\0' && (ptr = getenv("XDG_CONFIG_HOME")) != NULL) {
        strncpy(tmp, ptr, sizeof(tmp) - 1);
        strcat(tmp, "/");
    }

#ifdef CONFOPT_PWD_H

    struct passwd *p;

    /* get home dir from /etc/passwd entry */
    if (tmp[0] == '\0' && (p = getpwuid(getuid())) != NULL) {
        strncpy(tmp, p->pw_dir, sizeof(tmp) - 1);
        strcat(tmp, "/.config/");
    }

#endif

    /* still none? try the ENV variable $HOME */
    if (tmp[0] == '\0' && (ptr = getenv("HOME")) != NULL) {
        strncpy(tmp, ptr, sizeof(tmp) - 1);
        strcat(tmp, "/.config/");
    }

    if (tmp[0] != '\0')
        r = MPDM_MBS(tmp);

    return r;
}


/**
 * mpdm_home_dir - Returns the home user directory.
 *
 * Returns a system-dependent directory where the user can write
 * documents and create subdirectories.
 * [File Management]
 */
mpdm_t mpdm_home_dir(void)
{
    mpdm_t r = NULL;
    char *ptr = "";
    wchar_t *wptr = L"";
    char tmp[512] = "";

    memset(tmp, '\0', sizeof(tmp));

#ifdef CONFOPT_WIN32

    LPITEMIDLIST pidl;

    /* get the 'My Documents' folder */
    SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl);
    SHGetPathFromIDList(pidl, tmp);
    wptr = L"\\";

#endif

#ifdef CONFOPT_PWD_H

    struct passwd *p;

    /* get home dir from /etc/passwd entry */
    if (tmp[0] == '\0' && (p = getpwuid(getuid())) != NULL) {
        strncpy(tmp, p->pw_dir, sizeof(tmp) - 1);
        wptr = L"/";
    }

#endif

    /* still none? try the ENV variable $HOME */
    if (tmp[0] == '\0' && (ptr = getenv("HOME")) != NULL) {
        strncpy(tmp, ptr, sizeof(tmp) - 1);
        wptr = L"/";
    }

    if (tmp[0] != '\0')
        r = mpdm_strcat_wcs(MPDM_MBS(tmp), wptr);

    return r;
}


/**
 * mpdm_app_dir - Returns the applications directory.
 *
 * Returns a system-dependent directory where the applications store
 * their private data, as components or resources.
 *
 * If the global APPID MPDM variable is set, it's used to search for
 * the specific application installation folder (on MS Windows' registry)
 * and / or appended as the final folder.
 * [File Management]
 */
mpdm_t mpdm_app_dir(void)
{
    mpdm_t r = NULL;
    mpdm_t appid = mpdm_get_wcs(mpdm_root(), L"APPID");
    int aok = 0;

#ifdef CONFOPT_WIN32

    HKEY hkey;
    char tmp[MAX_PATH];
    LPITEMIDLIST pidl;

    tmp[0] = '\0';

    if (appid != NULL) {
        /* find the installation folder in the registry */
        char *ptr = mpdm_wcstombs(mpdm_string(appid), NULL);

        sprintf(tmp, "SOFTWARE\\%s\\appdir", ptr);

        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, tmp, 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
            int n = sizeof(tmp);

            if (RegQueryValueEx(hkey, NULL, NULL, NULL,
                (LPBYTE) tmp, (LPDWORD) &n) == ERROR_SUCCESS)
                    aok = 1;
                else
                    tmp[0] = '\0';
        }
        else
            tmp[0] = '\0';

        free(ptr);
    }

    if (tmp[0] == '\0') {
        /* get the 'Program Files' folder (can fail) */
        if (SHGetSpecialFolderLocation(NULL, CSIDL_PROGRAM_FILES, &pidl) == S_OK)
            SHGetPathFromIDList(pidl, tmp);

        /* if it's still empty, get from the registry */
        if (tmp[0] == '\0' && RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
                     0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
            int n = sizeof(tmp);

            if (RegQueryValueEx(hkey, "ProgramFilesDir",
                                NULL, NULL, (LPBYTE) tmp,
                                (LPDWORD) & n) != ERROR_SUCCESS)
                tmp[0] = '\0';
        }
    }

    if (tmp[0] != '\0') {
        r = mpdm_strcat_wcs(MPDM_MBS(tmp), L"\\");
    }
#endif

    /* still none? get the configured directory */
    if (r == NULL)
        r = MPDM_MBS(CONFOPT_PREFIX "/share/");

    if (appid && !aok)
        r = mpdm_strcat(r, appid);

    return r;
}


/** sockets **/

void init_sockets(void)
{
    static int init = 0;

    if (init == 0) {
        init = 1;

#ifdef CONFOPT_WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
}


mpdm_t mpdm_connect(mpdm_t host, mpdm_t serv)
/* opens a client socket to host:serv */
{
    mpdm_t f = NULL;
    char *h;
    char *s;
    int d = -1;

    mpdm_ref(host);
    mpdm_ref(serv);

    h = mpdm_wcstombs(mpdm_string(host), NULL);
    s = mpdm_wcstombs(mpdm_string(serv), NULL);

    init_sockets();

    {

#ifndef CONFOPT_WITHOUT_GETADDRINFO

    struct addrinfo *res;
    struct addrinfo hints;

    memset(&hints, '\0', sizeof(hints));

    hints.ai_socktype   = SOCK_STREAM;
    hints.ai_flags      = AI_ADDRCONFIG;

    if (getaddrinfo(h, s, &hints, &res) == 0) {
        struct addrinfo *r;

        for (r = res; r != NULL; r = r->ai_next) {
            d = socket(r->ai_family, r->ai_socktype, r->ai_protocol);

            if (d != -1) {
                if (connect(d, r->ai_addr, r->ai_addrlen) == 0)
                    break;

                close(d);
                d = -1;
            }
        }

        freeaddrinfo(res);
    }

#else   /* CONFOPT_WITHOUT_GETADDRINFO */

    /* traditional socket interface */
    struct hostent *he;

    if ((he = gethostbyname(h)) != NULL) {
        struct servent *se;

        if ((se = getservbyname(s, "tcp")) != NULL) {
            struct sockaddr_in host;
    
            memset(&host, '\0', sizeof(host));

            memcpy(&host.sin_addr, he->h_addr_list[0], he->h_length);
            host.sin_family = he->h_addrtype;
            host.sin_port   = se->s_port;
    
            if ((d = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
                if (connect(d, (struct sockaddr *)&host, sizeof(host)) == -1)
                    d = -1;
            }
        }
    }

#endif  /* CONFOPT_WITHOUT_GETADDRINFO */

    }

    /* create file value */
    if (d != -1) {
        struct mpdm_file *fs;

        f = MPDM_F(NULL);
        fs = (struct mpdm_file *) mpdm_data(f);

        fs->sock = d;
    }
    else
        store_syserr();

    free(s);
    free(h);

    mpdm_unref(serv);
    mpdm_unref(host);

    return f;
}


mpdm_t mpdm_server(mpdm_t addr, mpdm_t port)
/* opens a server socket in addr:port, addr can be NULL */
{
    mpdm_t f = NULL;
    char *h;
    int p;
    int d = -1;
    struct sockaddr_in host;

    mpdm_ref(addr);
    mpdm_ref(port);

    h = addr == NULL ? NULL : mpdm_wcstombs(mpdm_string(addr), NULL);
    p = mpdm_ival(port);

    init_sockets();

    memset(&host, '\0', sizeof(host));

    if (h != NULL) {
        struct hostent *he;

        if ((he = gethostbyname(h)) != NULL)
            memcpy(&host.sin_addr, he->h_addr_list[0], he->h_length);
        else
            goto end;
    }

    host.sin_family = AF_INET;
    host.sin_port   = htons(p);

    if ((d = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
        /* reuse addr */
        int i = 1;
        setsockopt(d, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i));

        if (bind(d, (struct sockaddr *)&host, sizeof(host)) == -1) {
            close(d);
            d = -1;
        }
        else
            listen(d, SOMAXCONN);
    }

    /* create file value */
    if (d != -1) {
        struct mpdm_file *fs;

        f = MPDM_F(NULL);
        fs = (struct mpdm_file *) mpdm_data(f);

        fs->sock = d;
    }
    else
        store_syserr();

end:
    free(h);

    mpdm_unref(port);
    mpdm_unref(addr);

    return f;
}


mpdm_t mpdm_accept(mpdm_t sock)
/* accepts a connection from a socket */
{
    mpdm_t f = NULL;
    struct sockaddr_in host;
    struct mpdm_file *fs;
    socklen_t l = sizeof(host);
    int d = -1;

    fs = (struct mpdm_file *)mpdm_data(sock);

    if ((d = accept(fs->sock, (struct sockaddr *)&host, &l)) != -1) {
        f = MPDM_F(NULL);
        fs = (struct mpdm_file *)mpdm_data(f);

        fs->sock = d;
    }
    else
        store_syserr();

    return f;
}


mpdm_t mpdm_socket_timeout(mpdm_t sock, mpdm_t rto, mpdm_t sto)
/* sets receive and send timout for sockets */
{
    int ret = 0;
    struct mpdm_file *fs;

    fs = (struct mpdm_file *)mpdm_data(sock);

    if (fs->sock != -1) {
        struct timeval tv;

        if (rto != NULL) {
            double d = mpdm_rval(rto);

            tv.tv_sec  = (int)d;
            tv.tv_usec = (int)((d - (double)(int)d) * 1000000.0);

            ret = setsockopt(fs->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

            if (ret == -1)
                store_syserr();
        }

        if (ret != -1 && sto != NULL) {
            double d = mpdm_rval(sto);

            tv.tv_sec  = (int)d;
            tv.tv_usec = (int)((d - (double)(int)d) * 1000000.0);

            ret = setsockopt(fs->sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

            if (ret == -1)
                store_syserr();
        }
    }
    else
        ret = -1;

    return MPDM_I(ret);
}


static int file_close(mpdm_t v)
/* close any type of file / pipe / socket */
{
    int r = -1;
    struct mpdm_file *fs;

    if (v && (fs = (struct mpdm_file *) mpdm_data(v))) {
#ifdef CONFOPT_ICONV
        if (fs->ic_enc != (iconv_t) - 1)
            iconv_close(fs->ic_enc);

        if (fs->ic_dec != fs->ic_enc && fs->ic_dec != (iconv_t) - 1)
            iconv_close(fs->ic_dec);

        fs->ic_enc = (iconv_t) - 1;
        fs->ic_dec = (iconv_t) - 1;
#endif

        if (fs->in != NULL)
            r = fclose(fs->in);

        if (fs->out != fs->in && fs->out != NULL)
            r = fclose(fs->out);

        fs->in = NULL;
        fs->out = NULL;

        if (fs->is_pipe) {
            r = sysdep_pclose(v);
            fs->is_pipe = 0;
        }

        if (fs->sock != -1) {
#ifdef CONFOPT_WIN32
            r = closesocket(fs->sock);
#else
            r = close(fs->sock);
#endif
            fs->sock = -1;
        }
    }

    return r;
}


static mpdm_t vc_file_destroy(mpdm_t v)
/* destroys an file value */
{
    file_close(v);

    return v;
}


mpdm_t mpdm_new_f(FILE *f)
/* creates a new file value */
{
    mpdm_t v = NULL;
    struct mpdm_file *fs;
    mpdm_t e;

    fs = calloc(sizeof(struct mpdm_file), 1);

    fs->sock      = -1;
    fs->is_pipe   = 0;
    fs->skip_wait = 0;
    fs->in        = f;
    fs->out       = f;

    /* default I/O functions */
    fs->f_read = read_auto;
    fs->f_write = write_wcs;

#ifdef CONFOPT_ICONV
    /* no iconv encodings by default */
    fs->ic_enc = fs->ic_dec = (iconv_t) - 1;
#endif

    /* autochomp? */
    fs->auto_chomp = mpdm_is_true(mpdm_get_wcs(mpdm_root(), L"AUTO_CHOMP"));

    v = mpdm_new(MPDM_TYPE_FILE, fs, sizeof(struct mpdm_file));

    e = mpdm_get_wcs(mpdm_root(), L"ENCODING");

    if (mpdm_size(e) == 0)
        e = mpdm_get_wcs(mpdm_root(), L"TEMP_ENCODING");

    if (mpdm_size(e)) {
        wchar_t *enc = mpdm_string(e);

        if (wcscmp(enc, L"utf-8") == 0) {
            fs->f_read = read_utf8_bom;
            fs->f_write = write_utf8;
        }
        else
        if (wcscmp(enc, L"utf-8bom") == 0) {
            fs->f_read = read_utf8_bom;
            fs->f_write = write_utf8_bom;
        }
        else
        if (wcscmp(enc, L"iso8859-1") == 0 ||
                 wcscmp(enc, L"8bit") == 0) {
            fs->f_read = read_iso8859_1;
            fs->f_write = write_iso8859_1;
        }
        else
        if (wcscmp(enc, L"utf-16le") == 0) {
            fs->f_read = read_utf16le;
            fs->f_write = write_utf16le_bom;
        }
        else
        if (wcscmp(enc, L"utf-16be") == 0) {
            fs->f_read = read_utf16be;
            fs->f_write = write_utf16be_bom;
        }
        else
        if (wcscmp(enc, L"utf-16") == 0) {
            fs->f_read = read_utf16;
            fs->f_write = write_utf16le_bom;
        }
        else
        if (wcscmp(enc, L"utf-32le") == 0) {
            fs->f_read = read_utf32le;
            fs->f_write = write_utf32le_bom;
        }
        else
        if (wcscmp(enc, L"utf-32be") == 0) {
            fs->f_read = read_utf32be;
            fs->f_write = write_utf32be_bom;
        }
        else
        if (wcscmp(enc, L"utf-32") == 0) {
            fs->f_read = read_utf32;
            fs->f_write = write_utf32le_bom;
        }
        else
        if (wcscmp(enc, L"msdos-437") == 0) {
            fs->f_read = read_msdos_437;
            fs->f_write = write_msdos_437;
        }
        else
        if (wcscmp(enc, L"msdos-850") == 0) {
            fs->f_read = read_msdos_850;
            fs->f_write = write_msdos_850;
        }
        else
        if (wcscmp(enc, L"windows-1252") == 0) {
            fs->f_read = read_windows_1252;
            fs->f_write = write_windows_1252;
        }
        else {
#ifdef CONFOPT_ICONV
            mpdm_t cs = mpdm_ref(MPDM_2MBS(mpdm_data(e)));

            if ((fs->ic_enc = iconv_open((char *) mpdm_data(cs), "WCHAR_T")) != (iconv_t) - 1 &&
                (fs->ic_dec = iconv_open("WCHAR_T", (char *) mpdm_data(cs))) != (iconv_t) - 1) {

                fs->f_read  = read_iconv;
                fs->f_write = write_iconv;
            }

            mpdm_unref(cs);
#endif                          /* CONFOPT_ICONV */
        }

        mpdm_set_wcs(mpdm_root(), NULL, L"TEMP_ENCODING");
    }

    return v;
}


/**
 * mpdm_close - Closes a file descriptor.
 * @fd: the value containing the file descriptor
 *
 * Closes the file descriptor.
 * [File Management]
 */
int mpdm_close(mpdm_t fd)
{
    int r;

    mpdm_ref(fd);
    r = file_close(fd);
    mpdm_unref(fd);

    return r;
}


/** file vc **/

static mpdm_t vc_file_get_i(const mpdm_t f, int index)
{
    mpdm_fseek(f, (long) index, 0);

    return mpdm_read(f);
}


static mpdm_t vc_file_get(const mpdm_t f, mpdm_t index)
{
    return vc_file_get_i(f, mpdm_ival(index));
}


static mpdm_t vc_file_exec(mpdm_t c, mpdm_t args, mpdm_t ctxt)
{
    mpdm_t r = NULL;

    /* if there are any arguments, treat as "write" */
    if (mpdm_size(args)) {
        int n;

        for (n = 0; n < mpdm_size(args); n++)
            mpdm_write(c, mpdm_get_i(args, n));

        r = NULL;
    }
    else
        r = mpdm_read(c);

    return r;
}


static int vc_file_iterator(mpdm_t set, int64_t *context, mpdm_t *v, mpdm_t *i)
{
    mpdm_t w = mpdm_read(set);
    int ret  = 0;

    if (w != NULL) {
        if (v)
            *v = w;
        else
            mpdm_void(w);

        if (i) *i = MPDM_I(*context);

        (*context) = (int64_t) mpdm_ftell(set);
        ret = 1;
    }

    return ret;
}


struct mpdm_type_vc mpdm_vc_file = { /* VC */
    L"file",                /* name */
    vc_file_destroy,        /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_file_get_i,          /* get_i */
    vc_file_get,            /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_default_set,         /* set */
    vc_file_exec,           /* exec */
    vc_file_iterator,       /* iterator */
    vc_default_map,         /* map */
    vc_default_can_exec,    /* can_exec */
    vc_default_clone        /* clone */
};
