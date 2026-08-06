#!/bin/sh
set -eu

READELF="${READELF:-readelf}"

if [ "$#" -eq 0 ]; then
    echo "usage: $0 ELF..." >&2
    exit 2
fi
if ! command -v "$READELF" >/dev/null 2>&1; then
    echo "missing ELF reader $READELF" >&2
    exit 1
fi

for binary in "$@"; do
    if [ ! -r "$binary" ]; then
        echo "missing ELF binary $binary" >&2
        exit 1
    fi

    headers=$("$READELF" -lW "$binary")
    dynamic=$("$READELF" -dW "$binary" 2>/dev/null || true)

    if printf '%s\n' "$headers" | grep -q 'INTERP'; then
        echo "$binary has a program interpreter" >&2
        exit 1
    fi
    if printf '%s\n' "$dynamic" | grep -q '(NEEDED)'; then
        echo "$binary has a dynamic dependency" >&2
        exit 1
    fi
    if ! printf '%s\n' "$headers" | grep -q 'GNU_STACK'; then
        echo "$binary has no GNU_STACK header" >&2
        exit 1
    fi
    if printf '%s\n' "$headers" | grep -q 'GNU_STACK.*RWE'; then
        echo "$binary has an executable stack" >&2
        exit 1
    fi
    if ! "$READELF" -sW "$binary" | awk '$7 != "UND" && $8 == "_start" { found = 1 } END { exit found ? 0 : 1 }'; then
        echo "$binary has no defined _start symbol" >&2
        exit 1
    fi

    echo "elf ok: $binary"
done
