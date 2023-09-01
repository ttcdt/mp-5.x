/*

    MPDM - Minimum Profit Data Manager
    mpdm_t.c - Threads, mutexes, semaphores and other stuff

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>

#ifdef CONFOPT_WIN32
#include <windows.h>
#endif

#ifdef CONFOPT_PTHREADS
#include <pthread.h>
#endif

#ifdef CONFOPT_POSIXSEMS
#include <semaphore.h>
#endif

#ifdef CONFOPT_GETTIMEOFDAY
#include <sys/time.h>
#endif

#ifdef CONFOPT_ZLIB
#include <zlib.h>
#endif

#include "mpdm.h"


/** code **/

/**
 * mpdm_sleep - Sleeps a number of milliseconds.
 * @msecs: the milliseconds to sleep
 *
 * Sleeps a number of milliseconds.
 * [Threading]
 */
void mpdm_sleep(int msecs)
{
#ifdef CONFOPT_WIN32

    Sleep(msecs);

#endif

#ifdef CONFOPT_NANOSLEEP
    struct timespec ts;

    ts.tv_sec = msecs / 1000;
    ts.tv_nsec = (msecs % 1000) * 1000000;

    nanosleep(&ts, NULL);
#endif
}


double mpdm_time(void)
{
    double r = 0.0;

#ifdef CONFOPT_GETTIMEOFDAY

    struct timeval tv;

    gettimeofday(&tv, NULL);

    r = ((double)tv.tv_sec) + (((double)tv.tv_usec) / 1000000.0);

#else /* CONFOPT_GETTIMEOFDAY */

    r = (double) time(NULL);

#endif /* CONFOPT_GETTIMEOFDAY */

    return r;
}


mpdm_t mpdm_random(mpdm_t v)
{
    static unsigned int seed = 0;
    mpdm_t r = NULL;

    if (mpdm_type(v) == MPDM_TYPE_ARRAY)
        r = mpdm_get(v, mpdm_random(MPDM_I(mpdm_size(v))));
    else {
        int range = mpdm_ival(v);
        int v = 0;

        /* crappy random seed */
        if (seed == 0) {
            time_t t = time(NULL);
            seed = t ^ (t << 16);
        }

        seed = (seed * 58321) + 11113;
        v = (seed >> 16);

        if (range)
            r = MPDM_I(v % range);
        else
            r = MPDM_R((v & 0xffff) / 65536.0);
    }

    return r;
}


/** mutexes **/

static mpdm_t vc_mutex_destroy(mpdm_t v)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mpdm_data(v);

    CloseHandle(*h);
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t *m = (pthread_mutex_t *) mpdm_data(v);

    pthread_mutex_destroy(m);
#endif

    v->data = NULL;

    return v;
}


/**
 * mpdm_new_mutex - Creates a new mutex.
 *
 * Creates a new mutex.
 * [Threading]
 */
mpdm_t mpdm_new_mutex(void)
{
    char *ptr = NULL;
    int size = 0;

#ifdef CONFOPT_WIN32
    HANDLE h;

    h = CreateMutex(NULL, FALSE, NULL);

    if (h != NULL) {
        size = sizeof(h);
        ptr = (char *) &h;
    }
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t m;

    if (pthread_mutex_init(&m, NULL) == 0) {
        size = sizeof(m);
        ptr = (char *) &m;
    }

#endif

    return MPDM_C(MPDM_TYPE_MUTEX, ptr, size);
}


/**
 * mpdm_mutex_lock - Locks a mutex.
 * @mutex: the mutex to be locked
 *
 * Locks a mutex. If the mutex is not already locked,
 * it waits until it is.
 * [Threading]
 */
void mpdm_mutex_lock(mpdm_t mutex)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mpdm_data(mutex);

    WaitForSingleObject(*h, INFINITE);
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t *m = (pthread_mutex_t *) mpdm_data(mutex);

    pthread_mutex_lock(m);
#endif
}


/**
 * mpdm_mutex_unlock - Unlocks a mutex.
 * @mutex: the mutex to be unlocked
 *
 * Unlocks a previously locked mutex. The thread
 * unlocking the mutex must be the one who locked it.
 * [Threading]
 */
void mpdm_mutex_unlock(mpdm_t mutex)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mpdm_data(mutex);

    ReleaseMutex(*h);
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t *m = (pthread_mutex_t *) mpdm_data(mutex);

    pthread_mutex_unlock(m);
#endif
}


/** semaphores **/

static mpdm_t vc_semaphore_destroy(mpdm_t v)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mpdm_data(v);

    CloseHandle(*h);
#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t *s = (sem_t *) mpdm_data(v);

    sem_destroy(s);
#endif

    v->data = NULL;

    return v;
}


/**
 * mpdm_new_semaphore - Creates a new semaphore.
 * @init_value: the initial value of the semaphore.
 *
 * Creates a new semaphore with an @init_value.
 * [Threading]
 */
mpdm_t mpdm_new_semaphore(int init_value)
{
    char *ptr = NULL;
    int size = 0;

#ifdef CONFOPT_WIN32
    HANDLE h;

    if ((h = CreateSemaphore(NULL, init_value, 0x7fffffff, NULL)) != NULL) {
        size = sizeof(h);
        ptr = (char *) &h;
    }

#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t s;

    if (sem_init(&s, 0, init_value) == 0) {
        size = sizeof(s);
        ptr = (char *) &s;
    }

#endif

    return MPDM_C(MPDM_TYPE_SEMAPHORE, ptr, size);
}


/**
 * mpdm_semaphore_wait - Waits for a semaphore to be ready.
 * @sem: the semaphore to wait onto
 *
 * Waits for the value of a semaphore to be > 0. If it's
 * not, the thread waits until it is.
 * [Threading]
 */
void mpdm_semaphore_wait(mpdm_t sem)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mpdm_data(sem);

    WaitForSingleObject(*h, INFINITE);
#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t *s = (sem_t *) mpdm_data(sem);

    sem_wait(s);
#endif
}


/**
 * mpdm_semaphore_post - Increments the value of a semaphore.
 * @sem: the semaphore to increment
 *
 * Increments by 1 the value of a semaphore.
 * [Threading]
 */
void mpdm_semaphore_post(mpdm_t sem)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mpdm_data(sem);

    ReleaseSemaphore(*h, 1, NULL);
#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t *s = (sem_t *) mpdm_data(sem);

    sem_post(s);
#endif
}


/** threads **/

static mpdm_t vc_thread_destroy(mpdm_t v)
{
    v->data = NULL;

    return v;
}


static void thread_caller(mpdm_t a)
{
    /* ignore return value */
    mpdm_void(mpdm_exec(mpdm_get_i(a, 0), mpdm_get_i(a, 1), mpdm_get_i(a, 2)));

    /* was referenced in mpdm_exec_thread() */
    mpdm_unref(a);
}


#ifdef CONFOPT_WIN32
DWORD WINAPI win32_thread(LPVOID param)
{
    thread_caller((mpdm_t) param);

    return 0;
}
#endif

#ifdef CONFOPT_PTHREADS
void *pthreads_thread(void *args)
{
    thread_caller((mpdm_t) args);

    return NULL;
}
#endif

/**
 * mpdm_exec_thread - Runs an executable value in a new thread.
 * @c: the executable value
 * @args: executable arguments
 * @ctxt: the context
 *
 * Runs the @c executable value in a new thread. The code
 * starts executing immediately. The @args and @ctxt arguments
 * are sent to the executable value as arguments.
 *
 * Returns a handle for the thread.
 * [Threading]
 */
mpdm_t mpdm_exec_thread(mpdm_t c, mpdm_t args, mpdm_t ctxt)
{
    mpdm_t a;
    char *ptr = NULL;
    int size = 0;

    if (ctxt == NULL)
        ctxt = MPDM_A(0);

    /* to be unreferenced at thread stop */
    a = mpdm_ref(MPDM_A(3));

    mpdm_set_i(a, c, 0);
    mpdm_set_i(a, args, 1);
    mpdm_set_i(a, ctxt, 2);

#ifdef CONFOPT_WIN32
    HANDLE t;

    t = CreateThread(NULL, 0, win32_thread, a, 0, NULL);

    if (t != NULL) {
        size = sizeof(t);
        ptr = (char *) &t;
    }

#endif

#ifdef CONFOPT_PTHREADS
    pthread_t pt;

    if (pthread_create(&pt, NULL, pthreads_thread, a) == 0) {
        size = sizeof(pthread_t);
        ptr = (char *) &pt;
    }

#endif

    return MPDM_C(MPDM_TYPE_THREAD, ptr, size);
}


/** zlib functions **/

unsigned char *mpdm_gzip_inflate(unsigned char *cbuf, size_t cz, size_t *dz)
{
    unsigned char *dbuf = NULL;

#ifdef CONFOPT_ZLIB

    if (cbuf[0] == 0x1f && cbuf[1] == 0x8b) {
        z_stream d_stream;
        int err;

        /* size % 2^32 is at the end */
        *dz = cbuf[cz - 1];
        *dz = (*dz * 256) + cbuf[cz - 2];
        *dz = (*dz * 256) + cbuf[cz - 3];
        *dz = (*dz * 256) + cbuf[cz - 4];

        dbuf = calloc(*dz + 1, 1);

        memset(&d_stream, '\0', sizeof(d_stream));

        d_stream.next_in   = cbuf;
        d_stream.avail_in  = cz;
        d_stream.next_out  = dbuf;
        d_stream.avail_out = *dz;

        if ((err = inflateInit2(&d_stream, 16 + MAX_WBITS)) == Z_OK) {
            while (err != Z_STREAM_END && err >= 0)
                err = inflate(&d_stream, Z_FINISH);

            err = inflateEnd(&d_stream);
        }

        if (err != Z_OK || *dz != d_stream.total_out)
            dbuf = realloc(dbuf, 0);
    }

#endif /* CONFOPT_ZLIB */

    return dbuf;
}


/** tar archives **/

unsigned char *mpdm_read_tar_mem(const char *fn, const char *tar,
                                 const char *tar_e, size_t *z)
{
    unsigned char *data = NULL;

    while (!data && tar < tar_e && *tar) {
        sscanf(&tar[124], "%lo", (long *)z);

        if (strcmp(fn, tar) == 0)
            data = memcpy(calloc(*z + 1, 1), tar + 512, *z);
        else
            tar += (1 + ((*z + 511) / 512)) * 512;
    }

    return data;
}


unsigned char *mpdm_read_tar_file(const char *fn, FILE *f, size_t *z)
{
    unsigned char *data = NULL;
    char tar[512];

    while (!data && fread(tar, sizeof(tar), 1, f) && *tar) {
        sscanf(&tar[124], "%lo", (long *)z);

        if (strcmp(fn, tar) == 0) {
            data = calloc(*z + 1, 1);
            if (!fread(data, *z, 1, f))
                data = realloc(data, 0);
        }
        else
            fseek(f, ((*z + 511) / 512) * 512, 1);
    }

    return data;
}


/** zip archives **/

struct zip_hdr {
    unsigned char sig[4];
    unsigned short min_ver;
    unsigned short gp_flag;
    unsigned short comp_meth;
    unsigned short mtime;
    unsigned short mdate;
    unsigned int crc32;
    unsigned int csz;
    unsigned int usz;
    unsigned short fnsz;
    unsigned short xfsz;
} __attribute__ ((__packed__));


int puff(unsigned char *dest,           /* pointer to destination pointer */
         unsigned long *destlen,        /* amount of output space */
         const unsigned char *source,   /* pointer to source data pointer */
         unsigned long *sourcelen);     /* amount of input available */

unsigned char *mpdm_read_zip_mem(const char *fn, const char *zip,
                                 const char *zip_e, size_t *z)
{
    unsigned char *data = NULL;
    int sz = strlen(fn);

    while (!data && zip < zip_e) {
        struct zip_hdr *hdr = (struct zip_hdr *)zip;
        unsigned char *cdata;

        if (hdr->sig[0] != 'P' || hdr->sig[1] != 'K' || hdr->fnsz == 0)
            break;

        cdata = (unsigned char *)zip + sizeof(*hdr) + hdr->fnsz + hdr->xfsz;

        if (sz == hdr->fnsz && memcmp(zip + sizeof(*hdr), fn, hdr->fnsz) == 0) {
            data = calloc(hdr->usz + 1, 1);

            if (hdr->comp_meth == 0)
                memcpy(cdata, data, hdr->usz);
            else
            if (hdr->comp_meth == 8) {
                unsigned long usz = hdr->usz;
                unsigned long csz = hdr->csz;
                puff(data, &usz, cdata, &csz);
            }
            else {
                data = realloc(data, 0);
                break;
            }

            *z = hdr->usz;
        }
        else
            zip = (char *)cdata + hdr->csz;
    }

    return data;
}


unsigned char *mpdm_read_zip_file(const char *fn, FILE *f, size_t *z)
{
    unsigned char *data = NULL;
    int sz = strlen(fn);
    char buf[sizeof(struct zip_hdr)];
    char *hdrfn = NULL;

    while (!data && fread(buf, sizeof(buf), 1, f)) {
        struct zip_hdr *hdr = (struct zip_hdr *)buf;

        if (hdr->sig[0] != 'P' || hdr->sig[1] != 'K' || hdr->fnsz == 0)
            break;

        /* alloc file name and read it */
        hdrfn = realloc(hdrfn, hdr->fnsz);

        if (!fread(hdrfn, hdr->fnsz, 1, f))
            break;

        /* skip xfsz */
        fseek(f, hdr->xfsz, SEEK_CUR);

        if (sz == hdr->fnsz && memcmp(hdrfn, fn, hdr->fnsz) == 0) {
            data = calloc(hdr->usz + 1, 1);

            if (hdr->comp_meth == 0)
                fread(data, hdr->usz, 1, f);
            else
            if (hdr->comp_meth == 8) {
                unsigned long usz = hdr->usz;
                unsigned long csz = hdr->csz;
                unsigned char *cdata = malloc(hdr->csz);

                /* read compressed data */
                fread(cdata, hdr->csz, 1, f);
                puff(data, &usz, cdata, &csz);
                free(cdata);
            }
            else {
                data = realloc(data, 0);
                break;
            }

            *z = hdr->usz;
        }
        else
            fseek(f, hdr->csz, SEEK_CUR);
    }

    free(hdrfn);

    return data;
}


/** 'generic' archives **/

unsigned char *mpdm_read_arch_mem(const char *fn, const char *arch,
                                  const char *arch_e, size_t *z)
{
    unsigned char *data = NULL;

    if (arch && arch[0] == 'P' && arch[1] == 'K')
        data = mpdm_read_zip_mem(fn, arch, arch_e, z);
    else
        data = mpdm_read_tar_mem(fn, arch, arch_e, z);

    return data;
}


mpdm_t mpdm_read_arch_mem_s(mpdm_t fn, const char *arch, const char *arch_e)
{
    mpdm_t f, r = NULL;
    unsigned char *data;
    size_t z;

    mpdm_ref(fn);

    /* convert to mbs */
    f = mpdm_ref(MPDM_2MBS(mpdm_string(fn)));

    if ((data = mpdm_read_arch_mem((const char *)mpdm_data(f), arch, arch_e, &z)) != NULL) {
        r = MPDM_MBS((char *)data);
        free(data);
    }

    mpdm_unref(f);
    mpdm_unref(fn);

    return r;
}


unsigned char *mpdm_read_arch_file(const char *fn, FILE *f, size_t *z)
{
    unsigned char *data = NULL;
    char sig[2];

    /* read signature */
    fread(sig, sizeof(sig), 1, f);
    rewind(f);

    if (sig[0] == 'P' && sig[1] == 'K')
        data = mpdm_read_zip_file(fn, f, z);
    else
        data = mpdm_read_tar_file(fn, f, z);

    return data;
}


mpdm_t mpdm_read_arch_file_s(mpdm_t fn, mpdm_t fd)
{
    mpdm_t fs, r = NULL;
    FILE *f = mpdm_get_filehandle(fd);

    mpdm_ref(fn);

    /* convert to mbs */
    fs = mpdm_ref(MPDM_2MBS(mpdm_string(fn)));

    if (f) {
        unsigned char *data = NULL;
        size_t z;

        if ((data = mpdm_read_arch_file((const char *)mpdm_data(fs), f, &z)) != NULL) {
            r = MPDM_MBS((char *)data);
            free(data);
        }
    }

    mpdm_unref(fs);
    mpdm_unref(fn);

    return r;
}


/** other utilities **/

void md5_simple(void *output, const void *input, unsigned long size);

mpdm_t mpdm_md5(mpdm_t v)
{
    mpdm_t r = NULL;

    mpdm_ref(v);

    if (mpdm_type(v) == MPDM_TYPE_STRING) {
        char *ptr;
        unsigned char md5[16];
        char md5_s[33];
        int n;

        ptr = mpdm_wcstombs(mpdm_string(v), &n);

        md5_simple(md5, ptr, n);

        for (n = 0; n < sizeof(md5); n++)
            snprintf(&md5_s[n * 2], 3, "%02x", md5[n]);
        md5_s[32] = '\0';

        free(ptr);

        r = MPDM_MBS(md5_s);
    }

    mpdm_unref(v);

    return r;
}


/** data vc **/

struct mpdm_type_vc mpdm_vc_mutex = { /* VC */
    L"mutex",               /* name */
    vc_mutex_destroy,       /* destroy */
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

struct mpdm_type_vc mpdm_vc_semaphore = { /* VC */
    L"semaphore",           /* name */
    vc_semaphore_destroy,   /* destroy */
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

struct mpdm_type_vc mpdm_vc_thread = { /* VC */
    L"thread",              /* name */
    vc_thread_destroy,      /* destroy */
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
