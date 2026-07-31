# init: freestanding PID 1 / supervisor / scheduler
#
# No libc, no crt0. The compiler is used as a driver only.
# Every flag below is load-bearing; do not "clean up" this list.

ARCHES  := x86_64 x86 aarch64 armv7 armv6 riscv64 loongarch64 mips mipsel

ARCH    ?= x86_64
BUILD   ?= build/$(ARCH)
TARGET  := $(BUILD)/init

CC      ?= gcc

# -fstack-protector-strong : SNTP parses untrusted network input. Keep it.
#                            Requires our own __stack_chk_fail/__stack_chk_guard.
# -static-pie              : so ASLR applies to PID 1. Needs the self-relocator
#                            in init.c; libc's crt would normally do this.
CFLAGS_COMMON := \
	-std=c11 -O2 \
	-nostdlib -ffreestanding \
	-fstack-protector-strong \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-fno-builtin -fno-common \
	-Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-I.

# ARMv6 and ARMv7 are NOT interchangeable: ARMv6 has no movw/movt and no NEON,
# and unaligned access is unsafe there.
#
# -mstack-protector-guard=global on x86: the default guard lives at %fs:0x28,
# a TLS slot only libc sets up. Without this the canary read faults.
# -marm on ARM: the syscall clobber list needs r7, the Thumb frame pointer.
ifeq ($(ARCH),x86_64)
  CFLAGS_ARCH := -m64 -mstack-protector-guard=global
else ifeq ($(ARCH),x86)
  CFLAGS_ARCH := -m32 -mstack-protector-guard=global
else ifeq ($(ARCH),aarch64)
  CFLAGS_ARCH := -march=armv8-a
else ifeq ($(ARCH),armv7)
  CFLAGS_ARCH := -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16 -marm -fomit-frame-pointer
else ifeq ($(ARCH),armv6)
  CFLAGS_ARCH := -march=armv6 -mfloat-abi=hard -mfpu=vfp -marm -fomit-frame-pointer
else ifeq ($(ARCH),riscv64)
  CFLAGS_ARCH := -march=rv64gc -mabi=lp64d
# -fno-jump-tables: loongarch emits switch tables as absolute addresses in
# .rodata, which a PIE cannot relocate. Other targets build them PC-relative.
else ifeq ($(ARCH),loongarch64)
  CFLAGS_ARCH := -mabi=lp64d -fno-jump-tables
# mips32r2 is the 24Kc/74Kc baseline every router-class MIPS board of the last
# fifteen years implements. -mno-abicalls drops the GOT and $gp convention that
# a static freestanding binary has no use for.
else ifeq ($(ARCH),mips)
  CFLAGS_ARCH := -march=mips32r2 -mno-abicalls -fno-pic -EB
  ARCH_PIE := 0
else ifeq ($(ARCH),mipsel)
  CFLAGS_ARCH := -march=mips32r2 -mno-abicalls -fno-pic -EL
  ARCH_PIE := 0
else
  $(error Unknown ARCH '$(ARCH)'. Use: $(ARCHES))
endif

# Escape hatch for toolchains that cannot produce a working static-pie. MIPS
# resolves position independence through the GOT, which the self-relocator in
# init.c cannot walk, so that port defaults off.
ARCH_PIE ?= 1
PIE ?= $(ARCH_PIE)
ifeq ($(PIE),1)
  LINKMODE := -static-pie
else
  LINKMODE := -static -no-pie
endif

# Features and tunables live in config.h, not here. Edit that file.
EXTRA_CFLAGS ?=

CFLAGS  := $(CFLAGS_COMMON) $(CFLAGS_ARCH) $(LINKMODE) $(EXTRA_CFLAGS)

# The boot tests bring their own task rules and flush the disk writer inside
# their window. Everything else matches a normal build.
BOOT_SNTP_SERVER ?= 127.0.0.1
BOOT_SNTP_PORT   ?= 40123
ifeq ($(BOOT_TEST),1)
  CFLAGS += -DCFG_LOGD_FLUSH_NS=2000000000ull \
            -DCFG_RESTART_GRACE_NS=500000000ull \
            -DCFG_LOGD_STALL_NS=1000000000ull \
            -DCFG_SNTP_SERVER='"$(BOOT_SNTP_SERVER)"' \
            -DCFG_SNTP_PORT=$(BOOT_SNTP_PORT) \
            -DCFG_SNTP_RETRY_NS=250000000ull \
            -DINIT_TASK_RULES_H='"tests/fixtures/test_rules.h"'
  CONFIG_DEPS := config.h tests/fixtures/test_rules.h
else
  CONFIG_DEPS := config.h
endif
# -lgcc supplies the operations the target ISA lacks: on every 32-bit target
# `u64 / u64` lowers to __udivdi3. It is the compiler's own runtime, shipped with
# the compiler already in use, the same category as the memcpy calls GCC emits
# under -ffreestanding. LP64 targets never reference it and --gc-sections drops
# it.
LDFLAGS := -nostdlib $(LINKMODE) -Wl,--gc-sections -Wl,-e,_start -Wl,-z,noexecstack -lgcc

# One translation unit; _start is the only thing that has to be assembly.
OBJ := $(BUILD)/init.o $(BUILD)/start.o

.PHONY: all clean check check-all test test-config-overrides test-ns test-qemu test-variant test-variants abi-check fixtures status-reader allarch help
.DEFAULT_GOAL := all

all: $(TARGET)

$(TARGET): $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)
	@echo "built $@"

$(BUILD)/%.o: %.c $(CONFIG_DEPS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)

# ---------------------------------------------------------------- testing

# INIT_HOSTED cuts init.c down to the half that issues no syscalls, so these
# build and run anywhere a C compiler does, against the shipped source rather
# than a copy of it.
HOST_CC    ?= cc
HOST_BUILD := build/host
HOST_SRC   := tests/host/test_main.c
HOST_DEPS  := tests/host/test_main.c init.c $(CONFIG_DEPS)
HOST_CFLAGS := -std=c11 -g -O1 -I. \
               -Wall -Wextra -Wshadow -Wconversion \
               -DINIT_HOSTED=1 -fno-builtin -pthread

SANITIZE ?= 1
ifeq ($(SANITIZE),1)
  HOST_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
endif

test: test-config-overrides $(HOST_BUILD)/unit
	@$(HOST_BUILD)/unit

test-config-overrides: | $(HOST_BUILD)
	$(HOST_CC) -std=c11 -I. -Werror -c tests/host/config_override.c \
	           -o $(HOST_BUILD)/config_override.o

$(HOST_BUILD)/unit: $(HOST_DEPS) | $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_SRC) -o $@

$(HOST_BUILD):
	@mkdir -p $(HOST_BUILD)

# Compile-only: static_asserts every hand-transcribed syscall number and ABI
# constant against the kernel UAPI headers for this ARCH.
abi-check: | $(BUILD)
	$(CC) $(CFLAGS_ARCH) -std=c11 -I. -DINIT_ABI_ONLY=1 -Wall -Wextra \
	      -c tests/abi/abi_check.c -o $(BUILD)/abi_check.o
	@echo "abi ok for $(ARCH)"

FIXTURE_SRC := $(wildcard tests/fixtures/*.c)
FIXTURE_BIN := $(patsubst tests/fixtures/%.c,$(BUILD)/fixtures/%,$(FIXTURE_SRC))
FIXTURE_CFLAGS := -std=c11 -O1 -nostdlib -ffreestanding -static -no-pie \
                  -fno-stack-protector -fno-builtin -fno-common \
                  -Wall -Wextra -I. -DINIT_FIXTURE=1 $(CFLAGS_ARCH) \
                  $(FIXTURE_EXTRA_CFLAGS)

fixtures: $(FIXTURE_BIN)

$(BUILD)/fixtures/%: tests/fixtures/%.c tests/fixtures/fstart.S \
                     tests/fixtures/fixture.h init.c $(CONFIG_DEPS) | $(BUILD)
	@mkdir -p $(BUILD)/fixtures
	$(CC) $(FIXTURE_CFLAGS) $< tests/fixtures/fstart.S -o $@ \
	      -nostdlib -static -no-pie -Wl,-e,_start -Wl,-z,noexecstack

STATUS_READER := $(BUILD)/init-status
STATUS_READER_CFLAGS := -std=c11 -O2 -nostdlib -ffreestanding -static -no-pie \
                         -fno-stack-protector -fno-builtin -fno-common \
                         -Wall -Wextra -I. -DINIT_FIXTURE=1 \
                         -DINIT_STATUS_READER=1 -DFIXTURE_ENTRY=StatusReaderMain \
                         $(CFLAGS_ARCH) \
                         $(EXTRA_CFLAGS)

status-reader: $(STATUS_READER)

$(STATUS_READER): tools/init-status.c tests/fixtures/fstart.S init.c $(CONFIG_DEPS) | $(BUILD)
	$(CC) $(STATUS_READER_CFLAGS) tools/init-status.c tests/fixtures/fstart.S \
	      -o $@ -nostdlib -static -no-pie -Wl,-e,_start -Wl,-z,noexecstack -lgcc

QEMU_BUILD ?= build/$(ARCH)-qemu
NS_BUILD   ?= build/$(ARCH)-ns
NS_ROOTFS  ?= tools/stage-rootfs.sh

test-qemu:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(QEMU_BUILD) BOOT_TEST=1 \
	        BOOT_SNTP_SERVER=162.159.200.1 BOOT_SNTP_PORT=123 all fixtures
	ARCH=$(ARCH) BUILD=$(QEMU_BUILD) INIT_SNTP_FIXTURE=0 sh tools/run-qemu.sh

# Same fixtures without an emulator: PID 1 inside a user+pid+mount namespace.
test-ns:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(NS_BUILD) BOOT_TEST=1 \
	        all fixtures status-reader
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_STATUS_READER=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_LOG_SYMLINK=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_STATUS_FALLBACK=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"

check: test abi-check all test-ns test-qemu

# CI runs the same variants as a parallel matrix
FEATURE_VARIANTS ?= OFFLINE_MODE=1 FEATURE_WATCHDOG=0 FEATURE_EXEC_PROBES=0 \
                    FEATURE_LOG_DISK=0 FEATURE_LOG_CAPTURE=0 \
                    FEATURE_CAPABILITY_DROP=0

CHECK_CC_x86_64     ?= gcc
CHECK_CC_x86        ?= gcc
CHECK_CC_aarch64    ?= aarch64-linux-gnu-gcc
CHECK_CC_armv7      ?= arm-linux-gnueabihf-gcc
CHECK_CC_armv6      ?= arm-linux-gnueabihf-gcc
CHECK_CC_riscv64    ?= riscv64-linux-gnu-gcc
CHECK_CC_loongarch64 ?= loongarch64-linux-gnu-gcc-14
CHECK_CC_mips       ?= mips-linux-gnu-gcc
CHECK_CC_mipsel     ?= mipsel-linux-gnu-gcc

check-all: check
	@set -e; \
	for spec in \
	  x86_64:$(CHECK_CC_x86_64) x86:$(CHECK_CC_x86) \
	  aarch64:$(CHECK_CC_aarch64) armv7:$(CHECK_CC_armv7) armv6:$(CHECK_CC_armv6) \
	  riscv64:$(CHECK_CC_riscv64) loongarch64:$(CHECK_CC_loongarch64) \
	  mips:$(CHECK_CC_mips) mipsel:$(CHECK_CC_mipsel); do \
		arch=$${spec%%:*}; cc=$${spec#*:}; \
		if ! command -v "$$cc" >/dev/null 2>&1; then \
			echo "SKIP: $$arch compiler $$cc not installed"; \
			continue; \
		fi; \
		echo "=== check $$arch with $$cc"; \
		$(MAKE) --no-print-directory ARCH=$$arch BUILD=build/check-$$arch \
		        CC=$$cc abi-check all status-reader; \
	done
	@$(MAKE) --no-print-directory test-variants

test-variant:
	$(if $(filter $(FEATURE_VARIANT),$(FEATURE_VARIANTS)),,$(error Unknown feature variant '$(FEATURE_VARIANT)'))
	@set -eu; \
	v='$(FEATURE_VARIANT)'; \
	k=$${v%%=*}; \
	extra="-U$$k -D$$v"; \
	fixture_extra=; \
	if [ "$$v" = FEATURE_EXEC_PROBES=0 ]; then \
		fixture_extra="-DFIXTURE_SETTLE_NS=25000000000ull"; \
	fi; \
	if [ "$$v" = FEATURE_LOG_CAPTURE=0 ]; then \
		fixture_extra="$$fixture_extra -DFIXTURE_CAPTURE_DISABLED=1"; \
	fi; \
	if [ "$$v" = FEATURE_LOG_DISK=0 ]; then \
		fixture_extra="$$fixture_extra -DFIXTURE_LOG_DISK_DISABLED=1"; \
	fi; \
	build="build/variant-$$k"; \
	rm -rf "$$build"; \
	$(MAKE) --no-print-directory BUILD="$$build" BOOT_TEST=1 \
	        EXTRA_CFLAGS="$$extra" FIXTURE_EXTRA_CFLAGS="$$fixture_extra" \
	        all fixtures; \
	FEATURE_VARIANT="$$v" sh tools/run-feature-variant.sh "$$build"

test-variants:
	@set -e; \
	for v in $(FEATURE_VARIANTS); do \
		echo "=== variant $$v"; \
		$(MAKE) --no-print-directory FEATURE_VARIANT="$$v" test-variant; \
	done
	@$(MAKE) --no-print-directory BUILD=build/var PIE=0 all
	@rm -rf build/var

allarch:
	@for a in $(ARCHES); do \
	  echo "=== $$a"; $(MAKE) --no-print-directory ARCH=$$a all || exit 1; \
	done

clean:
	@rm -rf build

help:
	@echo "ARCH is one of: $(ARCHES)"
	@echo "make [ARCH=...]                     build init"
	@echo "make test                           host unit tests"
	@echo "make abi-check ARCH=...             verify syscall numbers"
	@echo "make test-ns [INIT_NS_TIER=...]     boot test in a namespace"
	@echo "make status-reader [ARCH=...]       build the /run/init.status reader"
	@echo "make test-qemu ARCH=x86_64|aarch64  boot test under qemu"
	@echo "make test-variant FEATURE_VARIANT=  test one disabled feature"
	@echo "make test-variants                  test every disabled feature"
	@echo "make check                          everything runnable here"
	@echo "make check-all                      every installed toolchain and variant"
	@echo "make allarch                        build every target"
