#!/usr/bin/env python3

import importlib.util
import io
import subprocess
import sys
import tempfile
from pathlib import Path


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
READER_PATH = ROOT / "tools" / "init-log-read.py"
SPEC = importlib.util.spec_from_file_location("init_log_read", READER_PATH)
READER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(READER)


def Check(condition, message):
    if not condition:
        raise AssertionError(message)


def BuildFileHeader(fileSequence=0):
    raw = READER.FILE_HEADER.pack(
        READER.FILE_MAGIC, READER.FILE_VERSION, READER.FILE_HEADER_BYTES,
        0, fileSequence, 0, 0)
    return READER.FILE_HEADER.pack(
        READER.FILE_MAGIC, READER.FILE_VERSION, READER.FILE_HEADER_BYTES,
        0, fileSequence, 0,
        READER.HeaderCrc(raw, READER.FILE_HEADER_CRC_OFFSET))


def BuildFrame(sequence, payload, codec=READER.CODEC_RAW,
              decodedBytes=None, encodedBytes=None, flags=0, reserved=0):
    if decodedBytes is None:
        decodedBytes = len(payload)
    if encodedBytes is None:
        encodedBytes = len(payload)
    raw = READER.FRAME_HEADER.pack(
        READER.FRAME_MAGIC, READER.FRAME_HEADER_BYTES, codec, flags,
        sequence, decodedBytes, encodedBytes, READER.Crc32(payload), 0,
        reserved)
    return READER.FRAME_HEADER.pack(
        READER.FRAME_MAGIC, READER.FRAME_HEADER_BYTES, codec, flags,
        sequence, decodedBytes, encodedBytes, READER.Crc32(payload),
        READER.HeaderCrc(raw, READER.FRAME_HEADER_CRC_OFFSET), reserved) + payload


def WriteBytes(path, data):
    path.write_bytes(data)


def ExpectError(path):
    try:
        READER.ReadFile(path, io.BytesIO(), READER.MAX_FRAME_BYTES)
    except READER.LogError:
        return
    raise AssertionError(f"expected reader failure for {path}")


def TestValidAndLegacy(directory):
    base = directory / "init.log"
    WriteBytes(base, BuildFileHeader() +
               BuildFrame(0, b"new\n") + BuildFrame(1, b"last\n"))
    output = io.BytesIO()
    result = READER.ReadFile(base, output, READER.MAX_FRAME_BYTES)
    Check(not result["legacy"] and result["frames"] == 2,
          "valid frame count")
    Check(output.getvalue() == b"new\nlast\n", "valid extraction")

    legacy = directory / "legacy.log"
    WriteBytes(legacy, b"legacy line\n")
    output = io.BytesIO()
    result = READER.ReadFile(legacy, output, READER.MAX_FRAME_BYTES)
    Check(result["legacy"] and output.getvalue() == b"legacy line\n",
          "legacy extraction")

    lz4 = directory / "lz4.log"
    compressed = bytes((0x32, ord("a"), ord("b"), ord("c"), 3, 0))
    WriteBytes(lz4, BuildFileHeader() +
               BuildFrame(0, compressed, codec=READER.CODEC_LZ4,
                          decodedBytes=9))
    output = io.BytesIO()
    result = READER.ReadFile(lz4, output, READER.MAX_FRAME_BYTES)
    Check(result["frames"] == 1 and output.getvalue() == b"abcabcabc",
          "LZ4 extraction")


def TestRotation(directory):
    base = directory / "rotated.log"
    WriteBytes(Path(str(base) + ".2"), BuildFileHeader() + BuildFrame(0, b"old\n"))
    WriteBytes(Path(str(base) + ".1"), BuildFileHeader() + BuildFrame(0, b"middle\n"))
    WriteBytes(base, BuildFileHeader() + BuildFrame(0, b"new\n"))
    paths = READER.ExpandPaths([str(base)])
    Check(paths == [Path(str(base) + ".2"), Path(str(base) + ".1"), base],
          "rotation order")
    output = io.BytesIO()
    for path in paths:
        READER.ReadFile(path, output, READER.MAX_FRAME_BYTES)
    Check(output.getvalue() == b"old\nmiddle\nnew\n", "rotation extraction")


def TestTruncation(directory):
    full = BuildFileHeader() + BuildFrame(0, b"payload\n")
    for cut in range(len(full) + 1):
        path = directory / f"truncated-{cut}"
        WriteBytes(path, full[:cut])
        failed = False
        try:
            READER.ReadFile(path, io.BytesIO(), READER.MAX_FRAME_BYTES)
        except READER.LogError:
            failed = True
        expected = cut not in (0, READER.FILE_HEADER_BYTES, len(full))
        Check(failed == expected, f"truncation at byte {cut}")


def TestInvalidFrames(directory):
    malformed = directory / "malformed.log"
    WriteBytes(malformed, BuildFileHeader() +
               BuildFrame(0, b"x", encodedBytes=READER.MAX_FRAME_BYTES + 1))
    ExpectError(malformed)

    checksum = directory / "checksum.log"
    data = bytearray(BuildFileHeader() + BuildFrame(0, b"x"))
    data[READER.FILE_HEADER_BYTES + READER.FRAME_HEADER_BYTES] ^= 1
    WriteBytes(checksum, data)
    ExpectError(checksum)

    fileHeaderChecksum = directory / "file-header-checksum.log"
    data = bytearray(BuildFileHeader() + BuildFrame(0, b"x"))
    data[READER.FILE_HEADER_CRC_OFFSET] ^= 1
    WriteBytes(fileHeaderChecksum, data)
    ExpectError(fileHeaderChecksum)

    frameHeaderChecksum = directory / "frame-header-checksum.log"
    data = bytearray(BuildFileHeader() + BuildFrame(0, b"x"))
    data[READER.FILE_HEADER_BYTES + READER.FRAME_HEADER_CRC_OFFSET] ^= 1
    WriteBytes(frameHeaderChecksum, data)
    ExpectError(frameHeaderChecksum)

    unknown = directory / "unknown-codec.log"
    WriteBytes(unknown, BuildFileHeader() + BuildFrame(0, b"x", codec=7))
    ExpectError(unknown)

    gap = directory / "sequence-gap.log"
    WriteBytes(gap, BuildFileHeader() + BuildFrame(0, b"x") + BuildFrame(2, b"y"))
    ExpectError(gap)

    rawSize = directory / "raw-size.log"
    WriteBytes(rawSize, BuildFileHeader() + BuildFrame(0, b"x", decodedBytes=2))
    ExpectError(rawSize)


def TestCommand(directory):
    base = directory / "command.log"
    WriteBytes(base, BuildFileHeader() + BuildFrame(0, b"command\n"))
    verify = subprocess.run(
        [sys.executable, str(READER_PATH), "verify", str(base)],
        capture_output=True, check=False)
    Check(verify.returncode == 0, "verify command")

    outputPath = directory / "extracted.log"
    extract = subprocess.run(
        [sys.executable, str(READER_PATH), "extract", "-o", str(outputPath),
         str(base)], capture_output=True, check=False)
    Check(extract.returncode == 0 and outputPath.read_bytes() == b"command\n",
          "extract command")


def Main():
    with tempfile.TemporaryDirectory() as rawDirectory:
        directory = Path(rawDirectory)
        TestValidAndLegacy(directory)
        TestRotation(directory)
        TestTruncation(directory)
        TestInvalidFrames(directory)
        TestCommand(directory)
    print("log reader tests: ok")


if __name__ == "__main__":
    Main()
