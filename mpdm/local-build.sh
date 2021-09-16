# builder for MPDM
# loaded from build.sh

COMMON="config.h mpdm.h"
do_cc mpdm_v.o mpdm_v.c $COMMON
do_cc mpdm_a.o mpdm_a.c $COMMON
do_cc mpdm_d.o mpdm_d.c $COMMON
do_cc mpdm_f.o mpdm_f.c $COMMON
do_cc mpdm_o.o mpdm_o.c $COMMON
do_cc mpdm_r.o mpdm_r.c $COMMON
do_cc mpdm_s.o mpdm_s.c $COMMON
do_cc mpdm_t.o mpdm_t.c $COMMON
do_cc mpdm_x.o mpdm_x.c $COMMON
do_cc mpdm_dd.o mpdm_dd.c $COMMON
do_cc gnu_regex.o gnu_regex.c config.h
do_cc puff.o puff.c
do_cc md5.o md5.c
do_ar libmpdm.a mpdm_*.o *regex.o puff.o md5.o

do_cc mpdm-stress.o mpdm-stress.c $COMMON
do_link mpdm-stress mpdm-stress.o libmpdm.a
