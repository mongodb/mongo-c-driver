#!/usr/bin/env bash

echo "Debugging core files"

shopt -s nullglob
for i in *.core; do
   echo $i
   echo "backtrace full" | gdb -q ./src/libmongoc/test-libmongoc $i
done
