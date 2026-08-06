# Freestanding build without libc or crt0

ARCHES  := x86_64 x86 aarch64 armv7 armv6 riscv64 loongarch64 mips mipsel
PROFILES := standard offline volatile offline-volatile compressed persistent durable lean

ARCH    ?= x86_64
PROFILE ?= custom
RELEASE ?= development
OUT     ?= out/release

BUILD_SUFFIX :=
ifneq ($(PROFILE),custom)
  BUILD_SUFFIX := $(BUILD_SUFFIX)-$(PROFILE)
endif
ifneq ($(RELEASE),development)
  BUILD_SUFFIX := $(BUILD_SUFFIX)-$(RELEASE)
endif
BUILD   ?= build/$(ARCH)$(BUILD_SUFFIX)
TARGET  := $(BUILD)/init

CC      ?= gcc
CC_ID   := $(notdir $(firstword $(CC)))
PROFILE_BUILD_ROOT ?= build/profiles/$(ARCH)/$(CC_ID)/$(RELEASE)
PROFILE_SMOKE_BUILD_ROOT ?= build/profile-smoke/$(ARCH)/$(CC_ID)
RELEASE_SMOKE_BUILD_ROOT ?= build/release-smoke/$(ARCH)/$(CC_ID)
RELEASE_PROFILE_RUNNER ?= namespace

ifneq ($(PROFILE),custom)
  ifeq ($(filter $(PROFILE),$(PROFILES)),)
    $(error Unknown PROFILE '$(PROFILE)'. Use: $(PROFILES))
  endif
  PROFILE_HEADER := profiles/$(PROFILE).h
  PROFILE_CFLAGS := -include $(PROFILE_HEADER)
endif

RELEASE_CFLAGS := -DINIT_RELEASE='"$(RELEASE)"'

# Every flag here is load-bearing. Stack protection uses the runtime in init.c
CFLAGS_COMMON := \
	-std=c11 -O2 \
	-nostdlib -ffreestanding \
	-fstack-protector-strong \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-fno-builtin -fno-common \
	-Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-I.

# ARMv6 lacks movw/movt and NEON, unaligned access is unsafe
# x86 needs a global guard because libc never initializes TLS
# ARM syscall assembly requires r7, reserved as the Thumb frame pointer
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
# LoongArch static PIE relocations are not supported by the self-relocator
# Keep user code on the baseline LoongArch ISA, QEMU leaves user SIMD disabled
else ifeq ($(ARCH),loongarch64)
  CFLAGS_ARCH := -mabi=lp64d -fno-jump-tables -mno-lsx -mno-lasx
  ARCH_PIE := 0
# MIPS static builds omit the unsupported GOT and $gp ABI
else ifeq ($(ARCH),mips)
  CFLAGS_ARCH := -march=mips32r2 -mno-abicalls -fno-pic -EB
  ARCH_PIE := 0
else ifeq ($(ARCH),mipsel)
  CFLAGS_ARCH := -march=mips32r2 -mno-abicalls -fno-pic -EL
  ARCH_PIE := 0
else
  $(error Unknown ARCH '$(ARCH)'. Use: $(ARCHES))
endif

# MIPS defaults non-PIE because the self-relocator cannot walk its GOT
ARCH_PIE ?= 1
PIE ?= $(ARCH_PIE)
ifeq ($(PIE),1)
  COMPILE_MODE := -fPIE
  LINKMODE := -static-pie
  # Some ARM cross linkers retain PT_INTERP for -static-pie without this flag
  LINKER_PIE_FLAGS := -Wl,--no-dynamic-linker
  ifeq ($(ARCH),loongarch64)
    # LoongArch GCC emits text relocations for freestanding static PIE
    LINKER_PIE_FLAGS += -Wl,-z,notext
  endif
else
  COMPILE_MODE := -fno-pie
  LINKMODE := -static -no-pie
  LINKER_PIE_FLAGS :=
endif

# Build-time policy belongs in config.h
EXTRA_CFLAGS ?=
WERROR ?= 0
WARN_CFLAGS :=
ifeq ($(WERROR),1)
  WARN_CFLAGS := -Werror
endif

# Boot tests replace task rules and shorten writer timing
BOOT_SNTP_SERVER ?= 127.0.0.1
BOOT_SNTP_PORT   ?= 40123
BOOT_CFLAGS      :=
ifeq ($(BOOT_TEST),1)
ifeq ($(WATCHDOG_TEST),1)
  BOOT_RULES := -DINIT_TASK_RULES_H='"tests/fixtures/watchdog_rules.h"'
  CONFIG_DEPS := config.h tests/fixtures/watchdog_rules.h
else
  BOOT_RULES := -DINIT_TASK_RULES_H='"tests/fixtures/test_rules.h"'
  CONFIG_DEPS := config.h tests/fixtures/test_rules.h
endif
  BOOT_CFLAGS := -DCFG_LOGD_FLUSH_NS=2000000000ull \
                 -DCFG_RESTART_GRACE_NS=500000000ull \
                 -DCFG_LOGD_STALL_NS=1000000000ull \
                 -DCFG_SNTP_SERVER='"$(BOOT_SNTP_SERVER)"' \
                 -DCFG_SNTP_PORT=$(BOOT_SNTP_PORT) \
                 -DCFG_SNTP_RETRY_NS=250000000ull
else
  BOOT_RULES :=
  CONFIG_DEPS := config.h
endif
ifneq ($(PROFILE_HEADER),)
  CONFIG_DEPS += $(PROFILE_HEADER)
endif
ifeq ($(WATCHDOG_TEST),1)
  BOOT_CFLAGS += -DCFG_WDOG_TIMEOUT_SEC=2 \
                 -DCFG_WDOG_PET_NS=200000000ull \
                 -DCFG_PROBE_FAIL_LIMIT=100 \
                 -DOFFLINE_MODE=1
endif

CFLAGS  := $(CFLAGS_COMMON) $(CFLAGS_ARCH) $(COMPILE_MODE) \
            $(PROFILE_CFLAGS) $(RELEASE_CFLAGS) $(EXTRA_CFLAGS) \
            $(WARN_CFLAGS) \
            $(BOOT_CFLAGS) $(BOOT_RULES)

# 32-bit u64 division requires libgcc's __udivdi3
LDFLAGS := -nostdlib $(LINKMODE) $(LINKER_PIE_FLAGS) -Wl,--gc-sections -Wl,-e,_start -Wl,-z,noexecstack -lgcc

OBJ := $(BUILD)/init.o $(BUILD)/start.o

.PHONY: all clean check check-all lint test test-config-overrides test-elf test-log-reader test-ns test-faults test-qemu test-qemu-watchdog test-variant test-variants test-profile test-profiles test-release-profile test-release-profile-qemu test-release-profiles test-release-profiles-qemu abi-check fixtures status-reader allarch release-profiles release-archive print-profiles help
.DEFAULT_GOAL := all

all: $(TARGET)

$(TARGET): $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS_ARCH) $(OBJ) -o $@ $(LDFLAGS)
	@echo "built $@"

$(BUILD)/%.o: %.c $(CONFIG_DEPS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/start.o: armv6-div.S

$(BUILD):
	@mkdir -p $(BUILD)

# tests

# INIT_HOSTED excludes syscall code for native sanitizer tests
HOST_CC    ?= cc
PYTHON     ?= python3
READELF    ?= readelf
HOST_BUILD := build/host
HOST_SRC   := tests/host/test_main.c
HOST_DEPS  := tests/host/test_main.c init.c $(CONFIG_DEPS)
HOST_CFLAGS := -std=c11 -g -O1 -I. \
               -Wall -Wextra -Wshadow -Wconversion \
               -DINIT_HOSTED=1 -fno-builtin -pthread $(WARN_CFLAGS)

SANITIZE ?= 1
ifeq ($(SANITIZE),1)
  HOST_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
endif

test: test-config-overrides test-log-reader $(HOST_BUILD)/unit
	@$(HOST_BUILD)/unit

test-log-reader:
	$(PYTHON) tools/test-log-read.py

test-elf: $(TARGET)
	READELF="$(READELF)" sh tools/check-elf.sh "$(TARGET)"

lint:
	shellcheck tools/*.sh

test-config-overrides: | $(HOST_BUILD)
	$(HOST_CC) -std=c11 -I. -Werror -c tests/host/config_override.c \
	           -o $(HOST_BUILD)/config_override.o

$(HOST_BUILD)/unit: $(HOST_DEPS) | $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_SRC) -o $@

$(HOST_BUILD):
	@mkdir -p $(HOST_BUILD)

# Pin transcribed ABI constants against target UAPI headers
abi-check: | $(BUILD)
	$(CC) $(CFLAGS_ARCH) -std=c11 -I. -DINIT_ABI_ONLY=1 -Wall -Wextra $(WARN_CFLAGS) \
	      -c tests/abi/abi_check.c -o $(BUILD)/abi_check.o
	@echo "abi ok for $(ARCH)"

FIXTURE_SRC := $(wildcard tests/fixtures/*.c)
FIXTURE_BIN := $(patsubst tests/fixtures/%.c,$(BUILD)/fixtures/%,$(FIXTURE_SRC))
FIXTURE_CFLAGS := -std=c11 -O1 -nostdlib -ffreestanding -fno-pie \
                  -fno-stack-protector -fno-builtin -fno-common \
                  -Wall -Wextra -I. -DINIT_FIXTURE=1 $(CFLAGS_ARCH) \
                  $(PROFILE_CFLAGS) \
                  $(FIXTURE_EXTRA_CFLAGS) $(WARN_CFLAGS)
FIXTURE_LDFLAGS := -nostdlib -static -Wl,-no-pie -Wl,-e,_start -Wl,-z,noexecstack
# The pinned arm64 kernel uses 4 KiB pages
ifeq ($(ARCH),aarch64)
  FIXTURE_LDFLAGS += -Wl,-z,max-page-size=0x1000
endif

fixtures: $(FIXTURE_BIN)

$(BUILD)/fixtures/%: tests/fixtures/%.c tests/fixtures/fstart.S \
                     tests/fixtures/fixture.h init.c armv6-div.S $(CONFIG_DEPS) | $(BUILD)
	@mkdir -p $(BUILD)/fixtures
	$(CC) $(FIXTURE_CFLAGS) $< tests/fixtures/fstart.S -o $@ \
	      $(FIXTURE_LDFLAGS) -lgcc

STATUS_READER := $(BUILD)/init-status
STATUS_READER_CFLAGS := -std=c11 -O2 -nostdlib -ffreestanding -fno-pie \
                         -fno-stack-protector -fno-builtin -fno-common \
                         -Wall -Wextra -I. -DINIT_FIXTURE=1 \
                         -DINIT_STATUS_READER=1 -DFIXTURE_ENTRY=StatusReaderMain \
                         $(CFLAGS_ARCH) \
                         $(PROFILE_CFLAGS) \
                         $(EXTRA_CFLAGS) $(WARN_CFLAGS)

status-reader: $(STATUS_READER)

$(STATUS_READER): tools/init-status.c tests/fixtures/fstart.S init.c armv6-div.S \
                  $(CONFIG_DEPS) | $(BUILD)
	$(CC) $(STATUS_READER_CFLAGS) tools/init-status.c tests/fixtures/fstart.S \
	      -o $@ -nostdlib -static -Wl,-no-pie -Wl,-e,_start -Wl,-z,noexecstack -lgcc

QEMU_BUILD ?= build/$(ARCH)-qemu
WATCHDOG_QEMU_BUILD ?= build/$(ARCH)-watchdog-qemu
NS_BUILD   ?= build/$(ARCH)-ns
NS_ROOTFS  ?= tools/stage-rootfs.sh

test-qemu:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(QEMU_BUILD) BOOT_TEST=1 \
	        BOOT_SNTP_SERVER=162.159.200.1 BOOT_SNTP_PORT=123 all fixtures
	ARCH=$(ARCH) BUILD=$(QEMU_BUILD) KERNEL="$(KERNEL)" DTB="$(DTB)" BIOS="$(BIOS)" \
	        TIMEOUT="$(TIMEOUT)" \
	        INIT_SNTP_FIXTURE=0 sh tools/run-qemu.sh

test-qemu-watchdog:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(WATCHDOG_QEMU_BUILD) BOOT_TEST=1 \
	        WATCHDOG_TEST=1 all fixtures
	ARCH=$(ARCH) BUILD=$(WATCHDOG_QEMU_BUILD) KERNEL="$(KERNEL)" DTB="$(DTB)" BIOS="$(BIOS)" \
	        TIMEOUT="$(TIMEOUT)" WATCHDOG_TEST=1 INIT_SNTP_FIXTURE=0 sh tools/run-qemu.sh

test-ns:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(NS_BUILD) BOOT_TEST=1 \
	        all fixtures status-reader
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_STATUS_READER=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_LOG_SYMLINK=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_STATUS_FALLBACK=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_NS_REMOUNT_TEST=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_STATUS_READER=1 \
	        INIT_TASK_DISCOVERY_TEST=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"
	ARCH=$(ARCH) INIT_NS_TIER=$(INIT_NS_TIER) INIT_NETLINK_TEST=1 \
	        sh tools/run-namespace.sh "$(NS_BUILD)" "$(NS_ROOTFS)"

test-faults:
	$(MAKE) --no-print-directory ARCH=$(ARCH) BUILD=$(NS_BUILD) BOOT_TEST=1 all fixtures
	ARCH=$(ARCH) sh tools/run-fault-tests.sh "$(NS_BUILD)"

check:
	$(MAKE) --no-print-directory test
	$(MAKE) --no-print-directory ARCH=$(ARCH) abi-check all test-elf
	$(MAKE) --no-print-directory ARCH=$(ARCH) test-ns
	$(MAKE) --no-print-directory ARCH=$(ARCH) test-faults
	$(MAKE) --no-print-directory ARCH=$(ARCH) test-qemu

FEATURE_VARIANTS ?= OFFLINE_MODE=1 FEATURE_WATCHDOG=0 FEATURE_EXEC_PROBES=0 \
                    FEATURE_LOG_DISK=0 FEATURE_LOG_CAPTURE=0 \
                    FEATURE_CAPABILITY_DROP=0 FEATURE_TASK_DISCOVERY=0 \
                    FEATURE_PERSIST_SCHEDULE=1 FEATURE_STATIC_TASKS=1 \
                    FEATURE_NETLINK_EVENTS=0 FEATURE_LOG_COMPRESSION=1

CC_x86_64      ?= gcc
CC_x86         ?= gcc
CC_aarch64     ?= aarch64-linux-gnu-gcc
CC_armv7       ?= arm-linux-gnueabihf-gcc
CC_armv6       ?= arm-linux-gnueabihf-gcc
CC_riscv64     ?= riscv64-linux-gnu-gcc
CC_loongarch64 ?= loongarch64-linux-gnu-gcc-14
CC_mips        ?= mips-linux-gnu-gcc
CC_mipsel      ?= mipsel-linux-gnu-gcc

ARCH_CC_SPECS := \
	"x86_64:$(CC_x86_64)" "x86:$(CC_x86)" \
	"aarch64:$(CC_aarch64)" "armv7:$(CC_armv7)" "armv6:$(CC_armv6)" \
	"riscv64:$(CC_riscv64)" "loongarch64:$(CC_loongarch64)" \
	"mips:$(CC_mips)" "mipsel:$(CC_mipsel)"

check-all: check
	@set -e; \
	for spec in $(ARCH_CC_SPECS); do \
		arch=$${spec%%:*}; cc=$${spec#*:}; \
		cc_bin=$${cc%% *}; \
		if ! command -v "$$cc_bin" >/dev/null 2>&1; then \
			echo "SKIP: $$arch compiler $$cc not installed"; \
			continue; \
		fi; \
		echo "=== check $$arch with $$cc"; \
		$(MAKE) --no-print-directory ARCH=$$arch BUILD=build/check-$$arch \
		        CC="$$cc" abi-check all status-reader test-elf; \
	done
	@$(MAKE) --no-print-directory test-variants
	@$(MAKE) --no-print-directory test-profiles

test-variant:
	$(if $(filter $(FEATURE_VARIANT),$(FEATURE_VARIANTS)),,$(error Unknown feature variant '$(FEATURE_VARIANT)'))
	@set -eu; \
	v='$(FEATURE_VARIANT)'; \
	k=$${v%%=*}; \
	feature_extra="-U$$k -D$$v"; \
	extra="$$feature_extra"; \
	fixture_extra="$$feature_extra"; \
	if [ "$$v" = FEATURE_PERSIST_SCHEDULE=1 ]; then \
		extra="$$extra -DFIXTURE_CLOCK_SET_SUCCESS=1"; \
	fi; \
	if [ "$$v" = FEATURE_LOG_CAPTURE=0 ]; then \
		fixture_extra="$$fixture_extra -DFIXTURE_CAPTURE_DISABLED=1"; \
	fi; \
	if [ "$$v" = FEATURE_LOG_COMPRESSION=1 ]; then \
		fixture_extra="$$fixture_extra -DFIXTURE_CAPTURE_DISABLED=1 -DFIXTURE_LOG_FORMAT_DISABLED=1"; \
	fi; \
	if [ "$$v" = FEATURE_LOG_DISK=0 ]; then \
		fixture_extra="$$fixture_extra -DFIXTURE_LOG_DISK_DISABLED=1"; \
	fi; \
	if [ "$$v" = FEATURE_STATIC_TASKS=1 ]; then \
		extra="$$extra -DINIT_STATIC_TASKS_H='\"tests/fixtures/static_tasks.h\"'"; \
		fixture_extra="$$fixture_extra -DINIT_STATIC_TASKS_H='\"tests/fixtures/static_tasks.h\"'"; \
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

test-profile:
	$(if $(filter $(PROFILE),$(PROFILES)),,$(error Unknown PROFILE '$(PROFILE)'. Use: $(PROFILES)))
	@set -eu; \
	profile='$(PROFILE)'; \
	extra=''; \
	fixture_extra='-DFIXTURE_PROFILE_SMOKE=1'; \
	case "$$profile" in \
	  persistent|durable) extra="$$extra -DFIXTURE_CLOCK_SET_SUCCESS=1" ;; \
	esac; \
	case "$$profile" in \
	  compressed|durable) fixture_extra="$$fixture_extra -DFIXTURE_CAPTURE_DISABLED=1 -DFIXTURE_LOG_FORMAT_DISABLED=1" ;; \
	esac; \
	build="$(PROFILE_SMOKE_BUILD_ROOT)/$$profile"; \
	rm -rf "$$build"; \
	$(MAKE) --no-print-directory ARCH="$(ARCH)" CC="$(CC)" \
	        BUILD="$$build" PROFILE="$$profile" RELEASE="$(RELEASE)" \
	        BOOT_TEST=1 WERROR="$(WERROR)" \
	        EXTRA_CFLAGS="$$extra" FIXTURE_EXTRA_CFLAGS="$$fixture_extra" \
	        all fixtures; \
	PROFILE="$$profile" RELEASE="$(RELEASE)" ARCH="$(ARCH)" \
	INIT_NS_TIER="$(INIT_NS_TIER)" TIMEOUT="$(TIMEOUT)" \
	sh tools/run-profile.sh "$$build"

test-profiles:
	@set -e; \
	for profile in $(PROFILES); do \
	  echo "=== profile behavior $$profile"; \
	  $(MAKE) --no-print-directory PROFILE="$$profile" test-profile; \
	done

test-release-profile:
	$(if $(filter $(PROFILE),$(PROFILES)),,$(error Unknown PROFILE '$(PROFILE)'. Use: $(PROFILES)))
	$(if $(RELEASE_ARCHIVE),,$(error RELEASE_ARCHIVE is required))
	@set -eu; \
	profile='$(PROFILE)'; \
	fixture_extra='-UFEATURE_EXEC_PROBES -DFEATURE_EXEC_PROBES=0 -UFEATURE_LOG_CAPTURE -DFEATURE_LOG_CAPTURE=0 -DFIXTURE_CAPTURE_DISABLED=1'; \
	case "$$profile" in \
	  compressed|durable) fixture_extra="$$fixture_extra -DFIXTURE_LOG_FORMAT_DISABLED=1" ;; \
	esac; \
	build="$(RELEASE_SMOKE_BUILD_ROOT)/$$profile"; \
	rm -rf "$$build"; \
	$(MAKE) --no-print-directory ARCH="$(ARCH)" CC="$(CC)" \
	        BUILD="$$build" PROFILE="$$profile" RELEASE="$(RELEASE)" \
	        BOOT_TEST=1 WERROR="$(WERROR)" \
	        FIXTURE_EXTRA_CFLAGS="$$fixture_extra" fixtures; \
	PROFILE="$$profile" ARCH="$(ARCH)" INIT_NS_TIER="$(INIT_NS_TIER)" \
	RUNNER="$(RELEASE_PROFILE_RUNNER)" TIMEOUT="$(TIMEOUT)" sh tools/run-release-profile.sh \
	        "$(RELEASE_ARCHIVE)" "$$profile" "$$build"

test-release-profile-qemu: RELEASE_PROFILE_RUNNER=qemu
test-release-profile-qemu: test-release-profile

test-release-profiles:
	@set -e; \
	for profile in $(PROFILES); do \
	  echo "=== release artifact behavior $$profile"; \
	  $(MAKE) --no-print-directory PROFILE="$$profile" test-release-profile; \
	done

test-release-profiles-qemu:
	@set -e; \
	for profile in $(PROFILES); do \
	  echo "=== release artifact QEMU behavior $$profile"; \
	  $(MAKE) --no-print-directory PROFILE="$$profile" test-release-profile-qemu; \
	done

allarch:
	@set -e; \
	for spec in $(ARCH_CC_SPECS); do \
	  arch=$${spec%%:*}; cc=$${spec#*:}; \
	  cc_bin=$${cc%% *}; \
	  if ! command -v "$$cc_bin" >/dev/null 2>&1; then \
	    echo "SKIP: $$arch compiler $$cc not installed"; \
	    continue; \
	  fi; \
	  echo "=== build $$arch with $$cc"; \
	  $(MAKE) --no-print-directory ARCH=$$arch CC="$$cc" all; \
	done

print-profiles:
	@echo "$(PROFILES)"

release-profiles:
	@set -e; \
	for profile in $(PROFILES); do \
	  echo "=== profile $$profile for $(ARCH)"; \
	  $(MAKE) --no-print-directory ARCH="$(ARCH)" CC="$(CC)" \
	          BUILD="$(PROFILE_BUILD_ROOT)/$$profile" PROFILE="$$profile" \
	          RELEASE="$(RELEASE)" WERROR="$(WERROR)" all test-elf; \
	done

release-archive:
	@ARCH="$(ARCH)" CC="$(CC)" RELEASE="$(RELEASE)" OUT="$(OUT)" \
	  sh tools/package-release.sh

clean:
	@rm -rf build

help:
	@echo "ARCH is one of: $(ARCHES)"
	@echo "make [ARCH=...]                     build init"
	@echo "make test                           host unit tests"
	@echo "make lint                           check shell scripts"
	@echo "make abi-check ARCH=...             verify syscall numbers"
	@echo "make test-elf ARCH=...              verify static ELF properties"
	@echo "make test-ns [INIT_NS_TIER=...]     boot test in a namespace"
	@echo "make test-faults                    fault-injected child and clock tests"
	@echo "make status-reader [ARCH=...]       build the /run/init.status reader"
	@echo "make test-qemu ARCH=x86|x86_64|aarch64  boot test under qemu"
	@echo "make test-qemu-watchdog                  test hardware watchdog reset"
	@echo "make test-variant FEATURE_VARIANT=  test one disabled feature"
	@echo "make test-variants                  test every disabled feature"
	@echo "make test-profile PROFILE=...       test one release profile"
	@echo "make test-profiles                  test every release profile"
	@echo "make test-release-profile PROFILE=... RELEASE_ARCHIVE=...  test an extracted profile"
	@echo "make test-release-profiles RELEASE_ARCHIVE=...              test every extracted profile"
	@echo "make test-release-profile-qemu PROFILE=... RELEASE_ARCHIVE=...  boot one extracted profile in QEMU"
	@echo "make test-release-profiles-qemu RELEASE_ARCHIVE=...              boot every extracted profile in QEMU"
	@echo "make check                          everything runnable here"
	@echo "make check-all                      every installed toolchain and variant"
	@echo "make allarch                        build every installed target"
	@echo "make release-profiles ARCH=...      verify every release profile"
	@echo "make release-archive ARCH=... RELEASE=YYYY.MM.DD  package one target"
