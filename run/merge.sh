#!/bin/bash
set -e
err(){ echo "$1" >&2; exit 1; }

[ "$#" -ne 2 ] && err "usage: $0 input_dir merged_dir"
[ ! -d "$1"  ] && err "input directory \"$1\" does not exist"

path='.'
while [ "$path" != '/' ]
do
  if [ -x "$path/bin/merge" ]; then
    break
  else
    path="$(readlink -f "$path"/..)"
  fi
done
[ "$path" == '/' ] && err "cannot find bin/merge"

mkdir -p "$2"

for x in $(
  find "$1" -name '*.root' |
  sed 's:.*/\(.*\)_[0-9]\+\.root$:\1:' |
  sort -u
); do
  echo $x
  "$path/bin/merge" -ex "$2/${x}.root" "$1/${x}"_*.root
done
