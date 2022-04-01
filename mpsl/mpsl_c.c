/*

    MPSL - Minimum Profit Scripting Language
    mpsl_c.c - language core

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "mpdm.h"
#include "mpsl.h"

#ifndef CONFOPT_DEFAULT_COMPILER
#define CONFOPT_DEFAULT_COMPILER "c_mpsl"
#endif

/** data **/

/* global abort flag */
int mpsl_abort = 0;

/* cached opcodes */
mpdm_t mpsl_opcodes = NULL;

/* pointer to a trap function */
static mpdm_t mpsl_trap_func = NULL;

typedef enum {
    FLOW_RETURN = -1,
    FLOW_RUNNING = 0,
    FLOW_BREAK = 1
} mpsl_flow_t;


/** code **/

static mpdm_t find_symtbl(mpdm_t s, mpdm_t l)
/* finds the symbol table that holds l */
{
    int n;
    mpdm_t v = NULL;

    if (l != NULL) {
        /* if s is multiple, take just the first element */
        if (mpdm_type(s) == MPDM_TYPE_ARRAY)
            s = mpdm_get_i(s, 0);

        /* travel the local symbol table trying to find it */
        for (n = mpdm_size(l) - 1; n >= 0; n--) {
            mpdm_t h = mpdm_get_i(l, n);

            if (mpdm_exists(h, s)) {
                v = h;
                break;
            }
        }
    }

    /* the symbol is not defined in any of the local symtbls;
       test if it's on the global table */
    if (v == NULL) {
        if (mpdm_exists(mpdm_root(), s))
            v = mpdm_root();
    }

    /* the symbol does not exist; select a suitable symtbl */
    if (v == NULL)
        v = mpdm_root();

    return v;
}


static void set_local_symbols(mpdm_t s, mpdm_t v, mpdm_t l)
/* sets (or creates) a list of local symbols with a list of values */
{
    if (l != NULL) {
        mpdm_t h;

        mpdm_ref(s);
        mpdm_ref(v);
        mpdm_ref(l);

        /* gets the top local variable frame */
        h = mpdm_get_i(l, -1);

        if (mpdm_type(s) == MPDM_TYPE_ARRAY || mpdm_type(v) == MPDM_TYPE_ARRAY) {
            int n;
            mpdm_t a;

            for (n = 0; n < mpdm_size(s); n++)
                mpdm_set(h, mpdm_get_i(v, n), mpdm_get_i(s, n));

            if (n < mpdm_size(v)) {
                /* store the rest of arguments into _ */
                a = mpdm_set_wcs(h, MPDM_A(0), L"_");

                for (; n < mpdm_size(v); n++)
                    mpdm_push(a, mpdm_get_i(v, n));
            }
        }
        else
        if (mpdm_type(s) != MPDM_TYPE_NULL)
            mpdm_set(h, v, s);

        mpdm_unref(l);
        mpdm_unref(v);
        mpdm_unref(s);
    }
}


/**
 * mpsl_set_symbol - Sets value to a symbol.
 * @s: symbol name in array form
 * @v: value
 * @l: local symbol table
 *
 * Assigns the value @v to the @s symbol. If the value exists as
 * a local symbol, it's assigned to it; otherwise, it's set as a global
 * symbol (and created if it does not exist).
 */
mpdm_t mpsl_set_symbol(mpdm_t s, mpdm_t v, mpdm_t l)
{
    int n;
    mpdm_t r = NULL;

    mpdm_ref(s);
    mpdm_ref(v);
    mpdm_ref(l);

    /* if it's in string format, split it */
    if (mpdm_type(s) == MPDM_TYPE_STRING)
        r = mpsl_set_symbol(mpdm_split_wcs(s, L"."), v, l);
    else
    if (mpdm_type(s) == MPDM_TYPE_ARRAY) {
        r = find_symtbl(s, l);

        for (n = 0; r != NULL && n < mpdm_size(s) - 1; n++) {
            /* is executable? run it and take its output */
            while (mpdm_can_exec(r))
                r = mpdm_exec(r, NULL, NULL);

            if (mpdm_type(r) == MPDM_TYPE_OBJECT)
                r = mpdm_get(r, mpdm_get_i(s, n));
            else
            if (mpdm_type(r) == MPDM_TYPE_ARRAY)
                r = mpdm_get_i(r, mpdm_ival(mpdm_get_i(s, n)));
            else
                r = mpdm_void(r);
        }

        if (r != NULL) {
            /* yes; do the setting */
            if (mpdm_type(r) == MPDM_TYPE_OBJECT)
                r = mpdm_set(r, v, mpdm_get_i(s, n));
            else
            if (mpdm_type(r) == MPDM_TYPE_ARRAY)
                r = mpdm_set_i(r, v, mpdm_ival(mpdm_get_i(s, n)));
        }
    }

    mpdm_unref(l);
    mpdm_unref(s);
    mpdm_unref(v);

    return r;
}


/**
 * mpsl_get_symbol - Gets the value of a symbol.
 * @s: symbol name in array form
 * @l: local symbol table
 *
 * Gets the value of a symbol. The symbol can be local or global
 * (if the symbol exists in both tables, the local value will be returned).
 */
mpdm_t mpsl_get_symbol(mpdm_t s, mpdm_t l)
{
    int n;
    mpdm_t r = NULL;

    mpdm_ref(s);
    mpdm_ref(l);

    /* if it's in string format, split it */
    if (mpdm_type(s) == MPDM_TYPE_STRING)
        r = mpsl_get_symbol(mpdm_split_wcs(s, L"."), l);
    else {
        r = find_symtbl(s, l);

        for (n = 0; r != NULL && n < mpdm_size(s); n++) {
            /* is executable? run it and take its output */
            while (mpdm_can_exec(r))
                r = mpdm_exec(r, NULL, NULL);

            if (mpdm_type(r) == MPDM_TYPE_OBJECT ||
                mpdm_type(r) == MPDM_TYPE_ARRAY ||
                mpdm_type(r) == MPDM_TYPE_STRING)
                r = mpdm_get(r, mpdm_get_i(s, n));
            else
                r = mpdm_void(r);
        }
    }

    mpdm_unref(l);
    mpdm_unref(s);

    return r;
}


/**
 * mpsl_error - Generates an error.
 * @err: the error message
 *
 * Generates an error. The @err error message is stored in the ERROR
 * mpsl variable and the mpsl_abort global flag is set, so no further
 * mpsl code can be executed until reset.
 */
mpdm_t mpsl_error(mpdm_t err)
{
    /* abort further execution */
    mpsl_abort = 1;

    /* set the error */
    return mpdm_set_wcs(mpdm_root(), err, L"ERROR");
}


/** opcode macro helpers **/

#define O_TYPE static mpdm_t
#define O_ARGS mpdm_t c, mpdm_t a, mpdm_t l, mpsl_flow_t *f

O_TYPE mpsl_exec_i(O_ARGS);

#define C(n) mpdm_get_i(c, n)
#define C0 C(0)
#define C1 C(1)

#define M(n) mpsl_exec_i(C(n), a, l, f)
#define M1 M(1)
#define M2 M(2)
#define M3 M(3)
#define M4 M(4)

#define R(x) mpdm_rval(x)
#define I(x) mpdm_ival(x)

#define RM1 mpdm_rval(M(1))
#define RM2 mpdm_rval(M(2))
#define IM1 mpdm_ival(M(1))
#define IM2 mpdm_ival(M(2))

#define GET(m) mpsl_get_symbol(m, l)
#define SET(m, v) mpsl_set_symbol(m, v, l)
#define BOOL mpdm_bool

#define RF(v) mpdm_ref(v)
#define UF(v) v = mpdm_unref(v)
#define UFND(v) mpdm_unrefnd(v)

/** opcodes **/

O_TYPE O_literal(O_ARGS)
{
    return mpdm_clone(C1);
}

O_TYPE O_multi(O_ARGS)
{
    mpdm_t v = M1;

    if (*f == FLOW_RUNNING) {
        mpdm_void(v);
        v = M2;
    }

    return v;
}

O_TYPE O_symval(O_ARGS)
{
    return GET(M1);
}

O_TYPE O_assign(O_ARGS)
{
    return SET(M1, M2);
}

O_TYPE O_if(O_ARGS)
{
    return mpdm_is_true(M1) ? M2 : M3;
}

O_TYPE O_local(O_ARGS)
{
    set_local_symbols(M1, NULL, l);

    return NULL;
}

O_TYPE O_global(O_ARGS)
{
    mpdm_t v = RF(M1);

    if (mpdm_type(v) == MPDM_TYPE_ARRAY) {
        int n;

        for (n = 0; n < mpdm_size(v); n++)
            mpdm_set(mpdm_root(), NULL, mpdm_get_i(v, n));
    }
    else
        mpdm_set(mpdm_root(), NULL, v);

    UF(v);

    return NULL;
}


O_TYPE O_add(O_ARGS)
{
    return mpdm_join(M1, M2);
}

O_TYPE O_sub(O_ARGS)
{
    return mpdm_substract(M1, M2);
}

O_TYPE O_mul(O_ARGS)
{
    return mpdm_multiply(M1, M2);
}

O_TYPE O_div(O_ARGS)
{
    mpdm_t r = NULL;

    if ((r = mpdm_divide(M1, M2)) == NULL)
        mpsl_error(MPDM_S(L"Division by zero"));

    return r;
}

O_TYPE O_mod(O_ARGS)
{
    int d;
    mpdm_t r = NULL;

    if ((d = IM2) == 0)
        mpsl_error(MPDM_S(L"Division by zero"));
    else
        r = MPDM_I(IM1 % d);

    return r;
}

O_TYPE O_not(O_ARGS)
{
    return BOOL(!mpdm_is_true(M1));
}

O_TYPE O_and(O_ARGS)
{
    mpdm_t v = M1;
    mpdm_t r;

    mpdm_ref(v);
    if (mpdm_is_true(v)) {
        mpdm_unref(v);
        r = M2;
    }
    else
        r = mpdm_unrefnd(v);

    return r;
}

O_TYPE O_or(O_ARGS)
{
    mpdm_t v = M1;
    mpdm_t r;

    mpdm_ref(v);
    if (!mpdm_is_true(v)) {
        mpdm_unref(v);
        r = M2;
    }
    else
        r = mpdm_unrefnd(v);

    return r;
}

O_TYPE O_lt(O_ARGS)
{
    return BOOL(mpdm_cmp(M1, M2) < 0);
}

O_TYPE O_le(O_ARGS)
{
    return BOOL(mpdm_cmp(M1, M2) <= 0);
}

O_TYPE O_gt(O_ARGS)
{
    return BOOL(mpdm_cmp(M1, M2) > 0);
}

O_TYPE O_ge(O_ARGS)
{
    return BOOL(mpdm_cmp(M1, M2) >= 0);
}

O_TYPE O_eq(O_ARGS)
{
    return BOOL(mpdm_cmp(M1, M2) == 0);
}

O_TYPE O_break(O_ARGS)
{
    *f = FLOW_BREAK;

    return NULL;
}

O_TYPE O_return(O_ARGS)
{
    mpdm_t v = M1;

    *f = FLOW_RETURN;

    return v;
}

O_TYPE execsym(O_ARGS, int m)
{
    mpdm_t s, cp, p;
    mpdm_t v, o = NULL, r = NULL;

    /* gets the symbol name */
    s = RF(M1);

    /* gets the code position */
    cp = RF(M2);

    /* gets the arguments */
    p = RF(M3);

    /* if it's to be called as a method, the object
       should be inserted into the local symtable
       before searching the symbol */
    if (m && (o = mpdm_get_i(p, 0)) && mpdm_type(o) == MPDM_TYPE_OBJECT) {
        mpdm_push(l, o);
    }
    else
        m = 0;

    /* gets the symbol value */
    v = GET(s);

    if (!mpdm_can_exec(v)) {
        /* not found or NULL value? error */
        v = mpdm_strcat_wcs(cp, L": undefined function ");
        v = mpdm_strcat(v, mpdm_join_wcs(s, L"."));
        v = mpdm_strcat_wcs(v, L"()");

        mpsl_error(v);
    }
    else {
        /* execute */
        r = RF(mpdm_exec(v, p, l));
    }

    UF(s);
    UF(cp);
    UF(p);

    /* drop the object from the local symtable */
    if (m)
        mpdm_del_i(l, -1);

    return UFND(r);
}


O_TYPE O_execsym(O_ARGS)
/* executes the value of a symbol */
{
    return execsym(c, a, l, f, 0);
}


O_TYPE O_method(O_ARGS)
/* executes the value of a symbol including
   the first argument in the symtable */
{
    return execsym(c, a, l, f, 1);
}


O_TYPE O_while(O_ARGS)
/* while/for loop */
{
    mpdm_t r = NULL;

    for (mpdm_void(M3); *f == FLOW_RUNNING && mpdm_is_true(M1); mpdm_void(M4)) {
        UF(r);
        r = RF(M2);
    }

    if (*f == FLOW_BREAK)
        *f = FLOW_RUNNING;

    return UFND(r);
}


O_TYPE O_foreach(O_ARGS)
/* foreach loop. Can be:
   foreach (sv, o) c -- sv, 1; o, 2; c, 3
   or
   foreach (sv, si, o) c -- sv, 1; o, 2; c, 3; si, 4 */
{
    mpdm_t sv = RF(M1);
    mpdm_t o  = RF(M2);
    mpdm_t si = RF(M4);
    mpdm_t r = NULL;
    int64_t n = 0;
    mpdm_t v, i;
    mpdm_t *ip = NULL;

    if (si)
        ip = &i;

    while (mpdm_iterator(o, &n, &v, ip) && *f == FLOW_RUNNING) {
        SET(sv, v);
        if (ip) SET(si, *ip);
        UF(r);
        r = RF(M3);
    }

    if (*f == FLOW_BREAK)
        *f = FLOW_RUNNING;

    UF(si);
    UF(sv);
    UF(o);

    return UFND(r);
}


O_TYPE O_list(O_ARGS)
/* build list from instructions */
{
    mpdm_t ret = RF(mpdm_size(c) <= 2 ? MPDM_A(0) : M(2));

    if (mpdm_size(c) > 1)
        mpdm_push(ret, M(1));

    return UFND(ret);
}


O_TYPE O_ilist(O_ARGS)
/* build and inverse list from instructions */
{
    mpdm_t ret = RF(mpdm_size(c) <= 2 ? MPDM_A(0) : M(2));

    if (mpdm_size(c) > 1)
        mpdm_ins(ret, M(1), 0);

    return UFND(ret);
}


O_TYPE O_object(O_ARGS)
/* build object from instructions */
{
    mpdm_t ret = RF(mpdm_size(c) <= 3 ? MPDM_O() : M(3));

    if (mpdm_size(c) > 2)
        mpdm_set(ret, M2, M1);

    return UFND(ret);
}


O_TYPE O_blkframe(O_ARGS)
/* runs an instruction under a block frame */
{
    mpdm_t ret;

    /* no context? create one */
    if (l == NULL)
        l = MPDM_A(0);

    RF(l);

    /* create a new local symbol table */
    mpdm_push(l, MPDM_O());

    /* creates the arguments (if any) as local variables */
    set_local_symbols(M2, a, l);

    /* execute instruction */
    ret = RF(M1);

    /* destroy the local symbol table */
    mpdm_del_i(l, -1);

    UF(l);

    return UFND(ret);
}


O_TYPE O_subframe(O_ARGS)
/* runs an instruction inside a subroutine frame */
{
    /* like a block frame, but with its own symbol table */
    return O_blkframe(c, a, MPDM_A(0), f);
}


O_TYPE O_fmt(O_ARGS)
{
    return mpdm_fmt(M2, M1);
}


static struct mpsl_op_s {
    wchar_t *name;
    int foldable;
     mpdm_t(*func) (O_ARGS);
} op_table[] = {
    { L"LITERAL",   0, O_literal },  /* *must* be the zeroth */
    { L"MULTI",     0, O_multi },
    { L"SYMVAL",    0, O_symval },
    { L"ASSIGN",    0, O_assign },
    { L"EXECSYM",   0, O_execsym },
    { L"METHOD",    0, O_method },
    { L"IF",        0, O_if },
    { L"WHILE",     0, O_while },
    { L"FOREACH",   0, O_foreach },
    { L"SUBFRAME",  0, O_subframe },
    { L"BLKFRAME",  0, O_blkframe },
    { L"BREAK",     0, O_break },
    { L"RETURN",    0, O_return },
    { L"LOCAL",     0, O_local },
    { L"GLOBAL",    0, O_global },
    { L"LIST",      1, O_list },
    { L"ILIST",     1, O_ilist },
    { L"OBJECT",    1, O_object },
    { L"ADD",       1, O_add },
    { L"SUB",       1, O_sub },
    { L"MUL",       1, O_mul },
    { L"DIV",       1, O_div },
    { L"MOD",       1, O_mod },
    { L"NOT",       1, O_not },
    { L"AND",       1, O_and },
    { L"OR",        1, O_or },
    { L"EQ",        1, O_eq },
    { L"LT",        1, O_lt },
    { L"LE",        1, O_le },
    { L"GT",        1, O_gt },
    { L"GE",        1, O_ge },
    { L"FMT",       1, O_fmt },
    { NULL,         0, NULL }
};


O_TYPE mpsl_exec_i(O_ARGS)
/* Executes one MPSL instruction in the MPSL virtual machine. Called
   from mpsl_exec_p() (which holds the flow control status variable) */
{
    mpdm_t ret = NULL;

    mpdm_ref(c);
    mpdm_ref(a);
    mpdm_ref(l);

    /* if aborted or NULL, do nothing */
    if (!mpsl_abort && c != NULL) {
        /* gets the opcode and calls it */
        ret = op_table[mpdm_ival(C0)].func(c, a, l, f);

        if (mpsl_trap_func != NULL) {
            mpdm_t func = mpsl_trap_func;

            mpdm_ref(ret);

            mpsl_trap_func = NULL;
            mpdm_exec_3(func, c, a, ret, l);
            mpsl_trap_func = func;

            mpdm_unrefnd(ret);
        }
    }

    mpdm_ref(ret);

    mpdm_unref(l);
    mpdm_unref(a);
    mpdm_unref(c);

    mpdm_unrefnd(ret);

    return ret;
}


mpdm_t mpsl_exec_p(mpdm_t c, mpdm_t a, mpdm_t ctxt)
/* executes an MPSL instruction stream */
{
    mpsl_flow_t f = FLOW_RUNNING;

    /* execute first instruction with a new flow control variable */
    return mpsl_exec_i(c, a, ctxt, &f);
}


mpdm_t mpsl_constant_fold(mpdm_t i)
/* tries to fold complex but constant instructions into literals */
{
    int op;

    if ((op = mpdm_ival(mpdm_get_i(i, 0))) != 0) {
        int n, nl = 0;

        /* constant fold recursively all arguments */
        for (n = 1; n < mpdm_size(i); n++) {
            mpdm_t v;

            v = mpdm_get_i(i, n);
            v = mpdm_set_i(i, mpsl_constant_fold(v), n);

            /* count non-literals */
            if (mpdm_ival(mpdm_get_i(v, 0)) != 0)
                nl++;
        }

        if (nl == 0 && op_table[op].foldable) {
            /* if it does not contain non-literals,
               execute and convert to LITERAL */
            i = mpsl_exec_p(i, NULL, NULL);
            i = mpsl_mkins(L"LITERAL", 1, i, NULL, NULL, NULL);
        }
    }

    return i;
}


mpdm_t mpsl_mkins(wchar_t * opcode, int args, mpdm_t a1, mpdm_t a2,
                  mpdm_t a3, mpdm_t a4)
/* creates an instruction */
{
    mpdm_t o;
    mpdm_t v;

    v = MPDM_A(args + 1);

    /* inserts the opcode */
    o = mpdm_get_wcs(mpsl_opcodes, opcode);
    mpdm_set_i(v, o, 0);

    switch (args) {
    case 4: mpdm_set_i(v, a4, 4); /* no break */
    case 3: mpdm_set_i(v, a3, 3); /* no break */
    case 2: mpdm_set_i(v, a2, 2); /* no break */
    case 1: mpdm_set_i(v, a1, 1); /* no break */
    }

    return v;
}


mpdm_t mpsl_x(mpdm_t a1, mpdm_t a2, int sf)
/* creates a MPDM 'program' with the MPSL executor as the first
   argument, a compiled stream as the second and optional arguments */
{
    return MPDM_X2(mpsl_exec_p,
                   mpsl_mkins(sf ? L"SUBFRAME" : L"BLKFRAME",
                              a2 == NULL ? 1 : 2, a1, a2, NULL, NULL));
}


static mpdm_t build_opcodes(void)
/* builds the table of opcodes */
{
    int n;

    mpsl_opcodes = mpdm_ref(MPDM_O());

    for (n = 0; op_table[n].name != NULL; n++)
        mpdm_set(mpsl_opcodes, MPDM_I(n), MPDM_S(op_table[n].name));

    return mpsl_opcodes;
}


static mpdm_t inc_fopen(mpdm_t filename, mpdm_t inc)
/* opens filename searching in INC */
{
    mpdm_t r = NULL;
    int n;

    mpdm_ref(filename);

    /* if INC is NULL, try a direct open */
    if (inc == NULL) {
        r = mpdm_open(filename, MPDM_S(L"r"));
    }
    else {
        /* loop through INC, prepending each path to the filename */
        for (n = 0; r == NULL && n < mpdm_size(inc); n++) {
            wchar_t *t;
            mpdm_t v = mpdm_get_i(inc, n);

            if (mpdm_can_exec(v)) {
                /* executable: call with filename as argument */
                r = mpdm_exec_1(v, filename, NULL);
            }
            else
            if ((t = wcsrchr(mpdm_string(v), L'.')) &&
                    (wcscmp(t, L".tar") == 0 || wcscmp(t, L".zip") == 0)) {
                /* it's a tarfile; find filename inside it */
                mpdm_t f;

                if ((f = mpdm_open(v, MPDM_S(L"r"))) != NULL) {
                    r = mpdm_read_arch_file_s(filename, f);
                    mpdm_close(f);
                }
            }
            else {
                r = mpdm_open(mpdm_strcat(v,
                        mpdm_strcat(MPDM_S(L"/"), filename)), MPDM_S(L"r"));
            }
        }
    }

    mpdm_unref(filename);

    return r;
}


static mpdm_t call_compiler(mpdm_t code, mpdm_t source)
{
    mpdm_t v, c;

    v = mpdm_get_wcs(mpdm_root(), L"MPSL");
    c = mpdm_get_wcs(v, L"constant_fold");
    v = mpdm_get_wcs(v, L"compiler");

    /* call the compiler */
    v = mpdm_exec_2(v, code, source, NULL);

    /* if code was generated, do constant folding */
    if (v && mpdm_is_true(c))
        mpdm_set_i(v, mpsl_constant_fold(mpdm_get_i(v, 1)), 1);

    return v;
}


/**
 * mpsl_compile - Compiles a string of MPSL code.
 * @code: A value containing a string of MPSL code
 *
 * Compiles a string of MPSL code and returns an mpdm value executable
 * by mpdm_exec(). If there is a syntax (or other type) error, NULL
 * is returned instead.
 */
mpdm_t mpsl_compile(mpdm_t code, mpdm_t source)
{
    if (source == NULL)
        source = MPDM_S(L"<INLINE>");

    return call_compiler(code, source);
}


/**
 * mpsl_compile_file - Compiles a file of MPSL code.
 * @file: File stream or file name.
 * @inc: search path for source files.
 *
 * Compiles a source file of MPSL code and returns an mpdm value
 * executable by mpdm_exec(). If @file is an MPSL file descriptor,
 * it's read and compiled; otherwise, it's assumed to be a
 * file name, that will be searched for in any of the paths defined
 * in the @inc array. If the file cannot be found
 * or there is any other error, NULL is returned instead.
 */
mpdm_t mpsl_compile_file(mpdm_t file, mpdm_t inc)
{
    mpdm_t x = NULL;

    mpdm_ref(file);
    mpdm_ref(inc);

    if (mpdm_type(file) == MPDM_TYPE_FILE) {
        x = call_compiler(file, MPDM_S(L"<FILE>"));
    }
    else {
        mpdm_t v = NULL;

        if ((v = inc_fopen(file, inc)) == NULL)
            mpsl_error(mpdm_strcat(file, MPDM_S(L": file not found")));
        else {
            mpdm_ref(v);
            x = call_compiler(v, file);

            if (mpdm_type(v) == MPDM_TYPE_FILE)
                mpdm_close(v);

            mpdm_unref(v);
        }
    }

    mpdm_unref(inc);
    mpdm_unref(file);

    return x;
}


mpdm_t mpsl_resource(mpdm_t file, mpdm_t inc)
{
    return inc_fopen(file, inc);
}


/**
 * mpsl_eval - Evaluates MSPL code.
 * @code: A value containing a string of MPSL code, or executable code
 * @args: optional arguments for @code
 * @ctxt: context for @code
 *
 * Evaluates a piece of code. The @code can be a string containing MPSL source
 * code (that will be compiled) or a direct executable value. If the compilation
 * or the execution gives an error, the ERROR variable will be set to a printable
 * value and NULL returned. Otherwise, the exit value from the code is returned
 * and ERROR set to NULL. The abort flag is reset on exit.
 */
mpdm_t mpsl_eval(mpdm_t code, mpdm_t args, mpdm_t ctxt)
{
    mpdm_t cs, r;

    /* reset error */
    mpsl_error(NULL);
    mpsl_abort = 0;

    mpdm_ref(code);
    mpdm_ref(args);
    mpdm_ref(ctxt);

    /* if code is not executable, try to compile */
    if (!mpdm_can_exec(code))
        cs = mpsl_compile(code, NULL);
    else
        cs = code;

    /* execute, if possible */
    if (mpdm_can_exec(cs))
        r = mpdm_exec(cs, args, ctxt);
    else
        r = NULL;

    /* reset the abort flag */
    mpsl_abort = 0;

    mpdm_unref(ctxt);
    mpdm_unref(args);
    mpdm_unref(code);

    return r;
}


/**
 * mpsl_trap - Install a trapping function.
 * @trap_func: The trapping MPSL code
 *
 * Installs a trapping function. The function is an MPSL
 * executable value receiving 3 arguments: the code stream,
 * the arguments and the return value of the executed code.
 *
 * Returns NULL (previous versions returned the previous
 * trapping function).
 */
mpdm_t mpsl_trap(mpdm_t trap_func)
{
    mpdm_store(&mpsl_trap_func, trap_func);

    return NULL;
}


/**
 * mpsl_argv - Fills the ARGV global array.
 * @argc: number of arguments
 * @argv: array of string values
 *
 * Fills the ARGV global MPSL array with an array of arguments. These
 * are usually the ones sent to main().
 */
void mpsl_argv(int argc, char *argv[])
{
    int n;
    mpdm_t ARGV;

    /* create the ARGV array */
    ARGV = mpdm_set_wcs(mpdm_root(), MPDM_A(0), L"ARGV");

    for (n = 0; n < argc; n++)
        mpdm_push(ARGV, MPDM_MBS(argv[n]));
}


/* in mpsl_f.c */
mpdm_t mpsl_build_funcs(void);


extern char *mpsl_build_git_rev;
extern char *mpsl_build_timestamp;

/**
 * mpsl_startup - Initializes MPSL.
 *
 * Initializes the Minimum Profit Scripting Language. Returns 0 if
 * everything went OK.
 */
int mpsl_startup(void)
{
    mpdm_t r, m, v;

    /* startup MPDM */
    mpdm_startup();

    r = mpdm_root();

    /* creates INC, unless already defined */
    if (mpdm_get_wcs(r, L"INC") == NULL)
        mpdm_set_wcs(r, MPDM_A(0), L"INC");

    /* standard file descriptors */
    mpdm_set_wcs(r, MPDM_F(stdin),  L"STDIN");
    mpdm_set_wcs(r, MPDM_F(stdout), L"STDOUT");
    mpdm_set_wcs(r, MPDM_F(stderr), L"STDERR");

    /* home and application directories */
    mpdm_set_wcs(r, mpdm_home_dir(), L"HOMEDIR");
    mpdm_set_wcs(r, mpdm_app_dir(),  L"APPDIR");
    mpdm_set_wcs(r, mpdm_conf_dir(),  L"CONFDIR");

    /* fill now the MPSL object */
    m = mpdm_set_wcs(r, MPDM_O(), L"MPSL");

    /* store things there */
    mpdm_set_wcs(m, MPDM_MBS(VERSION),              L"VERSION");
    mpdm_set_wcs(m, build_opcodes(),                L"OPCODE");
    mpdm_set_wcs(m, MPDM_O(),                       L"LC");
    mpdm_set_wcs(m, mpsl_build_funcs(),             L"CORE");
    mpdm_set_wcs(m, MPDM_I(1),                      L"constant_fold");

#ifndef CONFOPT_WITHOUT_C_MPSL
    /* classic compiler */
    v = MPDM_X(mpsl_c_compiler_x);
    mpdm_set_wcs(m, v, L"c_mpsl");
#endif

    /* ad-hoc compiler */
    v = MPDM_X(mpsl_a_compiler_x);
    mpdm_set_wcs(m, v, L"a_mpsl");

    /* set the default compiler */
    mpdm_set_wcs(m, mpdm_get_wcs(m, L"" CONFOPT_DEFAULT_COMPILER), L"compiler");

    mpdm_dump_1 = mpsl_dump_1;

    return 0;
}


int mpsl_bootstrap(int argc, char *argv[], const char *code, int size)
{
    int r = 0;
    mpdm_t v;

    mpsl_startup();

    mpsl_argv(argc, argv);

    if ((v = mpsl_compile(MPDM_NMBS(code, size), NULL)) != NULL) {
        mpdm_void(mpdm_exec(v, NULL, NULL));
    }

    /* prints the error, if any */
    if ((v = mpdm_get_wcs(mpdm_root(), L"ERROR")) != NULL) {
        mpdm_write_wcs(stderr, mpdm_string(v));
        fprintf(stderr, "\n");

        r = 1;
    }

    mpsl_shutdown();

    return r;
}


/**
 * mpsl_shutdown - Shuts down MPSL.
 *
 * Shuts down MPSL. No MPSL functions should be used from now on.
 */
void mpsl_shutdown(void)
{
    mpdm_shutdown();
}
