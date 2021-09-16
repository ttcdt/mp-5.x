# build system -- ttcdt <dev@triptico.com> public domain

# get working dir
DIR="$1"
[ "$DIR" = "" ] && DIR="."

# load info built by config.sh
[ -f ${DIR}/.build.sh ] && . ${DIR}/.build.sh

[ "$CC" = "" ] && CC="cc"
[ "$AR" = "" ] && AR="ar"
[ "$CCLINK" = "" ] && CCLINK="$CC"

# building functions

dep() {
    local dir=$1 ; shift
    local target=$1 ; shift
    local ret=1

    for d in $* ; do
        # fail immediately if any dependency does not exist
        case $d in
        -*) true ;;
        *)  [ ! -f "${dir}/${d}" ] && echo "${dir}/${d} ???" && exit 1 ;;
        esac

        # account if any dependency is greater than the target
        [ ! -f "${target}" -o "${dir}/${d}" -nt "${target}" ] && ret=0
    done

    return $ret
}

do_cc() {
    local target=$1 ; shift
    local src=$1 ; shift

    if dep "${DIR}" $target $src $* ; then
        echo $CC $CFLAGS $src
        $CC -I${DIR} $CFLAGS -c "${DIR}/${src}" -o $target || exit 1
    fi
}

do_cpp() {
    local target=$1 ; shift
    local src=$1 ; shift

    if dep "${DIR}" $target $src $* ; then
        echo $CPP $CFLAGS $src
        $CPP $CFLAGS -c "${DIR}/${src}" -o $target || exit 1
    fi
}

do_flex() {
    local target=$1 ; shift
    local inter=$1 ; shift
    local src=$1 ; shift

    if dep "${DIR}" $target $src $* ; then
        echo flex $src
        flex "${DIR}/${src}" || exit 1
        echo $CC $inter
        $CC -I${DIR} $CFLAGS -c ${inter} -o $target || exit 1
    fi
}

do_yacc() {
    local target=$1 ; shift
    local inter=$1 ; shift
    local src=$1 ; shift

    if dep "${DIR}" $target $src $* ; then
        echo $YACC $src
        $YACC -d "${DIR}/${src}" || exit 1
        echo $CC $CFLAGS $inter
        $CC -I${DIR} $CFLAGS -c ${inter} -o $target || exit 1
    fi
}

do_ar() {
    local target=$1 ; shift

    if dep "." $target $* ; then
        echo $AR $target
        $AR r $target $* || exit 1
    fi
}

do_tar() {
    local target=$1 ; shift

    if dep "." $target $* ; then
        echo tar $target
        $TAR cf $target $*
    fi
}

do_objbin() {
    local target=$1 ; shift
    local src=$1 ; shift

    if dep "." $target $src ; then
        echo objbin $target
        $LD -r -b binary $src -o $target
    fi
}

do_link() {
    local target=$1 ; shift

    if dep "." $target $* ; then
        echo link ${LDFLAGS} $target
        $CCLINK ${CFLAGS} $* ${LDFLAGS} -o $target || exit 1
    fi
}

do_moc() {
    local target=$1 ; shift
    local src=$1 ; shift

    if dep "${DIR}" $target $src $* ; then
        echo $MOC $src
        $MOC -o $target "${DIR}/${src}" || exit 1
    fi
}

do_windres() {
    local target=$1 ; shift
    local src=$1 ; shift

    if dep "${DIR}" $target $src $* ; then
        echo $WINDRES $src
        $WINDRES "${DIR}/${src}" $target || exit 1
    fi
}

. ${DIR}/local-build.sh
