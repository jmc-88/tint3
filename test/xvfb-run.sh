#!/bin/sh

# Not quite the "unofficial BASH strict mode", but better than nothing.
set -eu
IFS="$(printf '\t\n.')"
IFS="${IFS%?}"

# Simple wrapper around Xvfb, used to run some test programs in a virtual X
# server. Provides a minimal replacement for xvfb-run which isn't universally
# available.
#
# Prerequisites:
#  - Xvfb is installed and available via $PATH
#  - xauth is installed and available via $PATH
#  - mcookie is installed and available via $PATH,
#    OR openssl is installed and available via $PATH,
#    OR md5sum is installed and available via $PATH,
#    OR md5 is installed and available via $PATH.
#
# If mcookie is unavailable and only md5 or md5sum are available, the system
# needs to also provide a random data source such as /dev/urandom or
# /dev/random.

_SERVERNUM=99
_XAUTH_PROTO='.'
_XVFB_ARGS='-screen 0 1024x768x24 +extension RANDR +extension XINERAMA'
_XVFB_PID=
_MCOOKIE_RANDOM_BYTES=1024

# Base check: don't even bother doing anything else if xvfb-run is available on
# the system, and just delegate to it.
# This is only performed if `which` is available: the stock xvfb-run requires
# it, but the proper dependency is missing in systems like Arch Linux.
if command -v xvfb-run >/dev/null 2>&1 && command -v which >/dev/null 2>&1; then
  exec xvfb-run -a -s "${_XVFB_ARGS}" "${@}"
fi

# Otherwise, unleash the might of this script onto the world.
if ! command -v Xvfb >/dev/null 2>&1; then
  echo " ✘  Xvfb not found!" >&2
  exit 1
fi

if ! command -v xauth >/dev/null 2>&1; then
  echo " ✘  xauth not found!" >&2
  exit 1
fi

if ! _XVFB_RUN_TMPDIR="$(mktemp -d "/tmp/xvfb-run.XXXXXX")"; then
  echo " ✘  Couldn't create the temporary directory." >&2
  exit 1
fi

if ! _AUTHFILE="$(mktemp "${_XVFB_RUN_TMPDIR}/Xauthority.XXXXXX")"; then
  echo " ✘  Couldn't create the temporary Xauthority file." >&2
  exit 1
fi

# {{{ cleanup

kill_running_processes() {
  if [ -n "${_XVFB_PID}" ]; then
    echo " ⌛  Sending SIGTERM to Xvfb..."
    kill "${_XVFB_PID}" 2>/dev/null
    wait "${_XVFB_PID}" || :
  fi

  if [ -e "${_AUTHFILE}" ]; then
    XAUTHORITY="${_AUTHFILE}" xauth remove ":${_SERVERNUM}"
  fi

  if [ -n "${_XVFB_RUN_TMPDIR}" ]; then
    rm -r "${_XVFB_RUN_TMPDIR}"
  fi
}

trap kill_running_processes EXIT

gracefully_terminate() {
  echo " >> Caught SIGINT, will now terminate."
  exit 0
}

trap gracefully_terminate INT

# }}} cleanup

# {{{ launch

alive() {
  kill -s 0 "${1}" 2>/dev/null
}

find_free_servernum() {
  i=${_SERVERNUM}
  while [ -f "/tmp/.X${i}-lock" ]; do
    i=$((i+1))
  done

  echo ${i}
  unset i
}

_mcookie_rand() {
  if command -v openssl >/dev/null 2>&1; then
    openssl rand ${_MCOOKIE_RANDOM_BYTES}
  else
    if ! command -v head; then
      echo " ✘  Can't even find \"head\". Giving up." >&2
      exit 1
    fi

    if [ -c /dev/urandom ]; then
      head -c ${_MCOOKIE_RANDOM_BYTES} /dev/urandom
    elif [ -c /dev/random ]; then
      head -c ${_MCOOKIE_RANDOM_BYTES} /dev/random
    else
      echo " ✘  No suitable random source was found." >&2
      exit 1
    fi
  fi
}

_mcookie_md5() {
  if command -v openssl >/dev/null 2>&1; then
    openssl md5 -r | cut -f 1 -d ' '
  elif command -v md5sum >/dev/null 2>&1; then
    md5sum | cut -f 1 -d ' '
  elif command -v md5 >/dev/null 2>&1; then
    md5 -q
  else
    echo " ✘  No suitable MD5 utility was found." >&2
    exit 1
  fi
}

_mcookie() {
  if command -v mcookie >/dev/null 2>&1; then
    mcookie
  else
    _mcookie_rand | _mcookie_md5
  fi
}

_SERVERNUM="$(find_free_servernum)"
if ! _MCOOKIE="$(_mcookie)"; then
  echo " ✘  Fetching a magic cookie failed." >&2
  exit 1
fi
export _MCOOKIE

_xauth() {
  # ${1}: Xauthority file
  # ${2}: display server number
  # ${3}: Xauth protocol name
  # ${4}: hex key as returned by _mcookie
  XAUTHORITY="${1?}" xauth source - <<EOF 2>&1
add :${2?} ${3?} ${4?}
EOF
}

if ! _xauth "${_AUTHFILE}" "${_SERVERNUM}" "${_XAUTH_PROTO}" "${_MCOOKIE}"; then
  echo " ✘  Modifying Xauthority failed." >&2
  exit 1
fi

trap : USR1
(
  IFS=' '  # to correctly split _XVFB_ARGS below
  trap '' USR1

  # shellcheck disable=SC2086
  exec Xvfb ":${_SERVERNUM}" ${_XVFB_ARGS} -auth "${_AUTHFILE}" >"${_XVFB_RUN_TMPDIR}/Xvfb.log" 2>&1
) &
_XVFB_PID="${!}"

wait || :

if alive "${_XVFB_PID}"; then
  echo " ✔  Launched Xvfb as PID ${_XVFB_PID}."
else
  echo " ✘  Launching Xvfb failed." >&2
  _XVFB_PID=
  exit 1
fi

# }}} launch

DISPLAY=":${_SERVERNUM}" XAUTHORITY="${_AUTHFILE}" "${@}" 2>&1
