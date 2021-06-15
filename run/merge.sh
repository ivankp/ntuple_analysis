#!/bin/bash
set -e

path='.'
while [ "$path" != '/' ]
do
  if [ -x "$path/bin/merge" ]
  then
    break
  else
    path="$(readlink -f "$path"/..)"
  fi
done

mkdir -p merged

for x in $(
  find out -name '*.root' |
  sed 's:.*/\(.*\)_[0-9]\+\.root$:\1:' |
  sort -u
); do
  echo $x
  "$path/bin/merge" -ex "merged/${x}.root" "out/${x}"_*.root
done
