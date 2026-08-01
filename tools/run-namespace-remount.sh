#!/bin/sh
set -eu

STAGE="${1:?usage: run-namespace-remount.sh <stage-dir>}"
BASE="$STAGE/remount-test"

mkdir -p "$BASE/early/a/b" "$BASE/space path" "$BASE/shallow" "$BASE/chain"
mount --bind "$BASE/early/a/b" "$BASE/early/a/b"

path="$BASE/chain"
i=0
while [ "$i" -lt 48 ]; do
    component=$(printf 'd%02d_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789' "$i")
    path="$path/$component"
    mkdir "$path"
    mount --bind "$path" "$path"
    i=$((i + 1))
done

path="$path/zz_deepest"
mkdir "$path"
mount --bind "$path" "$path"
mount --bind "$BASE/space path" "$BASE/space path"
mount --bind "$BASE/shallow" "$BASE/shallow"

awk '
    index($0, "zz_deepest") {
        if(bytes <= 4096)
            exit 1
        bFound = 1
    }
    { bytes += length($0) + 1 }
    END { exit bFound ? 0 : 1 }
' /proc/self/mounts

exec chroot "$STAGE" /init
