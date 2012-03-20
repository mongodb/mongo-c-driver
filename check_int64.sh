#!/bin/bash
# $1 is C compiler. $2 is header file
cat <<EOF >tmp.c && $1 -o header_check.tmp tmp.c
#include <$2>
int main() { int64_t i=0; return 0; }
EOF
