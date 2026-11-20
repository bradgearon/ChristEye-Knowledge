#!/usr/bin/env bash
# CI/operator helper. Mount only a newly created private disposable tmpfs.
set -euo pipefail
if [[ $# -ne 2 ]]; then echo 'Usage: run-isolated-pressure.sh TEST_EXECUTABLE NEW_REPORT_DIRECTORY' >&2; exit 2; fi
executable=$(realpath -- "$1")
report=$(realpath -m -- "$2")
if [[ ! -x "$executable" || -e "$report" || ! -d "$(dirname -- "$report")" ]]; then echo 'An executable and new report directory with existing parent are required' >&2; exit 2; fi
mountpoint=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/anaiero-pressure.XXXXXXXX")
mounted=0
cleanup() {
  if [[ $mounted == 1 ]]; then sudo -n umount -- "$mountpoint" || return 1; fi
  rmdir -- "$mountpoint"
}
trap cleanup EXIT
sudo -n mount -t tmpfs -o "size=64m,nr_inodes=4096,nodev,nosuid,noexec,mode=0700,uid=$(id -u),gid=$(id -g)" anaiero-pressure "$mountpoint"
mounted=1
nonce=$(basename -- "$mountpoint")
printf 'ANAIERO-STORAGE-PRESSURE/1\nnonce=%s\n' "$nonce" > "$mountpoint/OWNER"
"$executable" "$mountpoint" "$nonce" "$report"
