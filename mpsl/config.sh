#!/bin/sh

# Configuration shell script

TARGET=mpsl

# gets program version
VERSION=`cut -f2 -d\" VERSION`

# default installation prefix
PREFIX=/usr/local

# installation directory for documents
DOCDIR=""

# store command line args for configuring the libraries
CONF_ARGS="$*"

MINGW32_PREFIX=i686-w64-mingw32

C_MPSL_OBJS="mpsl_l.o mpsl_y.o"
C_MPSL_SRCS="y.tab.c y.tab.h lex.yy.c"

CONFOPT_DEFAULT_COMPILER="a_mpsl"

CONFOPT_DISABLE_CLASSIC_COMPILER=1

# parse arguments
while [ $# -gt 0 ] ; do

    case $1 in
    --help)         CONFIG_HELP=1 ;;

    --mingw32-prefix=*)     MINGW32_PREFIX=`echo $1 | sed -e 's/--mingw32-prefix=//'`
                            ;;

    --mingw32)      CC=${MINGW32_PREFIX}-gcc
                    WINDRES=${MINGW32_PREFIX}-windres
                    AR=${MINGW32_PREFIX}-ar
                    LD=${MINGW32_PREFIX}-ld
                    CPP=${MINGW32_PREFIX}-g++
                    ;;

    --prefix)       PREFIX=$2 ; shift ;;
    --prefix=*)     PREFIX=`echo $1 | sed -e 's/--prefix=//'` ;;

    --docdir)       DOCDIR=$2 ; shift ;;
    --docdir=*)     DOCDIR=`echo $1 | sed -e 's/--docdir=//'` ;;

    --compiler)     CONFOPT_DEFAULT_COMPILER=$2 ; shift ;;
    --compiler=*)   CONFOPT_DEFAULT_COMPILER=`echo $1 | sed -e 's/--compiler=//'` ;;

    --enable-classic-compiler)
                    CONFOPT_DISABLE_CLASSIC_COMPILER=0 ;;
    --disable-classic-compiler)
                    CONFOPT_DISABLE_CLASSIC_COMPILER=1 ;;

    esac

    shift
done

if [ "$CONFIG_HELP" = "1" ] ; then

    echo "Available options:"
    echo "--prefix=PREFIX            Installation prefix ($PREFIX)."
    echo "--docdir=DOCDIR            Instalation directory for documentation."
    echo "--compiler=COMPILER        Set default compiler. Values can be:"
    echo "                           c_mpsl (classic compiler, needs flex and yacc/bison)"
    echo "                           a_mpsl (ad-hoc compiler)"
    echo "--mingw32-prefix=PREFIX    Prefix name for mingw32 ($MINGW32_PREFIX)."
    echo "--mingw32                  Build using the mingw32 compiler."
    echo "--enable-classic-compiler  Enable classic compiler detection."
    echo "--disable-classic-compiler Disable classic compiler detection."

    echo
    echo "Environment variables:"
    echo "CC                    C Compiler."
    echo "AR                    Library Archiver."
    echo "YACC                  Parser."

    exit 1
fi

if [ "$DOCDIR" = "" ] ; then
    DOCDIR=$PREFIX/share/doc/mpsl
fi

echo "Configuring MPSL..."

echo "/* automatically created by config.sh - do not modify */" > config.h
echo "# automatically created by config.sh - do not modify" > makefile.opts
> config.ldflags
> config.cflags
> .config.log

# set compiler
[ "$CC" = "" ] && CC="cc"

# set archiver
[ "$AR" = "" ] && AR="ar"

# set parser
[ "$YACC" = "" ] && YACC="yacc"


# add version
echo "#include \"VERSION\"" >> config.h

# add installation prefix
echo "#define CONFOPT_PREFIX \"$PREFIX\"" >> config.h

#########################################################

# configuration directives

# CFLAGS
if [ -z "$CFLAGS" ] ; then
    CFLAGS="-g -Wall"
fi

echo -n "Testing if C compiler supports ${CFLAGS}... "
echo "int main(int argc, char *argv[]) { return 0; }" > .tmp.c

$CC .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "OK"
else
    echo "No; resetting to defaults"
    CFLAGS=""
fi

# MPDM
echo -n "Looking for MPDM... "

for MPDMi in ./mpdm/ ../mpdm/ NOTFOUND ; do
    if [ -d $MPDMi ] && [ -f $MPDMi/mpdm.h ] ; then
        break
    fi
done

if [ "$MPDMi" != "NOTFOUND" ] ; then
    echo "-I$MPDMi" >> config.cflags
    echo "-L$MPDMi -lmpdm" >> config.ldflags
    echo "OK ($MPDMi)"
else
    echo "No"
    exit 1
fi

echo
(cd $MPDMi ; ./config.sh $CONF_ARGS)
echo

# import MPDM build configuration
. $MPDMi.build.sh

LDFLAGS="$LDFLAGS -lm"

# test if flex and yacc are available
echo -n "Testing for flex and $YACC... "
if [ "$CONFOPT_DISABLE_CLASSIC_COMPILER" = "1" ] ; then
    echo "Disabled"
    echo "#define CONFOPT_WITHOUT_C_MPSL 1" >> config.h
    C_MPSL_OBJS=""
    C_MPSL_SRCS=""
    CONFOPT_DEFAULT_COMPILER="a_mpsl"
    YACC=""
else
    if ! (which flex && which $YACC) > /dev/null 2>&1 ; then
        echo "No; classic compiler not available"
        echo "#define CONFOPT_WITHOUT_C_MPSL 1" >> config.h
        C_MPSL_OBJS=""
        C_MPSL_SRCS=""
        CONFOPT_DEFAULT_COMPILER="a_mpsl"
        YACC=""
    else
        echo "Found; classic compiler is available"
    fi
fi

if [ ! -z "$CONFOPT_DEFAULT_COMPILER" ] ; then
    echo "#define CONFOPT_DEFAULT_COMPILER \"$CONFOPT_DEFAULT_COMPILER\"" >> config.h
fi

cat $MPDMi/config.ldflags >> config.ldflags
echo "-lm" >> config.ldflags

# if win32, the interpreter is called mpsl.exe
grep CONFOPT_WIN32 ${MPDMi}/config.h >/dev/null && TARGET=${TARGET}.exe


#########################################################

# final setup

echo "CC=$CC" >> makefile.opts
echo "AR=$AR" >> makefile.opts
echo "CFLAGS=$CFLAGS" >> makefile.opts
echo "YACC=$YACC" >> makefile.opts

echo "MPDMi=$MPDMi" >> makefile.opts
grep DOCS $MPDMi/makefile.opts >> makefile.opts
echo "VERSION=$VERSION" >> makefile.opts
echo "PREFIX=\$(DESTDIR)$PREFIX" >> makefile.opts
echo "DOCDIR=\$(DESTDIR)$DOCDIR" >> makefile.opts
echo "CONF_ARGS=$CONF_ARGS" >> makefile.opts
echo "TARGET=$TARGET" >> makefile.opts
echo "C_MPSL_OBJS=$C_MPSL_OBJS" >> makefile.opts
echo "C_MPSL_SRCS=$C_MPSL_SRCS" >> makefile.opts
echo >> makefile.opts

cat makefile.opts makefile.in makefile.depend > Makefile

rm -f .build.sh
echo "CC='$CC'" >> .build.sh
echo "AR='$AR'" >> .build.sh
echo "YACC='$YACC'" >> .build.sh
echo "CFLAGS='$CFLAGS'" >> .build.sh
echo "LDFLAGS='$LDFLAGS'" >> .build.sh
echo "MPDMi='$MPDMi'" >> .build.sh

#########################################################

# cleanup

rm -f .tmp.c .tmp.o

exit 0
