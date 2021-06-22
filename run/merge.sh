#!/bin/bash
set -e
err(){ echo "$1" >&2; exit 1; }

[ "$#" -ne 2 ] && err "usage: $0 out_dir merged_dir"
[ ! -d "$1"  ] && err "input directory \"$1\" does not exist"

path="$PWD"
while [ "$path" != '/' ]; do
  if [ -x "$path/bin/merge" ]; then
    break
  else
    path="$(readlink -f "$path"/..)"
  fi
done
[ "$path" == '/' ] && err "cannot find bin/merge"

mkdir -p "$2"

# merge runs --------------------------------------------------------
for x in $(
  find "$1" -name '*.root' \
  | sed -nr 's,.*/(.*)_[0-9]+\.root$,\1,p' \
  | sort -u
); do
  echo $x
  out="$2/${x}.root"
  in="$1/${x}_"
  m='y'
  if [ -f "$out" ]; then
    m=''
    for f in "$in"*.root; do
      if [ "$f" -nt "$out" ]; then
        m='y'
        break
      fi
    done
  fi
  [ ! -z "$m" ] && \
    "$path/bin/merge" -ex "$out" "$in"*.root
done

# merge NLO ---------------------------------------------------------
for nlo in $(
  find "$2" -name '*.root' \
  | sed -nr 's,(^|/)([^_]+)(B|RS|I|V)_,\1NLO_,p' \
  | sort -u
); do
  echo "$nlo" \
  | sed -r 's,((^|(.*/))[^_]+)NLO(_.*),\1B\4\n\1RS\4\n\1I\4\n\1V\4,' \
  | readarray parts
  m=''
  for f in "${parts[@]}"; do
    [ ! -f "$f" ] && continue 2
    [ -f "$nlo" ] || [ "$f" -nt "$nlo" ] && m='y'
  done
  [ ! -z "$m" ] && \
    "$path/bin/merge" "$nlo" "${parts[@]}"
done

