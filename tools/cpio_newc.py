#!/usr/bin/env python3
"""Pack a directory as uncompressed newc without cpio(1)."""

import os
import sys

MAGIC = b"070701"


def field(value):
    return b"%08X" % (value & 0xFFFFFFFF)


def pad4(stream, written):
    rem = written % 4
    if rem:
        stream.write(b"\0" * (4 - rem))
    return (4 - rem) if rem else 0


def entry(stream, name, mode, data, ino):
    raw = name.encode("utf-8") + b"\0"
    header = (
        MAGIC
        + field(ino)
        + field(mode)
        + field(0)  # uid
        + field(0)  # gid
        + field(1)  # nlink
        + field(0)  # mtime
        + field(len(data))
        + field(0) + field(0)  # dev major, minor
        + field(0) + field(0)  # rdev major, minor
        + field(len(raw))
        + field(0)  # check
    )
    stream.write(header)
    stream.write(raw)
    pad4(stream, len(header) + len(raw))
    if data:
        stream.write(data)
        pad4(stream, len(data))


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: cpio_newc.py <staging-dir> <output.cpio>")

    root, out = sys.argv[1], sys.argv[2]
    ino = 1

    with open(out, "wb") as f:
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames.sort()
            filenames.sort()
            rel = os.path.relpath(dirpath, root)
            if rel != ".":
                entry(f, rel.replace(os.sep, "/"), 0o040755, b"", ino)
                ino += 1
            for name in filenames:
                full = os.path.join(dirpath, name)
                relname = os.path.relpath(full, root).replace(os.sep, "/")
                with open(full, "rb") as src:
                    data = src.read()
                mode = 0o100755 if os.access(full, os.X_OK) else 0o100644
                entry(f, relname, mode, data, ino)
                ino += 1

        entry(f, "TRAILER!!!", 0, b"", ino)

    print("wrote %s (%d bytes)" % (out, os.path.getsize(out)))


if __name__ == "__main__":
    main()
