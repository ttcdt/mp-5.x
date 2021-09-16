/*

    MPDM - Minimum Profit Data Manager
    mpdm_v.c - Basic value management

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "mpdm.h"


/** data **/

mpdm_t mpdm_global_root = NULL;

extern struct mpdm_type_vc mpdm_vc_null;
extern struct mpdm_type_vc mpdm_vc_string;
extern struct mpdm_type_vc mpdm_vc_array;
extern struct mpdm_type_vc mpdm_vc_object;
extern struct mpdm_type_vc mpdm_vc_file;
extern struct mpdm_type_vc mpdm_vc_mbs;
extern struct mpdm_type_vc mpdm_vc_regex;
extern struct mpdm_type_vc mpdm_vc_mutex;
extern struct mpdm_type_vc mpdm_vc_semaphore;
extern struct mpdm_type_vc mpdm_vc_thread;
extern struct mpdm_type_vc mpdm_vc_function;
extern struct mpdm_type_vc mpdm_vc_program;
extern struct mpdm_type_vc mpdm_vc_integer;
extern struct mpdm_type_vc mpdm_vc_real;

/* data type information and virtual calls */
struct mpdm_type_vc *mpdm_type_vcs[] = {
    &mpdm_vc_null,
    &mpdm_vc_string,
    &mpdm_vc_array,
    &mpdm_vc_object,
    &mpdm_vc_file,
    &mpdm_vc_mbs,
    &mpdm_vc_regex,
    &mpdm_vc_mutex,
    &mpdm_vc_semaphore,
    &mpdm_vc_thread,
    &mpdm_vc_function,
    &mpdm_vc_program,
    &mpdm_vc_integer,
    &mpdm_vc_real
};

/* pointer to the destroy function */
mpdm_func1_t *mpdm_destroy = NULL;


/** code **/

struct mpdm_type_vc *mpdm_type_vc_by_t(mpdm_type_t t)
/* returns the mpdm_type_vc associated to a type */
{
    return mpdm_type_vcs[t];
}


struct mpdm_type_vc *mpdm_type_vc(mpdm_t v)
/* returns the mpdm_type_vc associated to a value */
{
    return mpdm_type_vc_by_t(mpdm_type(v));
}


mpdm_t mpdm_real_destroy(mpdm_t v)
/* destroys a value */
{
    /* call destroy function */
    v = mpdm_type_vc(v)->destroy(v);

    /* free data */
    free(mpdm_data(v));

    /* garble the memory block */
    memset(v, 0xaa, sizeof(*v));

    free(v);

    return NULL;
}


/**
 * mpdm_new - Creates a new value.
 * @type: data type
 * @data: pointer to real data
 * @size: size of data
 *
 * Creates a new value. @type is the data type, @data is a
 * pointer to the data the value will store and @size the size of these
 * data (if value is to be a multiple one, @size is a number of elements,
 * or a number of bytes otherwise).
 *
 * This function is normally not directly used; use any of the type
 * creation macros instead.
 * [Value Creation]
 */
mpdm_t mpdm_new(mpdm_type_t type, const void *data, int size)
{
    mpdm_t v;

    v = (mpdm_t) calloc(sizeof(*v), 1);

    v->type     = type;
    v->size     = size;
    v->ref      = 0;
    v->data     = data;

    return v;
}


mpdm_type_t mpdm_type(mpdm_t v)
{
    return v ? v->type : MPDM_TYPE_NULL;
}


wchar_t *mpdm_type_wcs(mpdm_t v)
{
    return mpdm_type_vc(v)->name;
}


/**
 * mpdm_ref - Increments the reference count of a value.
 * @v: the value
 *
 * Increments the reference count of a value.
 * [Value Management]
 */
mpdm_t mpdm_ref(mpdm_t v)
{
    if (v != NULL)
        v->ref++;

    return v;
}


/**
 * mpdm_unref - Decrements the reference count of a value.
 * @v: the value
 *
 * Decrements the reference count of a value. If the reference
 * count of the value reaches 0, it's destroyed.
 * [Value Management]
 */
mpdm_t mpdm_unref(mpdm_t v)
{
    if (v != NULL && --v->ref <= 0)
        v = mpdm_destroy(v);

    return v;
}


/**
 * mpdm_unrefnd - Decrements the reference count of a value, without destroy.
 * @v: the value
 *
 * Decrements the reference count of a value, without destroying
 * the value if it's unreferenced.
 * [Value Management]
 */
mpdm_t mpdm_unrefnd(mpdm_t v)
{
    if (v != NULL)
        v->ref--;

    return v;
}


/**
 * mpdm_size - Returns the size of an element.
 * @v: the element
 *
 * Returns the size of an element. It does not change the
 * reference count of the value.
 * [Value Management]
 */
int mpdm_size(const mpdm_t v)
{
    return v ? v->size : 0;
}


/**
 * mpdm_data - Returns the data of an element.
 * @v: the element
 *
 * Returns the data of an element. It does not change the
 * reference count of the value.
 * [Value Management]
 */
void *mpdm_data(const mpdm_t v)
{
    return v ? (void *)v->data : NULL;
}


/**
 * mpdm_root - Returns the root hash.
 *
 * Returns the root hash. This hash is stored internally and can be used
 * as a kind of global symbol table.
 * [Value Management]
 */
mpdm_t mpdm_root(void)
{
    return mpdm_global_root = mpdm_global_root ? mpdm_global_root : mpdm_ref(MPDM_O());
}


/**
 * mpdm_void - Refs then unrefs a value.
 * @v: the value
 *
 * References and unreferences a value. To be used to receive
 * the output of mpdm_exec() in case of it being void (i.e.
 * its return value ignored).
 */
mpdm_t mpdm_void(mpdm_t v)
{
    return mpdm_unref(mpdm_ref(v));
}


/**
 * mpdm_is_null - Returns 1 if a value is NULL.
 * @v: the value
 *
 * Returns 1 if a value is NULL. The reference count is touched.
 */
int mpdm_is_null(mpdm_t v)
{
    int r;

    mpdm_ref(v);
    r = (v == NULL);
    mpdm_unref(v);

    return r;
}


mpdm_t mpdm_store(mpdm_t *v, mpdm_t w)
{
    mpdm_ref(w);
    mpdm_unref(*v);
    *v = w;

    return w;
}


mpdm_t mpdm_new_copy(mpdm_type_t type, void *ptr, int size)
{
    mpdm_t r = NULL;

    if (ptr != NULL) {
        char *ptr2 = malloc(size);
        memcpy(ptr2, ptr, size);

        r = mpdm_new(type, ptr2, size);
    }

    return r;
}


int mpdm_wrap_pointers(mpdm_t v, int offset, int *del)
{
    /* wrap from the end */
    if (offset < 0) {
        offset = mpdm_size(v) + offset;

        /* still negative? restart */
        if (offset < 0)
            offset = 0;
    }

    if (del) {
        /* negative values mean 'count from the end' */
        if (*del < 0)
            *del = mpdm_size(v) + 1 - offset + *del;

        /* trim if trying to delete too far */
        if (offset + *del > mpdm_size(v))
            *del = mpdm_size(v) - offset;
    }

    return offset;
}


extern char **environ;

static mpdm_t build_env(void)
/* builds a hash with the environment */
{
    char **ptr;
    mpdm_t e = MPDM_O();

    mpdm_ref(e);

    for (ptr = environ; *ptr != NULL; ptr++) {
        char *eq = strchr(*ptr, '=');

        if (eq != NULL) {
            mpdm_t k, v;

            k = MPDM_NMBS((*ptr), eq - (*ptr));
            v = MPDM_MBS(eq + 1);

            mpdm_set(e, v, k);
        }
    }

    mpdm_unrefnd(e);

    return e;
}


extern char *mpdm_build_git_rev;
extern char *mpdm_build_timestamp;

/**
 * mpdm_startup - Initializes MPDM.
 *
 * Initializes the Minimum Profit Data Manager. Returns 0 if
 * everything went OK.
 */
int mpdm_startup(void)
{
    mpdm_t r, v, w;

    /* set the pointer to the destroy function */
    mpdm_destroy = mpdm_real_destroy;
//    mpdm_destroy = mpdm_delayed_destroy;

    r = mpdm_root();

    /* sets the locale */
    if (setlocale(LC_ALL, "") == NULL)
        if (setlocale(LC_ALL, "C.UTF-8") == NULL)
            setlocale(LC_ALL, "C");

    mpdm_encoding(NULL);

    /* store the MPDM object */
    v = mpdm_set_wcs(r, MPDM_O(), L"MPDM");
    mpdm_set_wcs(v, MPDM_MBS(VERSION),              L"version");

    /* store the ENV hash */
    mpdm_set_wcs(r, build_env(), L"ENV");

    /* store the special values TRUE and FALSE */
    mpdm_set_wcs(r, MPDM_I(1), L"TRUE");
    mpdm_set_wcs(r, MPDM_I(0), L"FALSE");

    /* store the configuration options */
    w = mpdm_set_wcs(v, MPDM_A(0), L"confopt");

#ifdef CONFOPT_WIN32
    mpdm_push(w, MPDM_S(L"win32"));
#endif
#ifdef CONFOPT_WITHOUT_GETADDRINFO
    mpdm_push(w, MPDM_S(L"without_getaddrinfo"));
#endif
#ifdef CONFOPT_GLOB_H
    mpdm_push(w, MPDM_S(L"glob_h"));
#endif
#ifdef CONFOPT_PCRE
    mpdm_push(w, MPDM_S(L"pcre"));
#endif
#ifdef CONFOPT_SYSTEM_REGEX
    mpdm_push(w, MPDM_S(L"system_regex"));
#endif
#ifdef CONFOPT_INCLUDED_REGEX
    mpdm_push(w, MPDM_S(L"included_regex"));
#endif
#ifdef CONFOPT_NO_REGEX
    mpdm_push(w, MPDM_S(L"no_regex"));
#endif
#ifdef CONFOPT_UNISTD_H
    mpdm_push(w, MPDM_S(L"unistd_h"));
#endif
#ifdef CONFOPT_SYS_TYPES_H
    mpdm_push(w, MPDM_S(L"sys_types_h"));
#endif
#ifdef CONFOPT_SYS_WAIT_H
    mpdm_push(w, MPDM_S(L"sys_wait_h"));
#endif
#ifdef CONFOPT_SYS_STAT_H
    mpdm_push(w, MPDM_S(L"sys_stat_h"));
#endif
#ifdef CONFOPT_SYS_FILE_H
    mpdm_push(w, MPDM_S(L"sys_file_h"));
#endif
#ifdef CONFOPT_PWD_H
    mpdm_push(w, MPDM_S(L"pwd_h"));
#endif
#ifdef CONFOPT_SYS_SOCKET_H
    mpdm_push(w, MPDM_S(L"sys_socket_h"));
#endif
#ifdef CONFOPT_NETDB_H
    mpdm_push(w, MPDM_S(L"netdb_h"));
#endif
#ifdef CONFOPT_CHOWN
    mpdm_push(w, MPDM_S(L"chown"));
#endif
#ifdef CONFOPT_GETTEXT
    mpdm_push(w, MPDM_S(L"gettext"));
#endif
#ifdef CONFOPT_ICONV
    mpdm_push(w, MPDM_S(L"iconv"));
#endif
#ifdef CONFOPT_WCWIDTH
    mpdm_push(w, MPDM_S(L"wcwidth"));
#endif
#ifdef CONFOPT_CANONICALIZE_FILE_NAME
    mpdm_push(w, MPDM_S(L"canonicalize_file_name"));
#endif
#ifdef CONFOPT_REALPATH
    mpdm_push(w, MPDM_S(L"realpath"));
#endif
#ifdef CONFOPT_FULLPATH
    mpdm_push(w, MPDM_S(L"fullpath"));
#endif
#ifdef CONFOPT_NANOSLEEP
    mpdm_push(w, MPDM_S(L"nanosleep"));
#endif
#ifdef CONFOPT_STRPTIME
    mpdm_push(w, MPDM_S(L"strptime"));
#endif
#ifdef CONFOPT_MALLOC_MALLOC_H
    mpdm_push(w, MPDM_S(L"malloc_malloc_h"));
#endif
#ifdef CONFOPT_GETTIMEOFDAY
    mpdm_push(w, MPDM_S(L"gettimeofday"));
#endif
#ifdef CONFOPT_ZLIB
    mpdm_push(w, MPDM_S(L"zlib"));
#endif
#ifdef CONFOPT_PTHREADS
    mpdm_push(w, MPDM_S(L"pthreads"));
#endif
#ifdef CONFOPT_POSIXSEMS
    mpdm_push(w, MPDM_S(L"posixsems"));
#endif

    /* everything went OK */
    return 0;
}


/**
 * mpdm_shutdown - Shuts down MPDM.
 *
 * Shuts down MPDM. No MPDM functions should be used from now on.
 */
void mpdm_shutdown(void)
{
    /* dummy, by now */
}

/**
 * MPDM_A - Creates an array value.
 * @n: Number of elements
 *
 * Creates a new array value with @n elements.
 * [Value Creation]
 */
/** mpdm_t MPDM_A(int n); */
/* ; */

/**
 * MPDM_O - Creates an object value.
 *
 * Creates a new object value.
 * [Value Creation]
 */
/** mpdm_t MPDM_O(void); */
/* ; */

/**
 * MPDM_S - Creates a string value from a string.
 * @wcs: the wide character string
 *
 * Creates a new string value from a wide character string. The value
 * will store a copy of the string that will be freed on destruction.
 * [Value Creation]
 */
/** mpdm_t MPDM_S(wchar_t * wcs); */
/* ; */

/**
 * MPDM_NS - Creates a string value from a string, with size.
 * @wcs: the wide character string
 * @s: the size in chars the string will hold
 *
 * Creates a new string value with a copy of the first @s characters
 * from the @wcs string.
 * [Value Creation]
 */
/** mpdm_t MPDM_NS(wchar_t * wcs, int s); */
/* ; */

/**
 * MPDM_ENS - Creates a string value from an external string, with size.
 * @wcs: the external wide character string
 * @s: the size in chars the string will hold
 *
 * Creates a new string value with size @s. The @wcs string must be
 * a dynamic value (i.e. allocated by malloc()) that will be freed on
 * destruction.
 * [Value Creation]
 */
/** mpdm_t MPDM_ENS(wchar_t * wcs, int s); */
/* ; */

/**
 * MPDM_I - Creates an integer value.
 * @i: the integer
 *
 * Creates a new integer value. MPDM integers are strings.
 * [Value Creation]
 */
/** mpdm_t MPDM_I(int i); */
/* ; */

/**
 * MPDM_R - Creates a real value.
 * @r: the real number
 *
 * Creates a new real value. MPDM integers are strings.
 * [Value Creation]
 */
/** mpdm_t MPDM_R(double r); */
/* ; */

/**
 * MPDM_F - Creates a file value.
 * @f: the file descriptor
 *
 * Creates a new file value.
 * [Value Creation]
 */
/** mpdm_t MPDM_F(FILE * f); */
/* ; */

/**
 * MPDM_MBS - Creates a string value from a multibyte string.
 * @mbs: the multibyte string
 *
 * Creates a new string value from a multibyte string, that will be
 * converted to wcs by mpdm_mbstowcs().
 * [Value Creation]
 */
/** mpdm_t MPDM_MBS(char * mbs); */
/* ; */

/**
 * MPDM_NMBS - Creates a string value from a multibyte string, with size.
 * @mbs: the multibyte string
 * @s: the size
 *
 * Creates a new string value with the first @s characters from the @mbs
 * multibyte string, that will be converted to wcs by mpdm_mbstowcs().
 * [Value Creation]
 */
/** mpdm_t MPDM_NMBS(char * mbs, int s); */
/* ; */

/**
 * MPDM_2MBS - Creates a multibyte string value from a wide char string.
 * @wcs: the wide char string
 *
 * Creates a multibyte string value from the @wcs wide char string,
 * converting it by mpdm_wcstombs(). Take note that multibyte string values
 * are not properly strings, so they cannot be used for string comparison
 * and such.
 * [Value Creation]
 */
/** mpdm_t MPDM_2MBS(wchar_t * wcs); */
/* ; */

/**
 * MPDM_X - Creates a new executable value.
 * @func: the C code function
 *
 * Creates a new executable value given a pointer to the @func C code function.
 * The function must receive an mpdm_t array value (that will hold their
 * arguments) and return another one.
 * [Value Creation]
 */
/** mpdm_t MPDM_X(mpdm_t (* func)(mpdm_t args)); */
/* ; */

/**
 * MPDM_C - Creates a new value with a copy of a buffer.
 * @type: data type
 * @ptr: pointer to data
 * @size: data size
 *
 * Create a new value with a copy of a buffer. The value will store a copy
 * of @ptr and have the specified @type.
 * [Value Creation]
 */
/** mpdm_t MPDM_C(mpdm_type_t type, void *ptr, int size); */
/* ; */


/** type vc **/

mpdm_t vc_default_destroy(mpdm_t v)                  { return v; }
mpdm_t vc_null_destroy(mpdm_t v)                     { v->data = NULL; return v; }
int vc_default_is_true(mpdm_t v)                     { return 1; }
int vc_default_count(mpdm_t v)                       { return mpdm_size(v); }
mpdm_t vc_default_get_i(mpdm_t v, int i)             { return NULL; }
mpdm_t vc_default_get(mpdm_t v, mpdm_t i)            { return NULL; }
mpdm_t vc_default_del_i(mpdm_t v, int i)             { return NULL; }
mpdm_t vc_default_del(mpdm_t v, mpdm_t i)            { return NULL; }
mpdm_t vc_default_set_i(mpdm_t v, mpdm_t e, int i)   { return NULL; }
mpdm_t vc_default_set(mpdm_t v, mpdm_t e, mpdm_t i)  { return NULL; }
mpdm_t vc_default_exec(mpdm_t v, mpdm_t a, mpdm_t c) { return NULL; }
int vc_default_iterator(mpdm_t s, int *c, mpdm_t *v, mpdm_t *i) { return 0; }
int vc_default_can_exec(mpdm_t v)                    { return 1; }
int vc_default_cannot_exec(mpdm_t v)                 { return 0; }


static int vc_null_is_true(mpdm_t v)
{
    return 0;
}

static wchar_t *vc_null_string(mpdm_t v)
{
    return L"[NULL]";
}


static mpdm_t vc_function_destroy(mpdm_t v)
{
    v->data = NULL;
    return v;
}

static mpdm_t vc_function_get(mpdm_t f, mpdm_t args)
{
    return mpdm_exec_1(f, args, NULL);
}

static mpdm_t vc_function_set(const mpdm_t f, mpdm_t v, mpdm_t index)
{
    return mpdm_exec_2(f, v, index, NULL);
}

static mpdm_t vc_function_exec(mpdm_t c, mpdm_t args, mpdm_t ctxt)
{
    mpdm_t r = NULL;
    mpdm_func2_t *func2;

    if ((func2 = (mpdm_func2_t *)mpdm_data(c)) != NULL)
        r = func2(args, ctxt);

    return r;
}

struct mpdm_type_vc mpdm_vc_null = { /* VC */
    L"null",                /* name */
    vc_default_destroy,     /* destroy */
    vc_null_is_true,        /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_default_get,         /* get */
    vc_null_string,         /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_default_set,         /* set */
    vc_default_exec,        /* exec */
    vc_default_iterator,    /* iterator */
    vc_default_map,         /* map */
    vc_default_cannot_exec  /* can_exec */
};

struct mpdm_type_vc mpdm_vc_function = { /* VC */
    L"function",            /* name */
    vc_function_destroy,    /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_function_get,        /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_function_set,        /* set */
    vc_function_exec,       /* exec */
    vc_default_iterator,    /* iterator */
    vc_default_map,         /* map */
    vc_default_can_exec     /* can_exec */
};
