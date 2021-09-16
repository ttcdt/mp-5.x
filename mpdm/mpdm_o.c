/*

    MPDM - Minimum Profit Data Manager
    mpdm_o.c - Object management

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

/** code **/

/* default hash buckets (must be prime) */
int mpdm_hash_buckets = 31;

/* prototype for the one-time wrapper hash function */
static int switch_hash_func(const wchar_t *, int);

int standard_hash_func(const wchar_t *string, int mod)
/* computes a hashing function on string */
{
    int c;

    for (c = 0; *string != L'\0' && ! (c & 0x1000); string++)
        c = (c << 1) ^ (int) *string;

    return c % mod;
}

#define FNV_OFFSET  2166136261
#define FNV_PRIME   16777619

int fnv_hash_func(const wchar_t *string, int mod)
/* https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function */
{
    unsigned int c = FNV_OFFSET;
 
    while (*string) {
        c ^= *string++;
        c *= FNV_PRIME;
    }

    return c % mod;
}

int null_hash_func(const wchar_t *string, int mod)
{
    return *string % mod;
}

/* pointer to the hashing function */
static int (*mpdm_hash_func) (const wchar_t *, int) = switch_hash_func;

static int switch_hash_func(const wchar_t *string, int mod)
/* one-time wrapper for hash method autodetection */
{
    char *hash = getenv("MPDM_HASH_FUNC");

    /* select the hashing function */
    if (hash == NULL || strcmp(hash, "std") == 0)
        mpdm_hash_func = standard_hash_func;
    else
    if (strcmp(hash, "fnv") == 0)
        mpdm_hash_func = fnv_hash_func;
    else
    if (strcmp(hash, "null") == 0)
        mpdm_hash_func = null_hash_func;

    /* and fall back to it */
    return mpdm_hash_func(string, mod);
}

#define HASH_BUCKET_S(h, k) mpdm_hash_func(k, mpdm_size(h))
#define HASH_BUCKET(h, k)   mpdm_hash_func(mpdm_string(k), mpdm_size(h))

/* interface */


mpdm_t mpdm_new_o(void)
/* creates a new object */
{
    return mpdm_new(MPDM_TYPE_OBJECT, NULL, 0);
}


/**
 * mpdm_get_wcs - Gets the value from an object by string index (string version).
 * @o: the object
 * @i: the index
 *
 * Returns the value from @o by index @i, or NULL if there is no
 * value addressable by that index.
 * [Objects]
 */
mpdm_t mpdm_get_wcs(const mpdm_t o, const wchar_t *i)
{
    mpdm_t b;
    mpdm_t v = NULL;
    int n = 0;

    if (mpdm_size(o)) {
        /* if hash is not empty... */
        if ((b = mpdm_get_i(o, HASH_BUCKET_S(o, i))) != NULL)
            n = mpdm_bseek_wcs(b, i, 2, NULL);

        if (n >= 0)
            v = mpdm_get_i(b, n + 1);
    }

    return v;
}


/**
 * mpdm_exists - Tests if there is a value available by index.
 * @o: the object
 * @i: the index
 *
 * Returns 1 if exists a value indexable by @i in @h, or 0 othersize.
 * [Hashes]
 */
int mpdm_exists(const mpdm_t o, const mpdm_t i)
{
    mpdm_t b;
    int ret = 0;

    mpdm_ref(i);

    if (mpdm_size(o)) {
        if ((b = mpdm_get_i(o, HASH_BUCKET(o, i))) != NULL) {
            /* if bucket exists, binary-seek it */
            if (mpdm_bseek(b, i, 2, NULL) >= 0)
                ret = 1;
        }
    }

    mpdm_unref(i);

    return ret;
}


/**
 * mpdm_set_wcs - Sets a value in an object (string version).
 * @o: the object
 * @v: the value
 * @i: the index
 *
 * Sets the value @v inside the object @o, accesible by index @i.
 * Returns @v.
 * [Objects]
 */
mpdm_t mpdm_set_wcs(mpdm_t o, mpdm_t v, const wchar_t *i)
{
    return mpdm_set(o, v, MPDM_S(i));
}


/* old hash compatibility layer */

/**
 * mpdm_hsize - Returns the number of pairs of a hash.
 * @h: the hash
 *
 * Returns the number of key-value pairs of a hash.
 * [Hashes]
 */
int mpdm_hsize(const mpdm_t h)
{
    return mpdm_count(h);
}


/**
 * mpdm_hget_s - Gets the value from a hash (string version).
 * @h: the hash
 * @k: the key
 *
 * Gets the value from the hash @h having @k as key, or
 * NULL if the key does not exist.
 * [Hashes]
 */
mpdm_t mpdm_hget_s(const mpdm_t h, const wchar_t *k)
{
    return mpdm_get_wcs(h, k);
}


/**
 * mpdm_hget - Gets a value from a hash.
 * @h: the hash
 * @k: the key
 *
 * Gets the value from the hash @h having @k as key, or
 * NULL if the key does not exist.
 * [Hashes]
 */
mpdm_t mpdm_hget(const mpdm_t h, const mpdm_t k)
{
    return mpdm_get(h, k);
}


/**
 * mpdm_hset - Sets a value in a hash.
 * @h: the hash
 * @k: the key
 * @v: the value
 *
 * Sets the value @v to the key @k in the hash @h. Returns
 * the new value (versions prior to 1.0.10 returned the old
 * value).
 * [Hashes]
 */
mpdm_t mpdm_hset(mpdm_t h, mpdm_t k, mpdm_t v)
{
    return mpdm_set(h, v, k);
}


/**
 * mpdm_hset_s - Sets a value in a hash (string version).
 * @h: the hash
 * @k: the key
 * @v: the value
 *
 * Sets the value @v to the key @k in the hash @h. Returns
 * the new value (versions prior to 1.0.10 returned the old
 * value).
 * [Hashes]
 */
mpdm_t mpdm_hset_s(mpdm_t h, const wchar_t *k, mpdm_t v)
{
    return mpdm_set_wcs(h, v, k);
}


/**
 * mpdm_hdel - Deletes a key from a hash.
 * @h: the hash
 * @k: the key
 *
 * Deletes the key @k from the hash @h. Returns NULL
 * (versions prior to 1.0.10 returned the deleted value).
 * [Hashes]
 */
mpdm_t mpdm_hdel(mpdm_t h, const mpdm_t k)
{
    return mpdm_del(h, k);
}


/**
 * mpdm_keys - Returns the keys of a hash.
 * @h: the hash
 *
 * Returns an array containing all the keys of the @h hash.
 * [Hashes]
 * [Arrays]
 */
mpdm_t mpdm_keys(const mpdm_t h)
{
    int c;
    mpdm_t a, i;

    printf("Warning: deprecated function mpdm_keys()\n");

    mpdm_ref(h);

    /* create an array with the same number of elements */
    a = MPDM_A(0);

    c = 0;
    while (mpdm_iterator(h, &c, NULL, &i))
        mpdm_push(a, i);

    mpdm_unref(h);

    return a;
}

/** data vc **/

static mpdm_t vc_object_destroy(mpdm_t o)
{
    struct mpdm_type_vc *vc = mpdm_type_vc_by_t(MPDM_TYPE_ARRAY);
    return vc->destroy(o);
}

static int vc_object_count(const mpdm_t o)
{
    int n;
    int ret = 0;

    for (n = 0; n < mpdm_size(o); n++) {
        mpdm_t b = mpdm_get_i(o, n);
        ret += mpdm_size(b);
    }

    return ret / 2;
}

static mpdm_t vc_object_get_i(const mpdm_t o, int index)
{
    struct mpdm_type_vc *vc = mpdm_type_vc_by_t(MPDM_TYPE_ARRAY);
    return vc->get_i(o, index);
}

static mpdm_t vc_object_get(const mpdm_t o, mpdm_t index)
{
    mpdm_t r;

    mpdm_ref(index);
    r = mpdm_get_wcs(o, mpdm_string(index));
    mpdm_unref(index);

    return r;
}

static mpdm_t vc_object_del(mpdm_t o, mpdm_t index)
{
    mpdm_t b;
    int n;

    mpdm_ref(index);

    if (mpdm_size(o)) {
        if ((b = mpdm_get_i(o, HASH_BUCKET(o, index))) != NULL) {
            /* bucket exists */
            if ((n = mpdm_bseek(b, index, 2, NULL)) >= 0) {
                /* collapse the bucket */
                mpdm_collapse(b, n, 2);
            }
        }
    }

    mpdm_unref(index);

    return o;
}

static mpdm_t vc_object_set_i(mpdm_t o, mpdm_t v, int index)
{
    return mpdm_type_vc_by_t(MPDM_TYPE_ARRAY)->set_i(o, v, index);
}

static mpdm_t vc_object_set(mpdm_t o, mpdm_t v, mpdm_t index)
{
    mpdm_t b, r;
    int n;

    mpdm_ref(index);
    mpdm_ref(v);

    /* if hash is empty, create an optimal number of buckets */
    if (mpdm_size(o) == 0)
        mpdm_expand(o, 0, mpdm_hash_buckets);

    n = HASH_BUCKET(o, index);

    if ((b = mpdm_get_i(o, n)) != NULL) {
        int pos;

        /* bucket exists; try to find the key there */
        n = mpdm_bseek(b, index, 2, &pos);

        if (n < 0) {
            /* the pair does not exist: create it */
            n = pos;
            mpdm_expand(b, n, 2);

            mpdm_set_i(b, index, n);
        }
    }
    else {
        /* the bucket does not exist; create it */
        b = MPDM_A(2);

        /* put the bucket into the hash */
        mpdm_set_i(o, b, n);

        /* offset 0 */
        n = 0;

        /* insert the key */
        mpdm_set_i(b, index, n);
    }

    r = mpdm_set_i(b, v, n + 1);

    mpdm_unref(v);
    mpdm_unref(index);

    return r;
}


static int vc_object_iterator(mpdm_t set, int *context, mpdm_t *v, mpdm_t *i)
{
    int ret = 0;

    if (mpdm_size(set)) {
        int bi, ei;

        /* get bucket and element index */
        bi = (*context) % mpdm_size(set);
        ei = (*context) / mpdm_size(set);

        while (ret == 0 && bi < mpdm_size(set)) {
            mpdm_t b;

            /* if bucket is empty or there are no more
               elements in it, pick the next one */
            if (!(b = mpdm_get_i(set, bi)) || ei >= mpdm_size(b)) {
                ei = 0;
                bi++;
            }
            else {
                /* get pair */
                if (v) *v = mpdm_get_i(b, ei + 1);
                if (i) *i = mpdm_get_i(b, ei);

                ei += 2;

                /* update context */
                *context = (ei * mpdm_size(set)) + bi;
                ret = 1;
            }
        }
    }

    return ret;
}


struct mpdm_type_vc mpdm_vc_object = { /* VC */
    L"object",              /* name */
    vc_object_destroy,      /* destroy */
    vc_default_is_true,     /* is_true */
    vc_object_count,        /* count */
    vc_object_get_i,        /* get_i */
    vc_object_get,          /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_object_del,          /* del */
    vc_object_set_i,        /* set_i */
    vc_object_set,          /* set */
    vc_default_exec,        /* exec */
    vc_object_iterator,     /* iterator */
    vc_default_map,         /* map */
    vc_default_cannot_exec  /* can_exec */
};
