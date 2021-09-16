# builder for MPSL
# loaded from build.sh

${MPDM}build.sh ${MPDM} || exit 1

CFLAGS="$CFLAGS -I${MPDM}"

COMMON="config.h mpsl.h ${MPDM}/mpdm.h"
do_cc mpsl_c.o mpsl_c.c $COMMON
do_cc mpsl_m.o mpsl_m.c $COMMON
do_cc mpsl_f.o mpsl_f.c $COMMON
do_cc mpsl_d.o mpsl_d.c $COMMON
do_cc mpsl_a.o mpsl_a.c $COMMON

# flex && yacc? build the classic compiler
if [ "$YACC" != "" ] ; then
    do_yacc mpsl_y.o y.tab.c mpsl.y ${MPDM}/mpdm.h
    do_flex mpsl_l.o lex.yy.c mpsl.l ${MPDM}/mpdm.h
fi

do_ar libmpsl.a mpsl_*.o

do_cc mpsl-stress.o mpsl-stress.c $COMMON
do_link mpsl-stress mpsl-stress.o libmpsl.a libmpdm.a
