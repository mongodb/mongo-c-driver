#!/bin/sh

echo "backtrace full" | ./libtool --mode execute gdb test-libmongoc $1

