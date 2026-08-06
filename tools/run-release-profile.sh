#!/bin/sh
set -eu

archive="${1:?usage: run-release-profile.sh <archive> <profile> <build-dir>}"
profile="${2:?usage: run-release-profile.sh <archive> <profile> <build-dir>}"
build="${3:?usage: run-release-profile.sh <archive> <profile> <build-dir>}"
extract="${TMPDIR:-/tmp}/init-release-profile-$$"
stage="${STAGE:-${TMPDIR:-/tmp}/init-release-$profile}"
result="$build/release.namespace"
runner="${RUNNER:-namespace}"

case "$profile" in
    standard|offline|volatile|offline-volatile|compressed|persistent|durable|lean)
        ;;
    *)
        echo "ERROR: unknown profile $profile"
        exit 1
        ;;
esac

case "$runner" in
    namespace|qemu)
        ;;
    *)
        echo "ERROR: unknown release profile runner $runner"
        exit 1
        ;;
esac

if [ ! -f "$archive" ]; then
    echo "ERROR: release archive not found: $archive"
    exit 1
fi

cleanup()
{
    rm -rf "$extract"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$extract"
tar -xJf "$archive" -C "$extract"
package_count=$(find "$extract" -mindepth 1 -maxdepth 1 -type d | wc -l)
if [ "$package_count" -ne 1 ]; then
    echo "ERROR: release archive has $package_count top-level directories"
    exit 1
fi
package=$(find "$extract" -mindepth 1 -maxdepth 1 -type d -print)
info="$package/BUILD-INFO.txt"

if [ ! -f "$info" ]; then
    echo "ERROR: release archive has no BUILD-INFO.txt"
    exit 1
fi
release=$(awk -F= '$1 == "release" { print $2; exit }' "$info")
arch=$(awk -F= '$1 == "architecture" { print $2; exit }' "$info")
if [ -z "$release" ] || [ -z "$arch" ]; then
    echo "ERROR: release archive has incomplete build identity"
    exit 1
fi

for name in standard offline volatile offline-volatile compressed persistent durable lean; do
    binary="$package/init-$name"
    if [ ! -x "$binary" ]; then
        echo "ERROR: release archive has no executable $binary"
        exit 1
    fi
done
for path in init-status "profiles/$profile.h" README.md LICENSE; do
    if [ ! -f "$package/$path" ]; then
        echo "ERROR: release archive has no $path"
        exit 1
    fi
done

binary="$package/init-$profile"
if ! grep -aFq "$release" "$binary" || ! grep -aFq "$profile" "$binary"; then
    echo "ERROR: $binary does not contain its release identity"
    exit 1
fi
READELF="${READELF:-readelf}" sh tools/check-elf.sh "$binary"
mkdir -p "$build"
cp "$binary" "$build/init"
chmod 0755 "$build/init"

if [ "$runner" = namespace ]; then
    MARKER_SHUTDOWN_SIGNAL=10 PROFILE="$profile" RELEASE="$release" ARCH="$arch" \
    BOOT_MARKERS=tools/release-profile-markers.sh \
    INIT_NS_RESULT="$result" STAGE="$stage" \
    sh tools/run-namespace.sh "$build" tools/stage-rootfs.sh
else
    logd_fixture=1
    case "$profile" in
        volatile|offline-volatile|lean)
            logd_fixture=0
            ;;
    esac
    PROFILE="$profile" RELEASE="$release" ARCH="$arch" BUILD="$build" \
    BOOT_MARKERS=tools/release-profile-markers.sh \
    INIT_LOGD_FIXTURE="$logd_fixture" INIT_SNTP_FIXTURE=0 \
    sh tools/run-qemu.sh
fi

echo "ok: release archive $archive ran profile $profile"
