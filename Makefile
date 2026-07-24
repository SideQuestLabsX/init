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
ifeq ($(BOOT_TEST),1)
  CFLAGS += -DCFG_LOGD_FLUSH_NS=500000000ull \
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

.PHONY: all clean check test test-ns test-qemu abi-check fixtures allarch help
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
               -DINIT_HOSTED=1 -fno-builtin

SANITIZE ?= 1
ifeq ($(SANITIZE),1)
  HOST_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
endif

test: $(HOST_BUILD)/unit
	@$(HOST_BUILD)/unit

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
                  -Wall -Wextra -I. -DINIT_FIXTURE=1 $(CFLAGS_ARCH)

fixtures: $(FIXTURE_BIN)

$(BUILD)/fixtures/%: tests/fixtures/%.c tests/fixtures/fstart.S \
                     tests/fixtures/fixture.h init.c $(CONFIG_DEPS) | $(BUILD)
	@mkdir -p $(BUILD)/fixtures
	$(CC) $(FIXTURE_CFLAGS) $< tests/fixtures/fstart.S -o $@ \
	      -nostdlib -static -no-pie -Wl,-e,_start -Wl,-z,noexecstack

QEMU_BUILD := build/$(ARCH)-qemu
NS_BUILD   := build/$(ARCH)-ns

test-qemu:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(QEMU_BUILD) BOOT_TEST=1 \
	        all fixtures
	ARCH=$(ARCH) BUILD=$(QEMU_BUILD) sh tools/run-qemu.sh

# Same fixtures without an emulator: PID 1 inside a user+pid+mount namespace.
test-ns:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(NS_BUILD) BOOT_TEST=1 \
	        all fixtures
	ARCH=$(ARCH) BUILD=$(NS_BUILD) sh tools/run-namespace.sh

check: test abi-check all test-ns test-qemu

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
	@echo "make test-ns                        boot test in a namespace"
	@echo "make test-qemu ARCH=x86_64|aarch64  boot test under qemu"
	@echo "make check                          everything runnable here"
	@echo "make allarch                        build every target"
