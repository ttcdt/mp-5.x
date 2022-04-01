/*

    MPDM - Minimum Profit Data Manager
    mpdm_x.c - Extended functions

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

#include "mpdm.h"

/** code **/

mpdm_t mpdm_new_x(mpdm_type_t type, const void *f, mpdm_t a)
{
    mpdm_t r = NULL;

    switch (type) {
    case MPDM_TYPE_FUNCTION:
        r = mpdm_new(type, f, 0);
        break;

    case MPDM_TYPE_PROGRAM:
        r = mpdm_new(type, NULL, 0);

        mpdm_push(r, MPDM_X(f));
        mpdm_push(r, a);

        break;

    default:
        r = NULL;
    }

    return r;
}


/**
 * mpdm_is_true - Returns 1 if a value is true.
 * @v: the value
 *
 * Returns 1 if @v is true. False values are: NULL, integers
 * with value 0, reals with value 0.0, empty strings and the
 * special string "0".
 * The reference count is touched.
 */
int mpdm_is_true(mpdm_t v)
{
    int r;

    mpdm_ref(v);
    r = mpdm_type_vc(v)->is_true(v);
    mpdm_unref(v);

    return r;
}


/**
 * mpdm_bool - Returns a boolean value.
 * @b: true or false
 *
 * Returns the stored values for TRUE or FALSE.
 */
mpdm_t mpdm_bool(int b)
{
    return mpdm_get_wcs(mpdm_root(), b ? L"TRUE" : L"FALSE");
}


int mpdm_count(mpdm_t v)
{
    return mpdm_type_vc(v)->count(v);
}


/**
 * mpdm_get_i - Gets an element by integer subscript.
 * @s: the set
 * @index: the subscript of the element
 *
 * Returns the element at @index of the set @s.
 */
mpdm_t mpdm_get_i(const mpdm_t s, int index)
{
    return mpdm_type_vc(s)->get_i(s, index);
}


/**
 * mpdm_get - Gets an element by index.
 * @s: the set
 * @index: the index
 *
 * Returns the element at @index of the set @s.
 */
mpdm_t mpdm_get(const mpdm_t s, mpdm_t index)
{
    return mpdm_type_vc(s)->get(s, index);
}


/**
 * mpdm_del_i - Deletes an element by integer subscript.
 * @s: the set
 * @index: subscript of the element to be deleted
 *
 * Deletes the element at @index of the @s set.
 */
mpdm_t mpdm_del_i(const mpdm_t s, int index)
{
    return mpdm_type_vc(s)->del_i(s, index);
}


mpdm_t mpdm_del(const mpdm_t s, mpdm_t index)
{
    return mpdm_type_vc(s)->del(s, index);
}


mpdm_t mpdm_set_i(mpdm_t s, mpdm_t e, int index)
{
    return mpdm_type_vc(s)->set_i(s, e, index);
}


mpdm_t mpdm_set(mpdm_t s, mpdm_t e, mpdm_t index)
{
    return mpdm_type_vc(s)->set(s, e, index);
}


int mpdm_can_exec(mpdm_t v)
{
    return mpdm_type_vc(v)->can_exec(v);
}


/**
 * mpdm_exec - Executes an executable value.
 * @c: the code value
 * @args: the arguments
 * @ctxt: the context
 *
 * Executes an executable value. If @c is a scalar value, its data
 * should be a pointer to a directly executable C function with a
 * prototype of mpdm_t func(mpdm_t args, mpdm_t ctxt); if it's a multiple
 * one, the first value's data should be a pointer to a directly executable
 * C function with a prototype of
 * mpdm_t func(mpdm_t b, mpdm_t args, mpdm_t ctxt) and
 * the second value will be passed as the @b argument. This value is used
 * to store bytecode or so when implementing virtual machines or compilers.
 * The @ctxt is meant to be used as a special context to implement local
 * symbol tables and such. Its meaning is free and can be NULL.
 * If @c is a file descriptor, a line is read from it if the call has no
 * arguments or otherwise all of them are written into it.
 *
 * Returns the return value of the code. If @c is NULL or not executable,
 * returns NULL.
 * [Value Management]
 */
mpdm_t mpdm_exec(mpdm_t c, mpdm_t args, mpdm_t ctxt)
{
    mpdm_t r = NULL;

    mpdm_ref(c);
    mpdm_ref(args);
    mpdm_ref(ctxt);

    r = mpdm_type_vc(c)->exec(c, args, ctxt);

    mpdm_ref(r);

    mpdm_unref(ctxt);
    mpdm_unref(args);
    mpdm_unref(c);

    return mpdm_unrefnd(r);
}


mpdm_t mpdm_exec_1(mpdm_t c, mpdm_t a1, mpdm_t ctxt)
{
    mpdm_t a = MPDM_A(1);

    mpdm_set_i(a, a1, 0);

    return mpdm_exec(c, a, ctxt);
}


mpdm_t mpdm_exec_2(mpdm_t c, mpdm_t a1, mpdm_t a2, mpdm_t ctxt)
{
    mpdm_t a = MPDM_A(2);

    mpdm_set_i(a, a1, 0);
    mpdm_set_i(a, a2, 1);

    return mpdm_exec(c, a, ctxt);
}


mpdm_t mpdm_exec_3(mpdm_t c, mpdm_t a1, mpdm_t a2, mpdm_t a3, mpdm_t ctxt)
{
    mpdm_t a = MPDM_A(3);

    mpdm_set_i(a, a1, 0);
    mpdm_set_i(a, a2, 1);
    mpdm_set_i(a, a3, 2);

    return mpdm_exec(c, a, ctxt);
}


/**
 * mpdm_iterator - Iterates through the content of a set.
 * @set: the set (hash, array, file or scalar)
 * @context: A pointer to an opaque context
 * @v: a pointer to a value to store the key
 * @i: a pointer to a value to store the index
 *
 * Iterates through the @set. If it's a hash, every value/index pair
 * is returned on each call. If it's an array, @v contains the
 * element and @i the index number on each call. If it's a file,
 * @v contains the line read and @i the index number. Otherwise, it's
 * assumed to be a string containing a numeral and @v and @i are filled
 * with values from 0 to @set - 1 on each call.
 *
 * Any of @v and @i pointers can be NULL if the value is not of interest.
 *
 * The @context pointer to integer is opaque and should be
 * initialized to zero on the first call.
 *
 * Returns 0 if no more data is left in @set.
 * [Hashes]
 * [Arrays]
 */
int mpdm_iterator(mpdm_t set, int64_t *context, mpdm_t *v, mpdm_t *i)
{
    int ret = 0;

    mpdm_ref(set);
    ret = mpdm_type_vc(set)->iterator(set, context, v, i);
    mpdm_unrefnd(set);

    return ret;
}


/**
 * mpdm_clone - Creates a clone of a value.
 * @v: the value
 *
 * Creates a clone of a value. If the value is multiple, a new value will
 * be created containing clones of all its elements; otherwise,
 * the same unchanged value is returned.
 * [Value Management]
 */
mpdm_t mpdm_clone(mpdm_t v)
{
    return mpdm_type_vc(v)->clone(v);
}



mpdm_t vc_default_map(mpdm_t set, mpdm_t filter, mpdm_t ctxt)
{
    mpdm_t r = MPDM_A(0);
    int64_t n = 0;
    mpdm_t v, i;

    while (mpdm_iterator(set, &n, &v, &i)) {
        mpdm_t w = NULL;
        mpdm_ref(v);
        mpdm_ref(i);

        switch (mpdm_type(filter)) {
        case MPDM_TYPE_FUNCTION:
        case MPDM_TYPE_PROGRAM:
            w = mpdm_exec_2(filter, v, i, ctxt);
            break;

        case MPDM_TYPE_ARRAY:
        case MPDM_TYPE_OBJECT:
            w = mpdm_get(filter, v);
            break;

        case MPDM_TYPE_REGEX:
            w = mpdm_regex(v, filter, 0);
            break;

        case MPDM_TYPE_STRING:
            w = mpdm_fmt(filter, v);
            break;

        default:
            w = v;
            break;
        }

        mpdm_push(r, w);

        mpdm_unref(i);
        mpdm_unref(v);
    }

    return r;
}


mpdm_t mpdm_map(mpdm_t set, mpdm_t filter, mpdm_t ctxt)
{
    mpdm_t r;

    mpdm_ref(set);
    mpdm_ref(filter);
    mpdm_ref(ctxt);

    r = mpdm_type_vc(set)->map(set, filter, ctxt);

    mpdm_unref(ctxt);
    mpdm_unref(filter);
    mpdm_unref(set);

    return r;
}


mpdm_t mpdm_omap(mpdm_t set, mpdm_t filter, mpdm_t ctxt)
{
    mpdm_t v, i;
    mpdm_t out = MPDM_O();
    int64_t n = 0;

    mpdm_ref(set);
    mpdm_ref(filter);
    mpdm_ref(ctxt);

    out = MPDM_O();

    while (mpdm_iterator(set, &n, &v, &i)) {
        mpdm_t w = NULL;
        mpdm_ref(i);
        mpdm_ref(v);

        switch (mpdm_type(filter)) {
        case MPDM_TYPE_NULL:
            /* invert hash */
            w = MPDM_A(2);
            mpdm_set_i(w, v, 0);
            mpdm_set_i(w, i, 1);

            break;

        case MPDM_TYPE_FUNCTION:
        case MPDM_TYPE_PROGRAM:
            w = mpdm_exec_2(filter, v, i, ctxt);
            break;

        case MPDM_TYPE_ARRAY:
            /* the set provides the values,
               the filter array provides the indexes */
            w = MPDM_A(2);
            mpdm_set_i(w, v, 0);
            mpdm_set_i(w, mpdm_get(filter, i), 1);

            break;

        case MPDM_TYPE_OBJECT:
            w = mpdm_get(filter, v);
            break;

        case MPDM_TYPE_REGEX:
            w = mpdm_regex(v, filter, 0);
            break;

        case MPDM_TYPE_STRING:
            w = mpdm_fmt(filter, v);
            break;

        default:
            break;
        }

        mpdm_ref(w);

        /* if the filtered value is an array, it's a value/index pair */
        if (mpdm_type(w) == MPDM_TYPE_ARRAY)
            mpdm_set(out, mpdm_get_i(w, 0), mpdm_get_i(w, 1));
        else
            mpdm_set(out, w, i);

        mpdm_unref(w);
        mpdm_unref(v);
        mpdm_unref(i);
    }

    mpdm_unref(ctxt);
    mpdm_unref(filter);
    mpdm_unref(set);

    return out;
}


mpdm_t mpdm_grep(mpdm_t set, mpdm_t filter, mpdm_t ctxt)
{
    mpdm_t out = NULL;

    mpdm_ref(set);
    mpdm_ref(filter);
    mpdm_ref(ctxt);

    if (set != NULL) {
        mpdm_t v, i;
        int64_t n = 0;

        out = mpdm_type(set) == MPDM_TYPE_OBJECT ? MPDM_O() : MPDM_A(0);

        while (mpdm_iterator(set, &n, &v, &i)) {
            mpdm_t w = NULL;
            mpdm_ref(i);
            mpdm_ref(v);

            switch (mpdm_type(filter)) {
            case MPDM_TYPE_FUNCTION:
            case MPDM_TYPE_PROGRAM:
                w = mpdm_exec_2(filter, v, i, ctxt);
                break;

            case MPDM_TYPE_REGEX:
            case MPDM_TYPE_STRING:
                w = mpdm_regex(v, filter, 0);
                break;

            case MPDM_TYPE_OBJECT:
                w = mpdm_bool(mpdm_exists(filter, v));
                break;

            default:
                break;
            }

            if (mpdm_is_true(w)) {
                if (mpdm_type(out) == MPDM_TYPE_OBJECT)
                    mpdm_set(out, v, i);
                else
                    mpdm_push(out, v);
            }

            mpdm_unref(v);
            mpdm_unref(i);
        }
    }

    mpdm_unref(ctxt);
    mpdm_unref(filter);
    mpdm_unref(set);

    return out;
}


/**
 * mpdm_join - Joins two values.
 * @a: first value
 * @b: second value
 *
 * Joins two values. If both are hashes, a new hash containing the
 * pairs in @a overwritten with the keys in @b is returned; if both
 * are arrays, a new array is returned with all elements in @a followed
 * by all elements in b; if @a is an array and @b is a string,
 * a new string is returned with all elements in @a joined using @b
 * as a separator; and if @a is a hash and @b is a string, a new array
 * is returned containing all pairs in @a joined using @b as a separator.
 * [Arrays]
 * [Hashes]
 * [Strings]
 */
mpdm_t mpdm_join(const mpdm_t a, const mpdm_t b)
{
    int64_t n;
    int c = 0;
    mpdm_t r, v, i;

    mpdm_ref(a);
    mpdm_ref(b);

    switch (mpdm_type(a)) {
    case MPDM_TYPE_OBJECT:

        switch (mpdm_type(b)) {
        case MPDM_TYPE_OBJECT:
            /* hash~hash -> hash */
            r = MPDM_O();

            n = 0;
            while (mpdm_iterator(a, &n, &v, &i))
                mpdm_set(r, v, i);
            n = 0;
            while (mpdm_iterator(b, &n, &v, &i))
                mpdm_set(r, v, i);

            break;

        case MPDM_TYPE_ARRAY:
            /* hash~array -> hash */
            r = MPDM_O();

            /* the array is a list of pairs */
            for (n = 0; n < mpdm_size(b); n += 2)
                mpdm_set(r, mpdm_get_i(b, n + 1), mpdm_get_i(b, n));

            break;

        case MPDM_TYPE_STRING:
            /* hash~string -> array */
            r = MPDM_A(mpdm_count(a));

            n = 0;
            while (mpdm_iterator(a, &n, &v, &i))
                mpdm_set_i(r, mpdm_strcat(i, mpdm_strcat(b, v)), c++);

            break;

        default:
            r = NULL;
            break;
        }

        break;

    case MPDM_TYPE_ARRAY:
    case MPDM_TYPE_FILE:
        switch (mpdm_type(b)) {
        case MPDM_TYPE_ARRAY:
        case MPDM_TYPE_FILE:
            /* array~array -> array */
            r = MPDM_A(0);

            n = 0;
            while (mpdm_iterator(a, &n, &v, NULL))
                mpdm_push(r, v);
            n = 0;
            while (mpdm_iterator(b, &n, &v, NULL))
                mpdm_push(r, v);

            break;

        case MPDM_TYPE_STRING:
        case MPDM_TYPE_NULL:
            /* array~string -> string */
            r = mpdm_join_wcs(a, b ? mpdm_string(b) : NULL);

            break;

        default:
            r = NULL;
            break;
        }

        break;

    case MPDM_TYPE_STRING:
        /* string~string -> string */
        r = mpdm_strcat(a, b);
        break;

    case MPDM_TYPE_INTEGER:
    case MPDM_TYPE_REAL:
        /* real~real -> sum! */
        r = MPDM_R(mpdm_rval(a) + mpdm_rval(b));
        break;

    default:
        r = NULL;
        break;
    }

    mpdm_unref(b);
    mpdm_unref(a);

    return r;
}


mpdm_t mpdm_splice_a(const mpdm_t v, const mpdm_t i,
                     int offset, int del, mpdm_t *n, mpdm_t *d);
mpdm_t mpdm_splice_s(const mpdm_t v, const mpdm_t i,
                     int offset, int del, mpdm_t *n, mpdm_t *d);

mpdm_t mpdm_splice(const mpdm_t v, const mpdm_t i,
                   int offset, int del, mpdm_t *n, mpdm_t *d)
{
    mpdm_t r;

    switch (mpdm_type(v)) {
    case MPDM_TYPE_NULL:
    case MPDM_TYPE_STRING:
        r = mpdm_splice_s(v, i, offset, del, n, d);
        break;

    case MPDM_TYPE_ARRAY:
        r = mpdm_splice_a(v, i, offset, del, n, d);
        break;

    default:
        r = NULL;
        break;
    }

    return r;
}


/**
 * mpdm_cmp - Compares two values.
 * @v1: the first value
 * @v2: the second value
 *
 * Compares two values. If both has the MPDM_STRING flag set,
 * a comparison using wcscoll() is returned; if both are arrays,
 * the size is compared first and, if they have the same number
 * elements, each one is compared; otherwise, a simple visual
 * representation comparison is done.
 * [Strings]
 */
int mpdm_cmp(const mpdm_t v1, const mpdm_t v2)
{
    int r;

    mpdm_ref(v1);
    mpdm_ref(v2);

    /* same values? */
    if (v1 == v2)
        r = 0;
    else
    if (v1 == NULL)
        r = -1;
    else
    if (v2 == NULL)
        r = 1;
    else {
        switch (mpdm_type(v1)) {
        case MPDM_TYPE_NULL:
            r = -1;
            break;

        case MPDM_TYPE_INTEGER:
            r = mpdm_ival(v1) - mpdm_ival(v2);
            break;

        case MPDM_TYPE_REAL:
            {
                double d = mpdm_rval(v1) - mpdm_rval(v2);
                r = d < 0.0 ? -1 : d > 0.0 ? 1 : 0;
            }
            break;

        case MPDM_TYPE_ARRAY:
        case MPDM_TYPE_OBJECT:
        case MPDM_TYPE_PROGRAM:

            if (mpdm_type(v2) == mpdm_type(v1)) {
                /* if they are the same size, compare elements one by one */
                if ((r = mpdm_size(v1) - mpdm_size(v2)) == 0) {
                    int64_t n = 0;
                    mpdm_t v, i;

                    while (mpdm_iterator(v1, &n, &v, &i)) {
                        if ((r = mpdm_cmp(v, mpdm_get(v2, i))) != 0)
                            break;
                    }
                }

                break;
            }

            /* fallthrough */

        default:
            r = mpdm_cmp_wcs(v1, v2 ? mpdm_string(v2) : NULL);
            break;
        }
    }

    mpdm_unref(v2);
    mpdm_unref(v1);

    return r;
}


mpdm_t mpdm_multiply(mpdm_t v, mpdm_t i)
{
    mpdm_t r = NULL;

    switch (mpdm_type(v)) {
    case MPDM_TYPE_INTEGER:
    case MPDM_TYPE_REAL:
        r = MPDM_R(mpdm_rval(v) * mpdm_rval(i));
        break;

    case MPDM_TYPE_STRING:
        /* replicate string */
        {
            int n = mpdm_ival(i);
            wchar_t *ptr = NULL;
            int z = 0;

            while (n) {
                ptr = mpdm_pokev(ptr, &z, v);
                n--;
            }

            r = MPDM_NS(ptr, z);
        }

        break;

    case MPDM_TYPE_ARRAY:
        /* replicate an array */
        {
            int m, n, c;

            c = mpdm_ival(i);
            r = MPDM_A(c * mpdm_size(v));

            for (n = 0; n < mpdm_size(v); n++) {
                mpdm_t w = mpdm_get_i(v, n);

                for (m = 0; m < c; m++)
                    mpdm_set_i(r, w, m * mpdm_size(v) + n);
            }
        }

        break;

    default:
        break;
    }

    return r;
}


mpdm_t mpdm_substract(mpdm_t m, mpdm_t s)
{
    int64_t n = 0;
    mpdm_t v, i;
    mpdm_t r = NULL;

    switch (mpdm_type(m)) {
    case MPDM_TYPE_INTEGER:
    case MPDM_TYPE_REAL:
    case MPDM_TYPE_STRING:
        r = MPDM_R(mpdm_rval(m) - mpdm_rval(s));
        break;

    case MPDM_TYPE_ARRAY:
        switch (mpdm_type(s)) {
        case MPDM_TYPE_ARRAY:
            r = MPDM_A(0);

            for (n = 0; n < mpdm_size(m); n++) {
                mpdm_t w = mpdm_get_i(m, n);

                if (mpdm_seek(s, w, 1) == -1)
                    mpdm_push(r, w);
            }

            break;

        case MPDM_TYPE_OBJECT:
            r = MPDM_A(0);

            for (n = 0; n < mpdm_size(m); n++) {
                mpdm_t w = mpdm_get_i(m, n);

                if (!mpdm_exists(s, w))
                    mpdm_push(r, w);
            }

            break;

        default:
            break;
        }

        break;

    case MPDM_TYPE_OBJECT:
        switch (mpdm_type(s)) {
        case MPDM_TYPE_ARRAY:
            r = MPDM_O();

            n = 0;
            while (mpdm_iterator(m, &n, &v, &i)) {
                if (mpdm_seek(s, i, 1) == -1)
                    mpdm_set(r, v, i);
            }

            break;

        case MPDM_TYPE_OBJECT:
            r = MPDM_O();

            n = 0;
            while (mpdm_iterator(m, &n, &v, &i)) {
                if (!mpdm_exists(s, i))
                    mpdm_set(r, v, i);
            }

            break;

        default:
            break;
        }

        break;

    default:
        break;
    }

    return r;
}


mpdm_t mpdm_divide(mpdm_t num, mpdm_t den)
{
    mpdm_t r = NULL;

    switch (mpdm_type(num)) {
    case MPDM_TYPE_INTEGER:
    case MPDM_TYPE_REAL:
    {
        double d;

        if ((d = mpdm_rval(den)) != 0.0)
            r = MPDM_R(mpdm_rval(num) / d);

        break;
    }

    case MPDM_TYPE_STRING:
        r = mpdm_split(num, den);
        break;

    default:
        break;
    }

    return r;
}
