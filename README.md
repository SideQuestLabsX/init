# init

PID 1 for embedded Linux. It brings the system up, keeps your programs running,
runs things on a schedule and shuts down without corrupting the filesystem.

It is one C file, links no libc, and allocates no heap.

## Why

An appliance needs very little from PID 1: mount a few filesystems, start some
programs, restart them when they die, run a cleanup job every night, reboot
cleanly. systemd does that and about four hundred other things. BusyBox init
does the first half and leaves you to script the rest.

This does the whole job in something you can read in an afternoon. On a device
you cannot attach a debugger to, that matters more than features.

## How it works

init runs **tasks**. A task is an executable, and the directory it sits in is
its schedule.

```
/tasks/always/resolver        started at boot, restarted whenever it dies
/tasks/boot/migrate           run at boot, done when it exits cleanly
/tasks/24h/rotate_logs        run every 24 hours
/tasks/1d-03-30/backup        run at 03:30 every day
/tasks/sun-04-00/deepclean    run at 04:00 every Sunday
```

That is the whole model. A daemon goes in `always/`, a cleanup job goes in an
interval directory, and both take the same path through init.

Tasks that die get restarted with exponential backoff, so a crash loop cannot
turn into a fork bomb. After enough consecutive failures init gives up on that
task and says so, rather than spinning forever.

To know whether a program is *working* rather than merely *alive*, drop an
executable next to it:

```
/tasks/always/resolver
/tasks/always/resolver.check     exit 0 means healthy
```

init runs the check on an interval and restarts the task if it keeps failing.
init has no idea what the check does: a program does not have to know anything
about init to be supervised by it.

Configuration is a header you edit and compile in. There is no config file at
runtime, and nothing to be missing or half-written when the power goes.

## Targets

`x86_64`, `x86`, `aarch64`, `armv7`, `armv6`, `riscv64`, `loongarch64`, `mips`,
`mipsel`.

## Building

```sh
make ARCH=x86_64
```

Any of `gcc`, `clang` or `zig cc` will do, as a compiler driver only. `make
allarch` builds every target whose configured compiler is installed. `PIE=0`
falls back to `-static -no-pie` where the toolchain cannot produce a working
static-pie.

## Testing

```sh
make check
```

Three tiers, each skipping cleanly when its prerequisites are absent:

| Command | What it exercises | Needs |
|---|---|---|
| `make test` | parsers, ring buffer, arena, backoff, `/proc` parsing, SNTP packets, under ASan/UBSan | a C compiler |
| `make test-ns` | the binary as a real PID 1 in a user+PID+mount namespace | `unshare`, unprivileged userns |
| `make test-qemu` | the same fixtures on a real kernel, including `devtmpfs` and `reboot(2)` | qemu, a kernel image |
| `make abi-check ARCH=…` | every hand-transcribed syscall number against the kernel's UAPI headers | that target's kernel headers |

On Windows, `tools/test.ps1` runs the suite through WSL, or falls back to the
host unit tests with a native compiler.

---

## Configuration

Edit [`config.h`](config.h), rebuild. Feature switches, timings, sizes, paths
and the per-task override table are all in there, each with a comment. `PIE=0`
on the make line is the one knob that is not.

### Schedules

The directory name under `/tasks/` is the schedule:

| Directory | When it runs |
|---|---|
| `always` | at boot, respawned whenever it dies |
| `boot` | at boot, a clean exit is final |
| `30s` `5m` `24h` `7d` | on that interval since boot, up to 366 days. Bare digits mean seconds |
| `<N>d-HH-MM` | every N days at that wall-clock time |
| `sun-HH-MM` … `sat-HH-MM` | at that time on that weekday |

A trailing `-HH-MM` makes it wall clock, at the fixed offset in
`CFG_TZ_OFFSET_SEC` with no DST handling. Without one it is an interval from
boot, which holds its phase for the life of that boot and takes a fresh one from
the next. Either way a slot that is missed entirely is dropped rather than
queued.

A board with no RTC boots at the epoch, so a wall-clock task fires early and is
re-dated on the first SNTP sync. An unsynced clock never holds one back.

Names are sorted byte-wise only so the same bounded task set is selected on each
boot. Numeric prefixes have no special meaning, and tasks are forked without
waiting. A task must tolerate a missing dependency and retry.

## Logging

init owns where output goes, not the tasks. Each task's `stdout` and
`stderr` are drained non-blocking into a shared ring buffer, and a forked child
does the disk writes so a stalled flash device can never block PID 1. Errors
reach persistent storage by default, routine chatter stays in RAM.

## Status

init publishes a consistent task snapshot at `/run/init.status`. Build the
freestanding reader for the target architecture and install it in the image:

```sh
make ARCH=x86_64 status-reader
build/x86_64/init-status
```

The output includes each task's PID, state, run count, failure count and latest
exit or probe result. The file lives on tmpfs and is recreated at boot.

## Design notes

One file, [`init.c`](init.c). `start.S` holds `_start`, which cannot be written
in C. The test suite compiles that same file three more times, cut down
at section boundaries, so nothing gets tested but the shipped source.

No libc, no heap, no third-party code. One static arena, sized at compile time,
is all the memory there is. Every OS interaction is a raw syscall through inline
assembly, and the syscall numbers are checked against the kernel's own headers
at build time.

## License

[The Unlicense](LICENSE). Public domain.
