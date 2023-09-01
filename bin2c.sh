#!/bin/sh
# converts a binary file to C source - public domain
# requires od and awk

SYMBOL="$1"
[ "${SYMBOL}" = "" ] && SYMBOL="data"

echo "const char ${SYMBOL}[] = "
od -bv | awk '// { printf("  \""); for (n = 2; n <= NF; n++) \
    { printf("\\%s", $n); } \
    printf("\"\n"); }'
echo ";"
echo "int ${SYMBOL}_size = sizeof(${SYMBOL});"
