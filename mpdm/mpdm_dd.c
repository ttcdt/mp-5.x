/*

    MPDM - Minimum Profit Data Manager
    mpdm_dd.c - Delayed Destroy

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

static mpdm_t dd_mutex = NULL;
static mpdm_t dd_sem   = NULL;
static mpdm_t dd_stack = NULL;


static mpdm_t dd_thread(mpdm_t v)
/* delayed destroy thread */
{
    for (;;) {
        mpdm_t v;

        /* wait for available value */
        mpdm_semaphore_wait(dd_sem);

        /* start protected area */
        mpdm_mutex_lock(dd_mutex);

        /* pick value to be destroyed */
        v = mpdm_get_i(dd_stack, -1);

        /* manually shrink stack */
        dd_stack->size--;

        /* end protected area */
        mpdm_mutex_unlock(dd_mutex);

        /* manually unref the value */
        v->ref = 0;

        /* do it */
        mpdm_real_destroy(v);
    }

    return NULL;
}


mpdm_t mpdm_delayed_destroy(mpdm_t v)
/* store values to be deleted from a detached thread */
{
    if (dd_mutex == NULL) {
        /* first time */
        dd_mutex = mpdm_ref(mpdm_new_mutex());
        dd_sem   = mpdm_ref(mpdm_new_semaphore(0));
        dd_stack = mpdm_ref(MPDM_A(0));

        mpdm_exec_thread(mpdm_ref(MPDM_X(dd_thread)), NULL, NULL);
    }

    /* start protected area */
    mpdm_mutex_lock(dd_mutex);

    /* push value to stack (ref is back to 1) */
    mpdm_push(dd_stack, v);

    /* end protected area */
    mpdm_mutex_unlock(dd_mutex);

    /* signal new value */
    mpdm_semaphore_post(dd_sem);

    return NULL;
}
