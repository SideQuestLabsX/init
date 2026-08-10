static const MountSpec MOUNT_TABLE[] =
{
    { "devtmpfs", "/dev", "devtmpfs", MS_NOSUID, NULL, false },
    { "proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL, false },
    { "sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL, false },
    { "tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL, false },
    { "/missing-mount-source", "/mount-test/required/leaf", NULL, MS_BIND, NULL, true },
};
