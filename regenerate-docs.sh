#!/bin/sh

_branch="v0.3.0"

die() {
  echo >&2 "${@}"
  exit 1
}

regen() {
  if [ ${#} -lt 1 ]; then
    die "regen: missing filename"
  fi

  _source="$(git show "master@{${_branch}}:${1}")"
  if [ ${?} -ne 0 ]; then
    die "regen: \"git show\" failed"
  fi

  printf "%s" "${_source}" | \
    pandoc --standalone --toc \
           --from markdown --to html5 \
           --css /tint3/css/man.css \
           > "${1%.md}.html"
  if [ ${?} -ne 0 ]; then
    die "regen: \"pandoc\" failed"
  fi
}

for page in doc/tint3.1.md doc/tint3rc.5.md; do
  regen "${page}"
done

unset _branch
