static const MountSpec MOUNT_TABLE[] =
{
    { "devtmpfs", "/dev", "devtmpfs", MS_NOSUID, NULL, false },
    { "proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL, false },
    { "sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL, false },
    { "tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL, false },
    { "mount-test", "/mount-test/parent/leaf", "tmpfs", MS_NOSUID, "mode=0700", true },
    { "mount-test", "/mount-test/optional", "init-no-such-fs", 0, NULL, false },
};
