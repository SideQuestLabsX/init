#!/bin/sh
# Lay out the test root: the binary as /init, and the fixtures under /tasks in
# the three schedule flavours. Shared by the initramfs packer and the namespace
# harness.
set -eu

BUILD="${1:?usage: stage-rootfs.sh <build-dir> <stage-dir>}"
STAGE="${2:?usage: stage-rootfs.sh <build-dir> <stage-dir>}"

if [ ! -x "$BUILD/init" ]; then
    echo "stage-rootfs: $BUILD/init not built" >&2
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/dev" "$STAGE/proc" "$STAGE/sys" "$STAGE/var/log" \
         "$STAGE/tasks/always" "$STAGE/tasks/boot" \
         "$STAGE/tasks/2s" "$STAGE/tasks/500ms" "$STAGE/tasks/1h" "$STAGE/tasks/3d" \
         "$STAGE/tasks/1d-03-30" "$STAGE/tasks/4d-03-30" "$STAGE/tasks/sun-04-00"

cp "$BUILD/init" "$STAGE/init"
chmod 0755 "$STAGE/init"

install_bin()
{
    cp "$BUILD/fixtures/$1" "$STAGE/$2"
    chmod 0755 "$STAGE/$2"
}

install_bin task_ok       tasks/always/ok
install_bin probe_ok      tasks/always/ok.check
install_bin task_flap     tasks/always/flap
install_bin task_ok       tasks/always/hangcheck
install_bin probe_hang    tasks/always/hangcheck.check
install_bin task_selftest tasks/always/selftest
install_bin task_oneshot  tasks/boot/oneshot
install_bin task_tick     tasks/2s/tick

# One directory per accepted spelling. Only 2s and 500ms can fire inside the
# test window; the rest are here so a parser regression shows up as a task that
# stopped being counted rather than as a schedule nobody noticed breaking.
install_bin task_oneshot  tasks/500ms/subsecond
install_bin task_oneshot  tasks/1h/hourly
install_bin task_oneshot  tasks/3d/interval_days
install_bin task_oneshot  tasks/1d-03-30/nightly
install_bin task_oneshot  tasks/4d-03-30/every_fourth
install_bin task_oneshot  tasks/sun-04-00/weekly
