#!/bin/sh
set -eu

state="${1:?usage: check-schedule-state.sh <state-file> [label]}"
label="${2:-schedule state}"

if [ ! -f "$state" ]; then
    echo "BAD: $label created no schedule state"
    exit 1
fi

state_header_bytes=16
state_record_bytes=268
state_path_bytes=256
state_bytes=$(wc -c < "$state")
state_magic=$(od -An -tx1 -N4 "$state" | tr -d ' \n')
state_version=$(od -An -tu4 -j4 -N4 "$state" | tr -d ' ')
state_count=$(od -An -tu4 -j8 -N4 "$state" | tr -d ' ')
state_reserved=$(od -An -tu4 -j12 -N4 "$state" | tr -d ' ')
if [ "$state_magic" != 53544131 ] || [ "$state_version" -ne 1 ] ||
   [ "$state_count" -le 0 ] || [ "$state_count" -gt 1024 ] ||
   [ "$state_reserved" -ne 0 ]; then
    echo "BAD: $label header is malformed"
    exit 1
fi

expected_bytes=$((state_header_bytes + state_count * state_record_bytes))
if [ "$state_bytes" -ne "$expected_bytes" ]; then
    echo "BAD: $label size does not match its record count"
    exit 1
fi

tick_found=0
record=0
while [ "$record" -lt "$state_count" ]; do
    offset=$((state_header_bytes + record * state_record_bytes))
    path_len=$(od -An -tu2 -j "$offset" -N2 "$state" | tr -d ' ')
    record_reserved=$(od -An -tu2 -j "$((offset + 2))" -N2 "$state" | tr -d ' ')
    last_run=$(od -An -tu8 -j "$((offset + 4))" -N8 "$state" | tr -d ' ')
    path=$(dd if="$state" bs=1 skip="$((offset + 12))" \
           count="$state_path_bytes" 2>/dev/null | tr -d '\0')

    if [ "$path_len" -le 0 ] || [ "$path_len" -ge "$state_path_bytes" ] ||
       [ "$record_reserved" -ne 0 ] || [ "$last_run" -eq 0 ] ||
       [ "${#path}" -ne "$path_len" ]; then
        echo "BAD: $label record $record is malformed"
        exit 1
    fi
    case "$path" in
        /tasks/*)
            ;;
        *)
            echo "BAD: $label record $record has an invalid path"
            exit 1
            ;;
    esac
    if [ "$path" = "/tasks/2s/tick" ]; then
        tick_found=1
    fi
    record=$((record + 1))
done

if [ "$tick_found" -eq 0 ]; then
    echo "BAD: $label has no successful interval record"
    exit 1
fi
echo "ok: $label wrote valid interval records"
