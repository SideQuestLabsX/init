/* init: a freestanding PID 1, process supervisor and task scheduler.
 *
 * One translation unit, deliberately. The point of the binary is that you can
 * read all of it, and following a boot path across twenty files is navigation,
 * not reading. Sections run in dependency order: kernel ABI, syscall-free
 * logic, syscalls, program. `_start` is in start.S, being unexpressible in C.
 *
 * No libc, no crt0. The only headers are the four a freestanding
 * implementation has to provide.
 *
 * Three cut-down builds compile this same file, so the tests never drift from
 * what ships:
 *
 *   INIT_ABI_ONLY   kernel ABI constants alone, for tests/abi/abi_check.c
 *   INIT_HOSTED     everything syscall-free, for tests/host under ASan/UBSan
 *   INIT_FIXTURE    the above plus syscall wrappers, for the boot fixtures
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* ======================================================================
 * types
 * ====================================================================== */

typedef int8_t   i8;
typedef uint8_t  u8;
typedef int16_t  i16;
typedef uint16_t u16;
typedef int32_t  i32;
typedef uint32_t u32;
typedef int64_t  i64;
typedef uint64_t u64;

/* register width on both LP64 and ILP32 */
typedef long          isize;
typedef unsigned long usize;

#define ARRAY_LEN(a)   (sizeof(a) / sizeof((a)[0]))
#define UNUSED(x)      ((void)(x))
#define NORETURN       __attribute__((noreturn))
#define NO_SSP         __attribute__((no_stack_protector))
#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)

#define NS_PER_US  1000ull
#define NS_PER_MS  1000000ull
#define NS_PER_SEC 1000000000ull

/* ======================================================================
 * syscall numbers
 * ====================================================================== */

/* Transcribed by hand so the freestanding build includes no kernel headers.
 * tests/abi/abi_check.c static_asserts every value below against <asm/unistd.h>
 * for the target arch. Build it for all five and a transcription slip is a
 * compile error rather than a runtime mystery.
 *
 * Only the openat/renameat/unlinkat/mkdirat/faccessat/dup3/pipe2/clone forms,
 * since the legacy variants are absent from the generic (arm64) table. */

#if defined(__x86_64__)

  #define SYS_read            0
  #define SYS_write           1
  #define SYS_close           3
  #define SYS_lseek           8
  #define SYS_mmap            9
  #define SYS_munmap          11
  #define SYS_rt_sigaction    13
  #define SYS_rt_sigprocmask  14
  #define SYS_rt_sigreturn    15
  #define SYS_ioctl           16
  #define SYS_sched_yield     24
  #define SYS_dup3            292
  #define SYS_fcntl           72
  #define SYS_nanosleep       35
  #define SYS_getpid          39
  #define SYS_socket          41
  #define SYS_connect         42
  #define SYS_bind            49
  #define SYS_sendto          44
  #define SYS_recvfrom        45
  #define SYS_setsockopt      54
  #define SYS_clone           56
  #define SYS_execve          59
  #define SYS_exit            60
  #define SYS_wait4           61
  #define SYS_kill            62
  #define SYS_fsync           74
  #define SYS_ftruncate       77
  #define SYS_getdents64      217
  #define SYS_chdir           80
  #define SYS_umask           95
  #define SYS_setuid          105
  #define SYS_setgid          106
  #define SYS_setgroups       116
  #define SYS_setsid          112
  #define SYS_capset          126
  #define SYS_prctl           157
  #define SYS_sync            162
  #define SYS_mount           165
  #define SYS_umount2         166
  #define SYS_reboot          169
  #define SYS_clock_settime   227
  #define SYS_clock_gettime   228
  #define SYS_exit_group      231
  #define SYS_openat          257
  #define SYS_mkdirat         258
  #define SYS_unlinkat        263
  #define SYS_renameat        264
  #define SYS_faccessat       269
  #define SYS_ppoll           271
  #define SYS_pipe2           293
  #define SYS_getrandom       318

  #define INIT_ARCH_NAME      "x86_64"
  #define INIT_HAS_SA_RESTORER 1

#elif defined(__i386__)

  #define SYS_read            3
  #define SYS_write           4
  #define SYS_close           6
  #define SYS_lseek           19
  #define SYS_mmap2           192
  #define SYS_munmap          91
  #define SYS_rt_sigaction    174
  #define SYS_rt_sigprocmask  175
  #define SYS_rt_sigreturn    173
  #define SYS_ioctl           54
  #define SYS_sched_yield     158
  #define SYS_dup3            330
  #define SYS_fcntl           55
  #define SYS_getpid          20
  #define SYS_socket          359
  #define SYS_connect         362
  #define SYS_bind            361
  #define SYS_sendto          369
  #define SYS_recvfrom        371
  #define SYS_setsockopt      366
  #define SYS_clone           120
  #define SYS_execve          11
  #define SYS_exit            1
  #define SYS_wait4           114
  #define SYS_kill            37
  #define SYS_fsync           118
  #define SYS_ftruncate       93
  #define SYS_getdents64      220
  #define SYS_chdir           12
  #define SYS_umask           60
  #define SYS_setuid          213      /* setuid32 */
  #define SYS_setgid          214      /* setgid32 */
  #define SYS_setgroups       206      /* setgroups32 */
  #define SYS_setsid          66
  #define SYS_capset          185
  #define SYS_prctl           172
  #define SYS_sync            36
  #define SYS_mount           21
  #define SYS_umount2         52
  #define SYS_reboot          88
  #define SYS_clock_settime64 404
  #define SYS_clock_gettime64 403
  #define SYS_exit_group      252
  #define SYS_openat          295
  #define SYS_mkdirat         296
  #define SYS_unlinkat        301
  #define SYS_renameat        302
  #define SYS_faccessat       307
  #define SYS_ppoll_time64    414
  #define SYS_pipe2           331
  #define SYS_getrandom       355

  #define INIT_ARCH_NAME      "x86"
  #define INIT_HAS_SA_RESTORER 1
  #define INIT_TIME32_ABI     1
  #define INIT_UID32_ABI      1
  #define INIT_MMAP2          1

#elif defined(__aarch64__) || \
      (defined(__riscv) && __riscv_xlen == 64) || \
      (defined(__loongarch__) && __loongarch_grlen == 64)

  /* asm-generic/unistd.h table, shared verbatim by every 64-bit arch that
   * adopted it. None of them has a legacy table to be compatible with, so the
   * numbers are the same and only the trap instruction differs. */
  #define SYS_read            63
  #define SYS_write           64
  #define SYS_close           57
  #define SYS_lseek           62
  #define SYS_mmap            222
  #define SYS_munmap          215
  #define SYS_rt_sigaction    134
  #define SYS_rt_sigprocmask  135
  #define SYS_rt_sigreturn    139
  #define SYS_ioctl           29
  #define SYS_sched_yield     124
  #define SYS_dup3            24
  #define SYS_fcntl           25
  #define SYS_nanosleep       101
  #define SYS_getpid          172
  #define SYS_socket          198
  #define SYS_connect         203
  #define SYS_bind            200
  #define SYS_sendto          206
  #define SYS_recvfrom        207
  #define SYS_setsockopt      208
  #define SYS_clone           220
  #define SYS_execve          221
  #define SYS_exit            93
  #define SYS_wait4           260
  #define SYS_kill            129
  #define SYS_fsync           82
  #define SYS_ftruncate       46
  #define SYS_getdents64      61
  #define SYS_chdir           49
  #define SYS_umask           166
  #define SYS_setuid          146
  #define SYS_setgid          144
  #define SYS_setgroups       159
  #define SYS_setsid          157
  #define SYS_capset          91
  #define SYS_prctl           167
  #define SYS_sync            81
  #define SYS_mount           40
  #define SYS_umount2         39
  #define SYS_reboot          142
  #define SYS_clock_settime   112
  #define SYS_clock_gettime   113
  #define SYS_exit_group      94
  #define SYS_openat          56
  #define SYS_mkdirat         34
  #define SYS_unlinkat        35
  /* arm64 opts into the `renameat` ABI group and still carries 38. riscv64 and
   * loongarch64 arrived after renameat2 superseded it and carry only that. See
   * syscall_abis_64 in each arch's kernel/Makefile.syscalls. */
  #if defined(__aarch64__)
    #define SYS_renameat      38
  #else
    #define SYS_renameat2     276
  #endif
  #define SYS_faccessat       48
  #define SYS_ppoll           73
  #define SYS_pipe2           59
  #define SYS_getrandom       278

  #if defined(__aarch64__)
    #define INIT_ARCH_NAME    "aarch64"
    /* arm64 does carry SA_RESTORER, so its struct sigaction has the extra
     * member and omitting it would put sa_mask at the wrong offset. riscv64 and
     * loongarch64 have no such member and take the kernel's vDSO trampoline. */
    #define INIT_HAS_SA_RESTORER 1
  #elif defined(__riscv)
    #define INIT_ARCH_NAME    "riscv64"
  #else
    #define INIT_ARCH_NAME    "loongarch64"
  #endif

#elif defined(__arm__)

  /* ARM EABI table (arch/arm/tools/syscall.tbl), shared by ARMv6 and ARMv7 */
  #define SYS_read            3
  #define SYS_write           4
  #define SYS_close           6
  #define SYS_lseek           19
  #define SYS_mmap2           192
  #define SYS_munmap          91
  #define SYS_rt_sigaction    174
  #define SYS_rt_sigprocmask  175
  #define SYS_rt_sigreturn    173
  #define SYS_ioctl           54
  #define SYS_sched_yield     158
  #define SYS_dup3            358
  #define SYS_fcntl           55
  #define SYS_getpid          20
  #define SYS_socket          281
  #define SYS_connect         283
  #define SYS_bind            282
  #define SYS_sendto          290
  #define SYS_recvfrom        292
  #define SYS_setsockopt      294
  #define SYS_clone           120
  #define SYS_execve          11
  #define SYS_exit            1
  #define SYS_wait4           114
  #define SYS_kill            37
  #define SYS_fsync           118
  #define SYS_ftruncate       93
  #define SYS_getdents64      217
  #define SYS_chdir           12
  #define SYS_umask           60
  #define SYS_setuid          213      /* setuid32 */
  #define SYS_setgid          214      /* setgid32 */
  #define SYS_setgroups       206      /* setgroups32 */
  #define SYS_setsid          66
  #define SYS_capset          185
  #define SYS_prctl           172
  #define SYS_sync            36
  #define SYS_mount           21
  #define SYS_umount2         52
  #define SYS_reboot          88
  #define SYS_clock_settime64 404
  #define SYS_clock_gettime64 403
  #define SYS_exit_group      248
  #define SYS_openat          322
  #define SYS_mkdirat         323
  #define SYS_unlinkat        328
  #define SYS_renameat        329
  #define SYS_faccessat       334
  #define SYS_ppoll_time64    414
  #define SYS_pipe2           359
  #define SYS_getrandom       384

  #define INIT_ARCH_NAME      "arm"
  #define INIT_HAS_SA_RESTORER 1
  #define INIT_TIME32_ABI     1
  #define INIT_UID32_ABI      1
  #define INIT_MMAP2          1

#elif defined(__mips__) && _MIPS_SIM == _ABIO32

  /* o32 table (arch/mips/kernel/syscalls/syscall_o32.tbl), offset by
   * __NR_Linux == 4000. MIPS diverges from every other port well beyond these
   * numbers, see INIT_MIPS_ABI below. */
  #define SYS_read            4003
  #define SYS_write           4004
  #define SYS_close           4006
  #define SYS_lseek           4019
  #define SYS_mmap2           4210
  #define SYS_munmap          4091
  #define SYS_rt_sigaction    4194
  #define SYS_rt_sigprocmask  4195
  #define SYS_rt_sigreturn    4193
  #define SYS_ioctl           4054
  #define SYS_sched_yield     4162
  #define SYS_dup3            4327
  #define SYS_fcntl           4055
  #define SYS_getpid          4020
  #define SYS_socket          4183
  #define SYS_connect         4170
  #define SYS_bind            4169
  #define SYS_sendto          4180
  #define SYS_recvfrom        4176
  #define SYS_setsockopt      4181
  #define SYS_clone           4120
  #define SYS_execve          4011
  #define SYS_exit            4001
  #define SYS_wait4           4114
  #define SYS_kill            4037
  #define SYS_fsync           4118
  #define SYS_ftruncate       4093
  #define SYS_getdents64      4219
  #define SYS_chdir           4012
  #define SYS_umask           4060
  #define SYS_setuid          4023
  #define SYS_setgid          4046
  #define SYS_setgroups       4081
  #define SYS_setsid          4066
  #define SYS_capset          4205
  #define SYS_prctl           4192
  #define SYS_sync            4036
  #define SYS_mount           4021
  #define SYS_umount2         4052
  #define SYS_reboot          4088
  #define SYS_clock_settime64 4404
  #define SYS_clock_gettime64 4403
  #define SYS_exit_group      4246
  #define SYS_openat          4288
  #define SYS_mkdirat         4289
  #define SYS_unlinkat        4294
  #define SYS_renameat        4295
  #define SYS_faccessat       4300
  #define SYS_ppoll_time64    4414
  #define SYS_pipe2           4328
  #define SYS_getrandom       4353

  #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define INIT_ARCH_NAME    "mips"
  #else
    #define INIT_ARCH_NAME    "mipsel"
  #endif

  /* o32 is a 32-bit ABI, so the time64 calls apply. The *32 uid calls do not:
   * MIPS uids were 32-bit from the start and it never grew the compat set. */
  #define INIT_TIME32_ABI     1
  #define INIT_MMAP2          1
  #define INIT_MIPS_ABI       1
  /* MIPS has no SA_RESTORER, the kernel supplies the return trampoline */

#else
  #error "unsupported architecture"
#endif

/* ======================================================================
 * kernel ABI
 * ====================================================================== */

/* Also hand-transcribed, also asserted by tests/abi/abi_check.c.
 *
 * INIT_MIPS_ABI guards the groups where MIPS kept its pre-Linux SunOS/BSD
 * numbering instead of the values every other port shares. SIGCHLD, O_NONBLOCK,
 * MAP_ANONYMOUS and SOCK_DGRAM all mean something else there. */

/* ---- errno (returned negated) ---- */
#define EPERM     1
#define ENOENT    2
#define EINTR     4
#define EIO       5
#define ENXIO     6
#define EBADF     9
#define ECHILD    10
#define EAGAIN    11
#define ENOMEM    12
#define EACCES    13
#define EFAULT    14
#define EBUSY     16
#define EEXIST    17
#define ENODEV    19
#define ENOTDIR   20
#define EISDIR    21
#define EINVAL    22
#define ENFILE    23
#define EMFILE    24
#define ENOSPC    28
#define ESPIPE    29
#define EROFS     30
#define ERANGE    34
/* above here is asm-generic/errno-base.h and shared, below is
 * asm-generic/errno.h, which MIPS replaces wholesale */
#ifdef INIT_MIPS_ABI
  #define ENOSYS    89
  #define ENOTEMPTY 93
  #define ELOOP     90
  #define ETIMEDOUT 145
#else
  #define ENOSYS    38
  #define ENOTEMPTY 39
  #define ELOOP     40
  #define ETIMEDOUT 110
#endif

/* ---- open / fcntl ---- */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

#ifdef INIT_MIPS_ABI
  #define O_APPEND    0x0008
  #define O_NONBLOCK  0x0080
  #define O_CREAT     0x0100
  #define O_TRUNC     0x0200
  #define O_EXCL      0x0400
  #define O_NOCTTY    0x0800
#else
  #define O_CREAT     0100
  #define O_EXCL      0200
  #define O_NOCTTY    0400
  #define O_TRUNC     01000
  #define O_APPEND    02000
  #define O_NONBLOCK  04000
#endif

/* Both ARM ports reorder the upper fcntl bits, arm64 included: it kept ARM32's
 * fcntl.h. At the generic values these two bits are O_DIRECT and O_LARGEFILE
 * there, so a directory open would carry O_DIRECT. Every other target, MIPS
 * included, inherits asm-generic/fcntl.h. */
#if defined(__arm__) || defined(__aarch64__)
  #define O_DIRECTORY 040000
  #define O_NOFOLLOW  0100000
#else
  #define O_DIRECTORY 0200000
  #define O_NOFOLLOW  0400000
#endif
#define O_CLOEXEC   02000000

#define AT_FDCWD          (-100)
#define AT_REMOVEDIR      0x200
#define AT_SYMLINK_NOFOLLOW 0x100

#define F_GETFL 3
#define F_SETFL 4

#define X_OK 1
#define F_OK 0

#define SEEK_SET 0
#define SEEK_END 2

/* ---- mount ---- */
#define MS_RDONLY   1u
#define MS_NOSUID   2u
#define MS_NODEV    4u
#define MS_NOEXEC   8u
#define MS_REMOUNT  32u
#define MS_NOATIME  1024u
#define MS_BIND     4096u

#define MNT_DETACH  2

/* ---- mmap ---- */
#define PROT_NONE   0
#define PROT_READ   1
#define PROT_WRITE  2
#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED   0x10
#ifdef INIT_MIPS_ABI
  #define MAP_ANONYMOUS 0x0800
#else
  #define MAP_ANONYMOUS 0x20
#endif
#define MAP_FAILED  ((void *)-1)

/* ---- signals ---- */
#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGABRT  6
#define SIGKILL  9
#define SIGSEGV  11
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15

/* the job-control and user signals are where MIPS parts company */
#ifdef INIT_MIPS_ABI
  #define SIGUSR1  16
  #define SIGUSR2  17
  #define SIGCHLD  18
  #define SIGSTOP  23
  #define SIGTSTP  24
  #define SIGCONT  25

  #define SA_NOCLDSTOP 0x00000001u
  #define SA_SIGINFO   0x00000008u
  #define SA_RESTART   0x10000000u

  #define SIG_BLOCK    1
  #define SIG_UNBLOCK  2
  #define SIG_SETMASK  3
#else
  #define SIGUSR1  10
  #define SIGUSR2  12
  #define SIGCHLD  17
  #define SIGCONT  18
  #define SIGSTOP  19
  #define SIGTSTP  20

  #define SA_NOCLDSTOP 0x00000001u
  #define SA_SIGINFO   0x00000004u
  #define SA_RESTORER  0x04000000u
  #define SA_RESTART   0x10000000u

  #define SIG_BLOCK    0
  #define SIG_UNBLOCK  1
  #define SIG_SETMASK  2
#endif

#define KSIGSET_BYTES 8

typedef struct
{
    u64 bits;
} KSigSet;

/* handler and restorer are integers because ISO C has no function-pointer to
 * void * conversion, and this struct only ever crosses the syscall boundary */
typedef struct
{
    usize handler;
    usize flags;
#ifdef INIT_HAS_SA_RESTORER
    usize restorer;
#endif
    u64   mask;
} KSigAction;

/* ---- wait ---- */
#define WNOHANG   1
#define WUNTRACED 2

#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) ((((s) & 0x7f) + 1) >> 1 > 0)

/* ---- poll ---- */
#define POLLIN   0x0001
#define POLLPRI  0x0002
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020

typedef struct
{
    i32 fd;
    i16 events;
    i16 revents;
} KPollFd;

/* ---- time ---- */
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1
#define CLOCK_BOOTTIME  7

/* 64-bit timespec throughout: used directly on LP64, and via the *_time64
 * syscalls on 32-bit targets, so nothing here is 2038-limited */
typedef struct
{
    i64 sec;
    i64 nsec;
} KTimeSpec;

/* ---- prctl ---- */
#define PR_SET_PDEATHSIG      1
#define PR_SET_KEEPCAPS       8
#define PR_SET_NAME           15
#define PR_CAPBSET_DROP       24
#define PR_SET_NO_NEW_PRIVS   38
#define PR_CAP_AMBIENT        47
#define PR_CAP_AMBIENT_RAISE      2
#define PR_CAP_AMBIENT_CLEAR_ALL  4

/* ---- capabilities ---- */
#define LINUX_CAPABILITY_VERSION_3 0x20080522u
#define CAP_LAST_CAP 40

typedef struct
{
    u32 version;
    i32 pid;
} KCapHeader;

typedef struct
{
    u32 effective;
    u32 permitted;
    u32 inheritable;
} KCapData;

/* ---- reboot ---- */
#define LINUX_REBOOT_MAGIC1     0xfee1deadu
#define LINUX_REBOOT_MAGIC2     672274793u
#define LINUX_REBOOT_CMD_RESTART   0x01234567u
#define LINUX_REBOOT_CMD_HALT      0xcdef0123u
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321fedcu

/* ---- dirent ---- */
#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8
#define DT_LNK     10

typedef struct
{
    u64 ino;
    i64 off;
    u16 reclen;
    u8  type;
    char name[];
} KDirent64;

/* ---- sockets ---- */
#define AF_INET       2

/* MIPS replaces enum sock_type outright for SunOS binary compatibility, and the
 * first two entries come out swapped. Getting this wrong opens a TCP socket. */
#ifdef INIT_MIPS_ABI
  #define SOCK_DGRAM  1
#else
  #define SOCK_DGRAM  2
#endif

/* the kernel spells these as the O_ bits, so they follow the fcntl block */
#define SOCK_NONBLOCK O_NONBLOCK
#define SOCK_CLOEXEC  O_CLOEXEC
#define IPPROTO_UDP   17

typedef struct
{
    u16 family;
    u16 port;    /* network byte order */
    u32 addr;    /* network byte order */
    u8  pad[8];
} KSockAddrIn;

/* ---- watchdog ioctls ('W' == 0x57) ---- */

/* MIPS packs the direction field into 3 bits at 29 with its own encoding
 * (NONE 1, READ 2, WRITE 4), so _IOR lands elsewhere. _IOWR coincides. */
#ifdef INIT_MIPS_ABI
  #define WDIOC_KEEPALIVE 0x40045705u
#else
  #define WDIOC_KEEPALIVE 0x80045705u
#endif
#define WDIOC_SETTIMEOUT  0xc0045706u

/* ---- getrandom ---- */
#define GRND_NONBLOCK 0x0001

#if !defined(INIT_ABI_ONLY)

/* ======================================================================
 * tunables
 * ====================================================================== */

/* Everything configurable is in one header, and init reads no configuration at
 * runtime. Included here because it needs the types above. */
#ifndef INIT_CONFIG_H
  #define INIT_CONFIG_H "config.h"
#endif
#include INIT_CONFIG_H

/* ======================================================================
 * strings and memory
 * ====================================================================== */

/* GCC lowers struct copies and array init to memcpy/memset calls even under
 * -ffreestanding, so those four symbols have to exist under their C names.
 * The host build takes libc's, since overriding them there fights the
 * sanitiser interceptors. */
#ifdef INIT_HOSTED
  #include <string.h>
#else
void  *memcpy(void *dst, const void *src, usize n);
void  *memmove(void *dst, const void *src, usize n);
void  *memset(void *dst, int c, usize n);
int    memcmp(const void *a, const void *b, usize n);
#endif

usize StrLen(const char *s);
i32   StrCmp(const char *a, const char *b);
i32   StrNCmp(const char *a, const char *b, usize n);
bool  StrEq(const char *a, const char *b);
bool  StrStartsWith(const char *s, const char *prefix);
bool  StrEndsWith(const char *s, const char *suffix);
const char *StrChr(const char *s, char c);
const char *StrRChr(const char *s, char c);

/* Bounded and NUL-terminating when cap is nonzero */
usize StrCopy(char *dst, usize cap, const char *src);
usize StrCopyN(char *dst, usize cap, const char *src, usize n);
usize StrCat(char *dst, usize cap, const char *src);
bool  StrCopyOk(char *dst, usize cap, const char *src);
bool  StrCopyNOk(char *dst, usize cap, const char *src, usize n);
bool  StrCatOk(char *dst, usize cap, const char *src);

/* "/tasks/always" + "resolver" -> "/tasks/always/resolver" */
usize PathJoin(char *dst, usize cap, const char *dir, const char *leaf);
bool  PathJoinOk(char *dst, usize cap, const char *dir, const char *leaf);

/* Digits only, no sign. False on empty input or overflow. *end, if given, gets
 * the first unconsumed character. */
bool ParseU64(const char *s, u64 *out, const char **end);

/* "500ms" "30s" "5m" "24h" "7d" -> nanoseconds. Bare digits mean seconds. */
bool ParseDuration(const char *s, u64 *outNs);

/* Wall-clock schedules. Everything below comes out of unixSec / 86400, which is
 * why day-of-week and every-N-days are cheap and a day-of-month would not be:
 * that needs month lengths, and then leap years, and then a calendar. */
#define CAL_EVERY_NDAY 0u   /* param = N days, phase taken from the epoch */
#define CAL_WEEKDAY    1u   /* param = 0 for Sunday */

#define CAL_MAX_PERIOD_DAYS 366u

typedef struct
{
    u32 kind;
    u32 param;
    u32 daySec;    /* seconds since local midnight */
} CalSpec;

/* "<N>d-HH-MM", and "sun-HH-MM" with the other six day names. Hours and minutes
 * are two digits each, so 1d-3-30 is rejected rather than guessed at. */
bool ParseCalendar(const char *s, CalSpec *out);

/* Local days since the epoch, seconds into that day, and the weekday of such a
 * day. The offset is compile-time (CFG_TZ_OFFSET_SEC) because a tzdata reader
 * is a parser, a file dependency and a DST table. */
u64 LocalDayNum(u64 unixSec, i32 tzOffsetSec);
u32 LocalDaySec(u64 unixSec, i32 tzOffsetSec);
u32 LocalWeekday(u64 dayNum);

/* Seconds until the spec next matches, always > 0: landing exactly on the
 * target waits for the following occurrence rather than refiring on the spot. */
u64 SecsUntilCalendar(const CalSpec *c, u64 unixSec, i32 tzOffsetSec);

/* The next interval deadline after prevNs, always > nowNs. Advancing from the
 * previous deadline rather than from the wake time is what keeps the phase: add
 * the interval to "now" instead and every wake-up's scheduling latency is kept,
 * so a task first run at 03:21 is minutes later by the end of the year.
 * Deadlines already missed are dropped, never queued. */
u64 IntervalAdvance(u64 prevNs, u64 intervalNs, u64 nowNs);

/* Dotted quad to a host-order u32. init resolves no hostnames. */
bool ParseIPv4(const char *s, u32 *out);

usize FmtU64(char *dst, usize cap, u64 v);
usize FmtI64(char *dst, usize cap, i64 v);
usize FmtHex(char *dst, usize cap, u64 v, usize minDigits);

/* printf subset: %s %c %d %i %u %x %% plus the l/ll/z length modifiers */
usize Fmt(char *dst, usize cap, const char *fmt, ...);
usize FmtV(char *dst, usize cap, const char *fmt, va_list ap);

u64   Hash64(const void *data, usize n);

bool NameSetInsert(char **names, char *storage, usize *count, usize maxEntries,
                   usize nameCap, const char *name);
u8 LogPolicyResolve(u8 policy, u8 defaultPolicy);

#ifndef INIT_HOSTED

void *memcpy(void *dst, const void *src, usize n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for(usize i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, usize n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    if(d == s || n == 0)
        return dst;
    if(d < s)
    {
        for(usize i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        for(usize i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int c, usize n)
{
    u8 *d = (u8 *)dst;
    u8 v = (u8)c;
    for(usize i = 0; i < n; i++)
        d[i] = v;
    return dst;
}

int memcmp(const void *a, const void *b, usize n)
{
    const u8 *x = (const u8 *)a;
    const u8 *y = (const u8 *)b;
    for(usize i = 0; i < n; i++)
    {
        if(x[i] != y[i])
            return (int)x[i] - (int)y[i];
    }
    return 0;
}

#endif /* INIT_HOSTED */

usize StrLen(const char *s)
{
    usize n = 0;
    while(s[n] != '\0')
        n++;
    return n;
}

i32 StrCmp(const char *a, const char *b)
{
    while(*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return (i32)(u8)*a - (i32)(u8)*b;
}

i32 StrNCmp(const char *a, const char *b, usize n)
{
    for(usize i = 0; i < n; i++)
    {
        if(a[i] != b[i] || a[i] == '\0')
            return (i32)(u8)a[i] - (i32)(u8)b[i];
    }
    return 0;
}

bool StrEq(const char *a, const char *b)
{
    return StrCmp(a, b) == 0;
}

bool StrStartsWith(const char *s, const char *prefix)
{
    return StrNCmp(s, prefix, StrLen(prefix)) == 0;
}

bool StrEndsWith(const char *s, const char *suffix)
{
    usize sl = StrLen(s);
    usize xl = StrLen(suffix);
    if(xl > sl)
        return false;
    return StrCmp(s + (sl - xl), suffix) == 0;
}

const char *StrChr(const char *s, char c)
{
    for(; *s != '\0'; s++)
    {
        if(*s == c)
            return s;
    }
    return c == '\0' ? s : NULL;
}

const char *StrRChr(const char *s, char c)
{
    const char *hit = NULL;
    for(; *s != '\0'; s++)
    {
        if(*s == c)
            hit = s;
    }
    return hit;
}

usize StrCopy(char *dst, usize cap, const char *src)
{
    if(cap == 0)
        return 0;
    usize i = 0;
    while(i + 1 < cap && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

usize StrCopyN(char *dst, usize cap, const char *src, usize n)
{
    if(cap == 0)
        return 0;
    usize i = 0;
    while(i + 1 < cap && i < n && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

usize StrCat(char *dst, usize cap, const char *src)
{
    usize len = StrLen(dst);
    if(len + 1 >= cap)
        return len;
    return len + StrCopy(dst + len, cap - len, src);
}

bool StrCopyOk(char *dst, usize cap, const char *src)
{
    usize len = StrLen(src);
    StrCopy(dst, cap, src);
    return len < cap;
}

bool StrCopyNOk(char *dst, usize cap, const char *src, usize n)
{
    usize len = 0;
    while(len < n && src[len] != '\0')
        len++;
    StrCopyN(dst, cap, src, n);
    return len < cap;
}

bool StrCatOk(char *dst, usize cap, const char *src)
{
    usize len = StrLen(dst);
    if(len >= cap)
        return false;
    return StrCopyOk(dst + len, cap - len, src);
}

usize PathJoin(char *dst, usize cap, const char *dir, const char *leaf)
{
    usize n = StrCopy(dst, cap, dir);
    if(n > 0 && dst[n - 1] != '/')
        n = StrCat(dst, cap, "/");
    return StrCat(dst, cap, leaf);
}

bool PathJoinOk(char *dst, usize cap, const char *dir, const char *leaf)
{
    if(!StrCopyOk(dst, cap, dir))
        return false;

    usize len = StrLen(dst);
    if(len > 0 && dst[len - 1] != '/' && !StrCatOk(dst, cap, "/"))
        return false;
    return StrCatOk(dst, cap, leaf);
}

bool ParseU64(const char *s, u64 *out, const char **end)
{
    if(s[0] < '0' || s[0] > '9')
        return false;
    u64 v = 0;
    usize i = 0;
    for(; s[i] >= '0' && s[i] <= '9'; i++)
    {
        u64 d = (u64)(s[i] - '0');
        if(v > (0xffffffffffffffffull - d) / 10ull)
            return false;
        v = v * 10ull + d;
    }
    *out = v;
    if(end != NULL)
        *end = s + i;
    return true;
}

bool ParseDuration(const char *s, u64 *outNs)
{
    u64 v = 0;
    const char *end = NULL;
    if(!ParseU64(s, &v, &end))
        return false;

    u64 mult;
    if(StrEq(end, "") || StrEq(end, "s"))
        mult = NS_PER_SEC;
    else if(StrEq(end, "ms"))
        mult = NS_PER_MS;
    else if(StrEq(end, "m"))
        mult = 60ull * NS_PER_SEC;
    else if(StrEq(end, "h"))
        mult = 3600ull * NS_PER_SEC;
    else if(StrEq(end, "d"))
        mult = 86400ull * NS_PER_SEC;
    else
        return false;

    if(v != 0 && v > 0xffffffffffffffffull / mult)
        return false;
    u64 ns = v * mult;
    if(ns > (u64)CAL_MAX_PERIOD_DAYS * 86400ull * NS_PER_SEC)
        return false;
    *outNs = ns;
    return true;
}

#define SECS_PER_DAY 86400u

static bool IsDig(char c)
{
    return c >= '0' && c <= '9';
}

/* Characters, not pointers. An array of string pointers needs a relocation per
 * element, which a read-only segment cannot carry on every target. */
static const char CAL_DAY_NAME[7][4] =
{
    "sun", "mon", "tue", "wed", "thu", "fri", "sat"
};

bool ParseCalendar(const char *s, CalSpec *out)
{
    usize len = StrLen(s);
    if(len < 7)
        return false;

    /* the trailing "-HH-MM" is fixed width, so whatever precedes it is the
       recurrence and can be told apart by shape alone */
    const char *t = s + len - 6;
    if(t[0] != '-' || t[3] != '-')
        return false;
    if(!IsDig(t[1]) || !IsDig(t[2]) || !IsDig(t[4]) || !IsDig(t[5]))
        return false;

    u32 hh = (u32)(t[1] - '0') * 10u + (u32)(t[2] - '0');
    u32 mm = (u32)(t[4] - '0') * 10u + (u32)(t[5] - '0');
    if(hh > 23 || mm > 59)
        return false;

    usize wlen = len - 6;

    if(s[wlen - 1] == 'd' && wlen >= 2 && IsDig(s[0]))
    {
        u32 n = 0;
        for(usize k = 0; k < wlen - 1; k++)
        {
            if(!IsDig(s[k]))
                return false;
            n = n * 10u + (u32)(s[k] - '0');
            if(n > CAL_MAX_PERIOD_DAYS)
                return false;
        }
        if(n == 0)
            return false;
        out->kind = CAL_EVERY_NDAY;
        out->param = n;
    }
    else if(wlen == 3)
    {
        usize day = 0;
        while(day < 7 && StrNCmp(s, CAL_DAY_NAME[day], 3) != 0)
            day++;
        if(day == 7)
            return false;
        out->kind = CAL_WEEKDAY;
        out->param = (u32)day;
    }
    else
    {
        return false;
    }

    out->daySec = hh * 3600u + mm * 60u;
    return true;
}

/* A west-of-UTC offset against a clock still at the epoch would go negative,
   which is every board with no RTC until its first sync. */
static u64 LocalSecs(u64 unixSec, i32 tzOffsetSec)
{
    i64 local = (i64)unixSec + tzOffsetSec;
    return local < 0 ? 0ull : (u64)local;
}

u64 LocalDayNum(u64 unixSec, i32 tzOffsetSec)
{
    return LocalSecs(unixSec, tzOffsetSec) / SECS_PER_DAY;
}

u32 LocalDaySec(u64 unixSec, i32 tzOffsetSec)
{
    return (u32)(LocalSecs(unixSec, tzOffsetSec) % SECS_PER_DAY);
}

/* 1970-01-01 was a Thursday, which is 4 when Sunday is 0 */
u32 LocalWeekday(u64 dayNum)
{
    return (u32)((dayNum + 4ull) % 7ull);
}

static bool CalMatchesDay(const CalSpec *c, u64 dayNum)
{
    if(c->kind == CAL_WEEKDAY)
        return LocalWeekday(dayNum) == c->param;

    /* ParseCalendar rejects a zero period, so this only guards a future caller.
       Worth the branch: dividing by zero here is SIGFPE in PID 1, which the
       kernel reports as a panic rather than a crash. */
    if(c->param == 0)
        return false;

    return (dayNum % c->param) == 0;
}

u64 IntervalAdvance(u64 prevNs, u64 intervalNs, u64 nowNs)
{
    if(intervalNs == 0)
        return nowNs;

    u64 next = prevNs + intervalNs;
    if(next <= nowNs)
    {
        /* more than one interval behind, so skip the slots that went by and
           land on the first one still ahead, keeping the original phase */
        u64 behind = nowNs - next;
        next += (behind / intervalNs + 1ull) * intervalNs;
    }
    return next;
}

u64 SecsUntilCalendar(const CalSpec *c, u64 unixSec, i32 tzOffsetSec)
{
    u64 dayNum = LocalDayNum(unixSec, tzOffsetSec);
    u64 daySec = LocalDaySec(unixSec, tzOffsetSec);

    /* a period of N repeats within N days and a weekday within 7, so the search
       terminates without knowing anything about months */
    u64 span = c->kind == CAL_WEEKDAY ? 7ull : c->param;

    for(u64 off = 0; off <= span; off++)
    {
        if(!CalMatchesDay(c, dayNum + off))
            continue;

        i64 delta = (i64)(off * SECS_PER_DAY) + (i64)c->daySec - (i64)daySec;
        if(delta > 0)
            return (u64)delta;
    }

    return span * SECS_PER_DAY;
}

bool ParseIPv4(const char *s, u32 *out)
{
    u32 addr = 0;
    for(usize part = 0; part < 4; part++)
    {
        u64 v = 0;
        const char *end = NULL;
        if(!ParseU64(s, &v, &end) || v > 255)
            return false;
        addr = (addr << 8) | (u32)v;

        if(part == 3)
        {
            if(*end != '\0')
                return false;
        }
        else
        {
            if(*end != '.')
                return false;
            s = end + 1;
        }
    }
    *out = addr;
    return true;
}

usize FmtU64(char *dst, usize cap, u64 v)
{
    char tmp[24];
    usize n = 0;
    do
    {
        tmp[n++] = (char)('0' + (v % 10ull));
        v /= 10ull;
    } while(v != 0);

    if(cap == 0)
        return 0;
    usize w = 0;
    while(n > 0 && w + 1 < cap)
        dst[w++] = tmp[--n];
    dst[w] = '\0';
    return w;
}

usize FmtI64(char *dst, usize cap, i64 v)
{
    if(v >= 0)
        return FmtU64(dst, cap, (u64)v);
    if(cap < 2)
    {
        if(cap == 1)
            dst[0] = '\0';
        return 0;
    }
    dst[0] = '-';
    /* negating INT64_MIN in unsigned space avoids the UB */
    u64 mag = (u64)0 - (u64)v;
    return 1 + FmtU64(dst + 1, cap - 1, mag);
}

usize FmtHex(char *dst, usize cap, u64 v, usize minDigits)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[16];
    usize n = 0;
    do
    {
        tmp[n++] = digits[v & 0xfull];
        v >>= 4;
    } while(v != 0);
    while(n < minDigits && n < sizeof(tmp))
        tmp[n++] = '0';

    if(cap == 0)
        return 0;
    usize w = 0;
    while(n > 0 && w + 1 < cap)
        dst[w++] = tmp[--n];
    dst[w] = '\0';
    return w;
}

usize FmtV(char *dst, usize cap, const char *fmt, va_list ap)
{
    if(cap == 0)
        return 0;

    usize w = 0;
    for(usize i = 0; fmt[i] != '\0' && w + 1 < cap; i++)
    {
        if(fmt[i] != '%')
        {
            dst[w++] = fmt[i];
            continue;
        }

        i++;
        /* 0 = int, 1 = long/size_t (register width), 2 = long long */
        i32 mod = 0;
        while(fmt[i] == 'l' || fmt[i] == 'z')
        {
            mod = (fmt[i] == 'z') ? 1 : (mod >= 1 ? 2 : 1);
            i++;
        }
        if(fmt[i] == '\0')
            break;

        switch(fmt[i])
        {
        case 's':
        {
            const char *s = va_arg(ap, const char *);
            if(s == NULL)
                s = "(null)";
            w += StrCopy(dst + w, cap - w, s);
            break;
        }
        case 'c':
            dst[w++] = (char)va_arg(ap, int);
            dst[w] = '\0';
            break;
        case 'd':
        case 'i':
        {
            i64 v = (mod == 2) ? va_arg(ap, i64)
                  : (mod == 1) ? (i64)va_arg(ap, isize)
                               : (i64)va_arg(ap, i32);
            w += FmtI64(dst + w, cap - w, v);
            break;
        }
        case 'u':
        case 'x':
        {
            u64 v = (mod == 2) ? va_arg(ap, u64)
                  : (mod == 1) ? (u64)va_arg(ap, usize)
                               : (u64)va_arg(ap, u32);
            if(fmt[i] == 'u')
                w += FmtU64(dst + w, cap - w, v);
            else
                w += FmtHex(dst + w, cap - w, v, 1);
            break;
        }
        case '%':
            dst[w++] = '%';
            break;
        default:
            dst[w++] = '?';
            break;
        }
        dst[w] = '\0';
    }

    dst[w] = '\0';
    return w;
}

usize Fmt(char *dst, usize cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    usize n = FmtV(dst, cap, fmt, ap);
    va_end(ap);
    return n;
}

u64 Hash64(const void *data, usize n)
{
    const u8 *p = (const u8 *)data;
    u64 h = 1469598103934665603ull;
    for(usize i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

bool NameSetInsert(char **names, char *storage, usize *count, usize maxEntries,
                   usize nameCap, const char *name)
{
    if(maxEntries == 0 || nameCap == 0 || StrLen(name) >= nameCap)
        return false;

    usize lo = 0;
    usize hi = *count;
    while(lo < hi)
    {
        usize mid = lo + (hi - lo) / 2;
        if(StrCmp(names[mid], name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if(*count == maxEntries && lo == maxEntries)
        return false;

    char *slot;
    if(*count < maxEntries)
    {
        slot = storage + *count * nameCap;
        (*count)++;
    }
    else
    {
        /* recycle the evicted slot so directory size cannot grow arena use */
        slot = names[maxEntries - 1];
    }

    StrCopy(slot, nameCap, name);
    for(usize i = *count - 1; i > lo; i--)
        names[i] = names[i - 1];
    names[lo] = slot;
    return true;
}

/* ======================================================================
 * boot arena
 * ====================================================================== */

/* One contiguous region fixed at compile time. Bump allocation only, there is
 * no free.
 * The scan region goes back with ArenaReset each event loop iteration.
 * Exhaustion returns NULL and is a logged, non-fatal condition. */
typedef struct
{
    u8    *base;
    usize  cap;
    usize  used;
    usize  peak;
    u32    exhaustions;
} Arena;

void   ArenaInit(Arena *a, void *mem, usize cap);
void  *ArenaAlloc(Arena *a, usize n, usize align);
char  *ArenaStrDup(Arena *a, const char *s);
usize  ArenaMark(const Arena *a);
void   ArenaReset(Arena *a, usize mark);

void ArenaInit(Arena *a, void *mem, usize cap)
{
    a->base = (u8 *)mem;
    a->cap = cap;
    a->used = 0;
    a->peak = 0;
    a->exhaustions = 0;
}

void *ArenaAlloc(Arena *a, usize n, usize align)
{
    if(align == 0)
        align = 1;
    usize off = (a->used + (align - 1)) & ~(align - 1);
    if(n > a->cap || off > a->cap - n)
    {
        a->exhaustions++;
        return NULL;
    }
    a->used = off + n;
    if(a->used > a->peak)
        a->peak = a->used;
    return a->base + off;
}

char *ArenaStrDup(Arena *a, const char *s)
{
    usize n = StrLen(s) + 1;
    char *p = (char *)ArenaAlloc(a, n, 1);
    if(p != NULL)
        memcpy(p, s, n);
    return p;
}

usize ArenaMark(const Arena *a)
{
    return a->used;
}

void ArenaReset(Arena *a, usize mark)
{
    if(mark <= a->used)
        a->used = mark;
}

/* ======================================================================
 * shared log ring
 * ====================================================================== */

/* Mapped MAP_SHARED|MAP_ANONYMOUS at boot so the forked writer reads the same
 * pages init writes, with no pipe and no copy.
 *
 * Fixed-size slots rather than a byte ring, so a lapped reader can resynchronise
 * by jumping its cursor forward. A variable-length ring would need a framing
 * scan to do that. Overflow drops the oldest slots, which is intended: the
 * alternative is stalling the producing task. */

#define LOG_SLOT_BYTES 128
#define LOG_TEXT_MAX   (LOG_SLOT_BYTES - 16)

#define LOG_RING_MAGIC 0x474f4c31u   /* "GOL1" */

/* stream ids */
#define LOG_SRC_INIT 0
#define LOG_SRC_OUT  1
#define LOG_SRC_ERR  2

/* slot flags */
#define LOG_F_DISK 0x1u   /* the writer should persist this record */
#define LOG_F_CONT 0x2u   /* continuation of an oversized line */

/* Shutdown reaches the writer through shared memory rather than a signal, so it
 * flushes what it has instead of dying on the default disposition mid-buffer */
#define LOG_CTL_SHUTDOWN 0x1u

/* Every counter the two processes share is register-width. A u64 on a 32-bit
 * target lowers to __atomic_load_8, which libatomic implements with a lock table
 * private to one process, worthless across the MAP_SHARED boundary these
 * counters exist to cross. At 32 bits they wrap, which the modular head - tail
 * arithmetic already handles, the gap never exceeding the slot count. */
typedef usize RingSeq;

typedef struct
{
    u16  len;
    u8   stream;
    u8   task;
    u32  flags;
    char text[LOG_TEXT_MAX];
} LogSlot;

typedef struct
{
    bool bValid;
    u8   task;
    u8   stream;
    u16  len;
    u32  flags;
    u64  hash;
} LogIdentity;

bool LogIdentityMatch(const LogIdentity *identity, const LogSlot *slot,
                      u32 flags, u64 hash)
{
    return identity->bValid && identity->task == slot->task &&
           identity->stream == slot->stream && identity->len == slot->len &&
           identity->flags == flags && identity->hash == hash;
}

void LogIdentitySet(LogIdentity *identity, const LogSlot *slot,
                    u32 flags, u64 hash)
{
    identity->bValid = true;
    identity->task = slot->task;
    identity->stream = slot->stream;
    identity->len = slot->len;
    identity->flags = flags;
    identity->hash = hash;
}

#define LOG_SLOT_WORDS   (LOG_SLOT_BYTES / sizeof(RingSeq))
#define LOG_PAYLOAD_WORDS (LOG_SLOT_WORDS - 1)

typedef struct
{
    RingSeq seq;
    RingSeq payload[LOG_PAYLOAD_WORDS];
} RingSlot;

_Static_assert(sizeof(LogSlot) <= LOG_PAYLOAD_WORDS * sizeof(RingSeq),
               "log payload size");
_Static_assert(sizeof(RingSlot) == LOG_SLOT_BYTES, "log slot size");

typedef struct
{
    u32 magic;
    u32 slots;          /* power of two */
    RingSeq head;       /* monotonic, owned by init */
    RingSeq tail;       /* monotonic, owned by the writer */
    RingSeq dropped;
    RingSeq control;
    RingSeq writerProgress;
    RingSlot slot[];
} LogRing;

/* bytes must be >= 2 * LOG_SLOT_BYTES. Slot count rounds down to a power of
 * two. False if the region is too small. */
bool RingInit(LogRing *r, usize bytes);

void RingWrite(LogRing *r, u8 stream, u8 task, u32 flags, const char *text, usize len);

/* False when no valid record was read. *lost counts lapped or malformed slots */
bool RingRead(LogRing *r, LogSlot *out, u64 *lost);

usize RingPending(const LogRing *r);

bool RingInit(LogRing *r, usize bytes)
{
    if(bytes <= sizeof(LogRing) + LOG_SLOT_BYTES)
        return false;

    usize avail = (bytes - sizeof(LogRing)) / LOG_SLOT_BYTES;
    usize slots = 1;
    while(slots * 2 <= avail)
        slots *= 2;
    if(slots < 2)
        return false;

    memset(r, 0, sizeof(LogRing));
    r->magic = LOG_RING_MAGIC;
    r->slots = (u32)slots;
    return true;
}

static RingSeq RingReadySeq(RingSeq position)
{
    return position * 2 + 2;
}

static void RingPut(LogRing *r, u8 stream, u8 task, u32 flags, const char *text, usize len)
{
    RingSeq head = __atomic_load_n(&r->head, __ATOMIC_RELAXED);
    RingSlot *s = &r->slot[head & (r->slots - 1)];
    RingSeq ready = RingReadySeq(head);

    LogSlot record;
    memset(&record, 0, sizeof(record));
    record.len = (u16)len;
    record.stream = stream;
    record.task = task;
    record.flags = flags;
    memcpy(record.text, text, len);

    RingSeq payload[LOG_PAYLOAD_WORDS];
    memset(payload, 0, sizeof(payload));
    memcpy(payload, &record, sizeof(record));

    __atomic_store_n(&s->seq, ready - 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    for(usize i = 0; i < LOG_PAYLOAD_WORDS; i++)
        __atomic_store_n(&s->payload[i], payload[i], __ATOMIC_RELAXED);

    __atomic_store_n(&s->seq, ready, __ATOMIC_RELEASE);
    __atomic_store_n(&r->head, head + 1, __ATOMIC_RELEASE);
}

void RingWrite(LogRing *r, u8 stream, u8 task, u32 flags, const char *text, usize len)
{
    if(r == NULL || r->magic != LOG_RING_MAGIC || len == 0)
        return;

    usize off = 0;
    while(off < len)
    {
        usize chunk = len - off;
        u32 f = flags;
        if(chunk > LOG_TEXT_MAX)
        {
            chunk = LOG_TEXT_MAX;
            f |= LOG_F_CONT;
        }
        RingPut(r, stream, task, f, text + off, chunk);
        off += chunk;
    }
}

bool RingRead(LogRing *r, LogSlot *out, u64 *lost)
{
    *lost = 0;
    if(r == NULL || r->magic != LOG_RING_MAGIC)
        return false;

    RingSeq head = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);
    RingSeq tail = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);
    if(tail == head)
        return false;

    /* lapped, the oldest slots are already overwritten */
    if(head - tail > r->slots)
    {
        RingSeq skip = (head - tail) - r->slots;
        tail += skip;
        *lost = skip;
        __atomic_store_n(&r->tail, tail, __ATOMIC_RELEASE);
        RingSeq dropped = __atomic_load_n(&r->dropped, __ATOMIC_RELAXED);
        __atomic_store_n(&r->dropped, dropped + skip, __ATOMIC_RELAXED);
    }

    RingSlot *s = &r->slot[tail & (r->slots - 1)];
    RingSeq expected = RingReadySeq(tail);
    RingSeq before = __atomic_load_n(&s->seq, __ATOMIC_ACQUIRE);
    if(before != expected)
        return false;

    RingSeq payload[LOG_PAYLOAD_WORDS];
    for(usize i = 0; i < LOG_PAYLOAD_WORDS; i++)
        payload[i] = __atomic_load_n(&s->payload[i], __ATOMIC_RELAXED);

    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    RingSeq after = __atomic_load_n(&s->seq, __ATOMIC_RELAXED);
    if(after != before)
        return false;

    memcpy(out, payload, sizeof(*out));
    if(out->len > LOG_TEXT_MAX)
    {
        *lost += 1;
        RingSeq dropped = __atomic_load_n(&r->dropped, __ATOMIC_RELAXED);
        __atomic_store_n(&r->dropped, dropped + 1, __ATOMIC_RELAXED);
        __atomic_store_n(&r->tail, tail + 1, __ATOMIC_RELEASE);
        return false;
    }
    __atomic_store_n(&r->tail, tail + 1, __ATOMIC_RELEASE);
    return true;
}

usize RingPending(const LogRing *r)
{
    if(r == NULL || r->magic != LOG_RING_MAGIC)
        return 0;
    RingSeq head = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);
    RingSeq tail = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);
    return (usize)(head - tail);
}

/* ======================================================================
 * task rules
 * ====================================================================== */

const TaskRule *TaskRuleFind(const char *name);

u8 LogPolicyResolve(u8 policy, u8 defaultPolicy)
{
    return policy == LOGP_INHERIT ? defaultPolicy : policy;
}

/* First match wins. Most tasks have no entry and run on the defaults. */
const TaskRule *TaskRuleFind(const char *name)
{
    for(const TaskRule *r = TASK_RULES; r->name != NULL; r++)
    {
        if(StrEq(r->name, name))
            return r;
    }
    return NULL;
}

/* ======================================================================
 * respawn backoff
 * ====================================================================== */

/* Instant respawn turns a segfault on a retransmitted packet into a
 * fork bomb. */

u64  BackoffNext(u64 currentNs);
bool BackoffStable(u64 startedNs, u64 exitedNs, u64 stableNs);
u32  RestartFailuresNext(u32 currentFails, u64 startedNs, u64 exitedNs, u64 stableNs);

u64 BackoffNext(u64 currentNs)
{
    if(currentNs == 0)
        return CFG_BACKOFF_MIN_NS;
    if(currentNs >= CFG_BACKOFF_MAX_NS)
        return CFG_BACKOFF_MAX_NS;

    u64 next = currentNs * 2ull;
    return next > CFG_BACKOFF_MAX_NS ? CFG_BACKOFF_MAX_NS : next;
}

bool BackoffStable(u64 startedNs, u64 exitedNs, u64 stableNs)
{
    if(exitedNs <= startedNs)
        return false;
    return (exitedNs - startedNs) >= stableNs;
}

u32 RestartFailuresNext(u32 currentFails, u64 startedNs, u64 exitedNs, u64 stableNs)
{
    return BackoffStable(startedNs, exitedNs, stableNs) ? 0 : currentFails + 1;
}

/* ======================================================================
 * /proc/PID/stat
 * ====================================================================== */

/* Tier 1 liveness: /proc/PID/stat inspection, always on, needs no cooperation
 * from the task. Heuristic only. An idle daemon and a deadlocked one both
 * show zero CPU, so this never triggers a restart on its own. */

typedef struct
{
    char state;    /* R S D Z T ... */
    u64  utime;    /* clock ticks */
    u64  stime;    /* clock ticks */
    u64  threads;
} ProcStat;

/* comm is parenthesised and can itself contain spaces and parens, so field
 * splitting starts after the last ')'. buf must be NUL-terminated. */
bool ProcStatParse(const char *buf, usize len, ProcStat *out);

bool ProcStatParse(const char *buf, usize len, ProcStat *out)
{
    usize close = 0;
    bool found = false;
    for(usize i = 0; i < len; i++)
    {
        if(buf[i] == ')')
        {
            close = i;
            found = true;
        }
    }
    if(!found || close + 2 >= len)
        return false;

    usize i = close + 1;
    while(i < len && buf[i] == ' ')
        i++;
    if(i >= len)
        return false;

    out->state = buf[i];
    out->utime = 0;
    out->stime = 0;
    out->threads = 0;

    /* field 3 is state, utime is 14, stime 15, num_threads 20 */
    u32 field = 3;
    while(i < len && field < 20)
    {
        while(i < len && buf[i] != ' ')
            i++;
        while(i < len && buf[i] == ' ')
            i++;
        if(i >= len)
            break;
        field++;

        u64 v = 0;
        if(!ParseU64(&buf[i], &v, NULL))
            continue;
        if(field == 14)
            out->utime = v;
        else if(field == 15)
            out->stime = v;
        else if(field == 20)
            out->threads = v;
    }

    return field >= 15;
}

/* ======================================================================
 * SNTP packets
 * ====================================================================== */

/* Client only: no listening socket, no server mode, no broadcast. Packet
 * handling is split from the socket so the parser, which eats untrusted network
 * input, is unit-testable on the host. */

#define SNTP_PKT_BYTES 48
#define NTP_UNIX_DELTA 2208988800ull   /* seconds between 1900 and 1970 */
#define NTP_ERA_SECONDS (1ull << 32)

/* transmitNtp comes back in the server's originate field, and is what
 * SntpParseReply matches against. */
void SntpBuildRequest(u8 pkt[SNTP_PKT_BYTES], u64 transmitNtp);

/* Returns false for anything malformed: wrong mode, stratum 0 (kiss-of-death)
 * or above 15, or an originate field that does not match the request. Every
 * field is read byte-wise, unaligned loads being unsafe on ARMv6. */
bool SntpParseReply(const u8 *pkt, usize len, u64 expectTransmitNtp, u64 *outUnixNs);

/* 64-bit NTP timestamp: 32.32 fixed point seconds since 1900 */
u64 SntpYearFloorSec(u64 year);
u64 SntpBuildFloorSec(void);
u64 SntpNtpFromUnixNs(u64 unixNs);
u64 SntpUnixNsFromNtp(u64 ntp, u64 floorUnixSec);
u64 SntpCorrectForRtt(u64 unixNs, u64 sentNs, u64 receivedNs);

/* The sockaddr fields are the only multi-byte integers init hands to the kernel
 * in network order. The packet itself goes through Load64BE/Store64BE. */
u16  Hton16(u16 v);
u32  Hton32(u32 v);

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ||
               __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__,
               "unsupported byte order");

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

u16 Hton16(u16 v)
{
    return v;
}

u32 Hton32(u32 v)
{
    return v;
}

#else

u16 Hton16(u16 v)
{
    return (u16)((v << 8) | (v >> 8));
}

u32 Hton32(u32 v)
{
    return ((v & 0xffu) << 24) | ((v & 0xff00u) << 8) |
           ((v >> 8) & 0xff00u) | ((v >> 24) & 0xffu);
}

#endif

static u64 Load64BE(const u8 *p)
{
    u64 v = 0;
    for(usize i = 0; i < 8; i++)
        v = (v << 8) | (u64)p[i];
    return v;
}

static void Store64BE(u8 *p, u64 v)
{
    for(usize i = 0; i < 8; i++)
        p[i] = (u8)((v >> (56 - 8 * i)) & 0xffull);
}

u64 SntpNtpFromUnixNs(u64 unixNs)
{
    u64 sec = unixNs / NS_PER_SEC + NTP_UNIX_DELTA;
    u64 frac = ((unixNs % NS_PER_SEC) << 32) / NS_PER_SEC;
    return ((sec & 0xffffffffull) << 32) | frac;
}

u64 SntpYearFloorSec(u64 year)
{
    u64 previousYear = year - 1;
    u64 leapDaysBefore1970 = 1969 / 4 - 1969 / 100 + 1969 / 400;
    u64 leapDays = previousYear / 4 - previousYear / 100 +
                   previousYear / 400 - leapDaysBefore1970;
    return ((year - 1970) * 365 + leapDays) * SECS_PER_DAY;
}

u64 SntpBuildFloorSec(void)
{
    /* __DATE__ is "Mmm dd yyyy", January 1 leaves room for clock skew */
    const char *date = __DATE__;
    u64 year = (u64)(date[7] - '0') * 1000 +
               (u64)(date[8] - '0') * 100 +
               (u64)(date[9] - '0') * 10 +
               (u64)(date[10] - '0');
    return SntpYearFloorSec(year);
}

u64 SntpUnixNsFromNtp(u64 ntp, u64 floorUnixSec)
{
    u64 sec = ntp >> 32;
    u64 floorNtpSec = floorUnixSec + NTP_UNIX_DELTA;
    u64 era = floorNtpSec / NTP_ERA_SECONDS;
    if(sec < floorNtpSec % NTP_ERA_SECONDS)
        era++;

    u64 absoluteSec = era * NTP_ERA_SECONDS + sec;
    u64 frac = ntp & 0xffffffffull;
    return (absoluteSec - NTP_UNIX_DELTA) * NS_PER_SEC +
           ((frac * NS_PER_SEC) >> 32);
}

u64 SntpCorrectForRtt(u64 unixNs, u64 sentNs, u64 receivedNs)
{
    if(receivedNs <= sentNs)
        return unixNs;
    return unixNs + (receivedNs - sentNs) / 2;
}

void SntpBuildRequest(u8 pkt[SNTP_PKT_BYTES], u64 transmitNtp)
{
    memset(pkt, 0, SNTP_PKT_BYTES);
    pkt[0] = 0x23;   /* LI 0, VN 4, Mode 3 (client) */
    pkt[1] = 0;
    pkt[2] = 4;      /* poll interval, 16s */
    pkt[3] = (u8)0xec;   /* precision, -20 */
    Store64BE(&pkt[40], transmitNtp);
}

bool SntpParseReply(const u8 *pkt, usize len, u64 expectTransmitNtp, u64 *outUnixNs)
{
    if(len < SNTP_PKT_BYTES)
        return false;

    u8 li = (u8)((pkt[0] >> 6) & 0x3u);
    u8 vn = (u8)((pkt[0] >> 3) & 0x7u);
    u8 mode = (u8)(pkt[0] & 0x7u);
    u8 stratum = pkt[1];

    if(li == 3)          /* clock not synchronised */
        return false;
    if(vn < 3 || vn > 4)
        return false;
    if(mode != 4)        /* server */
        return false;
    if(stratum == 0 || stratum > 15)
        return false;

    if(Load64BE(&pkt[24]) != expectTransmitNtp)
        return false;

    u64 xmit = Load64BE(&pkt[40]);
    *outUnixNs = SntpUnixNsFromNtp(xmit, SntpBuildFloorSec());
    return true;
}

/* ======================================================================
 * status interface
 * ====================================================================== */

#define STATUS_PATH    "/run/init.status"
#define STATUS_MAGIC   0x53544131u   /* "STA1" */
#define STATUS_VERSION 2u

#define TS_PENDING 0u   /* waiting for its first run */
#define TS_RUNNING 1u
#define TS_BACKOFF 2u   /* died, waiting out the respawn delay */
#define TS_IDLE    3u   /* periodic, waiting for the next interval */
#define TS_DONE    4u   /* boot task completed cleanly */
#define TS_FAILED  5u   /* gave up respawning */

#define STF_CRITICAL 0x1u
#define STF_PROBED   0x2u

typedef usize StatusSeq;

typedef struct
{
    char name[CFG_NAME_MAX];
    i32  pid;
    u32  state;
    u32  schedule;
    i32  lastExit;
    i32  lastSignal;
    u64  lastExitNs;
    u64  startedNs;
    u32  runs;
    u32  consecFails;
    i32  lastProbeRc;
    u32  probeFails;
    u64  lastProbeNs;
    u32  flags;
    u32  pad;
} StatusEntry;

typedef struct
{
    u32 magic;
    u32 version;
    u32 count;
    u32 entrySize;
    u64 bootNs;
    u64 arenaPeak;
    u64 logDropped;
    u64 shutdownNs;
    StatusEntry task[CFG_MAX_TASKS];
} StatusSnapshot;

#define STATUS_PAYLOAD_WORDS (sizeof(StatusSnapshot) / sizeof(StatusSeq))

_Static_assert(sizeof(StatusSnapshot) % sizeof(StatusSeq) == 0,
               "status snapshot must fill native words");

typedef struct
{
    StatusSeq sequence;
    StatusSeq payload[STATUS_PAYLOAD_WORDS];
} StatusBlock;

#if defined(INIT_HOSTED) || !defined(INIT_FIXTURE)
static void StatusPublish(StatusBlock *status, const StatusSnapshot *snapshot)
{
    StatusSeq sequence = __atomic_load_n(&status->sequence, __ATOMIC_RELAXED);
    if((sequence & 1u) != 0)
        sequence++;

    __atomic_store_n(&status->sequence, sequence + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    for(usize i = 0; i < STATUS_PAYLOAD_WORDS; i++)
    {
        StatusSeq word;
        memcpy(&word, (const u8 *)snapshot + i * sizeof(word), sizeof(word));
        __atomic_store_n(&status->payload[i], word, __ATOMIC_RELAXED);
    }
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&status->sequence, sequence + 2, __ATOMIC_RELAXED);
}
#endif

#if defined(INIT_HOSTED) || defined(INIT_STATUS_READER)
static bool StatusRead(const StatusBlock *status, StatusSnapshot *snapshot,
                       StatusSeq *sequenceOut)
{
    StatusSeq before = __atomic_load_n(&status->sequence, __ATOMIC_ACQUIRE);
    if((before & 1u) != 0)
        return false;

    for(usize i = 0; i < STATUS_PAYLOAD_WORDS; i++)
    {
        StatusSeq word = __atomic_load_n(&status->payload[i], __ATOMIC_RELAXED);
        memcpy((u8 *)snapshot + i * sizeof(word), &word, sizeof(word));
    }
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    StatusSeq after = __atomic_load_n(&status->sequence, __ATOMIC_RELAXED);
    if(before != after || (after & 1u) != 0)
        return false;

    *sequenceOut = after;
    return true;
}
#endif

#if !defined(INIT_HOSTED)

/* ======================================================================
 * syscalls
 * ====================================================================== */

/* One variant per architecture. Everything below returns the kernel's raw
 * result: negative errno on failure, and there is no errno global. */

#if defined(__x86_64__)

static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    register isize r10 __asm__("r10") = d;
    register isize r8  __asm__("r8")  = e;
    register isize r9  __asm__("r9")  = f;
    isize ret;
    __asm__ volatile("syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory");
    return ret;
}

#elif defined(__i386__)

/* ebp carries the 6th argument, so it has to be saved around int $0x80 */
static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    isize ret;
    __asm__ volatile(
        "push %[argf]\n\t"
        "push %%ebp\n\t"
        "mov 4(%%esp), %%ebp\n\t"
        "int $0x80\n\t"
        "pop %%ebp\n\t"
        "add $4, %%esp"
        : "=a"(ret)
        : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e), [argf] "rm"(f)
        : "memory");
    return ret;
}

#elif defined(__aarch64__)

static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    register isize x8 __asm__("x8") = n;
    register isize x0 __asm__("x0") = a;
    register isize x1 __asm__("x1") = b;
    register isize x2 __asm__("x2") = c;
    register isize x3 __asm__("x3") = d;
    register isize x4 __asm__("x4") = e;
    register isize x5 __asm__("x5") = f;
    __asm__ volatile("svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory", "cc");
    return x0;
}

#elif defined(__riscv) && __riscv_xlen == 64

static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    register isize a7 __asm__("a7") = n;
    register isize a0 __asm__("a0") = a;
    register isize a1 __asm__("a1") = b;
    register isize a2 __asm__("a2") = c;
    register isize a3 __asm__("a3") = d;
    register isize a4 __asm__("a4") = e;
    register isize a5 __asm__("a5") = f;
    __asm__ volatile("ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
        : "memory");
    return a0;
}

#elif defined(__loongarch__) && __loongarch_grlen == 64

static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    register isize a7 __asm__("$a7") = n;
    register isize a0 __asm__("$a0") = a;
    register isize a1 __asm__("$a1") = b;
    register isize a2 __asm__("$a2") = c;
    register isize a3 __asm__("$a3") = d;
    register isize a4 __asm__("$a4") = e;
    register isize a5 __asm__("$a5") = f;
    __asm__ volatile("syscall 0"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
        : "memory");
    return a0;
}

#elif defined(__arm__)

static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    register isize r7 __asm__("r7") = n;
    register isize r0 __asm__("r0") = a;
    register isize r1 __asm__("r1") = b;
    register isize r2 __asm__("r2") = c;
    register isize r3 __asm__("r3") = d;
    register isize r4 __asm__("r4") = e;
    register isize r5 __asm__("r5") = f;
    __asm__ volatile("svc #0"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7)
        : "memory");
    return r0;
}

#elif defined(__mips__) && _MIPS_SIM == _ABIO32

/* Two o32 departures. Arguments five and six travel in the caller's stack
 * argument slots, which is why $sp moves here and not in the prologue. Failure
 * arrives as a3 set with a positive errno in v0, so this is the one arch where
 * the sign gets applied by hand to keep every caller's `r < 0` test working. */
static inline isize SysCall6(isize n, isize a, isize b, isize c, isize d, isize e, isize f)
{
    register isize v0 __asm__("$2") = n;
    register isize a0 __asm__("$4") = a;
    register isize a1 __asm__("$5") = b;
    register isize a2 __asm__("$6") = c;
    register isize a3 __asm__("$7") = d;

    __asm__ volatile(
        "addiu $sp, $sp, -32\n\t"
        "sw    %5, 16($sp)\n\t"
        "sw    %6, 20($sp)\n\t"
        "syscall\n\t"
        "addiu $sp, $sp, 32"
        : "+r"(v0), "+r"(a3)
        : "r"(a0), "r"(a1), "r"(a2), "r"(e), "r"(f)
        : "memory", "cc", "at", "v1", "hi", "lo",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9");

    return a3 ? -v0 : v0;
}

#endif

static inline isize SysCall0(isize n)
{ return SysCall6(n, 0, 0, 0, 0, 0, 0); }
static inline isize SysCall1(isize n, isize a)
{ return SysCall6(n, a, 0, 0, 0, 0, 0); }
static inline isize SysCall2(isize n, isize a, isize b)
{ return SysCall6(n, a, b, 0, 0, 0, 0); }
static inline isize SysCall3(isize n, isize a, isize b, isize c)
{ return SysCall6(n, a, b, c, 0, 0, 0); }
static inline isize SysCall4(isize n, isize a, isize b, isize c, isize d)
{ return SysCall6(n, a, b, c, d, 0, 0); }
static inline isize SysCall5(isize n, isize a, isize b, isize c, isize d, isize e)
{ return SysCall6(n, a, b, c, d, e, 0); }

/* relocation types the static-pie self-relocator applies */
#if defined(__x86_64__)
  #define R_RELATIVE   8      /* R_X86_64_RELATIVE */
  #define RELOC_RELA   1
#elif defined(__i386__)
  #define R_RELATIVE   8      /* R_386_RELATIVE */
  #define RELOC_RELA   0
#elif defined(__aarch64__)
  #define R_RELATIVE   1027   /* R_AARCH64_RELATIVE */
  #define RELOC_RELA   1
#elif defined(__riscv) && __riscv_xlen == 64
  #define R_RELATIVE   3      /* R_RISCV_RELATIVE */
  #define RELOC_RELA   1
#elif defined(__loongarch__) && __loongarch_grlen == 64
  #define R_RELATIVE   3      /* R_LARCH_RELATIVE */
  #define RELOC_RELA   1
#elif defined(__arm__)
  #define R_RELATIVE   23     /* R_ARM_RELATIVE */
  #define RELOC_RELA   0
#elif defined(__mips__)
  /* MIPS resolves position independence through the GOT, so there is no table
   * of RELATIVE entries for this relocator to walk. The port builds
   * -static -no-pie, and the Makefile defaults PIE=0 for it. */
  #define RELOC_NONE   1
#endif

/* ---- typed wrappers ---- */

static inline isize SysRead(i32 fd, void *buf, usize n)
{ return SysCall3(SYS_read, fd, (isize)buf, (isize)n); }

static inline isize SysWrite(i32 fd, const void *buf, usize n)
{ return SysCall3(SYS_write, fd, (isize)buf, (isize)n); }

static inline isize SysOpen(const char *path, i32 flags, i32 mode)
{ return SysCall4(SYS_openat, AT_FDCWD, (isize)path, flags, mode); }

static inline isize SysClose(i32 fd)
{ return SysCall1(SYS_close, fd); }

static inline isize SysLseek(i32 fd, i64 off, i32 whence)
{ return SysCall3(SYS_lseek, fd, (isize)off, whence); }

static inline isize SysFtruncate(i32 fd, i64 len)
{ return SysCall2(SYS_ftruncate, fd, (isize)len); }

static inline isize SysFsync(i32 fd)
{ return SysCall1(SYS_fsync, fd); }

static inline isize SysMkdir(const char *path, i32 mode)
{ return SysCall3(SYS_mkdirat, AT_FDCWD, (isize)path, mode); }

static inline isize SysUnlink(const char *path)
{ return SysCall3(SYS_unlinkat, AT_FDCWD, (isize)path, 0); }

static inline isize SysRename(const char *from, const char *to)
{
#ifdef SYS_renameat2
    return SysCall5(SYS_renameat2, AT_FDCWD, (isize)from, AT_FDCWD, (isize)to, 0);
#else
    return SysCall4(SYS_renameat, AT_FDCWD, (isize)from, AT_FDCWD, (isize)to);
#endif
}

static inline isize SysAccess(const char *path, i32 mode)
{ return SysCall3(SYS_faccessat, AT_FDCWD, (isize)path, mode); }

static inline isize SysGetdents64(i32 fd, void *buf, usize n)
{ return SysCall3(SYS_getdents64, fd, (isize)buf, (isize)n); }

static inline isize SysChdir(const char *path)
{ return SysCall1(SYS_chdir, (isize)path); }

static inline isize SysUmask(i32 mask)
{ return SysCall1(SYS_umask, mask); }

static inline isize SysDup3(i32 old, i32 newfd, i32 flags)
{ return SysCall3(SYS_dup3, old, newfd, flags); }

static inline isize SysPipe2(i32 fds[2], i32 flags)
{ return SysCall2(SYS_pipe2, (isize)fds, flags); }

static inline isize SysFcntl(i32 fd, i32 cmd, isize arg)
{ return SysCall3(SYS_fcntl, fd, cmd, arg); }

static inline isize SysIoctl(i32 fd, u32 req, isize arg)
{ return SysCall3(SYS_ioctl, fd, (isize)(usize)req, arg); }

static inline void *SysMmap(void *addr, usize len, i32 prot, i32 flags, i32 fd, i64 off)
{
#ifdef INIT_MMAP2
    isize r = SysCall6(SYS_mmap2, (isize)addr, (isize)len, prot, flags, fd,
                       (isize)(off / 4096));
#else
    isize r = SysCall6(SYS_mmap, (isize)addr, (isize)len, prot, flags, fd, (isize)off);
#endif
    return (void *)r;
}

static inline isize SysMunmap(void *addr, usize len)
{ return SysCall2(SYS_munmap, (isize)addr, (isize)len); }

static inline isize SysMount(const char *src, const char *tgt, const char *fs,
                             u32 flags, const void *data)
{ return SysCall5(SYS_mount, (isize)src, (isize)tgt, (isize)fs, (isize)(usize)flags, (isize)data); }

static inline isize SysUmount2(const char *tgt, i32 flags)
{ return SysCall2(SYS_umount2, (isize)tgt, flags); }

static inline isize SysSync(void)
{ return SysCall0(SYS_sync); }

static inline isize SysReboot(u32 cmd)
{
    return SysCall4(SYS_reboot, (isize)(usize)LINUX_REBOOT_MAGIC1,
                    (isize)(usize)LINUX_REBOOT_MAGIC2, (isize)(usize)cmd, 0);
}

static inline isize SysGetPid(void)
{ return SysCall0(SYS_getpid); }

static inline isize SysSetSid(void)
{ return SysCall0(SYS_setsid); }

static inline isize SysFork(void)
{ return SysCall5(SYS_clone, SIGCHLD, 0, 0, 0, 0); }

static inline isize SysExecve(const char *path, char *const argv[], char *const envp[])
{ return SysCall3(SYS_execve, (isize)path, (isize)argv, (isize)envp); }

static inline NORETURN void SysExit(i32 code)
{
    SysCall1(SYS_exit_group, code);
    for(;;)
        SysCall1(SYS_exit, code);
}

static inline isize SysWait4(i32 pid, i32 *status, i32 options)
{ return SysCall4(SYS_wait4, pid, (isize)status, options, 0); }

static inline isize SysKill(i32 pid, i32 sig)
{ return SysCall2(SYS_kill, pid, sig); }

static inline isize SysPrctl(i32 opt, usize a2, usize a3, usize a4, usize a5)
{ return SysCall5(SYS_prctl, opt, (isize)a2, (isize)a3, (isize)a4, (isize)a5); }

static inline isize SysSetUid(u32 uid)
{ return SysCall1(SYS_setuid, (isize)uid); }

static inline isize SysSetGid(u32 gid)
{ return SysCall1(SYS_setgid, (isize)gid); }

static inline isize SysSetGroups(usize n, const u32 *list)
{ return SysCall2(SYS_setgroups, (isize)n, (isize)list); }

static inline isize SysCapSet(const KCapHeader *h, const KCapData *d)
{ return SysCall2(SYS_capset, (isize)h, (isize)d); }

static inline isize SysSigAction(i32 sig, const KSigAction *act, KSigAction *old)
{ return SysCall4(SYS_rt_sigaction, sig, (isize)act, (isize)old, KSIGSET_BYTES); }

static inline isize SysSigProcMask(i32 how, const KSigSet *set, KSigSet *old)
{ return SysCall4(SYS_rt_sigprocmask, how, (isize)set, (isize)old, KSIGSET_BYTES); }

static inline isize SysClockGetTime(i32 clk, KTimeSpec *ts)
{
#ifdef INIT_TIME32_ABI
    return SysCall2(SYS_clock_gettime64, clk, (isize)ts);
#else
    return SysCall2(SYS_clock_gettime, clk, (isize)ts);
#endif
}

static inline isize SysClockSetTime(i32 clk, const KTimeSpec *ts)
{
#ifdef INIT_TIME32_ABI
    return SysCall2(SYS_clock_settime64, clk, (isize)ts);
#else
    return SysCall2(SYS_clock_settime, clk, (isize)ts);
#endif
}

static inline isize SysPpoll(KPollFd *fds, usize n, const KTimeSpec *timeout,
                             const KSigSet *mask)
{
#ifdef INIT_TIME32_ABI
    return SysCall5(SYS_ppoll_time64, (isize)fds, (isize)n, (isize)timeout,
                    (isize)mask, KSIGSET_BYTES);
#else
    return SysCall5(SYS_ppoll, (isize)fds, (isize)n, (isize)timeout,
                    (isize)mask, KSIGSET_BYTES);
#endif
}

static inline isize SysSocket(i32 domain, i32 type, i32 proto)
{ return SysCall3(SYS_socket, domain, type, proto); }

static inline isize SysConnect(i32 fd, const void *addr, u32 addrlen)
{ return SysCall3(SYS_connect, fd, (isize)addr, (isize)addrlen); }

static inline isize SysBind(i32 fd, const void *addr, u32 addrlen)
{ return SysCall3(SYS_bind, fd, (isize)addr, (isize)addrlen); }

static inline isize SysSendTo(i32 fd, const void *buf, usize n, i32 flags,
                               const void *addr, u32 addrlen)
{ return SysCall6(SYS_sendto, fd, (isize)buf, (isize)n, flags, (isize)addr, (isize)addrlen); }

static inline isize SysRecvFrom(i32 fd, void *buf, usize n, i32 flags,
                                void *addr, u32 *addrlen)
{ return SysCall6(SYS_recvfrom, fd, (isize)buf, (isize)n, flags, (isize)addr, (isize)addrlen); }

static inline isize SysGetRandom(void *buf, usize n, u32 flags)
{ return SysCall3(SYS_getrandom, (isize)buf, (isize)n, (isize)(usize)flags); }

static inline isize SysSchedYield(void)
{ return SysCall0(SYS_sched_yield); }

/* ---- helpers built on the above ---- */

static inline u64 SysNow(i32 clk)
{
    KTimeSpec ts = { 0, 0 };
    if(SysClockGetTime(clk, &ts) < 0)
        return 0;
    return (u64)ts.sec * NS_PER_SEC + (u64)ts.nsec;
}

/* CLOCK_BOOTTIME, never CLOCK_MONOTONIC_RAW: RAW is undisciplined and drifts */
static inline u64 SysBootNs(void)
{ return SysNow(CLOCK_BOOTTIME); }

static inline u64 SysRealNs(void)
{ return SysNow(CLOCK_REALTIME); }

static inline bool SysSetNonBlock(i32 fd)
{
    isize fl = SysFcntl(fd, F_GETFL, 0);
    return fl >= 0 && SysFcntl(fd, F_SETFL, fl | O_NONBLOCK) >= 0;
}

#if !defined(INIT_FIXTURE)

/* ======================================================================
 * logging
 * ====================================================================== */

/* init owns logging policy. Tasks never decide where their output
 * goes. PID 1 drains their pipes non-blocking into the shared ring, and a
 * forked child does the disk writes. */

typedef struct
{
    char  buf[CFG_LINE_MAX];
    usize len;
} LineBuf;

void LogAttach(LogRing *ring, i32 consoleFd);
void LogSetVerbose(bool bOn);

/* init's own diagnostics: console plus ring, always flagged for disk */
void LogF(const char *fmt, ...);
void LogRaw(const char *text, usize len);

/* Oversized lines are emitted as lossless continuation records */
void LogFeed(LineBuf *lb, u8 stream, u8 task, u8 policy, const char *data, usize n);
void LogFlushPartial(LineBuf *lb, u8 stream, u8 task, u8 policy);

static LogRing *G_RING = NULL;
static i32      G_CONSOLE_FD = 2;
static bool     G_VERBOSE = true;

void LogAttach(LogRing *ring, i32 consoleFd)
{
    G_RING = ring;
    G_CONSOLE_FD = consoleFd;
}

void LogSetVerbose(bool bOn)
{
    G_VERBOSE = bOn;
}

/* text carries no trailing newline. The console gets one appended, the ring
 * stores one record per line and needs none. */
void LogRaw(const char *text, usize len)
{
    if(G_VERBOSE && G_CONSOLE_FD >= 0)
    {
        char out[CFG_LINE_MAX + 1];
        usize n = StrCopyN(out, sizeof(out) - 1, text, len);
        out[n++] = '\n';
        SysWrite(G_CONSOLE_FD, out, n);
    }
    RingWrite(G_RING, LOG_SRC_INIT, 0xff, LOG_F_DISK, text, len);
}

void LogF(const char *fmt, ...)
{
    char line[CFG_LINE_MAX];
    usize n = StrCopy(line, sizeof(line), "init: ");

    va_list ap;
    va_start(ap, fmt);
    n += FmtV(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);

    LogRaw(line, n);
}

static void LogEmit(u8 stream, u8 task, u8 policy, u32 extraFlags, const char *text, usize len)
{
    if(policy == LOGP_DROP || len == 0)
        return;
    u32 flags = extraFlags;
    if((policy & LOGP_DISK) != 0)
        flags |= LOG_F_DISK;
    RingWrite(G_RING, stream, task, flags, text, len);
}

void LogFeed(LineBuf *lb, u8 stream, u8 task, u8 policy, const char *data, usize n)
{
    for(usize i = 0; i < n; i++)
    {
        char c = data[i];
        if(c == '\n')
        {
            LogEmit(stream, task, policy, 0, lb->buf, lb->len);
            lb->len = 0;
            continue;
        }
        if(c == '\r')
            continue;

        if(lb->len + 1 >= sizeof(lb->buf))
        {
            /* The triggering byte starts the next fragment */
            LogEmit(stream, task, policy, LOG_F_CONT, lb->buf, lb->len);
            lb->len = 0;
        }
        lb->buf[lb->len++] = c;
    }
}

void LogFlushPartial(LineBuf *lb, u8 stream, u8 task, u8 policy)
{
    if(lb->len > 0)
    {
        LogEmit(stream, task, policy, 0, lb->buf, lb->len);
        lb->len = 0;
    }
}

void StatusInit(StatusBlock *status, StatusSnapshot *snapshot);

/* ======================================================================
 * state
 * ====================================================================== */

/* One type for everything init runs, discriminated by schedule. */
#define SCHED_ALWAYS 0u   /* /tasks/always/       supervised, respawned */
#define SCHED_BOOT   1u   /* /tasks/boot/         run at boot, not respawned */
#define SCHED_EVERY  2u   /* /tasks/<dur>/        interval since boot */
#define SCHED_CAL    3u   /* /tasks/1d-03-30/     wall clock recurrence */

typedef struct
{
    char path[CFG_PATH_MAX];
    u16  nameOffset;
    bool bHasCheck;

    u32  schedule;
    u64  intervalNs;
    CalSpec cal;        /* SCHED_CAL only */

    /* resolved from TASK_RULES at scan time */
    u32  uid;
    u32  gid;
    u64  capMask;
    bool bCritical;
    u32  maxRestarts;
    u64  stableNs;
    u64  probeIntervalNs;
    u64  probeTimeoutNs;
    u64  graceNs;
    u8   outPolicy;
    u8   errPolicy;

    u32  state;
    i32  pid;
    u64  startedNs;
    u64  nextRunNs;
    u64  backoffNs;
    u32  runs;
    u32  consecFails;
    u32  overruns;
    i32  lastExit;
    i32  lastSignal;
    u64  lastExitNs;
    u64  killDeadlineNs;
    bool bUnhealthyKill;

    i32     outFd;
    i32     errFd;
    LineBuf outLine;
    LineBuf errLine;

    i32  probePid;
    u64  probeStartNs;
    u64  nextProbeNs;
    i32  lastProbeRc;
    u64  lastProbeNs;
    u32  probeFails;
    bool bProbeKilled;   /* timeout already counted, do not count it twice */

    u64  lastCpuTicks;
    u64  lastSampleNs;
    char lastProcState;
} Task;

static const char *TaskName(const Task *t)
{
    return t->path + t->nameOffset;
}

#if FEATURE_EXEC_PROBES
static bool TaskCheckPath(const Task *t, char *path, usize cap)
{
    return StrCopyOk(path, cap, t->path) && StrCatOk(path, cap, ".check");
}
#endif

typedef struct
{
    Arena        arena;
    LogRing     *ring;
    StatusBlock *status;
    StatusSnapshot statusSnapshot;

    Task         task[CFG_MAX_TASKS];
    usize        taskCount;

    i32          consoleFd;
    i32          wdogFd;
    u64          wdogNextPetNs;

    i32          logdPid;
    u64          logdBackoffNs;
    u64          logdNextSpawnNs;
    u64          logdStartedNs;
    u64          logdProgressSeen;
    u64          logdProgressSeenNs;

    i32          sntpFd;
    u64          sntpNextNs;
    u64          sntpDeadlineNs;
    u64          sntpXmitNtp;
    u64          sntpSentNs;
    bool         bSntpSynced;

    bool         bShutdown;
    bool         bKillSent;
    u64          shutdownNs;
    u64          shutdownDeadlineNs;
    u32          shutdownCmd;
} InitState;

extern InitState G_INIT;
extern char *G_ENVP[];

/* set from signal handlers, consumed by the event loop */
extern volatile i32 G_SIG_CHLD;
extern volatile i32 G_SIG_SHUTDOWN;

void TaskScanAll(InitState *st);
void TaskStart(InitState *st, Task *t, u64 nowNs);
void TaskTick(InitState *st, u64 nowNs);
bool TaskReap(InitState *st, i32 pid, i32 status, u64 nowNs);
void TaskSignalAll(InitState *st, i32 sig);
bool TaskAnyAlive(const InitState *st);
u64  TaskNextDeadline(const InitState *st, u64 nowNs);
void TaskDrain(InitState *st, Task *t);
void TaskPublish(InitState *st);

void ProbeTick(InitState *st, Task *t, u64 nowNs);
bool ProbeReap(InitState *st, i32 pid, i32 status, u64 nowNs);
void ProcSample(Task *t, u64 nowNs);

void WdogOpen(InitState *st);
void WdogTick(InitState *st, u64 nowNs);
void WdogClose(InitState *st);

void SntpTick(InitState *st, u64 nowNs);
void SntpHandleReply(InitState *st, u64 nowNs);

void LogdSupervise(InitState *st, u64 nowNs);
bool LogdReap(InitState *st, i32 pid, i32 status, u64 nowNs);
void LogdCheckStall(InitState *st, u64 nowNs);
NORETURN void LogWriterMain(InitState *st);

NORETURN void ChildFail(const char *step, isize result);
void ChildCheck(const char *step, isize result);
void ChildDup(const char *step, i32 oldFd, i32 newFd);
void ChildPrepare(void);
void ChildApplyPrivileges(u32 uid, u32 gid, u64 capMask);
i32  SpawnChild(const char *path, i32 outFd, i32 errFd, const Task *t);

/* ======================================================================
 * directory listing
 * ====================================================================== */

/* Names are duplicated into the arena, the caller resets it when done. */

typedef struct
{
    char **name;
    usize  count;
} DirList;

/* DT_UNKNOWN entries are always accepted, some filesystems never fill d_type
 * in. Dotfiles are skipped. */
bool DirRead(const char *path, Arena *a, DirList *out, usize maxEntries, u8 wantType);

#define DIR_BUF_BYTES 4096

bool DirRead(const char *path, Arena *a, DirList *out, usize maxEntries, u8 wantType)
{
    out->name = NULL;
    out->count = 0;

    isize fd = SysOpen(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;

    char **names = (char **)ArenaAlloc(a, maxEntries * sizeof(char *), sizeof(char *));
    char *storage = (char *)ArenaAlloc(a, maxEntries * CFG_PATH_MAX, 1);
    if(names == NULL || storage == NULL)
    {
        SysClose((i32)fd);
        return false;
    }

    u8 buf[DIR_BUF_BYTES] __attribute__((aligned(8)));
    usize count = 0;
    usize dropped = 0;

    for(;;)
    {
        isize n = SysGetdents64((i32)fd, buf, sizeof(buf));
        if(n <= 0)
            break;

        usize off = 0;
        while(off + sizeof(KDirent64) <= (usize)n)
        {
            const KDirent64 *d = (const KDirent64 *)(buf + off);
            if(d->reclen == 0 || off + d->reclen > (usize)n)
                break;
            off += d->reclen;

            if(d->name[0] == '.')
                continue;
            if(d->type != DT_UNKNOWN && d->type != wantType)
                continue;

            if(StrLen(d->name) >= CFG_PATH_MAX)
            {
                LogF("%s/%s: name too long, skipped", path, d->name);
                continue;
            }

            if(count == maxEntries)
                dropped++;
            NameSetInsert(names, storage, &count, maxEntries, CFG_PATH_MAX, d->name);
        }
    }

    SysClose((i32)fd);

    if(dropped != 0)
        LogF("%s: %zu entries omitted after limit %zu", path, dropped, maxEntries);
    out->name = names;
    out->count = count;
    return true;
}

/* ======================================================================
 * tasks
 * ====================================================================== */

NORETURN void ChildFail(const char *step, isize result)
{
    char line[128];
    usize n = Fmt(line, sizeof(line), "init: child %s failed, errno %d\n",
                  step, (i32)-result);
    SysWrite(2, line, n);
    SysExit(126);
}

void ChildCheck(const char *step, isize result)
{
    if(result < 0)
        ChildFail(step, result);
}

void ChildDup(const char *step, i32 oldFd, i32 newFd)
{
    if(oldFd != newFd)
        ChildCheck(step, SysDup3(oldFd, newFd, 0));
}

void ChildPrepare(void)
{
    KSigAction sa;
    memset(&sa, 0, sizeof(sa));
    ChildCheck("sigaction SIGCHLD", SysSigAction(SIGCHLD, &sa, NULL));
    ChildCheck("sigaction SIGTERM", SysSigAction(SIGTERM, &sa, NULL));
    ChildCheck("sigaction SIGUSR1", SysSigAction(SIGUSR1, &sa, NULL));
    ChildCheck("sigaction SIGUSR2", SysSigAction(SIGUSR2, &sa, NULL));
    ChildCheck("sigaction SIGINT", SysSigAction(SIGINT, &sa, NULL));
    ChildCheck("sigaction SIGHUP", SysSigAction(SIGHUP, &sa, NULL));

    ChildCheck("setsid", SysSetSid());

    KSigSet none = { 0 };
    ChildCheck("sigprocmask", SysSigProcMask(SIG_SETMASK, &none, NULL));
}

void ChildApplyPrivileges(u32 uid, u32 gid, u64 capMask)
{
#if FEATURE_CAPABILITY_DROP
    /* Order is load-bearing: no_new_privs, bounding set, ids, capset.
     * The ambient set is what carries the retained caps across execve. */
    ChildCheck("PR_SET_NO_NEW_PRIVS",
               SysPrctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));

    for(u32 cap = 0; cap <= CAP_LAST_CAP; cap++)
    {
        if((capMask & (1ull << cap)) == 0)
            ChildCheck("PR_CAPBSET_DROP",
                       SysPrctl(PR_CAPBSET_DROP, cap, 0, 0, 0));
    }

    bool bDropping = uid != 0;
    if(bDropping && capMask != 0)
        ChildCheck("PR_SET_KEEPCAPS",
                   SysPrctl(PR_SET_KEEPCAPS, 1, 0, 0, 0));

    /* Dropping the uid without the gid leaves the child in group 0 holding
     * root's supplementary groups, which looks dropped and is not. A rule that
     * names a uid and no gid gets the uid as its gid. */
    if(bDropping || gid != 0)
    {
        ChildCheck("setgroups", SysSetGroups(0, NULL));
        ChildCheck("setgid", SysSetGid(gid != 0 ? gid : uid));
    }
    if(bDropping)
        ChildCheck("setuid", SysSetUid(uid));

    KCapHeader hdr = { LINUX_CAPABILITY_VERSION_3, 0 };
    KCapData data[2];
    memset(data, 0, sizeof(data));
    data[0].effective = (u32)(capMask & 0xffffffffull);
    data[0].permitted = data[0].effective;
    data[0].inheritable = data[0].effective;
    data[1].effective = (u32)(capMask >> 32);
    data[1].permitted = data[1].effective;
    data[1].inheritable = data[1].effective;
    ChildCheck("capset", SysCapSet(&hdr, data));

    for(u32 cap = 0; cap <= CAP_LAST_CAP; cap++)
    {
        if(capMask & (1ull << cap))
            ChildCheck("PR_CAP_AMBIENT_RAISE",
                       SysPrctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0));
    }

    if(bDropping && capMask != 0)
        ChildCheck("PR_SET_KEEPCAPS",
                   SysPrctl(PR_SET_KEEPCAPS, 0, 0, 0, 0));
#else
    UNUSED(capMask);
    if(uid != 0 || gid != 0)
    {
        ChildCheck("setgroups", SysSetGroups(0, NULL));
        ChildCheck("setgid", SysSetGid(gid != 0 ? gid : uid));
    }
    if(uid != 0)
        ChildCheck("setuid", SysSetUid(uid));
#endif
}

i32 SpawnChild(const char *path, i32 outFd, i32 errFd, const Task *t)
{
    isize pid = SysFork();
    if(pid != 0)
        return (i32)pid;

    ChildPrepare();

    isize devNull = SysOpen("/dev/null", O_RDWR, 0);
    if(devNull >= 0)
    {
        ChildDup("dup3 stdin", (i32)devNull, 0);
        if(outFd < 0)
            ChildDup("dup3 stdout", (i32)devNull, 1);
        if(errFd < 0)
            ChildDup("dup3 stderr", (i32)devNull, 2);
        if(devNull > 2)
            SysClose((i32)devNull);
    }
    if(outFd >= 0)
        ChildDup("dup3 stdout", outFd, 1);
    if(errFd >= 0)
        ChildDup("dup3 stderr", errFd, 2);

    if(t != NULL)
        ChildApplyPrivileges(t->uid, t->gid, t->capMask);

    char argv0[CFG_PATH_MAX];
    StrCopy(argv0, sizeof(argv0), path);
    char *argv[2];
    argv[0] = argv0;
    argv[1] = NULL;

    SysExecve(path, argv, G_ENVP);
    SysExit(127);
}

static void TaskApplyRule(Task *t)
{
    const TaskRule *r = TaskRuleFind(TaskName(t));

    t->uid = r != NULL ? r->uid : 0;
    t->gid = r != NULL ? r->gid : 0;
    t->capMask = r != NULL ? r->capMask : 0;
    t->bCritical = r != NULL && (r->flags & RULE_CRITICAL) != 0;

    t->maxRestarts = (r != NULL && r->maxRestarts != 0) ? r->maxRestarts : CFG_MAX_RESTARTS;
    t->stableNs = (r != NULL && r->stableNs != 0) ? r->stableNs : CFG_STABLE_NS;
    t->probeIntervalNs = (r != NULL && r->probeIntervalNs != 0)
        ? r->probeIntervalNs : CFG_PROBE_INTERVAL_NS;
    t->probeTimeoutNs = (r != NULL && r->probeTimeoutNs != 0)
        ? r->probeTimeoutNs : CFG_PROBE_TIMEOUT_NS;
    t->graceNs = (r != NULL && r->graceNs != 0) ? r->graceNs : CFG_PROBE_GRACE_NS;
    t->outPolicy = LogPolicyResolve(r != NULL ? r->outPolicy : LOGP_INHERIT,
                                    CFG_STDOUT_POLICY);
    t->errPolicy = LogPolicyResolve(r != NULL ? r->errPolicy : LOGP_INHERIT,
                                    CFG_STDERR_POLICY);
}

/* When a deadline-driven task next runs, on CLOCK_BOOTTIME like everything in
 * the event loop. SCHED_CAL reads the wall clock to place the deadline, so a
 * realtime step invalidates it and TaskRedateCal runs after every sync. */
static u64 TaskNextRunNs(const Task *t, u64 nowNs)
{
    if(t->schedule == SCHED_EVERY)
        return nowNs + t->intervalNs;

    if(t->schedule == SCHED_CAL)
    {
        u64 unixSec = SysRealNs() / NS_PER_SEC;
        return nowNs + SecsUntilCalendar(&t->cal, unixSec, CFG_TZ_OFFSET_SEC) * NS_PER_SEC;
    }

    return nowNs;
}

/* Move a deadline-driven task to its next slot after it has just fired. An
 * interval keeps its phase by counting from the deadline it met. A calendar
 * recurrence re-derives from the wall clock, which is its own anchor. */
static void TaskAdvanceDeadline(Task *t, u64 nowNs)
{
    t->nextRunNs = t->schedule == SCHED_EVERY
        ? IntervalAdvance(t->nextRunNs, t->intervalNs, nowNs)
        : TaskNextRunNs(t, nowNs);
}

/* An unsynced board boots at the epoch, so calendar tasks are scheduled against
 * a clock reading 1970 and land early. Re-dating them on the first sync is what
 * makes that a wrong start time rather than a task that never runs. */
#if !OFFLINE_MODE
static void TaskRedateCal(InitState *st, u64 nowNs)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        Task *t = &st->task[i];
        if(t->schedule != SCHED_CAL || t->state == TS_RUNNING)
            continue;

        u64 next = TaskNextRunNs(t, nowNs);
        u64 moved = next > t->nextRunNs ? next - t->nextRunNs : t->nextRunNs - next;
        t->nextRunNs = next;

        /* SecsUntilCalendar works in whole seconds against a nanosecond clock,
           so an untouched deadline still comes back a fraction different. Only
           a real step is worth a line: this runs after every poll. */
        if(moved > NS_PER_SEC)
        {
            LogF("%s: next run in %llus after clock step", TaskName(t),
                 (u64)((next - nowNs) / NS_PER_SEC));
        }
    }
}
#endif

void TaskScanAll(InitState *st)
{
    usize mark = ArenaMark(&st->arena);
    DirList dirs;

    if(!DirRead(CFG_TASK_DIR, &st->arena, &dirs, CFG_MAX_TASK_DIRS, DT_DIR))
    {
        LogF("no %s directory, nothing to run", CFG_TASK_DIR);
        ArenaReset(&st->arena, mark);
        return;
    }

    u64 nowNs = SysBootNs();

    for(usize d = 0; d < dirs.count; d++)
    {
        u32 schedule;
        u64 intervalNs = 0;
        CalSpec cal = { 0, 0, 0 };

        if(StrEq(dirs.name[d], "always"))
            schedule = SCHED_ALWAYS;
        else if(StrEq(dirs.name[d], "boot"))
            schedule = SCHED_BOOT;
        else if(ParseCalendar(dirs.name[d], &cal))
            schedule = SCHED_CAL;
        else if(ParseDuration(dirs.name[d], &intervalNs) && intervalNs != 0)
            schedule = SCHED_EVERY;
        else
        {
            LogF("%s: not a schedule, ignored", dirs.name[d]);
            continue;
        }

        char dirPath[CFG_PATH_MAX];
        if(!PathJoinOk(dirPath, sizeof(dirPath), CFG_TASK_DIR, dirs.name[d]))
        {
            LogF("%s/%s: schedule path too long, skipped", CFG_TASK_DIR, dirs.name[d]);
            continue;
        }

        usize inner = ArenaMark(&st->arena);
        DirList files;
        if(!DirRead(dirPath, &st->arena, &files, CFG_MAX_TASKS, DT_REG))
        {
            ArenaReset(&st->arena, inner);
            continue;
        }

        for(usize f = 0; f < files.count && st->taskCount < CFG_MAX_TASKS; f++)
        {
            if(StrEndsWith(files.name[f], ".check"))
                continue;
            if(StrLen(files.name[f]) >= CFG_NAME_MAX)
            {
                LogF("%s/%s: task name too long, skipped", dirPath, files.name[f]);
                continue;
            }

            Task *t = &st->task[st->taskCount];
            memset(t, 0, sizeof(*t));
            if(!PathJoinOk(t->path, sizeof(t->path), dirPath, files.name[f]))
            {
                LogF("%s/%s: task path too long, skipped", dirPath, files.name[f]);
                continue;
            }
            t->nameOffset = (u16)(StrLen(t->path) - StrLen(files.name[f]));

            if(SysAccess(t->path, X_OK) < 0)
            {
                LogF("%s: not executable, skipped", TaskName(t));
                continue;
            }

#if FEATURE_EXEC_PROBES
            char checkPath[CFG_PATH_MAX];
            if(!TaskCheckPath(t, checkPath, sizeof(checkPath)))
            {
                LogF("%s: check path too long, skipped", TaskName(t));
                continue;
            }
            t->bHasCheck = SysAccess(checkPath, X_OK) == 0;
#endif

            t->schedule = schedule;
            t->intervalNs = intervalNs;
            t->cal = cal;
            TaskApplyRule(t);

            t->outFd = -1;
            t->errFd = -1;
            t->lastProbeRc = -1;
            t->state = TS_PENDING;
            t->nextRunNs = TaskNextRunNs(t, nowNs);
            st->taskCount++;
        }

        ArenaReset(&st->arena, inner);
    }

    ArenaReset(&st->arena, mark);
    LogF("%zu tasks in %s", st->taskCount, CFG_TASK_DIR);
}

static void TaskClosePipes(Task *t)
{
    if(t->outFd >= 0)
    {
        SysClose(t->outFd);
        t->outFd = -1;
    }
    if(t->errFd >= 0)
    {
        SysClose(t->errFd);
        t->errFd = -1;
    }
}

static void SignalChild(i32 pid, i32 sig)
{
    if(pid <= 1)
        return;
    SysKill(-pid, sig);
    SysKill(pid, sig);
}

static void ProbeDiscard(Task *t, bool bLog)
{
    if(t->probePid <= 0)
        return;

    if(bLog)
        LogF("%s: cancelling probe pid %d", TaskName(t), t->probePid);
    SignalChild(t->probePid, SIGKILL);
    t->probePid = 0;
    t->bProbeKilled = false;
}

#if FEATURE_LOG_CAPTURE
static bool TaskCaptureOpen(Task *t, i32 fds[2], const char *stream)
{
    isize result = SysPipe2(fds, O_CLOEXEC);
    if(result < 0)
    {
        LogF("%s: %s capture disabled, pipe failed (%d)",
             TaskName(t), stream, (i32)result);
        return false;
    }

    if(SysSetNonBlock(fds[0]))
        return true;

    SysClose(fds[0]);
    SysClose(fds[1]);
    fds[0] = -1;
    fds[1] = -1;
    LogF("%s: %s capture disabled, nonblocking setup failed",
         TaskName(t), stream);
    return false;
}
#endif

void TaskStart(InitState *st, Task *t, u64 nowNs)
{
    UNUSED(st);

    i32 childOut = -1;
    i32 childErr = -1;
    i32 outPipe[2] = { -1, -1 };
    i32 errPipe[2] = { -1, -1 };

#if FEATURE_LOG_CAPTURE
    if(t->outPolicy != LOGP_DROP && TaskCaptureOpen(t, outPipe, "stdout"))
        childOut = outPipe[1];
    if(t->errPolicy != LOGP_DROP && TaskCaptureOpen(t, errPipe, "stderr"))
        childErr = errPipe[1];
#endif

    i32 pid = SpawnChild(t->path, childOut, childErr, t);

    if(outPipe[1] >= 0)
        SysClose(outPipe[1]);
    if(errPipe[1] >= 0)
        SysClose(errPipe[1]);

    if(pid < 0)
    {
        if(outPipe[0] >= 0)
            SysClose(outPipe[0]);
        if(errPipe[0] >= 0)
            SysClose(errPipe[0]);
        t->consecFails++;
        t->backoffNs = BackoffNext(t->backoffNs);
        t->nextRunNs = nowNs + t->backoffNs;
        t->state = TS_BACKOFF;
        LogF("%s: fork failed (%d)", TaskName(t), pid);
        return;
    }

    t->outFd = outPipe[0];
    t->errFd = errPipe[0];

    t->pid = pid;
    t->state = TS_RUNNING;
    t->startedNs = nowNs;
    t->lastSampleNs = nowNs;
    t->lastCpuTicks = 0;
    t->runs++;
    t->probeFails = 0;
    t->lastProbeRc = -1;
    t->nextProbeNs = nowNs + t->graceNs;

    LogF("%s: started pid %d", TaskName(t), pid);
}

void TaskDrain(InitState *st, Task *t)
{
    char buf[1024];
    u8 idx = (u8)(t - st->task);

    for(i32 which = 0; which < 2; which++)
    {
        i32 *fdSlot = which == 0 ? &t->outFd : &t->errFd;
        i32 fd = *fdSlot;
        if(fd < 0)
            continue;

        LineBuf *lb = which == 0 ? &t->outLine : &t->errLine;
        u8 stream = which == 0 ? LOG_SRC_OUT : LOG_SRC_ERR;
        u8 policy = which == 0 ? t->outPolicy : t->errPolicy;

        for(;;)
        {
            isize n = SysRead(fd, buf, sizeof(buf));
            if(n > 0)
            {
                LogFeed(lb, stream, idx, policy, buf, (usize)n);
                continue;
            }
            if(n == -EINTR)
                continue;
            if(n == -EAGAIN)
                break;
            if(n < 0)
                LogF("%s: %s capture read failed (%d)", TaskName(t),
                     which == 0 ? "stdout" : "stderr", (i32)n);
            LogFlushPartial(lb, stream, idx, policy);
            SysClose(fd);
            *fdSlot = -1;
            break;
        }
    }
}

static void TaskOnExit(InitState *st, Task *t, i32 status, u64 nowNs)
{
    bool bUnhealthyExit = t->bUnhealthyKill;

    SignalChild(t->pid, SIGKILL);
    t->lastExitNs = nowNs;
    t->pid = 0;
    t->killDeadlineNs = 0;
    t->bUnhealthyKill = false;
    ProbeDiscard(t, true);

    if(WIFEXITED(status))
    {
        t->lastExit = (i32)WEXITSTATUS(status);
        t->lastSignal = 0;
    }
    else
    {
        t->lastExit = -1;
        t->lastSignal = (i32)WTERMSIG(status);
    }

    TaskDrain(st, t);
    u8 idx = (u8)(t - st->task);
    LogFlushPartial(&t->outLine, LOG_SRC_OUT, idx, t->outPolicy);
    LogFlushPartial(&t->errLine, LOG_SRC_ERR, idx, t->errPolicy);
    TaskClosePipes(t);

    if(st->bShutdown)
    {
        t->state = TS_DONE;
        return;
    }

    bool bClean = t->lastSignal == 0 && t->lastExit == 0;

    if(t->schedule == SCHED_EVERY || t->schedule == SCHED_CAL)
    {
        t->state = TS_IDLE;
        if(!bClean)
            LogF("%s: exit %d sig %d after %ums", TaskName(t), t->lastExit, t->lastSignal,
                 (u32)((nowNs - t->startedNs) / NS_PER_MS));
        return;
    }

    if(t->schedule == SCHED_BOOT && bClean)
    {
        t->state = TS_DONE;
        LogF("%s: done", TaskName(t));
        return;
    }

    if(bUnhealthyExit)
    {
        t->consecFails++;
    }
    else
    {
        t->consecFails = RestartFailuresNext(t->consecFails, t->startedNs, nowNs,
                                             t->stableNs);
        if(t->consecFails == 0)
            t->backoffNs = 0;
    }

    if(t->consecFails >= t->maxRestarts)
    {
        t->state = TS_FAILED;
        LogF("%s: FAILED after %u consecutive failures (exit %d sig %d)",
             TaskName(t), t->consecFails, t->lastExit, t->lastSignal);
        return;
    }

    t->backoffNs = BackoffNext(t->backoffNs);
    t->nextRunNs = nowNs + t->backoffNs;
    t->state = TS_BACKOFF;
    LogF("%s: exit %d sig %d, respawn in %ums",
         TaskName(t), t->lastExit, t->lastSignal, (u32)(t->backoffNs / NS_PER_MS));
}

bool TaskReap(InitState *st, i32 pid, i32 status, u64 nowNs)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        if(st->task[i].pid == pid)
        {
            TaskOnExit(st, &st->task[i], status, nowNs);
            return true;
        }
    }
    return false;
}

void TaskTick(InitState *st, u64 nowNs)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        Task *t = &st->task[i];

        switch(t->state)
        {
        case TS_PENDING:
        case TS_IDLE:
            if(t->schedule == SCHED_ALWAYS || t->schedule == SCHED_BOOT)
            {
                TaskStart(st, t, nowNs);
            }
            else if(nowNs >= t->nextRunNs)
            {
                /* advance before spawning, or the tick below sees the deadline
                 * it just met and counts a spurious overrun */
                TaskAdvanceDeadline(t, nowNs);
                TaskStart(st, t, nowNs);
            }
            break;

        case TS_BACKOFF:
            if(nowNs >= t->nextRunNs)
                TaskStart(st, t, nowNs);
            break;

        case TS_RUNNING:
            if(t->bUnhealthyKill && t->killDeadlineNs != 0 &&
               nowNs >= t->killDeadlineNs)
            {
                LogF("%s: restart grace expired, sending SIGKILL", TaskName(t));
                SignalChild(t->pid, SIGKILL);
                t->killDeadlineNs = 0;
            }

            /* a periodic task still running when its next interval arrives is
             * logged and passed over, never queued */
            if((t->schedule == SCHED_EVERY || t->schedule == SCHED_CAL) &&
               nowNs >= t->nextRunNs)
            {
                t->overruns++;
                TaskAdvanceDeadline(t, nowNs);
                LogF("%s: still running, interval skipped (%u total)",
                     TaskName(t), t->overruns);
            }
            ProcSample(t, nowNs);
#if FEATURE_EXEC_PROBES
            ProbeTick(st, t, nowNs);
#endif
            break;

        default:
            break;
        }
    }
}

void TaskSignalAll(InitState *st, i32 sig)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        Task *t = &st->task[i];
        SignalChild(t->pid, sig);
        ProbeDiscard(t, false);
    }
}

bool TaskAnyAlive(const InitState *st)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        if(st->task[i].pid > 0 || st->task[i].probePid > 0)
            return true;
    }
    return false;
}

u64 TaskNextDeadline(const InitState *st, u64 nowNs)
{
    u64 best = nowNs + CFG_LOOP_MAX_WAIT_NS;

    for(usize i = 0; i < st->taskCount; i++)
    {
        const Task *t = &st->task[i];

        if((t->state == TS_BACKOFF || t->state == TS_IDLE) && t->nextRunNs < best)
            best = t->nextRunNs;
        if(t->state == TS_RUNNING && t->bHasCheck && !t->bUnhealthyKill &&
           t->nextProbeNs < best)
            best = t->nextProbeNs;
        if(t->probePid > 0 && !t->bUnhealthyKill)
        {
            u64 deadline = t->probeStartNs + t->probeTimeoutNs;
            if(deadline < best)
                best = deadline;
        }
        if(t->state == TS_RUNNING && t->killDeadlineNs != 0 &&
           t->killDeadlineNs < best)
            best = t->killDeadlineNs;
    }
    return best;
}

void TaskPublish(InitState *st)
{
    StatusBlock *sb = st->status;
    if(sb == NULL)
        return;

    StatusSnapshot *snapshot = &st->statusSnapshot;
    snapshot->count = (u32)st->taskCount;
    snapshot->arenaPeak = st->arena.peak;
    snapshot->logDropped = st->ring != NULL ?
        (u64)__atomic_load_n(&st->ring->dropped, __ATOMIC_RELAXED) : 0;
    snapshot->shutdownNs = st->shutdownNs;

    for(usize i = 0; i < st->taskCount; i++)
    {
        const Task *t = &st->task[i];
        StatusEntry *e = &snapshot->task[i];

        StrCopy(e->name, sizeof(e->name), TaskName(t));
        e->pid = t->pid;
        e->state = t->state;
        e->schedule = t->schedule;
        e->lastExit = t->lastExit;
        e->lastSignal = t->lastSignal;
        e->lastExitNs = t->lastExitNs;
        e->startedNs = t->startedNs;
        e->runs = t->runs;
        e->consecFails = t->consecFails;
        e->lastProbeRc = t->lastProbeRc;
        e->probeFails = t->probeFails;
        e->lastProbeNs = t->lastProbeNs;
        e->flags = 0;
        if(t->bCritical)
            e->flags |= STF_CRITICAL;
        if(t->bHasCheck)
            e->flags |= STF_PROBED;
    }

    StatusPublish(sb, snapshot);
}

/* ======================================================================
 * liveness
 * ====================================================================== */

/* Pull-based. wait4 only detects death, so a daemon that is deadlocked
 * or no longer answering its socket stays "alive" forever. */

void ProcSample(Task *t, u64 nowNs)
{
    if(t->pid <= 0 || nowNs - t->lastSampleNs < CFG_PROC_SAMPLE_NS)
        return;

    char path[64];
    Fmt(path, sizeof(path), "/proc/%d/stat", t->pid);

    isize fd = SysOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return;

    char buf[512];
    isize n = SysRead((i32)fd, buf, sizeof(buf) - 1);
    SysClose((i32)fd);
    if(n <= 0)
        return;
    buf[n] = '\0';

    ProcStat ps;
    if(!ProcStatParse(buf, (usize)n, &ps))
        return;

    /* never restarts on its own: an idle daemon at 03:00 and a deadlocked one
     * both show zero CPU */
    if(ps.state != t->lastProcState)
    {
        if(ps.state == 'D')
            LogF("%s: pid %d in uninterruptible sleep", TaskName(t), t->pid);
        else if(ps.state == 'Z')
            LogF("%s: pid %d is a zombie", TaskName(t), t->pid);
    }

    t->lastProcState = ps.state;
    t->lastCpuTicks = ps.utime + ps.stime;
    t->lastSampleNs = nowNs;
}

#if FEATURE_EXEC_PROBES

/* Shared by the reap path and the timeout path, so a probe that never comes
 * back is accounted for exactly like one that comes back failing. */
static void ProbeFailed(Task *t, u64 nowNs)
{
    /* probes firing before a task is ready cause restart loops */
    if(nowNs - t->startedNs < t->graceNs)
        return;

    t->probeFails++;
    LogF("%s: probe failed (%d), %u/%u", TaskName(t), t->lastProbeRc,
         t->probeFails, (u32)CFG_PROBE_FAIL_LIMIT);

    if(t->probeFails >= CFG_PROBE_FAIL_LIMIT && t->pid > 0)
    {
        LogF("%s: restarting on probe failure", TaskName(t));
        t->bUnhealthyKill = true;
        t->killDeadlineNs = nowNs + CFG_RESTART_GRACE_NS;
        SignalChild(t->pid, SIGTERM);
        t->probeFails = 0;
    }
}

void ProbeTick(InitState *st, Task *t, u64 nowNs)
{
    if(!t->bHasCheck || st->bShutdown || t->bUnhealthyKill)
        return;

    if(t->probePid > 0)
    {
        /* a hung check binary has to be killed, or one process leaks per
         * interval and it presents as a memory leak */
        if(nowNs - t->probeStartNs < t->probeTimeoutNs)
            return;

        SysKill(t->probePid, SIGKILL);
        t->probeStartNs = nowNs;

        /* SIGKILL does not remove a process from uninterruptible sleep, so a
           check wedged on dead I/O never reaches ProbeReap. Counting the
           timeout here is what stops that task from silently falling out of
           supervision, and the kill is repeated in case the I/O ever lands. */
        t->bProbeKilled = true;
        t->lastProbeNs = nowNs;
        t->lastProbeRc = -(i32)SIGKILL;
        LogF("%s: probe timed out, killing pid %d", TaskName(t), t->probePid);
        ProbeFailed(t, nowNs);
        return;
    }

    if(nowNs < t->nextProbeNs)
        return;

    char checkPath[CFG_PATH_MAX];
    if(!TaskCheckPath(t, checkPath, sizeof(checkPath)))
        return;

    i32 pid = SpawnChild(checkPath, -1, -1, t);
    if(pid <= 0)
    {
        t->nextProbeNs = nowNs + t->probeIntervalNs;
        return;
    }

    t->probePid = pid;
    t->probeStartNs = nowNs;
}

bool ProbeReap(InitState *st, i32 pid, i32 status, u64 nowNs)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        Task *t = &st->task[i];
        if(t->probePid != pid)
            continue;

        t->probePid = 0;
        t->lastProbeNs = nowNs;
        t->nextProbeNs = nowNs + t->probeIntervalNs;

        /* the timeout path already counted this one and killed it */
        if(t->bProbeKilled)
        {
            t->bProbeKilled = false;
            return true;
        }

        bool bOk = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        t->lastProbeRc = bOk ? 0
            : (WIFEXITED(status) ? (i32)WEXITSTATUS(status) : -(i32)WTERMSIG(status));

        if(bOk)
        {
            if(nowNs - t->startedNs >= t->graceNs)
                t->probeFails = 0;
            return true;
        }

        ProbeFailed(t, nowNs);
        return true;
    }
    return false;
}

#else

void ProbeTick(InitState *st, Task *t, u64 nowNs)
{
    UNUSED(st);
    UNUSED(t);
    UNUSED(nowNs);
}

bool ProbeReap(InitState *st, i32 pid, i32 status, u64 nowNs)
{
    UNUSED(st);
    UNUSED(pid);
    UNUSED(status);
    UNUSED(nowNs);
    return false;
}

#endif

/* ======================================================================
 * watchdog
 * ====================================================================== */

/* Petting is gated on liveness. An unconditional pet from the event
 * loop keeps a happy watchdog while a critical daemon is wedged, which protects
 * nothing. */

#if FEATURE_WATCHDOG

void WdogOpen(InitState *st)
{
    st->wdogFd = -1;

    isize fd = SysOpen(CFG_WATCHDOG_DEV, O_WRONLY | O_CLOEXEC, 0);
    if(fd < 0)
    {
        LogF("no %s (%d), running unguarded", CFG_WATCHDOG_DEV, (i32)fd);
        return;
    }

    i32 timeout = CFG_WDOG_TIMEOUT_SEC;
    if(SysIoctl((i32)fd, WDIOC_SETTIMEOUT, (isize)&timeout) < 0)
        LogF("watchdog: SETTIMEOUT rejected, keeping driver default");

    st->wdogFd = (i32)fd;
    st->wdogNextPetNs = 0;
    LogF("watchdog armed, %ds timeout", timeout);
}

static bool WdogCriticalHealthy(const InitState *st, u64 nowNs)
{
    for(usize i = 0; i < st->taskCount; i++)
    {
        const Task *t = &st->task[i];
        if(!t->bCritical)
            continue;

        if(t->state != TS_RUNNING || t->pid <= 0)
            return false;
        if(t->lastProcState == 'Z')
            return false;

        if(t->bHasCheck)
        {
            /* inside the grace window there is no verdict yet, so trust it */
            if(nowNs - t->startedNs < t->graceNs)
                continue;
            if(t->lastProbeRc != 0)
                return false;
            if(nowNs - t->lastProbeNs > t->probeIntervalNs * 3ull)
                return false;
        }
    }
    return true;
}

void WdogTick(InitState *st, u64 nowNs)
{
    if(st->wdogFd < 0 || nowNs < st->wdogNextPetNs)
        return;

    if(!WdogCriticalHealthy(st, nowNs))
        return;

    if(SysIoctl(st->wdogFd, WDIOC_KEEPALIVE, 0) < 0)
        SysWrite(st->wdogFd, "1", 1);

    st->wdogNextPetNs = nowNs + CFG_WDOG_PET_NS;
}

void WdogClose(InitState *st)
{
    /* closing without the magic 'V' leaves the timer armed, which is what
     * PID 1 wants */
    if(st->wdogFd >= 0)
    {
        SysClose(st->wdogFd);
        st->wdogFd = -1;
    }
}

#else

void WdogOpen(InitState *st)
{
    st->wdogFd = -1;
}

void WdogTick(InitState *st, u64 nowNs)
{
    UNUSED(st);
    UNUSED(nowNs);
}

void WdogClose(InitState *st)
{
    UNUSED(st);
}

#endif

/* ======================================================================
 * SNTP socket
 * ====================================================================== */

/* Asynchronous throughout, PID 1 never stalls on the network. Permanent failure
 * only degrades calendar scheduling. */

#if OFFLINE_MODE

void SntpTick(InitState *st, u64 nowNs)
{
    UNUSED(st);
    UNUSED(nowNs);
}

void SntpHandleReply(InitState *st, u64 nowNs)
{
    UNUSED(st);
    UNUSED(nowNs);
}

#else

static void SntpClose(InitState *st)
{
    if(st->sntpFd >= 0)
    {
        SysClose(st->sntpFd);
        st->sntpFd = -1;
    }
}

static void SntpSend(InitState *st, u64 nowNs)
{
    u32 addr = 0;
    if(!ParseIPv4(CFG_SNTP_SERVER, &addr))
    {
        LogF("sntp: %s is not a dotted quad, time sync disabled", CFG_SNTP_SERVER);
        st->sntpNextNs = ~0ull;
        return;
    }

    isize fd = SysSocket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
    if(fd < 0)
    {
        st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        return;
    }

    KSockAddrIn sa;
    memset(&sa, 0, sizeof(sa));
    sa.family = AF_INET;
    sa.port = Hton16(CFG_SNTP_PORT);
    sa.addr = Hton32(addr);

    if(SysConnect((i32)fd, &sa, (u32)sizeof(sa)) < 0)
    {
        SysClose((i32)fd);
        st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        return;
    }

    st->sntpXmitNtp = SntpNtpFromUnixNs(SysRealNs());

    u8 pkt[SNTP_PKT_BYTES];
    SntpBuildRequest(pkt, st->sntpXmitNtp);

    u64 sentNs = SysBootNs();
    isize sent = SysSendTo((i32)fd, pkt, sizeof(pkt), 0, NULL, 0);
    if(sent != (isize)sizeof(pkt))
    {
        SysClose((i32)fd);
        st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        return;
    }

    st->sntpFd = (i32)fd;
    st->sntpSentNs = sentNs;
    st->sntpDeadlineNs = sentNs + CFG_SNTP_TIMEOUT_NS;
}

void SntpTick(InitState *st, u64 nowNs)
{
    if(st->bShutdown)
        return;

    if(st->sntpFd >= 0)
    {
        if(nowNs >= st->sntpDeadlineNs)
        {
            SntpClose(st);
            st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        }
        return;
    }

    if(nowNs >= st->sntpNextNs)
        SntpSend(st, nowNs);
}

void SntpHandleReply(InitState *st, u64 nowNs)
{
    if(st->sntpFd < 0)
        return;

    u8 pkt[SNTP_PKT_BYTES * 2];
    isize n = SysRecvFrom(st->sntpFd, pkt, sizeof(pkt), 0, NULL, NULL);
    if(n == -EINTR || n == -EAGAIN)
        return;
    if(n < 0)
    {
        SntpClose(st);
        st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        return;
    }

    SntpClose(st);

    u64 unixNs = 0;
    if(!SntpParseReply(pkt, (usize)n, st->sntpXmitNtp, &unixNs))
    {
        LogF("sntp: reply rejected");
        st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        return;
    }

    unixNs = SntpCorrectForRtt(unixNs, st->sntpSentNs, nowNs);

    KTimeSpec ts;
    ts.sec = (i64)(unixNs / NS_PER_SEC);
    ts.nsec = (i64)(unixNs % NS_PER_SEC);
    st->sntpNextNs = nowNs + CFG_SNTP_POLL_NS;
    if(SysClockSetTime(CLOCK_REALTIME, &ts) < 0)
    {
        LogF("sntp: clock_settime rejected");
        st->sntpNextNs = nowNs + CFG_SNTP_RETRY_NS;
        return;
    }

    LogF("sntp: clock set to %llu", (u64)ts.sec);
    st->bSntpSynced = true;
    TaskRedateCal(st, nowNs);
}

#endif

/* ======================================================================
 * log writer
 * ====================================================================== */

/* Disk writes never happen in PID 1. Managed flash stalls for seconds
 * under garbage collection, and a write() stuck in PID 1 halts supervision
 * system-wide.
 *
 * Same binary, different entry point, reached by fork() without execve. That
 * needs no argv parsing, no /proc/self/exe resolution and no re-run of early
 * boot, and cannot pick up a replaced on-disk binary. */

#if FEATURE_LOG_DISK

#ifndef CFG_LOGD_UID
  #define CFG_LOGD_UID 0
#endif

typedef struct
{
    bool  bOpen;
    usize end;
} LogdChain;

typedef struct
{
    LogdChain task[CFG_MAX_TASKS][2];
    LogdChain init;
} LogdChains;

typedef struct
{
    char  buf[CFG_LOGD_BUF_BYTES];
    usize len;
    i32   fd;
    u64   fileBytes;
    u64   lastFlushNs;

    LogIdentity last;
    u32   repeats;

    LogdChains chains;
} LogdState;

/* Room for one record, its repeat summary and a ring-loss summary */
#define LOGD_DRAIN_RESERVE (3u * LOG_SLOT_BYTES)

_Static_assert(CFG_LOGD_BUF_BYTES >= LOGD_DRAIN_RESERVE,
               "log buffer cannot hold the drain reserve");

static void LogdSleep(u64 ns)
{
    KTimeSpec ts;
    ts.sec = (i64)(ns / NS_PER_SEC);
    ts.nsec = (i64)(ns % NS_PER_SEC);
    SysPpoll(NULL, 0, &ts, NULL);
}

static bool LogdRotate(LogdState *ls)
{
    if(ls->fd >= 0)
    {
        SysClose(ls->fd);
        ls->fd = -1;
    }

    char from[CFG_PATH_MAX];
    char to[CFG_PATH_MAX];
    bool bActiveReady = false;

    for(u32 i = CFG_LOGD_ROTATIONS; i > 0; i--)
    {
        if(i == 1)
            StrCopy(from, sizeof(from), CFG_LOG_PATH);
        else
            Fmt(from, sizeof(from), "%s.%u", CFG_LOG_PATH, i - 1);
        Fmt(to, sizeof(to), "%s.%u", CFG_LOG_PATH, i);

        isize rc = SysRename(from, to);
        if(rc < 0 && rc != -ENOENT)
        {
            LogF("log rotation %s to %s failed (%d)", from, to, (i32)rc);
            return false;
        }
        if(i == 1 && (rc >= 0 || rc == -ENOENT))
            bActiveReady = true;
    }

    if(bActiveReady)
        ls->fileBytes = 0;
    return bActiveReady;
}

static bool LogdOpen(LogdState *ls)
{
    if(ls->fd >= 0)
        return true;

    isize fd = SysOpen(CFG_LOG_PATH,
                        O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0640);
    if(fd < 0)
        return false;

    ls->fd = (i32)fd;
    isize end = SysLseek(ls->fd, 0, SEEK_END);
    if(end < 0)
    {
        SysClose(ls->fd);
        ls->fd = -1;
        return false;
    }
    ls->fileBytes = (u64)end;
    return true;
}

static void LogdFlush(LogdState *ls, u64 nowNs)
{
    if(ls->len == 0)
    {
        ls->lastFlushNs = nowNs;
        return;
    }

    if(!LogdOpen(ls))
        return;
    if(ls->fileBytes >= CFG_LOGD_MAX_BYTES ||
       ls->len > (usize)(CFG_LOGD_MAX_BYTES - ls->fileBytes))
    {
        if(!LogdRotate(ls) || !LogdOpen(ls))
            return;
    }

    usize off = 0;
    while(off < ls->len)
    {
        isize n = SysWrite(ls->fd, ls->buf + off, ls->len - off);
        if(n == -EINTR)
            continue;
        if(n <= 0)
        {
            SysClose(ls->fd);
            ls->fd = -1;
            break;
        }
        off += (usize)n;
    }

    /* Preserve only the unwritten suffix after a partial failure */
    if(off > 0)
    {
        memmove(ls->buf, ls->buf + off, ls->len - off);
        ls->len -= off;
        ls->fileBytes += off;
    }
    if(ls->len == 0)
        ls->lastFlushNs = nowNs;
}

static void LogdBreakChains(LogdState *ls)
{
    memset(&ls->chains, 0, sizeof(ls->chains));
    ls->last.bValid = false;
}

static bool LogdReserve(LogdState *ls, usize len, u64 nowNs)
{
    if(len <= sizeof(ls->buf) - ls->len)
        return true;

    LogdBreakChains(ls);
    LogdFlush(ls, nowNs);
    if(len <= sizeof(ls->buf) - ls->len)
        return true;

    return false;
}

static bool LogdAppend(LogdState *ls, const char *text, usize len, usize *endOut,
                       u64 nowNs)
{
    if(len + 1 > sizeof(ls->buf))
        len = sizeof(ls->buf) - 1;
    if(!LogdReserve(ls, len + 1, nowNs))
        return false;

    memcpy(ls->buf + ls->len, text, len);
    ls->len += len;
    if(endOut != NULL)
        *endOut = ls->len;
    ls->buf[ls->len++] = '\n';
    return true;
}

static void LogdShiftChainEnds(LogdChains *chains, usize from, usize amount)
{
    if(chains->init.bOpen && chains->init.end >= from)
        chains->init.end += amount;
    for(usize i = 0; i < CFG_MAX_TASKS; i++)
    {
        for(usize stream = 0; stream < 2; stream++)
        {
            LogdChain *chain = &chains->task[i][stream];
            if(chain->bOpen && chain->end >= from)
                chain->end += amount;
        }
    }
}

static bool LogdInsertChain(LogdState *ls, LogdChain *chain, const char *text,
                            usize len, u64 nowNs)
{
    if(len > sizeof(ls->buf) - ls->len)
    {
        LogdBreakChains(ls);
        LogdFlush(ls, nowNs);
        return false;
    }

    usize at = chain->end;
    memmove(ls->buf + at + len, ls->buf + at, ls->len - at);
    memcpy(ls->buf + at, text, len);
    ls->len += len;
    LogdShiftChainEnds(&ls->chains, at, len);
    return true;
}

/* damped dedup: during a crash loop this is kilobytes instead of megabytes */
static void LogdEmitRepeats(LogdState *ls, u64 nowNs)
{
    if(ls->repeats == 0)
        return;

    char line[64];
    usize n = Fmt(line, sizeof(line), "last message repeated %u times", ls->repeats);
    if(LogdAppend(ls, line, n, NULL, nowNs))
        ls->repeats = 0;
}

/* Unknown task ids use the standalone path */
static LogdChain *LogdChainSlot(LogdChains *chains, const LogSlot *slot)
{
    if(slot->task == 0xff)
        return &chains->init;
    if(slot->task >= CFG_MAX_TASKS)
        return NULL;
    return &chains->task[slot->task][slot->stream == LOG_SRC_ERR ? 1 : 0];
}

static void LogdRecordTag(char *line, usize cap, u64 nowNs, const LogSlot *slot)
{
    const char *tag = slot->stream == LOG_SRC_ERR ? "E" :
                      (slot->stream == LOG_SRC_OUT ? "O" : "I");
    Fmt(line, cap, "[%llu] %s%u ", nowNs / NS_PER_SEC, tag, (u32)slot->task);
}

static void LogdRecord(LogdState *ls, const LogSlot *slot, u64 nowNs)
{
    LogdChain *chain = LogdChainSlot(&ls->chains, slot);
    bool bWasOpen = chain != NULL && chain->bOpen;
    bool bMoreToCome = chain != NULL && (slot->flags & LOG_F_CONT) != 0;

    if(!bWasOpen && !bMoreToCome)
    {
        u64 h = Hash64(slot->text, slot->len);
        u32 identityFlags = slot->flags & (u32)~LOG_F_CONT;
        if(LogIdentityMatch(&ls->last, slot, identityFlags, h))
        {
            ls->repeats++;
            return;
        }

        LogdEmitRepeats(ls, nowNs);
        LogIdentitySet(&ls->last, slot, identityFlags, h);

        char line[LOG_TEXT_MAX + 32];
        LogdRecordTag(line, sizeof(line), nowNs, slot);
        usize n = StrLen(line);
        n += StrCopyN(line + n, sizeof(line) - n, slot->text, slot->len);
        LogdAppend(ls, line, n, NULL, nowNs);
        return;
    }

    LogdEmitRepeats(ls, nowNs);
    ls->last.bValid = false;

    if(bWasOpen && LogdInsertChain(ls, chain, slot->text, slot->len, nowNs))
    {
        if(!bMoreToCome)
            chain->bOpen = false;
        return;
    }

    char line[LOG_TEXT_MAX + 32];
    LogdRecordTag(line, sizeof(line), nowNs, slot);
    usize n = StrLen(line);
    n += StrCopyN(line + n, sizeof(line) - n, slot->text, slot->len);
    usize end = 0;
    if(LogdAppend(ls, line, n, &end, nowNs) && chain != NULL && bMoreToCome)
    {
        chain->bOpen = true;
        chain->end = end;
    }
}

NORETURN void LogWriterMain(InitState *st)
{
    SysPrctl(PR_SET_NAME, (usize)(uintptr_t)"init-logd", 0, 0, 0);
    ChildPrepare();

    /* Drop the watchdog fd and every task pipe. init's own watchdog description
     * stays open and armed. */
    WdogClose(st);
    for(usize i = 0; i < st->taskCount; i++)
    {
        if(st->task[i].outFd >= 0)
            SysClose(st->task[i].outFd);
        if(st->task[i].errFd >= 0)
            SysClose(st->task[i].errFd);
    }
    if(st->sntpFd >= 0)
        SysClose(st->sntpFd);

    /* console stays: the writer reports its own failures somewhere */
    LogAttach(NULL, st->consoleFd);

    SysMkdir("/var", 0755);
    SysMkdir("/var/log", 0755);

#if CFG_LOGD_UID != 0
    ChildApplyPrivileges(CFG_LOGD_UID, CFG_LOGD_UID, 0);
#endif

    static LogdState ls;
    memset(&ls, 0, sizeof(ls));
    ls.fd = -1;
    ls.lastFlushNs = SysBootNs();

    RingSeq progress = __atomic_load_n(&st->ring->writerProgress, __ATOMIC_RELAXED);

    for(;;)
    {
        /* A blocked write freezes the heartbeat */
        __atomic_store_n(&st->ring->writerProgress, ++progress, __ATOMIC_RELEASE);

        u64 nowNs = SysBootNs();
        LogSlot slot;
        u64 lostTotal = 0;
        u32 drained = 0;

        for(;;)
        {
            if(sizeof(ls.buf) - ls.len < LOGD_DRAIN_RESERVE)
            {
                LogdBreakChains(&ls);
                LogdEmitRepeats(&ls, nowNs);
                LogdFlush(&ls, nowNs);
                if(sizeof(ls.buf) - ls.len < LOGD_DRAIN_RESERVE)
                    break;
            }

            u64 lost = 0;
            bool bRead = RingRead(st->ring, &slot, &lost);
            if(lost > 0)
            {
                lostTotal += lost;
                LogdBreakChains(&ls);
            }
            if(!bRead)
            {
                if(lost > 0)
                    continue;
                break;
            }
            if((slot.flags & LOG_F_DISK) != 0)
                LogdRecord(&ls, &slot, nowNs);
            if(++drained >= 256)
                break;
        }

        if(lostTotal > 0)
        {
            char line[64];
            usize n = Fmt(line, sizeof(line), "ring loss, %llu records dropped",
                          lostTotal);
            LogdEmitRepeats(&ls, nowNs);
            LogdAppend(&ls, line, n, NULL, nowNs);
        }

        bool bStopping = st->ring != NULL &&
            (__atomic_load_n(&st->ring->control, __ATOMIC_ACQUIRE) & LOG_CTL_SHUTDOWN) != 0;

        bool bDue = nowNs - ls.lastFlushNs >= CFG_LOGD_FLUSH_NS;
        if(bDue || bStopping || ls.len >= CFG_LOGD_FLUSH_BYTES)
        {
            LogdBreakChains(&ls);
            LogdEmitRepeats(&ls, nowNs);
            LogdFlush(&ls, nowNs);
        }

        if(bStopping && drained == 0)
            SysExit(0);

        if(drained == 0)
            LogdSleep(200ull * NS_PER_MS);
    }
}

void LogdSupervise(InitState *st, u64 nowNs)
{
    /* Without the ring the writer has nothing to read and no way to be told to
     * stop, so it would sit through every shutdown until the SIGKILL. */
    if(st->ring == NULL || st->logdPid > 0 || st->bShutdown)
        return;
    if(nowNs < st->logdNextSpawnNs)
        return;

    isize pid = SysFork();
    if(pid == 0)
        LogWriterMain(st);

    if(pid < 0)
    {
        st->logdBackoffNs = BackoffNext(st->logdBackoffNs);
        st->logdNextSpawnNs = nowNs + st->logdBackoffNs;
        return;
    }

    st->logdPid = (i32)pid;
    st->logdStartedNs = nowNs;
    st->logdProgressSeen = __atomic_load_n(&st->ring->writerProgress, __ATOMIC_RELAXED);
    st->logdProgressSeenNs = nowNs;
    LogF("log writer pid %d", st->logdPid);
}

bool LogdReap(InitState *st, i32 pid, i32 status, u64 nowNs)
{
    if(st->logdPid != pid)
        return false;

    st->logdPid = 0;
    if(st->bShutdown)
        return true;

    st->logdBackoffNs = BackoffNext(st->logdBackoffNs);
    st->logdNextSpawnNs = nowNs + st->logdBackoffNs;
    i32 exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    i32 termSig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    LogF("log writer exited (exit %d sig %d), respawning in %ums",
         exitCode, termSig,
         (u32)(st->logdBackoffNs / NS_PER_MS));
    return true;
}

void LogdCheckStall(InitState *st, u64 nowNs)
{
    if(st->ring == NULL || st->logdPid <= 0 || st->bShutdown)
        return;

    RingSeq progress = __atomic_load_n(&st->ring->writerProgress, __ATOMIC_ACQUIRE);
    if(progress != st->logdProgressSeen)
    {
        st->logdProgressSeen = progress;
        st->logdProgressSeenNs = nowNs;
        if(BackoffStable(st->logdStartedNs, nowNs, CFG_STABLE_NS))
            st->logdBackoffNs = 0;
        return;
    }

    if(nowNs - st->logdProgressSeenNs >= CFG_LOGD_STALL_NS)
    {
        i32 pid = st->logdPid;
        i32 status = 0;
        if(SysWait4(pid, &status, WNOHANG) == pid)
        {
            LogdReap(st, pid, status, nowNs);
            return;
        }

        LogF("log writer stalled, killing pid %d", pid);
        isize rc = SysKill(pid, SIGKILL);
        if(rc < 0)
        {
            LogF("log writer kill failed (%d)", (i32)rc);
            st->logdProgressSeenNs = nowNs;
            return;
        }

        st->logdPid = 0;
        st->logdBackoffNs = BackoffNext(st->logdBackoffNs);
        st->logdNextSpawnNs = nowNs + st->logdBackoffNs;
        LogF("stalled log writer respawning in %ums",
             (u32)(st->logdBackoffNs / NS_PER_MS));
        st->logdProgressSeenNs = nowNs;
    }
}

#else

void LogdSupervise(InitState *st, u64 nowNs)
{
    UNUSED(st);
    UNUSED(nowNs);
}

bool LogdReap(InitState *st, i32 pid, i32 status, u64 nowNs)
{
    UNUSED(st);
    UNUSED(pid);
    UNUSED(status);
    UNUSED(nowNs);
    return false;
}

void LogdCheckStall(InitState *st, u64 nowNs)
{
    UNUSED(st);
    UNUSED(nowNs);
}

NORETURN void LogWriterMain(InitState *st)
{
    UNUSED(st);
    SysExit(0);
}

#endif

/* ======================================================================
 * self-relocation
 * ====================================================================== */

/* A static-pie binary normally gets its RELATIVE relocations applied by
 * rcrt1.o/_dl_relocate_static_pie, which lives in libc. Without that,
 * every pointer in .data.rel.ro still holds its link-time value and the first
 * dereference faults. So this runs before anything else in _start.
 *
 * Constraints below: no globals, no jump tables, no calls out. None of it can
 * lean on a relocation that has not been applied yet. */

#if UINTPTR_MAX > 0xffffffffu
  #define ELFCLASS_64 1
#else
  #define ELFCLASS_64 0
#endif

#if ELFCLASS_64
typedef u64 ElfAddr;
typedef i64 ElfSAddr;
typedef struct
{
    u8  ident[16];
    u16 type, machine;
    u32 version;
    u64 entry, phoff, shoff;
    u32 flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ElfEhdr;
typedef struct
{
    u32 type, flags;
    u64 offset, vaddr, paddr, filesz, memsz, align;
} ElfPhdr;
  #define ELF_R_TYPE(i) ((u32)((i) & 0xffffffffull))
#else
typedef u32 ElfAddr;
typedef i32 ElfSAddr;
typedef struct
{
    u8  ident[16];
    u16 type, machine;
    u32 version, entry, phoff, shoff, flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ElfEhdr;
typedef struct
{
    u32 type, offset, vaddr, paddr, filesz, memsz, flags, align;
} ElfPhdr;
  #define ELF_R_TYPE(i) ((u32)((i) & 0xffu))
#endif

typedef struct
{
    ElfSAddr tag;
    ElfAddr  val;
} ElfDyn;

typedef struct
{
    ElfAddr offset;
    ElfAddr info;
} ElfRel;

typedef struct
{
    ElfAddr  offset;
    ElfAddr  info;
    ElfSAddr addend;
} ElfRela;

#define PT_DYNAMIC 2
#define DT_NULL    0
#define DT_RELA    7
#define DT_RELASZ  8
#define DT_RELAENT 9
#define DT_REL     17
#define DT_RELSZ   18
#define DT_RELENT  19

void InitSelfRelocate(usize base);

NO_SSP void InitSelfRelocate(usize base)
{
#ifdef RELOC_NONE
    UNUSED(base);
#else
    const ElfEhdr *eh = (const ElfEhdr *)base;
    const ElfPhdr *ph = (const ElfPhdr *)(base + eh->phoff);
    const ElfDyn *dyn = NULL;

    for(u16 i = 0; i < eh->phnum; i++)
    {
        if(ph[i].type == PT_DYNAMIC)
        {
            dyn = (const ElfDyn *)(base + ph[i].vaddr);
            break;
        }
    }
    if(dyn == NULL)
        return;

    usize tab = 0, size = 0, ent = 0;
    for(const ElfDyn *d = dyn; d->tag != DT_NULL; d++)
    {
#if RELOC_RELA
        if(d->tag == DT_RELA)
            tab = (usize)d->val;
        else if(d->tag == DT_RELASZ)
            size = (usize)d->val;
        else if(d->tag == DT_RELAENT)
            ent = (usize)d->val;
#else
        if(d->tag == DT_REL)
            tab = (usize)d->val;
        else if(d->tag == DT_RELSZ)
            size = (usize)d->val;
        else if(d->tag == DT_RELENT)
            ent = (usize)d->val;
#endif
    }

    if(tab == 0 || ent == 0)
        return;

    for(usize off = 0; off + ent <= size; off += ent)
    {
#if RELOC_RELA
        const ElfRela *r = (const ElfRela *)(base + tab + off);
        if(ELF_R_TYPE(r->info) != R_RELATIVE)
            continue;
        *(usize *)(base + r->offset) = base + (usize)r->addend;
#else
        const ElfRel *r = (const ElfRel *)(base + tab + off);
        if(ELF_R_TYPE(r->info) != R_RELATIVE)
            continue;
        *(usize *)(base + r->offset) += base;
#endif
    }
#endif
}

/* ======================================================================
 * panic
 * ====================================================================== */

NORETURN void InitPanic(const char *msg);
NORETURN void __stack_chk_fail(void);

extern usize __stack_chk_guard;

/* -nostdlib means the stack protector's runtime is ours to supply. On x86 the
 * guard also has to be forced to the global symbol, since the default %fs:0x28
 * slot lives in a TLS block only libc sets up. See the Makefile. */
/* terminator bytes, so a str* overrun cannot write past the guard intact */
#if UINTPTR_MAX > 0xffffffffu
usize __stack_chk_guard = 0x00000aff0d0a0000ul;
#else
usize __stack_chk_guard = 0xff0d0a00ul;
#endif

void InitGuardSeed(void);

NO_SSP void InitGuardSeed(void)
{
    usize v = 0;
    if(SysGetRandom(&v, sizeof(v), GRND_NONBLOCK) != (isize)sizeof(v))
    {
        /* pool not ready this early. Boot time plus a stack address is weak,
         * but unpredictable across boots and better than a link-time constant */
        v = (usize)SysBootNs();
        v ^= (usize)(uintptr_t)&v;
    }
    /* a zero and a newline in the guard make string-write overruns trip it */
    __stack_chk_guard = (v & ~(usize)0xff) | 0x0a;
}

NORETURN void InitPanic(const char *msg)
{
    char line[256];
    usize n = Fmt(line, sizeof(line), "init: PANIC: %s\n", msg);
    SysWrite(2, line, n);
    SysSync();

    if(SysGetPid() != 1)
        SysExit(70);

    SysReboot(LINUX_REBOOT_CMD_RESTART);
    for(;;)
        SysSchedYield();
}

NORETURN void __stack_chk_fail(void)
{
    InitPanic("stack smashing detected");
}

/* libgcc's ARM division helpers route a zero divisor through __aeabi_idiv0,
 * which calls raise(). Nothing here divides by a value that can be zero, so
 * reaching this means init has a bug. */
#if defined(__arm__)
int raise(int sig);

int raise(int sig)
{
    UNUSED(sig);
    InitPanic("division by zero");
}
#endif

/* i386 emits a call to this local alias so the check costs no PLT entry in
 * position-independent code. Every other target calls the above directly. */
#if defined(__i386__)
NORETURN void __stack_chk_fail_local(void);

NORETURN void __stack_chk_fail_local(void)
{
    InitPanic("stack smashing detected");
}
#endif

/* ======================================================================
 * boot and event loop
 * ====================================================================== */

InitState G_INIT;

/* The boot arena. In .bss, so the kernel maps it zero-filled while it loads the
 * ELF, before _start runs: no syscall to make and no failure to handle, on the
 * same lazily faulted anonymous pages an mmap would have returned. It costs no
 * bytes in the file, only a size in the program header.
 *
 * The log ring and status block cannot move here. They are MAP_SHARED so the
 * forked writer sees the same pages, and .bss after fork is copy-on-write. */
static u8 G_ARENA[CFG_ARENA_BYTES] __attribute__((aligned(16)));

/* Unqualified on purpose. `char *const` lets a compiler place the array in
 * .rodata, where every element needs a relocation a read-only segment cannot
 * carry. Plain `char *` lands in .data.rel.ro, which the self-relocator already
 * fixes up. Most targets do that anyway, loongarch does not. */
char *G_ENVP[] =
{
    (char *)"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
    (char *)"TERM=linux",
    NULL
};

volatile i32 G_SIG_CHLD = 0;
volatile i32 G_SIG_SHUTDOWN = 0;

#ifdef INIT_HAS_SA_RESTORER
extern void InitSigRestore(void);
#endif

void InitMain(void);

/* ---------------------------------------------------------------- signals */

static void OnSigChld(i32 sig)
{
    UNUSED(sig);
    G_SIG_CHLD = 1;
}

static void OnSigShutdown(i32 sig)
{
    G_SIG_SHUTDOWN = sig;
}

static void InstallHandler(i32 sig, void (*fn)(i32))
{
    KSigAction sa;
    memset(&sa, 0, sizeof(sa));
    sa.handler = (usize)fn;
    sa.flags = SA_RESTART | SA_NOCLDSTOP;
#ifdef INIT_HAS_SA_RESTORER
    sa.flags |= SA_RESTORER;
    sa.restorer = (usize)InitSigRestore;
#endif
    SysSigAction(sig, &sa, NULL);
}

/* The kernel discards default-action signals sent to PID 1, so without handlers
 * the system cannot be told to reboot at all. Signals stay blocked
 * outside ppoll, closing the race between testing a flag and sleeping. */
static void SignalSetup(KSigSet *outUnblocked)
{
    InstallHandler(SIGCHLD, OnSigChld);
    InstallHandler(SIGTERM, OnSigShutdown);
    InstallHandler(SIGUSR1, OnSigShutdown);
    InstallHandler(SIGUSR2, OnSigShutdown);
    InstallHandler(SIGINT, OnSigShutdown);
    InstallHandler(SIGHUP, OnSigShutdown);

    KSigSet block;
    block.bits = (1ull << (SIGCHLD - 1)) | (1ull << (SIGTERM - 1)) |
                 (1ull << (SIGUSR1 - 1)) | (1ull << (SIGUSR2 - 1)) |
                 (1ull << (SIGINT - 1)) | (1ull << (SIGHUP - 1));
    SysSigProcMask(SIG_BLOCK, &block, NULL);

    outUnblocked->bits = 0;
}

/* ------------------------------------------------------------- early boot */

static bool MountOne(const char *src, const char *tgt, const char *fs, u32 flags)
{
    SysMkdir(tgt, 0755);
    isize r = SysMount(src, tgt, fs, flags, NULL);
    if(r < 0 && r != -EBUSY)
        LogF("mount %s on %s failed (%d)", fs, tgt, (i32)r);
    return r >= 0 || r == -EBUSY;
}

static bool MountEarly(void)
{
    MountOne("devtmpfs", "/dev", "devtmpfs", MS_NOSUID);
    MountOne("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC);
    MountOne("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC);
    return MountOne("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC);
}

static i32 OpenConsole(void)
{
    /* O_APPEND so a harness can point this at a regular file and still get an
     * ordered transcript from several writers */
    isize fd = SysOpen("/dev/console", O_WRONLY | O_APPEND | O_NOCTTY | O_CLOEXEC, 0);
    if(fd < 0)
        return 2;   /* kernel already handed init /dev/console as 0/1/2 */
    return (i32)fd;
}

static void *MapShared(usize bytes)
{
    void *p = SysMmap(NULL, bytes, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if((isize)(uintptr_t)p < 0 && (isize)(uintptr_t)p > -4096)
        return NULL;
    return p;
}

static void *MapStatus(bool bRunMounted)
{
    if(bRunMounted)
    {
        isize fd = SysOpen(STATUS_PATH,
                           O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
        if(fd >= 0)
        {
            if(SysFtruncate((i32)fd, (i64)sizeof(StatusBlock)) >= 0)
            {
                void *p = SysMmap(NULL, sizeof(StatusBlock), PROT_READ | PROT_WRITE,
                                  MAP_SHARED, (i32)fd, 0);
                SysClose((i32)fd);
                if(!((isize)(uintptr_t)p < 0 && (isize)(uintptr_t)p > -4096))
                    return p;
            }
            else
            {
                SysClose((i32)fd);
            }
            SysUnlink(STATUS_PATH);
        }
    }
    void *p = MapShared(sizeof(StatusBlock));
    if(p != NULL)
        LogF("status: using anonymous fallback");
    return p;
}

void StatusInit(StatusBlock *status, StatusSnapshot *snapshot)
{
    memset(status, 0, sizeof(*status));
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->magic = STATUS_MAGIC;
    snapshot->version = STATUS_VERSION;
    snapshot->entrySize = (u32)sizeof(StatusEntry);
    snapshot->bootNs = SysBootNs();
    StatusPublish(status, snapshot);
}

/* --------------------------------------------------------------- shutdown */

static void RemountReadOnly(Arena *arena)
{
    typedef struct
    {
        char *path;
        usize depth;
    } MountPoint;

    usize arenaMark = ArenaMark(arena);
    isize fd = SysOpen("/proc/self/mounts", O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
    {
        SysMount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
        return;
    }

    char *mounts = NULL;
    usize mountsLen = 0;
    bool bComplete = true;
    for(;;)
    {
        usize chunkMark = ArenaMark(arena);
        char *chunk = (char *)ArenaAlloc(arena, 4096, 1);
        if(chunk == NULL)
        {
            bComplete = false;
            break;
        }

        isize n = SysRead((i32)fd, chunk, 4096);
        if(n == -EINTR)
        {
            ArenaReset(arena, chunkMark);
            continue;
        }
        if(n < 0)
        {
            ArenaReset(arena, chunkMark);
            bComplete = false;
            break;
        }
        if(n == 0)
        {
            ArenaReset(arena, chunkMark);
            break;
        }

        if(mounts == NULL)
            mounts = chunk;
        mountsLen += (usize)n;
        ArenaReset(arena, chunkMark + (usize)n);
    }
    SysClose((i32)fd);

    char *terminator = (char *)ArenaAlloc(arena, 1, 1);
    if(!bComplete || mounts == NULL || terminator == NULL)
    {
        ArenaReset(arena, arenaMark);
        SysMount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
        return;
    }
    *terminator = '\0';

    MountPoint *point = NULL;
    usize count = 0;
    usize i = 0;

    while(i < mountsLen)
    {
        usize lineStart = i;
        while(i < mountsLen && mounts[i] != '\n')
            i++;
        if(i < mountsLen)
            mounts[i++] = '\0';

        char *field[3] = { NULL, NULL, NULL };
        usize f = 0;
        char *p = &mounts[lineStart];
        field[f++] = p;
        while(*p != '\0')
        {
            if(*p == ' ')
            {
                *p = '\0';
                if(f >= 3)
                    break;
                field[f++] = p + 1;
            }
            p++;
        }
        if(f < 3)
            continue;

        const char *fs = field[2];
        if(StrEq(fs, "proc") || StrEq(fs, "sysfs") || StrEq(fs, "devtmpfs") ||
           StrEq(fs, "tmpfs") || StrEq(fs, "devpts") || StrEq(fs, "cgroup") ||
           StrEq(fs, "cgroup2") || StrEq(fs, "debugfs") || StrEq(fs, "rootfs"))
            continue;

        char *path = field[1];
        usize read = 0;
        usize write = 0;
        while(path[read] != '\0')
        {
            if(path[read] == '\\' && path[read + 1] >= '0' && path[read + 1] <= '7' &&
               path[read + 2] >= '0' && path[read + 2] <= '7' &&
               path[read + 3] >= '0' && path[read + 3] <= '7')
            {
                path[write++] = (char)(((path[read + 1] - '0') << 6) |
                                       ((path[read + 2] - '0') << 3) |
                                       (path[read + 3] - '0'));
                read += 4;
            }
            else
            {
                path[write++] = path[read++];
            }
        }
        path[write] = '\0';

        MountPoint *entry = (MountPoint *)ArenaAlloc(arena, sizeof(*entry),
                                                      sizeof(void *));
        if(entry == NULL)
        {
            ArenaReset(arena, arenaMark);
            SysMount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
            return;
        }
        if(point == NULL)
            point = entry;
        entry->path = path;
        entry->depth = 0;
        for(usize k = 0; path[k] != '\0'; k++)
        {
            if(path[k] == '/' && path[k + 1] != '\0')
                entry->depth++;
        }
        count++;
    }

    if(count == 0)
    {
        ArenaReset(arena, arenaMark);
        SysMount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
        return;
    }

    for(usize k = 1; k < count; k++)
    {
        MountPoint current = point[k];
        usize pos = k;
        while(pos > 0 && point[pos - 1].depth < current.depth)
        {
            point[pos] = point[pos - 1];
            pos--;
        }
        point[pos] = current;
    }

    for(usize k = 0; k < count; k++)
        SysMount(NULL, point[k].path, NULL, MS_REMOUNT | MS_RDONLY, NULL);
}

static void ShutdownBegin(InitState *st, i32 sig, u64 nowNs)
{
    st->bShutdown = true;
    st->bKillSent = false;
    st->shutdownNs = nowNs;
    st->shutdownDeadlineNs = nowNs + CFG_SHUTDOWN_GRACE_NS;
    st->shutdownCmd = (sig == SIGUSR2) ? LINUX_REBOOT_CMD_POWER_OFF
                                       : LINUX_REBOOT_CMD_RESTART;
    LogF("shutdown requested by signal %d", sig);
    TaskSignalAll(st, SIGTERM);

    /* the writer drains and flushes on this rather than dying mid-buffer */
    if(st->ring != NULL)
        __atomic_store_n(&st->ring->control, LOG_CTL_SHUTDOWN, __ATOMIC_RELEASE);
    TaskPublish(st);
}

static NORETURN void ShutdownFinish(InitState *st)
{
    LogF("syncing");
    SysSync();
    WdogClose(st);
    RemountReadOnly(&st->arena);
    SysSync();

    SysReboot(st->shutdownCmd);

    /* reboot only returns on failure */
    InitPanic("reboot syscall failed");
}

/* ------------------------------------------------------------- event loop */

static void ReapAll(InitState *st, u64 nowNs)
{
    for(;;)
    {
        i32 status = 0;
        isize pid = SysWait4(-1, &status, WNOHANG);
        if(pid <= 0)
            break;

        i32 p = (i32)pid;
        if(TaskReap(st, p, status, nowNs))
            continue;
        if(ProbeReap(st, p, status, nowNs))
            continue;
        if(LogdReap(st, p, status, nowNs))
            continue;
        /* an orphan reparented to PID 1 that init never spawned */
    }
}

static u64 NextDeadline(InitState *st, u64 nowNs)
{
    u64 best = TaskNextDeadline(st, nowNs);
    if(st->bShutdown && st->shutdownDeadlineNs < best)
        best = st->shutdownDeadlineNs;
#if FEATURE_LOG_DISK
    if(!st->bShutdown && st->logdPid <= 0 && st->logdNextSpawnNs < best)
        best = st->logdNextSpawnNs;
#endif
    return best;
}

static void EventLoop(InitState *st, const KSigSet *unblocked)
{
    KPollFd fds[CFG_MAX_TASKS * 2 + 1];
    Task *owner[CFG_MAX_TASKS * 2 + 1];

    for(;;)
    {
        u64 nowNs = SysBootNs();

        if(G_SIG_CHLD != 0)
        {
            G_SIG_CHLD = 0;
            ReapAll(st, nowNs);
        }

        if(G_SIG_SHUTDOWN != 0 && !st->bShutdown)
        {
            i32 sig = G_SIG_SHUTDOWN;
            G_SIG_SHUTDOWN = 0;
            ShutdownBegin(st, sig, nowNs);
        }

        if(st->bShutdown)
        {
            if(!TaskAnyAlive(st) && st->logdPid <= 0)
                ShutdownFinish(st);
            if(!st->bKillSent && nowNs >= st->shutdownDeadlineNs)
            {
                LogF("grace period expired, sending SIGKILL");
                TaskSignalAll(st, SIGKILL);
                if(st->logdPid > 0)
                    SysKill(st->logdPid, SIGKILL);
                st->bKillSent = true;
                st->shutdownDeadlineNs = nowNs + CFG_SHUTDOWN_GRACE_NS;
            }
            else if(st->bKillSent && nowNs >= st->shutdownDeadlineNs)
            {
                LogF("children will not die, rebooting anyway");
                ShutdownFinish(st);
            }
        }
        else
        {
            TaskTick(st, nowNs);
            SntpTick(st, nowNs);
            LogdSupervise(st, nowNs);
            LogdCheckStall(st, nowNs);
            WdogTick(st, nowNs);
        }

        TaskPublish(st);

        usize nfds = 0;
        for(usize i = 0; i < st->taskCount; i++)
        {
            Task *t = &st->task[i];
            if(t->outFd >= 0)
            {
                owner[nfds] = t;
                fds[nfds].fd = t->outFd;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
            }
            if(t->errFd >= 0)
            {
                owner[nfds] = t;
                fds[nfds].fd = t->errFd;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
            }
        }

        usize sntpSlot = (usize)-1;
        if(st->sntpFd >= 0)
        {
            sntpSlot = nfds;
            owner[nfds] = NULL;
            fds[nfds].fd = st->sntpFd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        u64 deadline = NextDeadline(st, nowNs);
        u64 waitNs = deadline > nowNs ? deadline - nowNs : 0;
        if(waitNs > CFG_LOOP_MAX_WAIT_NS)
            waitNs = CFG_LOOP_MAX_WAIT_NS;

        KTimeSpec ts;
        ts.sec = (i64)(waitNs / NS_PER_SEC);
        ts.nsec = (i64)(waitNs % NS_PER_SEC);

        isize r = SysPpoll(nfds > 0 ? fds : NULL, nfds, &ts, unblocked);
        if(r <= 0)
            continue;

        for(usize i = 0; i < nfds; i++)
        {
            if(fds[i].revents == 0)
                continue;
            if(i == sntpSlot)
            {
                SntpHandleReply(st, SysBootNs());
                continue;
            }
            /* Drain even on POLLHUP. A child blocked writing to a pipe nobody
             * reads hangs while still looking alive to wait4. */
            TaskDrain(st, owner[i]);
        }
    }
}

/* -------------------------------------------------------------------- boot */

void InitMain(void)
{
    InitState *st = &G_INIT;
    memset(st, 0, sizeof(*st));
    st->wdogFd = -1;
    st->sntpFd = -1;
    st->consoleFd = 2;

    SysUmask(0022);

    bool bRunMounted = MountEarly();
    st->consoleFd = OpenConsole();

    void *ringMem = MapShared(CFG_LOG_RING_BYTES);
    if(ringMem != NULL && RingInit((LogRing *)ringMem, CFG_LOG_RING_BYTES))
        st->ring = (LogRing *)ringMem;

    LogAttach(st->ring, st->consoleFd);
    LogF("init " INIT_ARCH_NAME " starting, pid %d", (i32)SysGetPid());

    void *statusMem = MapStatus(bRunMounted);
    if(statusMem != NULL)
    {
        st->status = (StatusBlock *)statusMem;
        StatusInit(st->status, &st->statusSnapshot);
    }

    ArenaInit(&st->arena, G_ARENA, sizeof(G_ARENA));

    KSigSet unblocked;
    SignalSetup(&unblocked);

    /* forked before the remaining boot steps so their failures get captured */
    LogdSupervise(st, SysBootNs());

    WdogOpen(st);

    TaskScanAll(st);

    st->sntpNextNs = SysBootNs();

    EventLoop(st, &unblocked);
}

#endif /* !INIT_FIXTURE */
#endif /* !INIT_HOSTED */
#endif /* !INIT_ABI_ONLY */
