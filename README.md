# init

Freestanding PID 1, process supervisor and task scheduler for embedded Linux.
The program is one C translation unit, uses raw syscalls, links no libc and
allocates no heap.

## Setup

Build for the target, install `build/<arch>/init` as `/bin/init` and boot the
kernel with `rdinit=/bin/init`. Put executable tasks under
`/tasks/<schedule>/`:

```text
/tasks/always/resolver
/tasks/boot/migrate
/tasks/24h/rotate_logs
/tasks/1d-03-30/backup
/tasks/sun-04-00/deepclean
/tasks/event-link/link_state
/tasks/event-address/address_state
```

Tasks start concurrently. Names are sorted byte-wise to select a deterministic
bounded set and assign stable status slots. Prefixes do not define order or
dependencies. Each task must tolerate unavailable dependencies and retry or
exit.

Tasks eligible for respawn use exponential backoff. After the configured number
of consecutive failures, init marks the task failed and stops respawning it.

## Schedules

The directory below `/tasks/` defines the schedule:

| Directory | Behavior |
|---|---|
| `always` | Start at boot and respawn after exit |
| `boot` | Start at boot; a clean exit is final |
| `500ms`, `30s`, `5m`, `24h`, `7d` | Run at that interval from boot, up to 366 days; bare digits mean seconds |
| `<N>d-HH-MM` | Run every N days at that wall-clock time |
| `sun-HH-MM` through `sat-HH-MM` | Run at that time on the named weekday |
| `event-link` | Run on link add or remove notifications |
| `event-address` | Run on IPv4 or IPv6 address add or remove notifications |

Intervals use boot time and keep their phase until reboot. When
`FEATURE_PERSIST_SCHEDULE` is enabled, successful interval runs are recorded on
the persistent state path and reused after the next successful SNTP sync.
Calendar schedules use `CFG_TZ_OFFSET_SEC`, with no DST handling. Missed slots
and overruns are skipped rather than queued.

Event schedules wait for their matching netlink notification, coalesce repeated
notifications and do not respawn after a run exits. Notifications received while
the task is active are skipped.

A machine without a working RTC schedules calendar tasks against its current
clock. They may run early and are re-dated after the first successful SNTP sync.
An unsynced clock never blocks them.

When `FEATURE_TASK_DISCOVERY` is enabled, init applies `/tasks` changes from the
event loop. Unchanged tasks keep their runtime state. Removed tasks stay stopped
and replacements wait for the old process group and probe to drain. Without
inotify, init rescans at `CFG_TASK_SCAN_NS`.

## Probes

An optional executable sibling checks whether an `always` task is working:

```text
/tasks/always/resolver
/tasks/always/resolver.check
```

Exit status 0 is healthy. Init applies a startup grace period, times out a hung
probe and restarts the task after repeated failures. Probes run with the task's
identity and capabilities. `/proc/PID/stat` sampling is always available for
state diagnostics and does not require task cooperation.

## Configuration

Edit [`config.h`](config.h) and rebuild. It contains feature switches, paths,
limits, timing, output routes, the fixed timezone offset, SNTP settings, the
schedule state paths and the per-task rules table. There is no runtime
configuration file. Immutable images can define `FEATURE_STATIC_TASKS=1` and
provide `INIT_STATIC_TASKS_H` with a NULL-terminated `STATIC_TASKS` table. This
loads only the compiled task list and disables runtime discovery.

Release builds provide named profiles:

| Profile | Behavior |
|---|---|
| `standard` | Shipped defaults |
| `offline` | Shipped defaults without runtime SNTP networking |
| `volatile` | Shipped defaults without disk logging |
| `offline-volatile` | No runtime SNTP networking or disk logging |
| `compressed` | Shipped defaults with checksummed LZ4-framed disk logs |
| `persistent` | Shipped defaults with reboot-persistent interval schedules |
| `durable` | Persistent schedules and compressed disk logs |
| `lean` | Offline supervision without watchdog, probes, capture, disk logging, live discovery or netlink events |

Every profile keeps capability dropping enabled. Static task tables remain
image-specific and are not distributed as a generic profile. `PROFILE`,
`RELEASE` and `PIE` are the make-time configuration switches outside the header.

## Build

Supported targets:

`x86_64`, `x86`, `aarch64`, `armv7`, `armv6`, `riscv64`, `loongarch64`, `mips`
and `mipsel`.

```sh
make ARCH=x86_64
make ARCH=x86_64 PROFILE=offline
make allarch
```

GCC, Clang and `zig cc` are supported as compiler drivers. `make allarch` builds
targets whose configured compilers are installed. The default is static PIE,
except LoongArch and MIPS; `PIE=0` selects `-static -no-pie` elsewhere. Named
profiles use separate output directories such as `build/x86_64-offline`.
CalVer releases use tags such as `v2026.08.06`. Manually running the release
workflow derives the current UTC date, resumes an incomplete same-commit draft
and skips an existing published release. Each release contains one archive per
architecture with every profile executable, a matching status reader, build
metadata and profile definitions. Released executables report their release and
profile in the startup message. Install the selected `init-<profile>` executable
as `/bin/init`.

Published releases are immutable and include a GitHub attestation. Verify one
with `gh release verify v2026.08.06 --repo SideQuestLabsX/init`.

## Test

```sh
make check
make check-all
```

| Command | Coverage | Requirements |
|---|---|---|
| `make test` | Parsers, arena, ring, backoff, `/proc` parsing and SNTP packets under ASan/UBSan | C compiler |
| `make lint` | Shell harness static analysis | ShellCheck |
| `make test-elf` | Static linkage, stack permissions and entry symbol | `readelf` |
| `make test-ns` | The binary as PID 1 in user, PID and mount namespaces | `unshare`, unprivileged user namespaces |
| `make test-qemu` | Base boot fixtures on a real kernel, including `devtmpfs` and `reboot(2)` | QEMU and a kernel image |
| `make test-qemu-watchdog` | Hardware watchdog arm, keepalive, withheld pets and reset | x86_64 QEMU and a pinned kernel |
| `make test-faults` | Syscall failure injection against a namespace boot | `unshare`, `timeout`, `strace`, unprivileged user namespaces |
| `make test-variant FEATURE_VARIANT=...` | One feature build with behavior markers and syscall checks | `strace`, `/usr/bin/time`, unprivileged user namespaces |
| `make test-profile PROFILE=...` | One named profile with runtime checks | `strace`, `/usr/bin/time`, unprivileged user namespaces |
| `make test-profiles` | Every named profile with namespace runtime checks | `strace`, `/usr/bin/time`, unprivileged user namespaces |
| `make test-release-profiles RELEASE_ARCHIVE=...` | Every extracted package profile with namespace checks | `tar`, `xz`, unprivileged user namespaces |
| `make test-release-profiles-qemu RELEASE_ARCHIVE=...` | Every extracted package profile in x86_64 QEMU | QEMU and a pinned kernel image |
| `make abi-check ARCH=...` | Hand-transcribed syscall and ABI constants against UAPI headers | Target kernel headers |

Runtime harnesses report a skip when namespace, QEMU or kernel prerequisites are
missing. Compiler and header failures are errors. `make check-all` also builds
and ABI-checks installed cross-toolchains, then runs feature variants.

On Windows, `tools/test.ps1` runs host tests, an x86_64 build and namespace tests
through WSL. Without WSL, it runs native host tests.

CI builds and ABI-checks all nine targets, builds the production binary with GCC
and Clang and runs every feature variant. Each target runs the full QEMU boot
test against a pinned Linux 6.1.75 kernel generated from source. CI caches only
the final kernel outputs. A separate x86_64 kernel covers a real hardware
watchdog reset test. A weekly QEMU-only run rebuilds every kernel without cache.
The `rebuild_kernels` manual workflow input starts the same check.

## Watchdog and shutdown

When enabled, init arms `CFG_WATCHDOG_DEV` and pets it only while every
`RULE_CRITICAL` task is healthy. Closing the device leaves the watchdog armed.

| Signal | Action |
|---|---|
| `SIGUSR2` | Clean shutdown, then power off |
| `SIGTERM`, `SIGUSR1`, `SIGINT`, `SIGHUP` | Clean shutdown, then restart |

## Logging

Init drains captured task output through non-blocking pipes into a shared ring.
A forked `init-logd` child performs batched disk writes, so PID 1 does not block
on storage. With the shipped defaults, stderr is persisted while stdout is
captured and discarded after transport. Per-task rules can select `drop`,
`ring`, `disk` or `ring+disk` for either stream.

Set `FEATURE_LOG_COMPRESSION=1` to write checksummed LZ4-framed batches. The
dependency-free host reader validates or extracts them:

```sh
python3 tools/init-log-read.py verify /var/log/init.log
python3 tools/init-log-read.py extract -o init.log.txt /var/log/init.log
```

## Status

When `/run` is available, init publishes a consistent task snapshot at
`/run/init.status`. Build and install the matching freestanding reader:

```sh
make ARCH=x86_64 status-reader
build/x86_64/init-status
```

The reader reports each task's PID, state, run count, failure count and latest
exit or probe result. The tmpfs file is recreated at boot. If file mapping
fails, init keeps an anonymous status block and continues without an external
reader.

## License

[The Unlicense](LICENSE). Public domain.
