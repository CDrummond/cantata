#!/bin/sh

PATH=.:$PATH
for app in lupdate lconvert find sort grep ; do
    which $app > /dev/null 2>&1
    if [ $? -ne 0 ] ; then
        echo "ERROR: Could not find $app"
        exit
    fi
done

find .. -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.ui' | grep -v -E '(build|solid-lite|3rdparty)' | sort > filelist
lupdate @filelist -ts ${1-*.ts} ${1+"$@"}
rm filelist
