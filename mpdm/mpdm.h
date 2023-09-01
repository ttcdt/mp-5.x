/*

    MPDM - Minimum Profit Data Manager

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#ifndef MPDM_H_
#define MPDM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    MPDM_TYPE_NULL,
    MPDM_TYPE_STRING,
    MPDM_TYPE_ARRAY,
    MPDM_TYPE_OBJECT,
    MPDM_TYPE_FILE,
    MPDM_TYPE_MBS,
    MPDM_TYPE_REGEX,
    MPDM_TYPE_MUTEX,
    MPDM_TYPE_SEMAPHORE,
    MPDM_TYPE_THREAD,
    MPDM_TYPE_FUNCTION,
    MPDM_TYPE_PROGRAM,
    MPDM_TYPE_INTEGER,
    MPDM_TYPE_REAL
} mpdm_type_t;

/* mpdm values */
typedef struct mpdm_val *mpdm_t;

/* a value */
struct mpdm_val {
    mpdm_type_t type:4;     /* value type */
    int size:28;            /* data size */
    int ref;                /* reference count */
    union {                 /* payload: */
        int ival;           /* integer value */
        const void *data;   /* pointer to data */
    };
};

/* function typedefs */
typedef mpdm_t mpdm_func1_t(mpdm_t);
typedef mpdm_t mpdm_func2_t(mpdm_t, mpdm_t);
typedef mpdm_t mpdm_func3_t(mpdm_t, mpdm_t, mpdm_t);

/* value type testing macros */

#define MPDM_CAN_GET(v)     (mpdm_type(v) == MPDM_TYPE_FUNCTION || \
                             mpdm_type(v) == MPDM_TYPE_PROGRAM || \
                             mpdm_type(v) == MPDM_TYPE_ARRAY || \
                             mpdm_type(v) == MPDM_TYPE_OBJECT)

/* value creation utility macros */

#define MPDM_A(n)       mpdm_new_a(n)
#define MPDM_O()        mpdm_new_o()
#define MPDM_S(s)       mpdm_new_wcs(s, -1, 1)
#define MPDM_NS(s,n)    mpdm_new_wcs(s, n, 1)
#define MPDM_ENS(s,n)   mpdm_new_wcs(s, n, 0)
#define MPDM_C(t,p,s)   mpdm_new_copy(t, p, s)

#define MPDM_I(i)       mpdm_new_i((i))
#define MPDM_R(r)       mpdm_new_r((r))
#define MPDM_MBS(s)     mpdm_new_mbstowcs(s, -1)
#define MPDM_NMBS(s,n)  mpdm_new_mbstowcs(s, n)
#define MPDM_2MBS(s)    mpdm_new_wcstombs(s)

#define MPDM_X(f)       mpdm_new_x(MPDM_TYPE_FUNCTION, (const void *)f, NULL)
#define MPDM_X2(f,a)    mpdm_new_x(MPDM_TYPE_PROGRAM, (const void *)f, a)

#define MPDM_F(f)       mpdm_new_f(f)

extern mpdm_func1_t *mpdm_destroy;

/* virtual call information for types */
struct mpdm_type_vc {
    wchar_t *name;
    mpdm_t (*destroy)(mpdm_t);
    int (*is_true)(mpdm_t);
    int (*count)(mpdm_t);
    mpdm_t (*get_i)(mpdm_t, int);
    mpdm_t (*get)(mpdm_t, mpdm_t);
    wchar_t *(*string)(mpdm_t);
    mpdm_t (*del_i)(mpdm_t, int);
    mpdm_t (*del)(mpdm_t, mpdm_t);
    mpdm_t (*set_i)(mpdm_t, mpdm_t, int);
    mpdm_t (*set)(mpdm_t, mpdm_t, mpdm_t);
    mpdm_t (*exec)(mpdm_t, mpdm_t, mpdm_t);
    int (*iterator)(mpdm_t, int64_t *, mpdm_t *, mpdm_t *);
    mpdm_t (*map)(mpdm_t, mpdm_t, mpdm_t);
    int (*can_exec)(mpdm_t);
    mpdm_t (*clone)(mpdm_t);
};

mpdm_t vc_default_destroy(mpdm_t v);
mpdm_t vc_null_destroy(mpdm_t v);
int vc_default_is_true(mpdm_t v);
int vc_default_count(mpdm_t v);
mpdm_t vc_default_get_i(mpdm_t v, int i);
mpdm_t vc_default_get(mpdm_t v, mpdm_t i);
wchar_t *vc_default_string(mpdm_t v);
mpdm_t vc_default_del_i(mpdm_t v, int i);
mpdm_t vc_default_del(mpdm_t v, mpdm_t i);
mpdm_t vc_default_set_i(mpdm_t v, mpdm_t e, int i);
mpdm_t vc_default_set(mpdm_t v, mpdm_t e, mpdm_t i);
mpdm_t vc_default_exec(mpdm_t c, mpdm_t args, mpdm_t ctxt);
int vc_default_iterator(mpdm_t set, int64_t *context, mpdm_t *v, mpdm_t *i);
mpdm_t vc_default_map(mpdm_t set, mpdm_t filter, mpdm_t ctxt);
int vc_default_can_exec(mpdm_t v);
int vc_default_cannot_exec(mpdm_t v);
mpdm_t vc_default_clone(mpdm_t v);

struct mpdm_type_vc *mpdm_type_vc_by_t(mpdm_type_t t);
struct mpdm_type_vc *mpdm_type_vc(mpdm_t);
mpdm_t mpdm_real_destroy(mpdm_t v);
mpdm_t mpdm_new(mpdm_type_t type, const void *data, int size);
mpdm_type_t mpdm_type(mpdm_t v);
wchar_t *mpdm_type_wcs(mpdm_t v);
mpdm_t mpdm_ref(mpdm_t v);
mpdm_t mpdm_unref(mpdm_t v);
mpdm_t mpdm_unrefnd(mpdm_t v);
int mpdm_size(const mpdm_t v);
void *mpdm_data(const mpdm_t v);
mpdm_t mpdm_root(void);
mpdm_t mpdm_void(mpdm_t v);
int mpdm_is_null(mpdm_t v);
mpdm_t mpdm_store(mpdm_t *v, mpdm_t w);
mpdm_t mpdm_new_copy(mpdm_type_t type, void *ptr, int size);
int mpdm_wrap_pointers(mpdm_t v, int offset, int *del);
int mpdm_startup(void);
void mpdm_shutdown(void);

extern wchar_t * (*mpdm_dump_1) (const mpdm_t v, int l, wchar_t *ptr, int *size);
wchar_t *mpdm_dumper_wcs(mpdm_t v, int *size);
mpdm_t mpdm_dumper(const mpdm_t v);
void mpdm_dump(const mpdm_t v);

mpdm_t mpdm_new_a(int size);
mpdm_t mpdm_expand(mpdm_t a, int index, int num);
mpdm_t mpdm_collapse(mpdm_t a, int index, int num);
mpdm_t mpdm_set_i(mpdm_t a, mpdm_t e, int index);
mpdm_t mpdm_ins(mpdm_t a, mpdm_t e, int index);
mpdm_t mpdm_del_i(mpdm_t a, int index);
mpdm_t mpdm_shift(mpdm_t a);
mpdm_t mpdm_push(mpdm_t a, mpdm_t e);
mpdm_t mpdm_pop(mpdm_t a);
mpdm_t mpdm_queue(mpdm_t a, mpdm_t e, int size);
mpdm_t mpdm_clone(const mpdm_t v);
int mpdm_seek_wcs(const mpdm_t a, const wchar_t *v, int step);
int mpdm_seek(const mpdm_t a, const mpdm_t v, int step);
int mpdm_bseek_wcs(const mpdm_t a, const wchar_t *v, int step, int *pos);
int mpdm_bseek(const mpdm_t a, const mpdm_t v, int step, int *pos);
mpdm_t mpdm_sort(mpdm_t a, int step);
mpdm_t mpdm_sort_cb(mpdm_t a, int step, mpdm_t asort_cb);
mpdm_t mpdm_split_wcs(const mpdm_t v, const wchar_t *s);
mpdm_t mpdm_split(const mpdm_t a, const mpdm_t s);
mpdm_t mpdm_join_wcs(const mpdm_t a, const wchar_t *s);
mpdm_t mpdm_reverse(const mpdm_t a);

void *mpdm_poke_2(void *dst, int *dsize, int *offset, const void *org, int osize, int esize);
void *mpdm_poke(void *dst, int *dsize, const void *org, int osize, int esize);
wchar_t *mpdm_pokewsn(wchar_t *dst, int *dsize, const wchar_t *str, int slen);
wchar_t *mpdm_pokews(wchar_t *dst, int *dsize, const wchar_t *str);
wchar_t *mpdm_pokev(wchar_t *dst, int *dsize, const mpdm_t v);
wchar_t *mpdm_mbstowcs(const char *str, int *s, int l);
char *mpdm_wcstombs(const wchar_t * str, int *s);
mpdm_t mpdm_new_wcs(const wchar_t *str, int size, int cpy);
mpdm_t mpdm_new_mbstowcs(const char *str, int l);
mpdm_t mpdm_new_wcstombs(const wchar_t *str);
mpdm_t mpdm_new_i(int ival);
mpdm_t mpdm_new_r(double rval);
wchar_t *mpdm_string(const mpdm_t v);
int mpdm_cmp_wcs(const mpdm_t v1, const wchar_t *v2);
mpdm_t mpdm_strcat_wcsn(const mpdm_t s1, const wchar_t *s2, int size);
mpdm_t mpdm_strcat_wcs(const mpdm_t s1, const wchar_t *s2);
mpdm_t mpdm_strcat(const mpdm_t s1, const mpdm_t s2);
int mpdm_ival_mbs(char *str);
int mpdm_ival(mpdm_t v);
double mpdm_rval_mbs(char *str);
double mpdm_rval(mpdm_t v);
mpdm_t mpdm_gettext(const mpdm_t str);
int mpdm_gettext_domain(const mpdm_t dom, const mpdm_t data);
int mpdm_wcwidth(wchar_t c);
mpdm_t mpdm_fmt(const mpdm_t fmt, const mpdm_t arg);
mpdm_t mpdm_sprintf(const mpdm_t fmt, const mpdm_t args);
mpdm_t mpdm_ulc(const mpdm_t s, int u);
mpdm_t mpdm_sscanf(const mpdm_t str, const mpdm_t fmt, int offset);
mpdm_t mpdm_tr(mpdm_t str, mpdm_t s1, mpdm_t s2);
mpdm_t mpdm_escape(mpdm_t v, wchar_t low, wchar_t high, mpdm_t f);
int mpdm_utf8_to_wc(wchar_t *w, int *s, char c);
mpdm_t mpdm_chomp(mpdm_t s);
char *mpdm_base64enc_mbs(const unsigned char *str, int iz);
mpdm_t mpdm_base64enc(mpdm_t v);
wchar_t *mpdm_unicode_nfd_wcs(wchar_t *str);
wchar_t *mpdm_unicode_nfc_wcs(wchar_t *str);
mpdm_t mpdm_unicode_nfd(mpdm_t s);
mpdm_t mpdm_unicode_nfc(mpdm_t s);

mpdm_t mpdm_new_o(void);
mpdm_t mpdm_get_wcs(const mpdm_t o, const wchar_t *i);
int mpdm_exists(const mpdm_t h, const mpdm_t i);
mpdm_t mpdm_set_wcs(mpdm_t o, mpdm_t v, const wchar_t *i);

wchar_t *mpdm_read_mbs(FILE *f, int *s);
int mpdm_write_wcs(FILE * f, const wchar_t * str);
mpdm_t mpdm_open(mpdm_t filename, mpdm_t mode);
mpdm_t mpdm_read(const mpdm_t fd);
wchar_t *mpdm_eol(mpdm_t fd);
mpdm_t mpdm_getchar(const mpdm_t fd);
int mpdm_putchar(const mpdm_t fd, const mpdm_t c);
int mpdm_write(const mpdm_t fd, const mpdm_t v);
int mpdm_fseek(const mpdm_t fd, long offset, int whence);
long mpdm_ftell(const mpdm_t fd);
int mpdm_feof(const mpdm_t fd);
int mpdm_flock(mpdm_t fd, int operation);
FILE * mpdm_get_filehandle(const mpdm_t fd);
int mpdm_encoding(mpdm_t charset);
int mpdm_unlink(const mpdm_t filename);
int mpdm_link(const mpdm_t src, const mpdm_t dest);
int mpdm_rename(const mpdm_t o, const mpdm_t n);
mpdm_t mpdm_stat(const mpdm_t filename);
int mpdm_chmod(const mpdm_t filename, mpdm_t perms);
int mpdm_chdir(const mpdm_t dir);
int mpdm_mkdir(const mpdm_t dir, const mpdm_t mode);
mpdm_t mpdm_getcwd(void);
int mpdm_chown(const mpdm_t filename, mpdm_t uid, mpdm_t gid);
mpdm_t mpdm_glob(mpdm_t spec, mpdm_t base);
mpdm_t mpdm_popen(const mpdm_t prg, const mpdm_t mode);
mpdm_t mpdm_popen2(const mpdm_t prg);
int mpdm_pclose(mpdm_t fd);
mpdm_t mpdm_conf_dir(void);
mpdm_t mpdm_home_dir(void);
mpdm_t mpdm_app_dir(void);

mpdm_t mpdm_connect(mpdm_t host, mpdm_t serv);
mpdm_t mpdm_server(mpdm_t addr, mpdm_t serv);
mpdm_t mpdm_accept(mpdm_t sock);
mpdm_t mpdm_socket_timeout(mpdm_t sock, mpdm_t rto, mpdm_t sto);

mpdm_t mpdm_new_f(FILE * f);
int mpdm_close(mpdm_t fd);

extern int mpdm_regex_offset;
extern int mpdm_regex_size;
extern int mpdm_sregex_count;

mpdm_t mpdm_regcomp(mpdm_t r);
mpdm_t mpdm_regex(const mpdm_t v, const mpdm_t r, int offset);
mpdm_t mpdm_sregex(const mpdm_t v, const mpdm_t r, const mpdm_t s, int offset);

void mpdm_sleep(int msecs);
double mpdm_time(void);
mpdm_t mpdm_random(mpdm_t v);
mpdm_t mpdm_new_mutex(void);
void mpdm_mutex_lock(mpdm_t mutex);
void mpdm_mutex_unlock(mpdm_t mutex);
mpdm_t mpdm_new_semaphore(int init_value);
void mpdm_semaphore_wait(mpdm_t sem);
void mpdm_semaphore_post(mpdm_t sem);
mpdm_t mpdm_exec_thread(mpdm_t c, mpdm_t args, mpdm_t ctxt);
unsigned char *mpdm_gzip_inflate(unsigned char *cbuf, size_t cz, size_t *dz);
unsigned char *mpdm_read_tar_mem(const char *fn, const char *tar,
                                 const char *tar_e, size_t *z);
unsigned char *mpdm_read_tar_file(const char *fn, FILE *f, size_t *z);
unsigned char *mpdm_read_zip_mem(const char *fn, const char *zip,
                                 const char *zip_e, size_t *z);
unsigned char *mpdm_read_zip_file(const char *fn, FILE *f, size_t *z);
unsigned char *mpdm_read_arch_mem(const char *fn, const char *arch,
                                  const char *arch_e, size_t *z);
mpdm_t mpdm_read_arch_mem_s(mpdm_t fn, const char *arch, const char *arch_e);
unsigned char *mpdm_read_arch_file(const char *fn, FILE *f, size_t *z);
mpdm_t mpdm_read_arch_file_s(mpdm_t fn, mpdm_t fd);
mpdm_t mpdm_md5(mpdm_t v);

int mpdm_is_true(mpdm_t v);
mpdm_t mpdm_bool(int b);
mpdm_t mpdm_new_x(mpdm_type_t type, const void *f, mpdm_t a);
int mpdm_count(mpdm_t v);
mpdm_t mpdm_get_i(const mpdm_t a, int index);
mpdm_t mpdm_get(mpdm_t set, mpdm_t i);
mpdm_t mpdm_del(mpdm_t set, mpdm_t i);
mpdm_t mpdm_set(mpdm_t set, mpdm_t v, mpdm_t i);
int mpdm_can_exec(mpdm_t v);
mpdm_t mpdm_exec(mpdm_t c, mpdm_t args, mpdm_t ctxt);
mpdm_t mpdm_exec_1(mpdm_t c, mpdm_t a1, mpdm_t ctxt);
mpdm_t mpdm_exec_2(mpdm_t c, mpdm_t a1, mpdm_t a2, mpdm_t ctxt);
mpdm_t mpdm_exec_3(mpdm_t c, mpdm_t a1, mpdm_t a2, mpdm_t a3, mpdm_t ctxt);
int mpdm_iterator(mpdm_t set, int64_t *context, mpdm_t *v, mpdm_t *i);
mpdm_t mpdm_map(mpdm_t set, mpdm_t filter, mpdm_t ctxt);
mpdm_t mpdm_omap(mpdm_t set, mpdm_t filter, mpdm_t ctxt);
mpdm_t mpdm_grep(mpdm_t set, mpdm_t filter, mpdm_t ctxt);
mpdm_t mpdm_join(const mpdm_t a, const mpdm_t s);
mpdm_t mpdm_splice(const mpdm_t v, const mpdm_t i, int offset, int del, mpdm_t *n, mpdm_t *d);
int mpdm_cmp(const mpdm_t v1, const mpdm_t v2);
mpdm_t mpdm_multiply(mpdm_t v, mpdm_t i);
mpdm_t mpdm_substract(mpdm_t m, mpdm_t s);
mpdm_t mpdm_divide(mpdm_t num, mpdm_t den);


#ifdef MPDM_OLD_COMPAT

/* old compatibility layer */

mpdm_t mpdm_aset(mpdm_t a, mpdm_t e, int index);
mpdm_t mpdm_aget(const mpdm_t a, int index);
mpdm_t mpdm_adel(mpdm_t a, int index);

#define MPDM_H(n)       MPDM_O()

int mpdm_hsize(const mpdm_t h);
mpdm_t mpdm_hget_s(const mpdm_t h, const wchar_t *k);
mpdm_t mpdm_hget(const mpdm_t h, const mpdm_t k);
mpdm_t mpdm_hset(mpdm_t h, mpdm_t k, mpdm_t v);
mpdm_t mpdm_hset_s(mpdm_t h, const wchar_t *k, mpdm_t v);
mpdm_t mpdm_hdel(mpdm_t h, const mpdm_t k);
mpdm_t mpdm_keys(const mpdm_t h);

#endif

#ifdef __cplusplus
}
#endif

#endif /* MPDM_H_ */
