#!/bin/sh

PATH=.:$PATH

find .. -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.ui' | grep -v -E '(build|solid-lite|3rdparty)' | sort > filelist
which lupdate > /dev/null 2>&1
if [ $? -eq 0 ] ; then
    lupdate @filelist -ts ${1-*.ts} ${1+"$@"}
else
    which lupdate-qt5 > /dev/null 2>&1
    if [ $? -eq 0 ] ; then
        lupdate-qt5 @filelist -ts ${1-*.ts} ${1+"$@"}
    else
        echo "ERROR: Could not find lupdate or lupdate-qt5"
    fi
fi
rm filelist
