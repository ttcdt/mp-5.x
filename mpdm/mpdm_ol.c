/*

    MPDM - Minimum Profit Data Manager
    mpdm_ol.c - Object (as lists) management

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

struct ol {
    struct ol *prev;    /* previous in list */
    struct ol *next;    /* next in list */
    mpdm_t index;       /* index of pair */
    mpdm_t value;       /* value of pair */
};


static void ol_to_head(mpdm_t o, struct ol *e)
/* moves an ol to the head */
{
    struct ol *fe = mpdm_data(o);

    if (e != fe && e != NULL) {
        if (e->prev)
            e->prev->next = e->next;
        if (e->next)
            e->next->prev = e->prev;

        e->prev = NULL;
        e->next = fe;

        if (fe)
            fe->prev = e;

        o->data = e;
    }
}


static struct ol *find_ol(mpdm_t o, mpdm_t i)
/* finds the pair with the given index */
{
    struct ol *e;

    mpdm_ref(i);

    /* find an ol object with this index */
    for (e = mpdm_data(o); e != NULL && mpdm_cmp(e->index, i) != 0; e = e->next);

    /* if found, move to head */
    if (e != NULL)
        ol_to_head(o, e);

    mpdm_unref(i);

    return e;
}


static struct ol *find_ol_wcs(mpdm_t o, const wchar_t *str)
/* finds the pair with the given index (as a string) */
{
    struct ol *e;

    /* find an ol object with this index */
    for (e = mpdm_data(o); e != NULL; e = e->next) {
        if (mpdm_type(e->index) == MPDM_TYPE_STRING &&
            wcscmp(mpdm_data(e->index), str) == 0)
                break;
    }

    /* if found, move to head */
    if (e != NULL)
        ol_to_head(o, e);

    return e;
}


/** interface **/

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
    struct ol *e = find_ol_wcs(o, i);

    return e != NULL ? e->value : NULL;
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
    return !!find_ol(o, i);
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


/** data vc **/

static mpdm_t vc_object_destroy(mpdm_t o)
{
    struct ol *e = (struct ol *) mpdm_data(o);

    /* free all ol objects */
    while (e != NULL) {
        struct ol *n = e->next;

        mpdm_store(&e->index, NULL);
        mpdm_store(&e->value, NULL);

        free(e);
        e = n;
    }

    o->data = NULL;
    return o;
}


static mpdm_t vc_object_get(const mpdm_t o, mpdm_t index)
{
    struct ol *e = find_ol(o, index);

    return e != NULL ? e->value : NULL;
}


static mpdm_t vc_object_del(mpdm_t o, mpdm_t index)
{
    struct ol *e = find_ol(o, index);

    /* found? it's the first */
    if (e != NULL) {
        struct ol *n = e->next;

        mpdm_store(&e->index, NULL);
        mpdm_store(&e->value, NULL);

        if (n != NULL)
            n->prev = NULL;

        o->data = n;
        free(e);

        o->size -= 1;
    }

    return o;
}


static mpdm_t vc_object_set(mpdm_t o, mpdm_t v, mpdm_t index)
{
    struct ol *e;

    mpdm_ref(index);
    mpdm_ref(v);

    e = find_ol(o, index);

    /* not found? create a new ol at the head */
    if (e == NULL) {
        struct ol *n = mpdm_data(o);

        e = calloc(sizeof(struct ol), 1);

        if (n != NULL)
            n->prev = e;

        e->next = n;

        o->data = e;

        mpdm_store(&e->index, index);

        o->size += 1;
    }

    /* replace the value */
    mpdm_store(&e->value, v);

    mpdm_unref(v);
    mpdm_unref(index);

    return v;
}


static int vc_object_iterator(mpdm_t set, int64_t *context, mpdm_t *v, mpdm_t *i)
{
    int ret = 0;
    struct ol *e;

    memcpy(&e, context, sizeof(void *));

    if (e == NULL)
        e = (struct ol *) mpdm_data(set);
    else
        e = e->next;

    if (e != NULL) {
        if (i) *i = e->index;
        if (v) *v = e->value;
        ret = 1;
    }

    memcpy(context, &e, sizeof(void *));

    return ret;
}


static mpdm_t vc_object_clone(mpdm_t o)
{
    int64_t c = 0;
    mpdm_t w = MPDM_O();
    mpdm_t v, i;
    struct ol *p = NULL;

    mpdm_ref(o);

    /* add all values manually */
    while (mpdm_iterator(o, &c, &v, &i)) {
        struct ol *e = calloc(sizeof(struct ol), 1);

        if (p)
            p->prev = e;

        e->next = p;
        mpdm_store(&e->index, i);
        mpdm_store(&e->value, mpdm_clone(v));

        p = e;
    }

    w->data = p;

    mpdm_unref(o);

    return w;
}


struct mpdm_type_vc mpdm_vc_object = { /* VC */
    L"object",              /* name */
    vc_object_destroy,      /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_object_get,          /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_object_del,          /* del */
    vc_default_set_i,       /* set_i */
    vc_object_set,          /* set */
    vc_default_exec,        /* exec */
    vc_object_iterator,     /* iterator */
    vc_default_map,         /* map */
    vc_default_cannot_exec, /* can_exec */
    vc_object_clone         /* clone */
};

int mpdm_hash_buckets;
