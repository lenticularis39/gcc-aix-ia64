#!/bin/sh
for gperf_file in $(find . -name '*.gperf' -and -not -name 'objc*')
do
  echo "$gperf_file"
  touch "$gperf_file"
  touch "${gperf_file%.*}.h"
done

for bison_file in $(find . -name '*.y')
do
  echo "$bison_file"
  touch "$bison_file"
  touch "${bison_file%.*}.c"
  [ -e "${bison_file%.*}.h" ] && touch "${bison_file%.*}.h"
done
