#!/bin/sh
set -eu

ARCH="${ARCH:?ARCH is required}"
CC="${CC:?CC is required}"
RELEASE="${RELEASE:?RELEASE is required}"
OUT="${OUT:-out/release}"

if ! printf '%s\n' "$RELEASE" | grep -Eq '^[0-9]{4}\.[0-9]{2}\.[0-9]{2}(\.[0-9]+)?$'; then
    echo "invalid CalVer release $RELEASE" >&2
    exit 2
fi

PACKAGE="init-$RELEASE-$ARCH"
STAGE="$OUT/$PACKAGE"
ARCHIVE="$OUT/$PACKAGE.tar.xz"
BUILD_ROOT="build/release/$RELEASE/$ARCH"
PROFILES=$(make --no-print-directory print-profiles)

rm -rf "$STAGE"
rm -f "$ARCHIVE"
mkdir -p "$STAGE/profiles"

for profile in $PROFILES; do
    build="$BUILD_ROOT/$profile"
    make --no-print-directory ARCH="$ARCH" CC="$CC" BUILD="$build" \
        PROFILE="$profile" RELEASE="$RELEASE" WERROR=1 all test-elf
    install -m 0755 "$build/init" "$STAGE/init-$profile"
done

make --no-print-directory ARCH="$ARCH" CC="$CC" \
    BUILD="$BUILD_ROOT/standard" PROFILE=standard RELEASE="$RELEASE" \
    WERROR=1 status-reader
install -m 0755 "$BUILD_ROOT/standard/init-status" "$STAGE/init-status"
install -m 0644 profiles/*.h "$STAGE/profiles"
install -m 0644 README.md LICENSE "$STAGE"

compiler=${CC%% *}
{
    printf 'release=%s\n' "$RELEASE"
    printf 'architecture=%s\n' "$ARCH"
    printf 'commit=%s\n' "$(git rev-parse HEAD)"
    printf 'compiler=%s\n' "$($compiler --version | sed -n '1p')"
    printf 'profiles=%s\n' "$PROFILES"
} > "$STAGE/BUILD-INFO.txt"

SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}"
export SOURCE_DATE_EPOCH
XZ_OPT=-9e tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" \
    --owner=0 --group=0 --numeric-owner -C "$OUT" -cJf "$ARCHIVE" "$PACKAGE"

printf '%s\n' "$ARCHIVE"
