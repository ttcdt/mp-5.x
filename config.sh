#!/bin/sh

# Minimum Profit autoconfiguration script

DRIVERS=""
DRV_OBJS=""
MORE_OBJS=""
MORE_TARGETS=""
APPNAME="mp-5"
TARGET=""

# gets program version
VERSION=`cut -f2 -d\" VERSION`

# default installation prefix
PREFIX=/usr/local

# set PKG_CONFIG if it isn't set already
PKG_CONFIG=${PKG_CONFIG:-pkg-config}

# store command line args for configuring the libraries
CONF_ARGS="$*"

# No KDE4 by default
WITHOUT_KDE4=1

# No msgfmt by default
WITHOUT_MSGFMT=1

WITH_EXTERNAL_ARCH=0

# store command line args for configuring the libraries
CONF_ARGS="$*"

MINGW32_PREFIX=i686-w64-mingw32

# parse arguments
while [ $# -gt 0 ] ; do

    case $1 in
    --without-curses)    WITHOUT_CURSES=1 ;;
    --without-gtk)       WITHOUT_GTK=1 ;;
    --without-win32)     WITHOUT_WIN32=1 ;;
    --with-kde4)         WITHOUT_KDE4=0 ;;
    --without-qt)        WITHOUT_QT5=1 && WITHOUT_QT4=1 ;;
    --without-qt4)       WITHOUT_QT4=1 ;;
    --without-qt5)       WITHOUT_QT5=1 ;;

    --without-ansi)      WITHOUT_ANSI=1 ;;
    --with-external-arch) WITH_EXTERNAL_ARCH=1 ;;
    --with-external-tar) WITH_EXTERNAL_ARCH=1 ;;
    --without-zip)       WITHOUT_ZIP=1 ;;
    --help)              CONFIG_HELP=1 ;;

    --mingw32-prefix=*)     MINGW32_PREFIX=`echo $1 | sed -e 's/--mingw32-prefix=//'`
                            ;;

    --mingw32)          CC=${MINGW32_PREFIX}-gcc
                        WINDRES=${MINGW32_PREFIX}-windres
                        AR=${MINGW32_PREFIX}-ar
                        LD=${MINGW32_PREFIX}-ld
                        CPP=${MINGW32_PREFIX}-g++
                        ;;

    --prefix)           PREFIX=$2 ; shift ;;
    --prefix=*)         PREFIX=`echo $1 | sed -e 's/--prefix=//'` ;;
    --with-moc=*)       MOC=`echo $1 | sed -e 's/--with-moc=//'` ;;
    esac

    shift
done

if [ "$CONFIG_HELP" = "1" ] ; then

    echo "Available options:"
    echo "--prefix=PREFIX         Installation prefix ($PREFIX)."
    echo "--without-curses        Disable curses (text) interface detection."
    echo "--without-gtk           Disable GTK interface detection."
    echo "--without-win32         Disable win32 interface detection."
    echo "--with-kde4             Enable KDE4 interface detection."
    echo "--without-qt            Disable Qt interface detection."
    echo "--without-qt4           Disable Qt4 interface detection."
    echo "--without-qt5           Disable Qt5 interface detection."
    echo "--with-moc              Path to your Qt moc. Ie: --with-moc=/usr/lib64/qt4/bin/moc"
    echo "--without-ansi          Disable ANSI terminal interface detection."
    echo "--with-included-regex   Use included regex code (gnu_regex.c)."
    echo "--with-pcre             Enable PCRE library detection."
    echo "--without-gettext       Disable gettext usage."
    echo "--without-iconv         Disable iconv usage."
    echo "--without-wcwidth       Disable system wcwidth() (use workaround)."
    echo "--with-null-hash        Tell MPDM to use a NULL hashing function."
    echo "--mingw32-prefix=PREFIX Prefix name for mingw32 ($MINGW32_PREFIX)."
    echo "--mingw32               Build using the mingw32 compiler."
    echo "--with-external-arch    Store code in external archive (vs. embedded)."
    echo "--with-external-tar     Same as --with-external-arch."
    echo "--with-zlib             Enable Zlib support."
    echo "--without-zlib          Disable Zlib support."
    echo "--without-zip           Disable generation of archive in ZIP format."

    echo
    echo "Environment variables:"
    echo "CC                    C Compiler."
    echo "CPP                   C++ Compiler."
    echo "LD                    Linker."
    echo "AR                    Library Archiver."
    echo "CFLAGS                Compile flags (i.e., -O3)."
    echo "WINDRES               MS Windows resource compiler."

    exit 1
fi

chk_pkgconfig() {
    ${PKG_CONFIG} --cflags "$1" >/dev/null 2>&1
}

chk_compiles() {
    if $USE_CXX ; then
        MY_CC="$CPP"
        OUT=.tmp.cpp
    else
        MY_CC="$CC"
        OUT=.tmp.c
    fi
    printf "%s\n" "$1" > $OUT
    printf "# compiling testcase:\n%s\n" "$1" >> .config.log
    printf "%s\n" "$MY_CC $CFLAGS $TMP_CFLAGS $OUT $TMP_LDFLAGS -o .tmp.o" >> .config.log
    $MY_CC $CFLAGS $TMP_CFLAGS $OUT $TMP_LDFLAGS -o .tmp.o 2>> .config.log
}
USE_CXX=false

echo "Configuring..."

echo "/* automatically created by config.sh - do not modify */" > config.h
echo "# automatically created by config.sh - do not modify" > makefile.opts
> config.ldflags
> config.cflags
> .config.log

# set compiler
[ "$CC" = "" ] && CC="cc"

# set archiver
[ "$AR" = "" ] && AR="ar"

# set c++ compiler
[ "$CPP" = "" ] && CPP="c++"

# set linker
if [ "$LD" = "" ] ; then
    if which ld.bfd > /dev/null 2>&1 ; then
        LD="ld.bfd"
    else
        LD="ld"
    fi
fi

# set tar
[ "$TAR" = "" ] && TAR="tar"

# cc as a linker
[ "$CCLINK" = "" ] && CCLINK="$CC"

# set windres
[ "$WINDRES" = "" ] && WINDRES="windres"

# add version
echo "#include \"VERSION\"" >> config.h

# add installation prefix and application name
echo "#define CONFOPT_PREFIX \"$PREFIX\"" >> config.h
echo "#define CONFOPT_APPNAME \"$APPNAME\"" >> config.h

################################################################

# CFLAGS
if [ -z "$CFLAGS" ] ; then
    CFLAGS="-g -Wall"
fi

echo -n "Testing if C compiler supports ${CFLAGS}... "
if chk_compiles "int main(int argc, char *argv[]) { return 0; }" ; then
    echo "OK"
else
    echo "No; resetting to defaults"
    CFLAGS=""
fi

# MPDM
echo -n "Looking for MPDM... "

for MPDM in ./mpdm/ ../mpdm/ NOTFOUND ; do
    if [ -d $MPDM ] && [ -f $MPDM/mpdm.h ] ; then
        break
    fi
done

if [ "$MPDM" != "NOTFOUND" ] ; then
    echo "-I$MPDM" >> config.cflags
    echo "-L$MPDM -lmpdm" >> config.ldflags
    echo "OK ($MPDM)"
else
    echo "No"
    exit 1
fi

# MPSL
echo -n "Looking for MPSL... "

for MPSL in ./mpsl/ ../mpsl/ NOTFOUND ; do
    if [ -d $MPSL ] && [ -f $MPSL/mpsl.h ] ; then
        break
    fi
done

if [ "$MPSL" != "NOTFOUND" ] ; then
    echo "-I$MPSL" >> config.cflags
    echo "-L$MPSL -lmpsl" >> config.ldflags
    echo "OK ($MPSL)"
else
    echo "No"
    exit 1
fi

echo
(cd $MPSL && ./config.sh --prefix=$PREFIX --docdir=$PREFIX/share/doc/$APPNAME $CONF_ARGS)
echo

# import MPSL build configuration
[ -f "$MPSL.build.sh" ] && . $MPSL.build.sh

cat $MPDM/config.ldflags >> config.ldflags
cat $MPSL/config.ldflags >> config.ldflags

# Win32

echo -n "Testing for win32... "
if [ "$WITHOUT_WIN32" = "1" ] ; then
    echo "Disabled"
else
    grep CONFOPT_WIN32 ${MPDM}/config.h >/dev/null

    if [ $? = 0 ] ; then
        echo "-mwindows -lcomctl32" >> config.ldflags
        echo "#define CONFOPT_WIN32 1" >> config.h
        echo "OK"
        DRIVERS="win32 $DRIVERS"
        DRV_OBJS="mpv_win32.o mpv_win32c.o $DRV_OBJS"
        WITHOUT_UNIX_GLOB=1
        WITHOUT_KDE4=1
        WITHOUT_GTK=1
        WITHOUT_CURSES=1
        WITHOUT_QT4=1
        WITHOUT_ANSI=1
        TARGET="mp-5.exe mp-5c.exe"
        CFLAGS="$CFLAGS $TMP_CFLAGS"
        LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
        DRV_WIN32=1
    else
        echo "No"
    fi
fi

# test for curses / ncurses library
echo -n "Testing for ncursesw... "

if [ "$WITHOUT_CURSES" = "1" ] ; then
    echo "Disabled"
else
    if chk_pkgconfig ncursesw ; then
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags ncursesw)"
        TMP_LDFLAGS="$(${PKG_CONFIG} --libs ncursesw)"
    elif chk_pkgconfig ncurses ; then
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags ncurses)"
        TMP_LDFLAGS="$(${PKG_CONFIG} --libs ncurses)"
    elif type ncursesw6-config >/dev/null 2>&1 ; then
        TMP_CFLAGS="$(ncursesw6-config --cflags)"
        TMP_LDFLAGS="$(ncursesw6-config --libs)"
    elif type ncursesw5-config >/dev/null 2>&1 ; then
        TMP_CFLAGS="$(ncursesw5-config --cflags)"
        TMP_LDFLAGS="$(ncursesw5-config --libs)"
    elif type ncurses6-config >/dev/null 2>&1 ; then
        TMP_CFLAGS="$(ncurses6-config --cflags)"
        TMP_LDFLAGS="$(ncurses6-config --libs)"
    elif type ncurses5-config >/dev/null 2>&1 ; then
        TMP_CFLAGS="$(ncurses5-config --cflags)"
        TMP_LDFLAGS="$(ncurses5-config --libs)"
    else
        TMP_CFLAGS=
        TMP_LDFLAGS=-lncurses
    fi

    if chk_compiles "$(cat <<EOF
#include <ncurses.h>
int main(void) { initscr(); endwin(); return 0; }
EOF
)" ; then
        echo "#define CONFOPT_CURSES 1" >> config.h
        echo $TMP_CFLAGS >> config.cflags
        echo $TMP_LDFLAGS >> config.ldflags
        echo "OK (ncursesw)"
        DRIVERS="ncursesw $DRIVERS"
        DRV_OBJS="mpv_curses.o $DRV_OBJS"
        WITHOUT_ANSI=1
        DRV_CURSES=1
        CURSES_CFLAGS="$TMP_CFLAGS"
        CURSES_LDFLAGS="$TMP_LDFLAGS"
    else
        echo "#include <ncurses.h>" > .tmp.c
        echo "int main(void) { initscr(); endwin(); return 0; }" >> .tmp.c

        TMP_CFLAGS="-I/usr/local/include -I/usr/include/ncurses -I/usr/include/ncursesw"
        TMP_LDFLAGS="-L/usr/local/lib -lncurses"

        $CC $CFLAGS $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
        if [ $? = 0 ] ; then
            echo "#define CONFOPT_CURSES 1" >> config.h
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags
            echo "OK (ncurses)"
            DRIVERS="ncursesw $DRIVERS"
            DRV_OBJS="mpv_curses.o $DRV_OBJS"
            WITHOUT_ANSI=1
            DRV_CURSES=1
        else
            echo "No"
            WITHOUT_CURSES=1
        fi
    fi
    TMP_CFLAGS=
    TMP_LDFLAGS=
fi

if [ "$WITHOUT_CURSES" != "1" ] ; then
    # test for transparent colors in curses
    echo -n "Testing for transparency support in curses... "
    TMP_CFLAGS="$CURSES_CFLAGS"
    TMP_LDFLAGS="$CURSES_LDFLAGS"
    if chk_compiles "$(cat <<EOF
#include <ncurses.h>
int main(void) { initscr(); use_default_colors(); endwin(); return 0; }
EOF
)" ; then
        echo "#define CONFOPT_TRANSPARENCY 1" >> config.h
        echo "OK"
    else
        echo "No"
    fi

    # test now for wget_wch() existence
    echo -n "Testing for wget_wch()... "

    if chk_compiles "$(cat <<EOF
#include <wchar.h>
#include <ncurses.h>
int main(void) { wchar_t c[2]; initscr(); wget_wch(stdscr, c);
endwin(); return 0; }
EOF
)" ; then
        echo "#define CONFOPT_WGET_WCH 1" >> config.h
        echo "OK"
    else
        echo "No"
    fi
    TMP_CFLAGS=
    TMP_LDFLAGS=
fi

# ANSI
echo -n "Testing for ANSI terminal support... "

if [ "$WITHOUT_ANSI" = "1" ] ; then
    echo "Disabled"
else

    TMP_CFLAGS=""
    TMP_LDFLAGS=""

    if chk_compiles "$(cat <<EOF
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
int main(void) { struct termios o; tcgetattr(0, &o); return 0; }
EOF
)" ; then
        echo "#define CONFOPT_ANSI 1" >> config.h
        echo $TMP_CFLAGS >> config.cflags
        echo $TMP_LDFLAGS >> config.ldflags
        echo "OK"
        DRIVERS="ansi $DRIVERS"
        DRV_OBJS="mpv_ansi.o $DRV_OBJS"
        DRV_ANSI=1
    else
        echo "No"
        WITHOUT_ANSI=1
    fi
fi

# KDE4

echo -n "Testing for KDE4... "
if [ "$WITHOUT_KDE4" = "1" ] ; then
    echo "Disabled"
else
    if which kde4-config > /dev/null 2>&1
    then
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags QtGui)"
        TMP_CFLAGS="$TMP_CFLAGS -I`kde4-config --install include` -I`kde4-config --install include`KDE"

        TMP_LDFLAGS="$(${PKG_CONFIG} --libs QtGui) -lX11"
        TMP_LDFLAGS="$TMP_LDFLAGS -L`kde4-config --install lib` -L`kde4-config --install lib`/kde4/devel -lkio -lkfile -lkdeui -lkdecore"

        if USE_CXX=true chk_compiles "$(cat <<EOF
#include <KApplication>
int main(void) { new KApplication() ; return 0; }
EOF
)" ; then
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags

            echo "#define CONFOPT_KDE4 1" >> config.h
            echo "OK"

            DRIVERS="kde4 $DRIVERS"
            DRV_OBJS="mpv_kde4.o $DRV_OBJS"
            CCLINK="$CPP"

            WITHOUT_GTK=1
            WITHOUT_QT4=1

            CFLAGS="$CFLAGS $TMP_CFLAGS"
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
            DRV_KDE4=1
        else
            echo "No"
        fi
        TMP_CFLAGS=
        TMP_LDFLAGS=
    else
        echo "No"
    fi
fi

# Qt5

echo -n "Testing for Qt5... "
if [ "$WITHOUT_QT5" = "1" ] ; then
    echo "Disabled"
else
    if chk_pkgconfig Qt5Widgets
    then
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags Qt5Widgets 2>/dev/null) -fPIC"
        TMP_LDFLAGS="$(${PKG_CONFIG} --libs Qt5Widgets 2>/dev/null)"

        if USE_CXX=true chk_compiles "$(cat <<EOF
#include <QtWidgets>
int main(int argc, char *argv[]) { new QApplication(argc, argv) ; return 0; }
EOF
)" ; then
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags

            echo "#define CONFOPT_QT5 1" >> config.h
            echo "OK"

            DRIVERS="qt5 $DRIVERS"
            DRV_OBJS="mpv_qt4.o $DRV_OBJS"
            CCLINK="$CPP"

            WITHOUT_QT4=1
            WITHOUT_GTK=1

            [ -z "$MOC" ] && which moc-qt5 > /dev/null 2>&1 && MOC=moc-qt5
            [ -z "$MOC" ] && MOC="moc -qt5"

            CFLAGS="$CFLAGS $TMP_CFLAGS"
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
            DRV_QT4=1
        else
            echo "No"
        fi
        TMP_CFLAGS=
        TMP_LDFLAGS=
    else
        echo "No"
    fi
fi


# Qt4

echo -n "Testing for Qt4... "
if [ "$WITHOUT_QT4" = "1" ] ; then
    echo "Disabled"
else
    if chk_pkgconfig QtGui > /dev/null 2>&1
    then
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags QtGui 2>/dev/null)"
        TMP_LDFLAGS="$(${PKG_CONFIG} --libs QtGui 2>/dev/null) -lX11"

        if USE_CXX=true chk_compiles "$(cat <<EOF
#include <QtGui>
int main(int argc, char *argv[]) { new QApplication(argc, argv) ; return 0; }
EOF
)" ; then
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags

            echo "#define CONFOPT_QT4 1" >> config.h
            echo "OK"

            DRIVERS="qt4 $DRIVERS"
            DRV_OBJS="mpv_qt4.o $DRV_OBJS"
            CCLINK="$CPP"

            WITHOUT_GTK=1

            [ -z "$MOC" ] && which moc-qt4 > /dev/null 2>&1 && MOC=moc-qt4
            [ -z "$MOC" ] && MOC="moc -qt4"

            CFLAGS="$CFLAGS $TMP_CFLAGS"
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
            DRV_QT4=1
        else
            echo "No"
        fi
        TMP_CFLAGS=
        TMP_LDFLAGS=
    else
        echo "No"
    fi
fi

# GTK
echo -n "Testing for GTK... "

if [ "$WITHOUT_GTK" = "1" ] ; then
    echo "Disabled"
else
    # Try first GTK 3.0
    if chk_pkgconfig gtk+-3.0 ; then
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags gtk+-3.0 2>/dev/null)"
        TMP_LDFLAGS="$(${PKG_CONFIG} --libs gtk+-3.0 2>/dev/null)"

        if chk_compiles "$(cat <<EOF
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
int main(void) { gtk_main(); return 0; }
EOF
)" ; then
            echo "#define CONFOPT_GTK 3" >> config.h
            echo "$TMP_CFLAGS " >> config.cflags
            echo "$TMP_LDFLAGS " >> config.ldflags
            echo "OK (3.0)"
            DRIVERS="gtk $DRIVERS"
            DRV_OBJS="mpv_gtk.o $DRV_OBJS"
            CFLAGS="$CFLAGS $TMP_CFLAGS"
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
            DRV_GTK=1
        fi
        TMP_CFLAGS=
        TMP_LDFLAGS=
    fi
    if test "$DRV_GTK" != 1 && chk_pkgconfig gtk+-2.0 ; then
        # Try now GTK 2.0
        TMP_CFLAGS="$(${PKG_CONFIG} --cflags gtk+-2.0 2>/dev/null)"
        TMP_LDFLAGS="$(${PKG_CONFIG} --libs gtk+-2.0 2>/dev/null)"

        if chk_compiles "$(cat <<EOF
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
int main(void) { gtk_main(); return 0; }
EOF
)" ; then
            echo "#define CONFOPT_GTK 2" >> config.h
            echo "$TMP_CFLAGS " >> config.cflags
            echo "$TMP_LDFLAGS " >> config.ldflags
            echo "OK (2.0)"
            DRIVERS="gtk $DRIVERS"
            DRV_OBJS="mpv_gtk.o $DRV_OBJS"
            CFLAGS="$CFLAGS $TMP_CFLAGS"
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
            DRV_GTK=1
        fi
        TMP_CFLAGS=
        TMP_LDFLAGS=
    fi
    if test "$DRV_GTK" != 1 ; then
        echo "No"
    fi
fi

# msgfmt
echo -n "Testing for msgfmt... "

if [ "$WITHOUT_MSGFMT" = "1" ] ; then
    echo "Disabled"
    echo "BUILDMO=" >> makefile.opts
    echo "INSTALLMO=" >> makefile.opts
    echo "UNINSTALLMO=" >> makefile.opts
else
    if which msgfmt > /dev/null 2>&1 ; then
        echo "OK"
        echo "BUILDMO=build-mo" >> makefile.opts
        echo "INSTALLMO=install-mo" >> makefile.opts
        echo "UNINSTALLMO=uninstall-mo" >> makefile.opts
    else
        echo "No"
        echo "BUILDMO=" >> makefile.opts
        echo "INSTALLMO=" >> makefile.opts
        echo "UNINSTALLMO=" >> makefile.opts
    fi
fi

# xgettext
echo -n "Testing for xgettext... "

if which xgettext > /dev/null 2>&1 ; then
    echo "OK"
    echo "XGETTEXT_TARGETS=po/minimum-profit.pot" >> makefile.opts
else
    echo "No"
fi

# zip

echo -n "Testing for zip... "

if [ "$WITHOUT_ZIP" = "1" ] ; then
    echo "Disabled"
else
    if command -v zip > /dev/null ; then
        echo "Yes"
    else
        echo "No"
        WITHOUT_ZIP=1
    fi
fi

if [ "$WITHOUT_ZIP" = "1" ] ; then
    ARCH_OBJ="mp.tar.o"
    ARCH_SYM="mp_tar"
else
    ARCH_OBJ="mp.zip.o"
    ARCH_SYM="mp_zip"
    echo "#define CONFOPT_WITH_ZIP 1" >> config.h
fi

# test for embeddable binaries
echo -n "Testing for embeddable binaries... "
if [ "$WITH_EXTERNAL_ARCH" = "1" ] ; then
    echo "Disabled"
else
    echo "test" > tmp.bin
    $LD -r -b binary tmp.bin -o tmp.bin.o

    if chk_compiles "$(cat <<EOF
extern const char _binary_tmp_bin_start;
extern const char _binary_tmp_bin_end;
extern const char binary_tmp_bin_start;
extern const char binary_tmp_bin_end;
int main(void) {
#ifdef CONFOPT_EMBED_NOUNDER
  int c = &binary_tmp_bin_end - &binary_tmp_bin_start;
#else
  int c = &_binary_tmp_bin_end - &_binary_tmp_bin_start;
#endif
  return c;
}
EOF
)" ; then
        echo "Yes (with underscores)"
        MORE_OBJS="${ARCH_OBJ} ${MORE_OBJS}"
        echo "extern const char _binary_${ARCH_SYM}_start;" >> config.h
        echo "extern const char _binary_${ARCH_SYM}_end;" >> config.h
        echo "#define ARCH_START &_binary_${ARCH_SYM}_start" >> config.h
        echo "#define ARCH_END &_binary_${ARCH_SYM}_end" >> config.h
    else
        $CC $CFLAGS -DCONFOPT_EMBED_NOUNDER .tmp.c tmp.bin.o -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "Yes (without underscores)"
            MORE_OBJS="${ARCH_OBJ} ${MORE_OBJS}"
            echo "#define CONFOPT_EMBED_NOUNDER 1" >> config.h
            echo "extern const char binary_${ARCH_SYM}_start;" >> config.h
            echo "extern const char binary_${ARCH_SYM}_end;" >> config.h
            echo "#define ARCH_START &binary_${ARCH_SYM}_start" >> config.h
            echo "#define ARCH_END &binary_${ARCH_SYM}_end" >> config.h
        else
            echo "No"
            WITH_EXTERNAL_ARCH=1
            LD=""
        fi
    fi

    rm -f tmp.bin tmp.bin.o
fi

if [ "$WITH_EXTERNAL_ARCH" = "1" ] ; then
    echo "#define CONFOPT_EXTERNAL_ARCH 1" >> config.h
    echo "#define ARCH_START NULL" >> config.h
    echo "#define ARCH_END NULL" >> config.h
    MORE_TARGETS="mp.tar"
    MORE_INSTALL_TARGETS="install-tar $MORE_INSTALL_TARGETS"
fi

echo >> config.h

if [ "$TARGET" = "" ] ; then
    TARGET="$APPNAME"
fi

echo "CC=$CC" >> makefile.opts
echo "CPP=$CPP" >> makefile.opts
echo "LD=$LD" >> makefile.opts
echo "TAR=$TAR" >> makefile.opts
echo "CFLAGS=$CFLAGS" >> makefile.opts
echo "MPDM=$MPDM" >> makefile.opts
echo "MPSL=$MPSL" >> makefile.opts
echo "MOC=$MOC" >> makefile.opts
echo "CCLINK=$CCLINK" >> makefile.opts
echo "WINDRES=$WINDRES" >> makefile.opts

grep DOCS $MPDM/makefile.opts >> makefile.opts
echo "VERSION=$VERSION" >> makefile.opts
echo "PREFIX=\$(DESTDIR)$PREFIX" >> makefile.opts
echo "APPNAME=$APPNAME" >> makefile.opts
echo "TARGET=$TARGET" >> makefile.opts
echo "DRV_OBJS=$DRV_OBJS" >> makefile.opts
echo "MORE_OBJS=$MORE_OBJS" >> makefile.opts
echo "MORE_TARGETS=$MORE_TARGETS" >> makefile.opts
echo "MORE_INSTALL_TARGETS=$MORE_INSTALL_TARGETS" >> makefile.opts
echo "CONF_ARGS=$CONF_ARGS" >> makefile.opts
echo >> makefile.opts

cat makefile.opts makefile.in makefile.depend > Makefile

rm -f .build.sh
echo "CC='$CC'" >> .build.sh
echo "CPP='$CPP'" >> .build.sh
echo "LD='$LD'" >> .build.sh
echo "TAR='$TAR'" >> .build.sh
echo "CFLAGS='$CFLAGS'" >> .build.sh
echo "LDFLAGS='$LDFLAGS'" >> .build.sh
echo "MPDM='$MPDM'" >> .build.sh
echo "MPSL='$MPSL'" >> .build.sh
echo "MOC='$MOC'" >> .build.sh
echo "CCLINK='$CCLINK'" >> .build.sh
echo "WINDRES='$WINDRES'" >> .build.sh

echo "DRV_WIN32='$DRV_WIN32'" >> .build.sh
echo "DRV_WIN32C='$DRV_WIN32C'" >> .build.sh
echo "DRV_CURSES='$DRV_CURSES'" >> .build.sh
echo "DRV_ANSI='$DRV_ANSI'" >> .build.sh
echo "DRV_KDE4='$DRV_KDE4'" >> .build.sh
echo "DRV_QT4='$DRV_QT4'" >> .build.sh
echo "DRV_GTK='$DRV_GTK'" >> .build.sh

##############################################

if [ "$DRIVERS" = "" ] ; then

    echo
    echo "*ERROR* No usable drivers (interfaces) found"
    echo "See the README file for the available options."

    exit 1
fi

echo
echo "Configured drivers:" $DRIVERS
echo
echo "Type 'make' to build Minimum Profit."

# insert driver detection code into config.h

TRY_DRIVERS="#define TRY_DRIVERS() ("
echo >> config.h
for drv in $DRIVERS ; do
    echo "int ${drv}_drv_detect(int * argc, char *** argv);" >> config.h
    TRY_DRIVERS="$TRY_DRIVERS ${drv}_drv_detect(&argc, &argv) || "
done

echo >> config.h
echo $TRY_DRIVERS '0)' >> config.h

# cleanup
rm -f .tmp.c .tmp.cpp .tmp.o

exit 0
