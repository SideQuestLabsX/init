/* SOCK_DGRAM is absent from installed UAPI headers, MIPS runtime coverage is open */

#include <stddef.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <linux/fcntl.h>
#include <linux/capability.h>
#include <linux/mount.h>
#include <linux/mman.h>
#include <linux/prctl.h>
#include <linux/reboot.h>
#include <linux/time.h>
#include <linux/watchdog.h>
#include <asm/signal.h>

/* Preserve UAPI values before init.c redefines their names */
enum
{
    K_O_NONBLOCK  = O_NONBLOCK,
    K_O_CLOEXEC   = O_CLOEXEC,
    K_O_DIRECTORY = O_DIRECTORY,
    K_O_NOFOLLOW  = O_NOFOLLOW,
    K_O_CREAT     = O_CREAT,
    K_O_APPEND    = O_APPEND,
    K_O_TRUNC     = O_TRUNC,
    K_O_EXCL      = O_EXCL,
    K_O_NOCTTY    = O_NOCTTY,
    K_AT_FDCWD    = AT_FDCWD,
    K_MS_RDONLY   = MS_RDONLY,
    K_MS_REMOUNT  = MS_REMOUNT,
    K_MS_NOSUID   = MS_NOSUID,
    K_MS_NODEV    = MS_NODEV,
    K_MS_NOEXEC   = MS_NOEXEC,
    K_PROT_READ   = PROT_READ,
    K_PROT_WRITE  = PROT_WRITE,
    K_MAP_SHARED  = MAP_SHARED,
    K_MAP_PRIVATE = MAP_PRIVATE,
    K_MAP_ANON    = MAP_ANONYMOUS,
    K_MAP_FIXED   = MAP_FIXED,
    K_SIGHUP      = SIGHUP,
    K_SIGKILL     = SIGKILL,
    K_SIGUSR1     = SIGUSR1,
    K_SIGUSR2     = SIGUSR2,
    K_SIGTERM     = SIGTERM,
    K_SIGCHLD     = SIGCHLD,
    K_SIGCONT     = SIGCONT,
    K_SIGSTOP     = SIGSTOP,
    K_SIGTSTP     = SIGTSTP,
    K_SIG_BLOCK   = SIG_BLOCK,
    K_SIG_UNBLOCK = SIG_UNBLOCK,
    K_SIG_SETMASK = SIG_SETMASK,
    K_SA_RESTART  = (int)SA_RESTART,
    K_SA_SIGINFO  = (int)SA_SIGINFO,
    K_SA_NOCLDSTOP = (int)SA_NOCLDSTOP,
    K_ENOSYS      = ENOSYS,
    K_ENOTEMPTY   = ENOTEMPTY,
    K_ELOOP       = ELOOP,
    K_ETIMEDOUT   = ETIMEDOUT,
    K_EAGAIN      = EAGAIN,
    K_ECHILD      = ECHILD,
    K_EINTR       = EINTR,
#ifdef SA_RESTORER
    K_SA_RESTORER = (int)SA_RESTORER,
    K_HAS_RESTORER = 1,
#else
    K_HAS_RESTORER = 0,
#endif
    K_PR_SET_NAME     = PR_SET_NAME,
    K_PR_SET_KEEPCAPS = PR_SET_KEEPCAPS,
    K_PR_CAPBSET_DROP = PR_CAPBSET_DROP,
    K_PR_SET_NNP      = PR_SET_NO_NEW_PRIVS,
    K_PR_CAP_AMBIENT  = PR_CAP_AMBIENT,
    K_PR_CAP_AMB_RAISE = PR_CAP_AMBIENT_RAISE,
    K_CAP_LAST_CAP    = CAP_LAST_CAP,
    K_CLOCK_REALTIME  = CLOCK_REALTIME,
    K_CLOCK_BOOTTIME  = CLOCK_BOOTTIME,
};

static const unsigned long long K_REBOOT_MAGIC1 = (unsigned long long)LINUX_REBOOT_MAGIC1;
static const unsigned long long K_REBOOT_MAGIC2 = (unsigned long long)LINUX_REBOOT_MAGIC2;
static const unsigned long long K_REBOOT_RESTART = (unsigned long long)LINUX_REBOOT_CMD_RESTART;
static const unsigned long long K_REBOOT_POWEROFF = (unsigned long long)LINUX_REBOOT_CMD_POWER_OFF;
static const unsigned long long K_WDIOC_KEEPALIVE = (unsigned long long)WDIOC_KEEPALIVE;
static const unsigned long long K_WDIOC_SETTIMEOUT = (unsigned long long)WDIOC_SETTIMEOUT;

#if defined(__i386__) || defined(__arm__)
typedef struct
{
    unsigned long handler;
    unsigned long flags;
    unsigned long restorer;
    unsigned long mask[2];
} KCompatSigAction;

typedef struct
{
    unsigned long bits[2];
} KCompatSigSet;

enum
{
    K_SIGSET_BYTES = sizeof(KCompatSigSet),
    K_SIGACTION_SIZE = sizeof(KCompatSigAction),
    K_SIGACTION_FLAGS_OFFSET = offsetof(KCompatSigAction, flags),
    K_SIGACTION_HANDLER_OFFSET = offsetof(KCompatSigAction, handler),
    K_SIGACTION_MASK_OFFSET = offsetof(KCompatSigAction, mask),
};
#else
enum
{
    K_SIGSET_BYTES = sizeof(sigset_t),
    K_SIGACTION_SIZE = sizeof(struct sigaction),
    K_SIGACTION_FLAGS_OFFSET = offsetof(struct sigaction, sa_flags),
    K_SIGACTION_HANDLER_OFFSET = offsetof(struct sigaction, sa_handler),
    K_SIGACTION_MASK_OFFSET = offsetof(struct sigaction, sa_mask),
};
#endif

#undef O_NONBLOCK
#undef O_CLOEXEC
#undef O_DIRECTORY
#undef O_NOFOLLOW
#undef O_CREAT
#undef O_APPEND
#undef O_EXCL
#undef O_NOCTTY
#undef O_TRUNC
#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef AT_FDCWD
#undef MS_RDONLY
#undef MS_REMOUNT
#undef MS_NOSUID
#undef MS_NODEV
#undef MS_NOEXEC
#undef MS_NOATIME
#undef MS_BIND
#undef PROT_READ
#undef PROT_WRITE
#undef PROT_NONE
#undef MAP_SHARED
#undef MAP_PRIVATE
#undef MAP_ANONYMOUS
#undef MAP_FIXED
#undef SIGHUP
#undef SIGINT
#undef SIGQUIT
#undef SIGILL
#undef SIGABRT
#undef SIGKILL
#undef SIGUSR1
#undef SIGSEGV
#undef SIGUSR2
#undef SIGPIPE
#undef SIGALRM
#undef SIGTERM
#undef SIGCHLD
#undef SIGCONT
#undef SIGSTOP
#undef SIGTSTP
#undef SA_RESTART
#undef SA_RESTORER
#undef SA_NOCLDSTOP
#undef SA_SIGINFO
#undef SIG_BLOCK
#undef SIG_UNBLOCK
#undef SIG_SETMASK
#undef PR_SET_NAME
#undef PR_SET_KEEPCAPS
#undef PR_CAPBSET_DROP
#undef PR_SET_NO_NEW_PRIVS
#undef PR_SET_PDEATHSIG
#undef PR_CAP_AMBIENT
#undef PR_CAP_AMBIENT_RAISE
#undef PR_CAP_AMBIENT_CLEAR_ALL
#undef CAP_LAST_CAP
#undef CLOCK_REALTIME
#undef CLOCK_MONOTONIC
#undef CLOCK_BOOTTIME
#undef LINUX_REBOOT_MAGIC1
#undef LINUX_REBOOT_MAGIC2
#undef LINUX_REBOOT_CMD_RESTART
#undef LINUX_REBOOT_CMD_HALT
#undef LINUX_REBOOT_CMD_POWER_OFF
#undef WDIOC_KEEPALIVE
#undef WDIOC_SETTIMEOUT
#undef SEEK_SET
#undef SEEK_END
#undef F_GETFL
#undef F_SETFL
#undef EPERM
#undef ENOENT
#undef EINTR
#undef EIO
#undef ENXIO
#undef EBADF
#undef ECHILD
#undef EAGAIN
#undef ENOMEM
#undef EACCES
#undef EFAULT
#undef EBUSY
#undef EEXIST
#undef ENODEV
#undef ENOTDIR
#undef EISDIR
#undef EINVAL
#undef ENFILE
#undef EMFILE
#undef ENOSPC
#undef ESPIPE
#undef EROFS
#undef ERANGE
#undef ENOSYS
#undef ENOTEMPTY
#undef ELOOP
#undef ETIMEDOUT

#include "init.c"

#define SAME(ours, theirs) _Static_assert((ours) == (theirs), #ours " != " #theirs)

SAME(KSIGSET_BYTES, K_SIGSET_BYTES);
SAME(sizeof(KSigAction), K_SIGACTION_SIZE);
SAME(offsetof(KSigAction, flags), K_SIGACTION_FLAGS_OFFSET);
SAME(offsetof(KSigAction, handler), K_SIGACTION_HANDLER_OFFSET);
SAME(offsetof(KSigAction, mask), K_SIGACTION_MASK_OFFSET);

SAME(SYS_read, __NR_read);
SAME(SYS_write, __NR_write);
SAME(SYS_close, __NR_close);
SAME(SYS_lseek, __NR_lseek);
SAME(SYS_munmap, __NR_munmap);
SAME(SYS_ioctl, __NR_ioctl);
SAME(SYS_sched_yield, __NR_sched_yield);
SAME(SYS_dup3, __NR_dup3);
SAME(SYS_fcntl, __NR_fcntl);
SAME(SYS_getpid, __NR_getpid);
SAME(SYS_socket, __NR_socket);
SAME(SYS_connect, __NR_connect);
SAME(SYS_bind, __NR_bind);
SAME(SYS_sendto, __NR_sendto);
SAME(SYS_recvfrom, __NR_recvfrom);
SAME(SYS_setsockopt, __NR_setsockopt);
SAME(SYS_clone, __NR_clone);
SAME(SYS_execve, __NR_execve);
SAME(SYS_exit, __NR_exit);
SAME(SYS_exit_group, __NR_exit_group);
SAME(SYS_wait4, __NR_wait4);
SAME(SYS_kill, __NR_kill);
SAME(SYS_fsync, __NR_fsync);
SAME(SYS_ftruncate, __NR_ftruncate);
SAME(SYS_getdents64, __NR_getdents64);
SAME(SYS_chdir, __NR_chdir);
SAME(SYS_umask, __NR_umask);
SAME(SYS_setsid, __NR_setsid);
SAME(SYS_capset, __NR_capset);
SAME(SYS_prctl, __NR_prctl);
SAME(SYS_sync, __NR_sync);
SAME(SYS_mount, __NR_mount);
SAME(SYS_umount2, __NR_umount2);
SAME(SYS_reboot, __NR_reboot);
SAME(SYS_openat, __NR_openat);
SAME(SYS_mkdirat, __NR_mkdirat);
SAME(SYS_unlinkat, __NR_unlinkat);
#ifdef SYS_renameat2
SAME(SYS_renameat2, __NR_renameat2);
#else
SAME(SYS_renameat, __NR_renameat);
#endif
SAME(SYS_faccessat, __NR_faccessat);
SAME(SYS_pipe2, __NR_pipe2);
SAME(SYS_getrandom, __NR_getrandom);
SAME(SYS_rt_sigaction, __NR_rt_sigaction);
SAME(SYS_rt_sigprocmask, __NR_rt_sigprocmask);
SAME(SYS_rt_sigreturn, __NR_rt_sigreturn);

#ifdef INIT_MMAP2
SAME(SYS_mmap2, __NR_mmap2);
#else
SAME(SYS_mmap, __NR_mmap);
#endif

/* 32-bit targets use time64 syscalls */
#ifdef INIT_TIME32_ABI
SAME(SYS_clock_gettime64, __NR_clock_gettime64);
SAME(SYS_clock_settime64, __NR_clock_settime64);
SAME(SYS_ppoll_time64, __NR_ppoll_time64);
#else
SAME(SYS_clock_gettime, __NR_clock_gettime);
SAME(SYS_clock_settime, __NR_clock_settime);
SAME(SYS_ppoll, __NR_ppoll);
#endif

/* MIPS uses time32 but never needed uid32 syscalls */
#ifdef INIT_UID32_ABI
SAME(SYS_setuid, __NR_setuid32);
SAME(SYS_setgid, __NR_setgid32);
SAME(SYS_setgroups, __NR_setgroups32);
#else
SAME(SYS_setuid, __NR_setuid);
SAME(SYS_setgid, __NR_setgid);
SAME(SYS_setgroups, __NR_setgroups);
#endif

SAME(O_NONBLOCK, K_O_NONBLOCK);
SAME(O_CLOEXEC, K_O_CLOEXEC);
SAME(O_DIRECTORY, K_O_DIRECTORY);   /* ARM32 differs */
SAME(O_NOFOLLOW, K_O_NOFOLLOW);
SAME(O_CREAT, K_O_CREAT);
SAME(O_APPEND, K_O_APPEND);
SAME(O_TRUNC, K_O_TRUNC);
SAME(O_EXCL, K_O_EXCL);
SAME(O_NOCTTY, K_O_NOCTTY);
SAME(AT_FDCWD, K_AT_FDCWD);

SAME(MS_RDONLY, K_MS_RDONLY);
SAME(MS_REMOUNT, K_MS_REMOUNT);
SAME(MS_NOSUID, K_MS_NOSUID);
SAME(MS_NODEV, K_MS_NODEV);
SAME(MS_NOEXEC, K_MS_NOEXEC);

SAME(PROT_READ, K_PROT_READ);
SAME(PROT_WRITE, K_PROT_WRITE);
SAME(MAP_SHARED, K_MAP_SHARED);
SAME(MAP_PRIVATE, K_MAP_PRIVATE);
SAME(MAP_ANONYMOUS, K_MAP_ANON);
SAME(MAP_FIXED, K_MAP_FIXED);

SAME(SIGHUP, K_SIGHUP);
SAME(SIGKILL, K_SIGKILL);
SAME(SIGUSR1, K_SIGUSR1);
SAME(SIGUSR2, K_SIGUSR2);
SAME(SIGTERM, K_SIGTERM);
SAME(SIGCHLD, K_SIGCHLD);
SAME(SIGCONT, K_SIGCONT);
SAME(SIGSTOP, K_SIGSTOP);
SAME(SIGTSTP, K_SIGTSTP);
SAME(SIG_BLOCK, K_SIG_BLOCK);
SAME(SIG_UNBLOCK, K_SIG_UNBLOCK);
SAME(SIG_SETMASK, K_SIG_SETMASK);
SAME(SA_RESTART, (unsigned)K_SA_RESTART);
SAME(SA_SIGINFO, (unsigned)K_SA_SIGINFO);
SAME(SA_NOCLDSTOP, (unsigned)K_SA_NOCLDSTOP);

SAME(EINTR, K_EINTR);
SAME(EAGAIN, K_EAGAIN);
SAME(ECHILD, K_ECHILD);
SAME(ENOSYS, K_ENOSYS);
SAME(ENOTEMPTY, K_ENOTEMPTY);
SAME(ELOOP, K_ELOOP);
SAME(ETIMEDOUT, K_ETIMEDOUT);

#ifdef INIT_HAS_SA_RESTORER
_Static_assert(K_HAS_RESTORER == 1, "arch defines SA_RESTORER but we do not");
SAME(SA_RESTORER, (unsigned)K_SA_RESTORER);
#else
_Static_assert(K_HAS_RESTORER == 0, "arch has no SA_RESTORER but we assume one");
#endif

SAME(PR_SET_NAME, K_PR_SET_NAME);
SAME(PR_SET_KEEPCAPS, K_PR_SET_KEEPCAPS);
SAME(PR_CAPBSET_DROP, K_PR_CAPBSET_DROP);
SAME(PR_SET_NO_NEW_PRIVS, K_PR_SET_NNP);
SAME(PR_CAP_AMBIENT, K_PR_CAP_AMBIENT);
SAME(PR_CAP_AMBIENT_RAISE, K_PR_CAP_AMB_RAISE);
SAME(CAP_LAST_CAP, K_CAP_LAST_CAP);

SAME(CLOCK_REALTIME, K_CLOCK_REALTIME);
SAME(CLOCK_BOOTTIME, K_CLOCK_BOOTTIME);

/* These constants exceed enum width on 32-bit targets */
int AbiCheckWide(void);
int AbiCheckWide(void)
{
    return (LINUX_REBOOT_MAGIC1 == K_REBOOT_MAGIC1) &&
           (LINUX_REBOOT_MAGIC2 == K_REBOOT_MAGIC2) &&
           (LINUX_REBOOT_CMD_RESTART == K_REBOOT_RESTART) &&
           (LINUX_REBOOT_CMD_POWER_OFF == K_REBOOT_POWEROFF) &&
           (WDIOC_KEEPALIVE == K_WDIOC_KEEPALIVE) &&
           (WDIOC_SETTIMEOUT == K_WDIOC_SETTIMEOUT);
}
