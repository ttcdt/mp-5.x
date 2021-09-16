#!/bin/sh

# Configuration shell script

# gets program version
VERSION=`cut -f2 -d\" VERSION`

# default installation prefix
PREFIX=/usr/local

# installation directory for documents
DOCDIR=""

# store this script arguments for later
CONF_ARGS="$*"

MINGW32_PREFIX=i686-w64-mingw32

WITHOUT_ZLIB=1

# parse arguments
while [ $# -gt 0 ] ; do

    case $1 in
    --without-win32)        WITHOUT_WIN32=1 ;;
    --without-unix-glob)    WITHOUT_UNIX_GLOB=1 ;;
    --with-included-regex)  WITH_INCLUDED_REGEX=1 ;;
    --with-pcre)            WITH_PCRE=1 ;;
    --without-gettext)      WITHOUT_GETTEXT=1 ;;
    --without-iconv)        WITHOUT_ICONV=1 ;;
    --without-wcwidth)      WITHOUT_WCWIDTH=1 ;;
    --without-zlib)         WITHOUT_ZLIB=1 ;;
    --with-zlib)            WITHOUT_ZLIB=0 ;;
    --without-pwd)          WITHOUT_PWD=1 ;;
    --help)                 CONFIG_HELP=1 ;;

    --mingw32-prefix=*)     MINGW32_PREFIX=`echo $1 | sed -e 's/--mingw32-prefix=//'`
                            ;;

    --mingw32)              CC=${MINGW32_PREFIX}-gcc
                            WINDRES=${MINGW32_PREFIX}-windres
                            AR=${MINGW32_PREFIX}-ar
                            LD=${MINGW32_PREFIX}-ld
                            CPP=${MINGW32_PREFIX}-g++
                            ;;

    --prefix)               PREFIX=$2 ; shift ;;
    --prefix=*)             PREFIX=`echo $1 | sed -e 's/--prefix=//'` ;;

    --docdir)               DOCDIR=$2 ; shift ;;
    --docdir=*)             DOCDIR=`echo $1 | sed -e 's/--docdir=//'` ;;

    esac

    shift
done

if [ "$CONFIG_HELP" = "1" ] ; then

    echo "Available options:"
    echo "--prefix=PREFIX         Installation prefix ($PREFIX)."
    echo "--docdir=DOCDIR         Instalation directory for documentation."
    echo "--without-win32         Disable win32 interface detection."
    echo "--without-unix-glob     Disable glob.h usage (use workaround)."
    echo "--with-included-regex   Use included regex code (gnu_regex.c)."
    echo "--with-pcre             Enable PCRE library detection."
    echo "--without-gettext       Disable gettext usage."
    echo "--without-iconv         Disable iconv usage."
    echo "--without-wcwidth       Disable system wcwidth() (use Marcus Kuhn's)."
    echo "--mingw32-prefix=PREFIX Prefix name for mingw32 ($MINGW32_PREFIX)."
    echo "--mingw32               Build using the mingw32 compiler."
    echo "--with-zlib             Enable Zlib support."
    echo "--without-zlib          Disable Zlib support."
    echo "--without-pwd           Disable pwd.h and getpwuid()."
    echo
    echo "Environment variables:"
    echo "CC                    C Compiler."
    echo "AR                    Library Archiver."

    exit 1
fi

if [ "$DOCDIR" = "" ] ; then
    DOCDIR=$PREFIX/share/doc/mpdm
fi

echo "Configuring MPDM..."

echo "/* automatically created by config.sh - do not modify */" > config.h
echo "# automatically created by config.sh - do not modify" > makefile.opts
> config.ldflags
> config.cflags
> .config.log

# set compiler
[ "$CC" = "" ] && CC="cc"

# set archiver
[ "$AR" = "" ] && AR="ar"

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

# Win32
echo -n "Testing for win32... "
if [ "$WITHOUT_WIN32" = "1" ] ; then
    echo "Disabled by user"
else
    echo "#include <windows.h>" > .tmp.c
    echo "#include <commctrl.h>" >> .tmp.c
    echo "int CALLBACK WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int m)" >> .tmp.c
    echo "{ return 0; }" >> .tmp.c

    TMP_LDFLAGS="-lws2_32"
    $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_WIN32 1" >> config.h
        echo "OK"
        WITHOUT_UNIX_GLOB=1
        WITH_WIN32=1
        echo $TMP_LDFLAGS >> config.ldflags

        LDFLAGS="$LDFLAGS $TMP_LDFLAGS"

        echo -n "Testing for getaddrinfo() in winsock... "
        echo "#include <winsock2.h>" > .tmp.c
        echo "#include <ws2tcpip.h>" >> .tmp.c

        echo "int STDCALL WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int m)" >> .tmp.c
        echo "{ struct addrinfo *res; struct addrinfo hints; " >> .tmp.c
        echo "getaddrinfo(\"google.es\", \"www\", &hints, &res);" >> .tmp.c

        echo "return 0; }" >> .tmp.c

        $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "OK"
        else
            echo "No"
            echo "#define CONFOPT_WITHOUT_GETADDRINFO 1" >> config.h
        fi
    else
        echo "No"
    fi
fi

# glob.h support
if [ "$WITHOUT_UNIX_GLOB" != 1 ] ; then
    echo -n "Testing for unix-like glob.h... "
    echo "#include <stdio.h>" > .tmp.c
    echo "#include <glob.h>" >> .tmp.c
    echo "int main(void) { glob_t g; g.gl_offs=1; glob(\"*\",GLOB_MARK,NULL,&g); return 0; }" >> .tmp.c

    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_GLOB_H 1" >> config.h
        echo "OK"
    else
        echo "No; activating workaround"
    fi
fi

# regex
echo -n "Testing for regular expressions... "

if [ "$WITH_PCRE" = 1 ] ; then
    # try first the pcre library
    TMP_CFLAGS="-I/usr/local/include"
    TMP_LDFLAGS="-L/usr/local/lib -lpcre -lpcreposix"
    echo "#include <pcreposix.h>" > .tmp.c
    echo "int main(void) { regex_t r; regmatch_t m; regcomp(&r,\".*\",REG_EXTENDED|REG_ICASE); return 0; }" >> .tmp.c

    $CC $CFLAGS $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "OK (using pcre library)"
        echo "#define CONFOPT_PCRE 1" >> config.h
        echo "$TMP_CFLAGS " >> config.cflags
        echo "$TMP_LDFLAGS " >> config.ldflags
        LDFLAGS="$LDFLAGS $TMP_LDFLAGS"

        REGEX_YET=1
    fi
fi

if [ "$REGEX_YET" != 1 -a "$WITH_INCLUDED_REGEX" != 1 ] ; then
    echo "#include <sys/types.h>" > .tmp.c
    echo "#include <regex.h>" >> .tmp.c
    echo "int main(void) { regex_t r; regmatch_t m; regcomp(&r,\".*\",REG_EXTENDED|REG_ICASE); return 0; }" >> .tmp.c

    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "OK (using system one)"
        echo "#define CONFOPT_SYSTEM_REGEX 1" >> config.h
        REGEX_YET=1
    fi
fi

if [ "$REGEX_YET" != 1 ] ; then
    # if system libraries lack regex, try compiling the
    # included gnu_regex.c

    $CC $CFLAGS -c -DSTD_HEADERS -DREGEX gnu_regex.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "OK (using included gnu_regex.c)"
        echo "#define HAVE_STRING_H 1" >> config.h
        echo "#define REGEX 1" >> config.h
        echo "#define CONFOPT_INCLUDED_REGEX 1" >> config.h
    else
        echo "#define CONFOPT_NO_REGEX 1" >> config.h
        echo "No (No usable regex library)"
        exit 1
    fi
fi

# unistd.h detection
echo -n "Testing for unistd.h... "
echo "#include <unistd.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_UNISTD_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# sys/types.h detection
echo -n "Testing for sys/types.h... "
echo "#include <sys/types.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_SYS_TYPES_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# sys/wait.h detection
echo -n "Testing for sys/wait.h... "
echo "#include <sys/wait.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_SYS_WAIT_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# sys/stat.h detection
echo -n "Testing for sys/stat.h... "
echo "#include <sys/stat.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_SYS_STAT_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# sys/file.h detection
echo -n "Testing for sys/file.h... "
echo "#include <sys/file.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_SYS_FILE_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# pwd.h detection
echo -n "Testing for pwd.h... "
if [ "$WITHOUT_PWD" = "1" ] ; then
    echo "Disabled by user"
else
    echo "#include <pwd.h>" > .tmp.c
    echo "#include <unistd.h>" >> .tmp.c
    echo "int main(void) { struct passwd *p; p = getpwuid(getpid()); return(p != NULL); }" >> .tmp.c

    # NOTE: when setting CFLAGS=-static, the executable crashes
    # because of getpwuid(). The compiler (somewhat) warns about this,
    # but that warning is not converted to an error with -Werror,
    # so it cannot be detected without using acrobatics on
    # the compiler's output (that will also be too error-prone)
    $CC -Wall -Wextra -Werror $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_PWD_H 1" >> config.h 
        echo "OK"
    else
        echo "No"
    fi
fi

# sys/socket.h detection
echo -n "Testing for sys/socket.h... "
echo "#include <sys/socket.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_SYS_SOCKET_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# netdb.h detection
echo -n "Testing for netdb.h... "
echo "#include <netdb.h>" > .tmp.c
echo "int main(void) { return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_NETDB_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# netinet/in.h detection
echo -n "Testing for netinet/in.h... "
echo "#include <netinet/in.h>" > .tmp.c
echo "int main(void) { struct sockaddr_in si; return(0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_NETINET_IN_H 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# chown() detection
echo -n "Testing for chown()... "
echo "#include <sys/types.h>" > .tmp.c
echo "#include <unistd.h>" >> .tmp.c
echo "int main(void) { chown(\"file\", 0, 0); }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_CHOWN 1" >> config.h
    echo "OK"
else
    echo "No"
fi

# gettext support
echo -n "Testing for gettext... "

if [ "$WITHOUT_GETTEXT" = "1" ] ; then
    echo "Disabled by user"
else
    echo "#include <libintl.h>" > .tmp.c
    echo "#include <locale.h>" >> .tmp.c
    echo "int main(void) { setlocale(LC_ALL, \"\"); gettext(\"hi\"); return 0; }" >> .tmp.c

    # try first to compile without -lintl
    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "OK"
        echo "#define CONFOPT_GETTEXT 1" >> config.h
    else
        # try now with -lintl
        TMP_LDFLAGS="-lintl"

        $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "OK (libintl needed)"
            echo "#define CONFOPT_GETTEXT 1" >> config.h
            echo "$TMP_LDFLAGS" >> config.ldflags
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
        else
            echo "No"
        fi
    fi
fi


# iconv support
echo -n "Testing for iconv... "

if [ "$WITHOUT_ICONV" = "1" ] ; then
    echo "Disabled by user"
else
    echo "#include <iconv.h>" > .tmp.c
    echo "#include <locale.h>" >> .tmp.c
    echo "int main(void) { setlocale(LC_ALL, \"\"); iconv_open(\"WCHAR_T\", \"ISO-8859-1\"); return 0; }" >> .tmp.c

    # try first to compile without -liconv
    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "OK"
        echo "#define CONFOPT_ICONV 1" >> config.h
    else
        # try now with -liconv
        TMP_LDFLAGS="-liconv"

        $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "OK (libiconv needed)"
            echo "#define CONFOPT_ICONV 1" >> config.h
            echo "$TMP_LDFLAGS" >> config.ldflags
            LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
        else
            echo "No"
        fi
    fi
fi

# wcwidth() existence
echo -n "Testing for wcwidth()... "

if [ "$WITHOUT_WCWIDTH" = "1" ] ; then
    echo "Disabled by user (using Markus Kuhn's)"
else
    echo "#include <wchar.h>" > .tmp.c
    echo "int main(void) { wcwidth(L'a'); return 0; }" >> .tmp.c

    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "OK"
        echo "#define CONFOPT_WCWIDTH 1" >> config.h
    else
        echo "No; using Markus Kuhn's wcwidth()"
    fi
fi

# canonicalize_file_name() detection
echo -n "Testing for file canonicalization... "
echo "#include <stdlib.h>" > .tmp.c
echo "int main(void) { canonicalize_file_name(\"file\"); return 0; }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_CANONICALIZE_FILE_NAME 1" >> config.h
    echo "canonicalize_file_name()"
else
    # try now realpath
    echo "#include <stdlib.h>" > .tmp.c
    echo "int main(void) { char tmp[2048]; realpath(\"file\", tmp); return 0; }" >> .tmp.c

    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_REALPATH 1" >> config.h
        echo "realpath()"
    else
        # try now _fullpath
        echo "#include <stdlib.h>" > .tmp.c
        echo "int main(void) { char tmp[2048]; _fullpath(tmp, \"file\", _MAX_PATH); return 0; }" >> .tmp.c

        $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "#define CONFOPT_FULLPATH 1" >> config.h
            echo "_fullpath()"
        else
            echo "No"
        fi
    fi
fi

if [ "$WITH_WIN32" != 1 ] ; then
    echo -n "Testing for nanosleep()... "
    echo "#include <time.h>" > .tmp.c
    echo "int main(void) { struct timespec ts; ts.tv_sec = 1; ts.tv_nsec = 0; nanosleep(&ts, NULL); return 0; }" >> .tmp.c

    $CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_NANOSLEEP 1" >> config.h
        echo "OK"
    else
        echo "No"
    fi
fi

echo -n "Testing for strptime()... "
echo "#include <time.h>" > .tmp.c
echo "int main(void) { struct tm tm; char tmp[256] = \"01\"; strptime(\"%h\", tmp, &tm); return 0; }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_STRPTIME 1" >> config.h
    echo "OK"
else
    echo "No"
fi

echo -n "Testing for malloc/malloc.h... "
echo "#include <malloc/malloc.h>" > .tmp.c
echo "int main(void) { return 0; }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_MALLOC_MALLOC_H 1" >> config.h
    echo "OK"
else
    echo "No (assuming malloc.h in standard place)"
fi

echo -n "Testing for gettimeofday()... "
echo "#include <sys/time.h>" > .tmp.c
echo "int main(void) { struct timeval tv; gettimeofday(&tv, 0); return 0; }" >> .tmp.c

$CC $CFLAGS .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "#define CONFOPT_GETTIMEOFDAY 1" >> config.h
    echo "OK"
else
    echo "No"
fi

echo -n "Testing for zlib... "

if [ "$WITHOUT_ZLIB" = "1" ] ; then
    echo "Disabled"
else
    echo "#include <zlib.h>" > .tmp.c
    echo "int main(void) { z_stream zs; return 0; }" >> .tmp.c

    TMP_LDFLAGS="-lz"

    $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_ZLIB 1" >> config.h
        echo "$TMP_LDFLAGS" >> config.ldflags
        LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
        echo "OK"
    else
        echo "No"
    fi
fi

if [ "$WITH_WIN32" != 1 ] ; then
    echo -n "Testing for POSIX threads... "
    echo "#include <pthread.h>" > .tmp.c
    echo "void *f(void *p) { return p; } int main(void) { pthread_t t; pthread_create(&t, NULL, f, NULL); return 0; }" >> .tmp.c

    TMP_LDFLAGS="-pthread"
    $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_PTHREADS 1" >> config.h
        echo $TMP_LDFLAGS >> config.ldflags
        LDFLAGS="$LDFLAGS $TMP_LDFLAGS"
        WITH_PTHREADS=1
        echo "OK"
    else
        echo "No"
    fi
fi

if [ "$WITH_WIN32" != 1 -a "$WITH_PTHREADS" = 1 ] ; then
    echo -n "Testing for POSIX semaphores... "
    echo "#include <semaphore.h>" > .tmp.c
    echo "int main(void) { sem_t s; sem_init(&s, 0, 0); return 0; }" >> .tmp.c

    TMP_LDFLAGS="-pthread"
    $CC $CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "#define CONFOPT_POSIXSEMS 1" >> config.h
        echo "OK"
    else
        echo "No"
    fi
fi

# test for Grutatxt
echo -n "Testing if Grutatxt is installed... "

DOCS="\$(ADD_DOCS)"

if which grutatxt > /dev/null 2>&1 ; then
    echo "OK"
    echo "GRUTATXT=yes" >> makefile.opts
    DOCS="$DOCS \$(GRUTATXT_DOCS)"
else
    echo "No"
    echo "GRUTATXT=no" >> makefile.opts
fi

# test for mp_doccer
echo -n "Testing if mp_doccer is installed... "
MP_DOCCER=$(which mp_doccer > /dev/null 2>&1||which mp-doccer > /dev/null 2>&1)

if [ $? = 0 ] ; then

    if ${MP_DOCCER} --help | grep grutatxt > /dev/null ; then

        echo "OK"

        echo "MP_DOCCER=yes" >> makefile.opts
        DOCS="$DOCS \$(MP_DOCCER_DOCS)"

        grep GRUTATXT=yes makefile.opts > /dev/null && DOCS="$DOCS \$(G_AND_MP_DOCS)"
    else
        echo "Outdated (No)"
        echo "MP_DOCCER=no" >> makefile.opts
    fi
else
    echo "No"
    echo "MP_DOCCER=no" >> makefile.opts
fi

if [ "$WITH_NULL_HASH" = "1" ] ; then
    echo "Selecting NULL hash function"

    echo "#define CONFOPT_NULL_HASH 1" >> config.h
fi

#########################################################

# final setup

echo "CC=$CC" >> makefile.opts
echo "AR=$AR" >> makefile.opts
echo "CFLAGS=$CFLAGS" >> makefile.opts

echo "DOCS=$DOCS" >> makefile.opts
echo "VERSION=$VERSION" >> makefile.opts
echo "PREFIX=\$(DESTDIR)$PREFIX" >> makefile.opts
echo "DOCDIR=\$(DESTDIR)$DOCDIR" >> makefile.opts
echo "CONF_ARGS=$CONF_ARGS" >> makefile.opts
echo >> makefile.opts

cat makefile.opts makefile.in makefile.depend > Makefile

rm -f .build.sh
echo "CC='$CC'" >> .build.sh
echo "AR='$AR'" >> .build.sh
echo "CFLAGS='$CFLAGS'" >> .build.sh
echo "LDFLAGS='$LDFLAGS'" >> .build.sh

#########################################################

# cleanup

rm -f .tmp.c .tmp.o

exit 0
