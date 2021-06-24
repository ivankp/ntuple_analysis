#!/bin/bash
set -e
err(){ echo "$1" >&2; exit 1; }

[ "$#" -ne 2 ] && err "usage: $0 out_dir merged_dir"
[ ! -d "$1"  ] && err "\"$1\" is not a directory"

path="$PWD"
while [ "$path" != '/' ]; do
  if [ -x "$path/bin/merge" ]; then
    break
  else
    path="$(readlink -f "$path"/..)"
  fi
done
[ "$path" == '/' ] && err "cannot find bin/merge"

mkdir -pv "$2"

# merge runs --------------------------------------------------------
find "$1" -name '*.root' |
sed -nr 's|_[0-9]+\.root$||p' |
sort -u |
while read -r name; do
  out="$(sed "s|^$1|$2|" <<< "$name").root"
  echo "$out"
  m='y'
  if [ -f "$out" ]; then # if output file exists
    m='' # maybe skip
    for f in "$name"*.root; do
      if [ "$f" -nt "$out" ]; then # check if input was updated
        m='y'
        break
      fi
    done
  fi
  if [ -n "$m" ]; then
    # merge histograms
    # scale to cross section (-x)
    # produce scale and PDF variation envelopes (-e)
    "$path/bin/merge" -ex "$out" "$name"*.root
  fi
done

# merge NLO ---------------------------------------------------------
find "$2" -name '*.root' |
sed -nr 's,((^|/)[^_]*)(B|RS|I|V)_,\1NLO_,p' |
sort -u |
while read -r nlo; do
  echo "$nlo"
  readarray -t parts < <(
    sed -r 's,(.*(^|/)[^_]*)NLO(_.*),\1B\3\n\1RS\3\n\1I\3\n\1V\3,' \
    <<< "$nlo" )
  m=''
  for f in "${parts[@]}"; do
    [ ! -r "$f" ] && continue 2 # skip if a part is missing
    if [ ! -f "$nlo" ] || [ "$f" -nt "$nlo" ]; then
      # don't skip if output doesn't exist or input was updated
      m='y'
    fi
  done
  if [ -n "$m" ]; then
    # merge histograms
    "$path/bin/merge" "$nlo" "${parts[@]}"
  fi
done

