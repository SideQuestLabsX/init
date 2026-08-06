#!/usr/bin/env python3
"""Read framed init logs without third-party modules"""

import argparse
import binascii
import struct
import sys
from pathlib import Path


FILE_MAGIC = b"INITLOG\0"
FILE_VERSION = 1
FILE_HEADER_BYTES = 32
FILE_HEADER = struct.Struct("<8sHHIQII")
FILE_HEADER_CRC_OFFSET = 28

FRAME_MAGIC = b"IFRM"
FRAME_HEADER_BYTES = 36
FRAME_HEADER = struct.Struct("<4sHBBQIIIII")
FRAME_HEADER_CRC_OFFSET = 28

CODEC_RAW = 0
CODEC_LZ4 = 1
MAX_FRAME_BYTES = 64 * 1024 * 1024


class LogError(Exception):
    def __init__(self, path, offset, message):
        super().__init__(f"{path}:{offset}: {message}")
        self.path = path
        self.offset = offset
        self.message = message


def Crc32(data):
    return binascii.crc32(data) & 0xffffffff


def HeaderCrc(raw, offset):
    checked = bytearray(raw)
    struct.pack_into("<I", checked, offset, 0)
    return Crc32(checked)


def ExpandPaths(names):
    paths = []
    seen = set()
    for name in names:
        path = Path(name)
        suffix = path.name.rsplit(".", 1)[-1] if "." in path.name else ""
        candidates = []
        if not suffix.isdigit():
            for candidate in path.parent.glob(path.name + ".*"):
                candidateSuffix = candidate.name[len(path.name) + 1:]
                if candidate.is_file() and candidateSuffix.isdigit():
                    candidates.append((int(candidateSuffix), candidate))
        for _, candidate in sorted(candidates, reverse=True):
            key = str(candidate)
            if key not in seen:
                seen.add(key)
                paths.append(candidate)
        key = str(path)
        if key not in seen:
            seen.add(key)
            paths.append(path)
    return paths


def MissingRotations(names):
    for name in names:
        path = Path(name)
        suffix = path.name.rsplit(".", 1)[-1] if "." in path.name else ""
        if suffix.isdigit():
            continue
        numbers = []
        for candidate in path.parent.glob(path.name + ".*"):
            candidateSuffix = candidate.name[len(path.name) + 1:]
            if candidate.is_file() and candidateSuffix.isdigit():
                numbers.append(int(candidateSuffix))
        if numbers:
            highest = max(numbers)
            missing = [str(number) for number in range(1, highest + 1)
                       if not (path.parent / f"{path.name}.{number}").is_file()]
            if missing:
                yield path, missing


def CopyLegacy(stream, output):
    total = 0
    while True:
        chunk = stream.read(65536)
        if not chunk:
            break
        total += len(chunk)
        if output is not None:
            output.write(chunk)
    return {"legacy": True, "frames": 0, "decodedBytes": total}


def ReadLz4Length(payload, position, nibble):
    length = nibble
    if nibble != 15:
        return length, position
    while True:
        if position >= len(payload):
            raise ValueError("truncated LZ4 length")
        value = payload[position]
        position += 1
        length += value
        if value != 255:
            return length, position


def DecodeLz4(payload, decodedBytes, maxFrameBytes):
    if decodedBytes > maxFrameBytes:
        raise ValueError(f"decoded LZ4 frame exceeds limit {decodedBytes}")
    output = bytearray()
    position = 0
    while position < len(payload):
        token = payload[position]
        position += 1
        literalLen, position = ReadLz4Length(payload, position, token >> 4)
        literalEnd = position + literalLen
        if literalEnd > len(payload):
            raise ValueError("truncated LZ4 literals")
        output.extend(payload[position:literalEnd])
        position = literalEnd
        if len(output) > decodedBytes:
            raise ValueError("LZ4 output exceeds declared size")
        if position == len(payload):
            break
        if position + 2 > len(payload):
            raise ValueError("truncated LZ4 offset")
        offset = payload[position] | (payload[position + 1] << 8)
        position += 2
        if offset == 0 or offset > len(output):
            raise ValueError("invalid LZ4 offset")
        matchLen, position = ReadLz4Length(payload, position, token & 0x0f)
        matchLen += 4
        if len(output) + matchLen > decodedBytes:
            raise ValueError("LZ4 match exceeds declared size")
        for _ in range(matchLen):
            output.append(output[-offset])
    if len(output) != decodedBytes:
        raise ValueError("LZ4 output size mismatch")
    return bytes(output)


def ReadFramed(path, stream, output, maxFrameBytes):
    rawFileHeader = stream.read(FILE_HEADER_BYTES)
    if len(rawFileHeader) != FILE_HEADER_BYTES:
        raise LogError(path, 0, "truncated file header")

    (magic, version, headerBytes, flags, fileSequence, reserved,
     headerCrc) = FILE_HEADER.unpack(rawFileHeader)
    if magic != FILE_MAGIC:
        raise LogError(path, 0, "invalid file magic")
    if version != FILE_VERSION:
        raise LogError(path, 8, f"unsupported format version {version}")
    if headerBytes != FILE_HEADER_BYTES:
        raise LogError(path, 10, f"invalid file header size {headerBytes}")
    if flags != 0:
        raise LogError(path, 12, f"unknown file flags 0x{flags:x}")
    if reserved != 0:
        raise LogError(path, 24, "nonzero reserved file header field")
    if HeaderCrc(rawFileHeader, FILE_HEADER_CRC_OFFSET) != headerCrc:
        raise LogError(path, 28, "file header checksum mismatch")

    del fileSequence
    expectedSequence = 0
    frameCount = 0
    decodedTotal = 0
    while True:
        frameOffset = stream.tell()
        rawMagic = stream.read(len(FRAME_MAGIC))
        if not rawMagic:
            break
        if len(rawMagic) != len(FRAME_MAGIC):
            raise LogError(path, frameOffset, "truncated frame magic")
        rawHeader = rawMagic + stream.read(FRAME_HEADER_BYTES - len(FRAME_MAGIC))
        if len(rawHeader) != FRAME_HEADER_BYTES:
            raise LogError(path, frameOffset, "truncated frame header")

        (magic, headerBytes, codec, flags, sequence, decodedBytes,
         encodedBytes, payloadCrc, headerCrc, reserved) = FRAME_HEADER.unpack(rawHeader)
        if magic != FRAME_MAGIC:
            raise LogError(path, frameOffset, "invalid frame magic")
        if headerBytes != FRAME_HEADER_BYTES:
            raise LogError(path, frameOffset + 4,
                           f"invalid frame header size {headerBytes}")
        if flags != 0:
            raise LogError(path, frameOffset + 7,
                           f"unknown frame flags 0x{flags:x}")
        if reserved != 0:
            raise LogError(path, frameOffset + 32,
                           "nonzero reserved frame header field")
        if HeaderCrc(rawHeader, FRAME_HEADER_CRC_OFFSET) != headerCrc:
            raise LogError(path, frameOffset + FRAME_HEADER_CRC_OFFSET,
                           "frame header checksum mismatch")
        if sequence != expectedSequence:
            raise LogError(path, frameOffset + 8,
                           f"frame sequence {sequence}, expected {expectedSequence}")
        if decodedBytes > maxFrameBytes:
            raise LogError(path, frameOffset + 16,
                           f"decoded frame exceeds limit {decodedBytes}")
        if encodedBytes > maxFrameBytes:
            raise LogError(path, frameOffset + 20,
                           f"encoded frame exceeds limit {encodedBytes}")

        payload = stream.read(encodedBytes)
        if len(payload) != encodedBytes:
            raise LogError(path, frameOffset + FRAME_HEADER_BYTES,
                           "truncated frame payload")
        if Crc32(payload) != payloadCrc:
            raise LogError(path, frameOffset + 24,
                           "frame payload checksum mismatch")
        if codec == CODEC_RAW:
            if encodedBytes != decodedBytes:
                raise LogError(path, frameOffset + 20,
                               "raw frame size mismatch")
            decoded = payload
        elif codec == CODEC_LZ4:
            try:
                decoded = DecodeLz4(payload, decodedBytes, maxFrameBytes)
            except ValueError as error:
                raise LogError(path, frameOffset + FRAME_HEADER_BYTES, str(error))
        else:
            raise LogError(path, frameOffset + 6,
                           f"unsupported codec {codec}")

        if output is not None:
            output.write(decoded)
        decodedTotal += decodedBytes
        frameCount += 1
        expectedSequence += 1

    return {"legacy": False, "frames": frameCount, "decodedBytes": decodedTotal}


def ReadFile(path, output, maxFrameBytes):
    with path.open("rb") as stream:
        prefix = stream.read(len(FILE_MAGIC))
        stream.seek(0)
        if not prefix:
            return CopyLegacy(stream, output)
        if prefix != FILE_MAGIC:
            if FILE_MAGIC.startswith(prefix):
                raise LogError(path, 0, "truncated file magic")
            return CopyLegacy(stream, output)
        return ReadFramed(path, stream, output, maxFrameBytes)


def PositiveInt(value):
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return parsed


def BuildParser():
    parser = argparse.ArgumentParser(description="read init disk logs")
    commands = parser.add_subparsers(dest="command", required=True)

    verify = commands.add_parser("verify", help="validate logs without output")
    verify.add_argument("--max-frame-bytes", type=PositiveInt,
                        default=MAX_FRAME_BYTES)
    verify.add_argument("paths", nargs="+")

    extract = commands.add_parser("extract", help="stream decoded log bytes")
    extract.add_argument("--max-frame-bytes", type=PositiveInt,
                         default=MAX_FRAME_BYTES)
    extract.add_argument("-o", "--output", default="-",
                         help="output file, or - for stdout")
    extract.add_argument("paths", nargs="+")
    return parser


def Report(path, result):
    if result["legacy"]:
        kind = "legacy"
    else:
        kind = f"framed frames={result['frames']}"
    print(f"OK {path} {kind} bytes={result['decodedBytes']}", file=sys.stderr)


def Main(argv=None):
    parser = BuildParser()
    args = parser.parse_args(argv)
    paths = ExpandPaths(args.paths)
    for path, missing in MissingRotations(args.paths):
        print(f"WARNING {path}: missing rotations {','.join(missing)}", file=sys.stderr)
    output = None
    outputFile = None
    if args.command == "extract":
        if args.output == "-":
            output = getattr(sys.stdout, "buffer", sys.stdout)
        else:
            outputFile = open(args.output, "wb")
            output = outputFile

    failures = 0
    try:
        for path in paths:
            try:
                result = ReadFile(path, None if args.command == "verify" else output,
                                  args.max_frame_bytes)
                Report(path, result)
            except (OSError, LogError) as error:
                print(f"ERROR {error}", file=sys.stderr)
                failures = 1
    finally:
        if outputFile is not None:
            outputFile.close()
    return failures


if __name__ == "__main__":
    sys.exit(Main())
