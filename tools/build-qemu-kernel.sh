#!/bin/sh
set -eu

ARCH="${1:-}"
if [ -z "$ARCH" ]; then
    echo "usage: $0 ARCH" >&2
    exit 2
fi

KERNEL_VERSION=6.1.75
KERNEL_SHA256=6cd19410330c13ec4c18fd28a83d3e40fc12a152815fb7c3e1b0764329093a56
KERNEL_ROOT="${QEMU_KERNEL_DIR:-${RUNNER_TEMP:-/tmp}/init-qemu-kernel-$ARCH}"
KERNEL_ARCH=
CROSS_COMPILE=
DEFCONFIG=
BUILD_TARGETS=
KERNEL_FILE=
DTB_FILE=
KERNEL_OUTPUT_DIR="${QEMU_KERNEL_OUTPUT_DIR:-}"

case "$ARCH" in
    x86)
        KERNEL_ARCH=x86
        DEFCONFIG=i386_defconfig
        BUILD_TARGETS=bzImage
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/x86/boot/bzImage"
        ;;
    x86_64)
        KERNEL_ARCH=x86
        DEFCONFIG=x86_64_defconfig
        BUILD_TARGETS=bzImage
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/x86/boot/bzImage"
        ;;
    aarch64)
        KERNEL_ARCH=arm64
        CROSS_COMPILE=aarch64-linux-gnu-
        DEFCONFIG=defconfig
        BUILD_TARGETS=Image
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/arm64/boot/Image"
        ;;
    armv6)
        KERNEL_ARCH=arm
        CROSS_COMPILE=arm-linux-gnueabihf-
        DEFCONFIG=realview_defconfig
        BUILD_TARGETS='zImage dtbs'
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/arm/boot/zImage"
        DTB_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/arm/boot/dts/arm-realview-eb.dtb"
        ;;
    armv7)
        KERNEL_ARCH=arm
        CROSS_COMPILE=arm-linux-gnueabihf-
        DEFCONFIG=multi_v7_defconfig
        BUILD_TARGETS=zImage
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/arm/boot/zImage"
        ;;
    riscv64)
        KERNEL_ARCH=riscv
        CROSS_COMPILE=riscv64-linux-gnu-
        DEFCONFIG=defconfig
        BUILD_TARGETS=Image
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/riscv/boot/Image"
        ;;
    loongarch64)
        KERNEL_ARCH=loongarch
        CROSS_COMPILE=loongarch64-linux-gnu-
        DEFCONFIG=loongson3_defconfig
        BUILD_TARGETS=vmlinuz.efi
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/arch/loongarch/boot/vmlinuz.efi"
        ;;
    mips)
        KERNEL_ARCH=mips
        CROSS_COMPILE=mips-linux-gnu-
        DEFCONFIG=malta_defconfig
        BUILD_TARGETS=vmlinux
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/vmlinux"
        ;;
    mipsel)
        KERNEL_ARCH=mips
        CROSS_COMPILE=mipsel-linux-gnu-
        DEFCONFIG=malta_defconfig
        BUILD_TARGETS=vmlinux
        KERNEL_FILE="$KERNEL_ROOT/linux-$KERNEL_VERSION/vmlinux"
        ;;
    *)
        echo "unsupported QEMU kernel ARCH=$ARCH" >&2
        exit 2
        ;;
esac

PublishKernel()
{
    if [ -n "${QEMU_KERNEL_ENV:-}" ]; then
        printf 'KERNEL=%s\n' "$KERNEL_FILE" >> "$QEMU_KERNEL_ENV"
        if [ -n "$DTB_FILE" ]; then
            printf 'DTB=%s\n' "$DTB_FILE" >> "$QEMU_KERNEL_ENV"
        fi
    else
        printf '%s\n' "$KERNEL_FILE"
    fi
}

if [ "${QEMU_WATCHDOG:-0}" -ne 0 ] && [ "$ARCH" != x86_64 ]; then
    echo "QEMU watchdog kernel requires ARCH=x86_64" >&2
    exit 2
fi

if [ -n "$KERNEL_OUTPUT_DIR" ]; then
    CACHED_KERNEL="$KERNEL_OUTPUT_DIR/kernel"
    CACHED_DTB=
    if [ -n "$DTB_FILE" ]; then
        CACHED_DTB="$KERNEL_OUTPUT_DIR/board.dtb"
    fi
    if [ -r "$CACHED_KERNEL" ] && { [ -z "$CACHED_DTB" ] || [ -r "$CACHED_DTB" ]; }; then
        KERNEL_FILE="$CACHED_KERNEL"
        DTB_FILE="$CACHED_DTB"
        echo "using cached QEMU kernel for $ARCH" >&2
        PublishKernel
        exit 0
    fi
fi

KERNEL_CC="${KERNEL_CC:-${CROSS_COMPILE}gcc}"
if ! command -v "$KERNEL_CC" >/dev/null 2>&1; then
    for candidate in "${CROSS_COMPILE}gcc-14" "${CROSS_COMPILE}gcc-13"; do
        if command -v "$candidate" >/dev/null 2>&1; then
            KERNEL_CC="$candidate"
            break
        fi
    done
fi
if ! command -v "$KERNEL_CC" >/dev/null 2>&1; then
    echo "missing kernel compiler $KERNEL_CC" >&2
    exit 1
fi

mkdir -p "$KERNEL_ROOT"
KERNEL_ARCHIVE="$KERNEL_ROOT/linux-$KERNEL_VERSION.tar.xz"
KERNEL_SOURCE="$KERNEL_ROOT/linux-$KERNEL_VERSION"
if [ ! -d "$KERNEL_SOURCE" ]; then
    if [ ! -r "$KERNEL_ARCHIVE" ]; then
        KERNEL_DOWNLOAD="$KERNEL_ARCHIVE.part"
        rm -f "$KERNEL_DOWNLOAD"
        curl -fsSL --http1.1 --retry 5 --retry-delay 2 \
            --retry-max-time 120 --retry-all-errors \
            "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VERSION.tar.xz" \
            -o "$KERNEL_DOWNLOAD"
        mv "$KERNEL_DOWNLOAD" "$KERNEL_ARCHIVE"
    fi
    echo "$KERNEL_SHA256  $KERNEL_ARCHIVE" | sha256sum -c - >&2
    tar -xf "$KERNEL_ARCHIVE" -C "$KERNEL_ROOT"
fi

make -C "$KERNEL_SOURCE" ARCH="$KERNEL_ARCH" CROSS_COMPILE="$CROSS_COMPILE" \
    CC="$KERNEL_CC" \
    "$DEFCONFIG" >&2
(
    cd "$KERNEL_SOURCE"
    ./scripts/config \
        --enable DEVTMPFS \
        --enable DEVTMPFS_MOUNT \
        --enable BLK_DEV_INITRD \
        --enable TMPFS
    case "$ARCH" in
        x86_64)
            if [ "${QEMU_WATCHDOG:-0}" -ne 0 ]; then
                ./scripts/config --enable WATCHDOG --enable I6300ESB_WDT
            fi
            ;;
        mips)
            ./scripts/config --enable CPU_BIG_ENDIAN --disable CPU_LITTLE_ENDIAN
            ;;
        mipsel)
            ./scripts/config --disable CPU_BIG_ENDIAN --enable CPU_LITTLE_ENDIAN
            ;;
    esac
)
make -C "$KERNEL_SOURCE" ARCH="$KERNEL_ARCH" CROSS_COMPILE="$CROSS_COMPILE" \
    CC="$KERNEL_CC" \
    olddefconfig >&2
# shellcheck disable=SC2086
make -C "$KERNEL_SOURCE" ARCH="$KERNEL_ARCH" CROSS_COMPILE="$CROSS_COMPILE" \
    CC="$KERNEL_CC" \
    -j"${KERNEL_JOBS:-$(nproc)}" $BUILD_TARGETS >&2
test -r "$KERNEL_FILE"
if [ -n "$DTB_FILE" ]; then
    test -r "$DTB_FILE"
fi

if [ -n "$KERNEL_OUTPUT_DIR" ]; then
    mkdir -p "$KERNEL_OUTPUT_DIR"
    cp "$KERNEL_FILE" "$KERNEL_OUTPUT_DIR/kernel"
    if [ -n "$DTB_FILE" ]; then
        cp "$DTB_FILE" "$KERNEL_OUTPUT_DIR/board.dtb"
        DTB_FILE="$KERNEL_OUTPUT_DIR/board.dtb"
    fi
    KERNEL_FILE="$KERNEL_OUTPUT_DIR/kernel"
fi
PublishKernel
