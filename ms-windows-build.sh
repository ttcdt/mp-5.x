#!/bin/sh

set -e

[ -f Makefile ] && make realdistclean

./config.sh --mingw32-prefix=i686-w64-mingw32 --mingw32

make

mv mp-5.exe .32
mv mp-5c.exe .32c

make realdistclean

./config.sh --mingw32-prefix=x86_64-w64-mingw32 --mingw32

make

mv .32 mp-5-portable-32.exe
mv .32c mp-5c-portable-32.exe
mv mp-5.exe mp-5-portable-64.exe
mv mp-5c.exe mp-5c-portable-64.exe

i686-w64-mingw32-strip mp-5-portable-32.exe
i686-w64-mingw32-strip mp-5c-portable-32.exe
x86_64-w64-mingw32-strip mp-5-portable-64.exe
x86_64-w64-mingw32-strip mp-5c-portable-64.exe
