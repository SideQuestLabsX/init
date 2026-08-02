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

Intervals use boot time and keep their phase until reboot. Calendar schedules
use `CFG_TZ_OFFSET_SEC`, with no DST handling. Missed slots and overruns are
skipped rather than queued.

A machine without a working RTC schedules calendar tasks against its current
clock. They may run early and are re-dated after the first successful SNTP sync.
An unsynced clock never blocks them.

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
limits, timing, output routes, the fixed timezone offset, SNTP settings and the
per-task rules table. There is no runtime configuration file. `PIE=0` is the
only make-time configuration switch outside the header.

## Build

Supported targets:

`x86_64`, `x86`, `aarch64`, `armv7`, `armv6`, `riscv64`, `loongarch64`, `mips`
and `mipsel`.

```sh
make ARCH=x86_64
make allarch
```

GCC, Clang and `zig cc` are supported as compiler drivers. `make allarch` builds
targets whose configured compilers are installed. The default is static PIE,
except LoongArch and MIPS; `PIE=0` selects `-static -no-pie` elsewhere.

## Test

```sh
make check
make check-all
```

| Command | Coverage | Requirements |
|---|---|---|
| `make test` | Parsers, arena, ring, backoff, `/proc` parsing and SNTP packets under ASan/UBSan | C compiler |
| `make test-ns` | The binary as PID 1 in user, PID and mount namespaces | `unshare`, unprivileged user namespaces |
| `make test-qemu` | Base boot fixtures on a real kernel, including `devtmpfs` and `reboot(2)` | QEMU and a kernel image |
| `make abi-check ARCH=...` | Hand-transcribed syscall and ABI constants against UAPI headers | Target kernel headers |

Runtime harnesses report a skip when namespace, QEMU or kernel prerequisites are
missing. Compiler and header failures are errors. `make check-all` also runs
feature variants, then builds and ABI-checks installed cross-toolchains.

On Windows, `tools/test.ps1` runs host tests, an x86_64 build and namespace tests
through WSL. Without WSL, it runs native host tests.

CI builds and ABI-checks all nine targets. It also runs architecture-specific QEMU
boot tests with pinned kernels for every target.

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
