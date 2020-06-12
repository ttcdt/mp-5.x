# builder for Minimum Profit
# loaded from build.sh

${MPSL}/build.sh ${MPSL} || exit 1

CFLAGS="$CFLAGS -I${MPDM} -I${MPSL}"

COMMON="config.h ${MPSL}/mpsl.h ${MPDM}/mpdm.h mp.h"

do_cc mp_core.o mp_core.c $COMMON
do_tar mp.tar *.mpsl lang/*.mpsl syntax/*.mpsl

[ "$DRV_ANSI" = "1" ] && do_cc mpv_ansi.o mpv_ansi.c $COMMON
[ "$DRV_NCURSES" = "1" ] && do_cc mpv_curses.o mpv_ansi.c $COMMON
[ "$DRV_GTK" = "1" ] && do_cc mpv_gtk.o mpv_gtk.c $COMMON
[ "$DRV_QT4" = "1" ] && do_moc mpv_qk_common.moc mpv_qk_common.cpp && do_cpp mpv_qt4.o mpv_qt4.cpp $COMMON
[ "$DRV_KDE4" = "1" ] && do_moc mpv_qk_common.moc mpv_qk_common.cpp && do_cpp mpv_kde4.o mpv_kde4.cpp $COMMON

[ "$LD" != "" ] && do_objbin mp_tar.o mp.tar

if [ "$DRV_WIN32" = "1" ] ; then
    do_cc mpv_win32.o mpv_win32.c $COMMON
    do_cc mpv_win32c.o mpv_win32c.c $COMMON
    do_windres mp_res.o mp_res.rc
    do_link "mp-5.exe" mp_*.o mpv_win32.o libmpsl.a libmpdm.a -mwindows -lcomctl32
    do_link "mp-5c.exe" mp_*.o mpv_win32c.o libmpsl.a libmpdm.a
else
    do_link "mp-5" mp_*.o mpv_*.o libmpsl.a libmpdm.a
fi
