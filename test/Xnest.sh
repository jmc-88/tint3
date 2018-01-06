#!/bin/sh

# Not quite the "unofficial BASH strict mode", but better than nothing.
set -eu
IFS="$(printf '\n\t')"

# Simple test script to launch the locally-built tint3 and the Openbox window
# manager in a nested X session.
#
# Prerequisites:
#  - Xnest is installed and available via $PATH
#  - openbox is installed and available via $PATH
#  - the CMake build directory is named "build" and can be found inside the
#    repository root directory

if [ $# -ne 1 ]; then
  echo " ✘  Wrong number of arguments." >&2
  echo " Expecting: a display name (e.g., \":1\")" >&2
  exit 1
fi

_DISPLAY="${1}"
_ROOT="$(cd "$(dirname "${0}")/.."; pwd)"
_XNEST_PID=""
_OPENBOX_PID=""
_TINT3_PID=""

if ! command -v Xnest >/dev/null 2>&1; then
  echo " ✘  Xnest not found!" >&2
  exit 1
fi

if ! command -v openbox >/dev/null 2>&1; then
  echo " ✘  openbox not found!" >&2
  exit 1
fi

if [ ! -x "${_ROOT}/build/tint3" ]; then
  echo " ✘  local tint3 build not found!" >&2
  exit 1
fi

# {{{ cleanup

kill_running_processes() {
  if [ -n "${_TINT3_PID}" ]; then
    echo " ⌛  Sending SIGTERM to tint3..."
    kill "${_TINT3_PID}" 2>/dev/null
    wait "${_TINT3_PID}"
  fi

  if [ -n "${_OPENBOX_PID}" ]; then
    echo " ⌛  Sending SIGTERM to openbox..."
    kill "${_OPENBOX_PID}" 2>/dev/null
    wait "${_OPENBOX_PID}"
  fi

  if [ -n "${_XNEST_PID}" ]; then
    echo " ⌛  Sending SIGTERM to Xnest..."
    kill "${_XNEST_PID}" 2>/dev/null
    wait "${_XNEST_PID}"
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

Xnest "${_DISPLAY}" >/tmp/Xnest.log 2>&1 &
_XNEST_PID="${!}"

if alive "${_XNEST_PID}"; then
  echo " ✔  Launched Xnest as PID ${_XNEST_PID}."
  sleep 1
else
  echo " ✘  Launching Xnest failed." >&2
  exit 1
fi

env -i DISPLAY="${_DISPLAY}" \
  openbox >/tmp/openbox.log 2>&1 &
_OPENBOX_PID="${!}"

if alive "${_OPENBOX_PID}"; then
  echo " ✔  Launched openbox as PID ${_OPENBOX_PID}."
  sleep 1
else
  echo " ✘  Launching openbox failed." >&2
  exit 1
fi

env -i DISPLAY="${_DISPLAY}" \
  "${_ROOT}/build/tint3" >/tmp/tint3.log 2>&1 &
_TINT3_PID="${!}"

if alive "${_TINT3_PID}"; then
  echo " ✔  Launched tint3 as PID ${_TINT3_PID}."
  sleep 1
else
  echo " ✘  Launching tint3 failed." >&2
  exit 1
fi

# }}} launch

wait
