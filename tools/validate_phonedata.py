#!/usr/bin/env python3
"""校验 phone.dat 的结构完整性。

这个脚本不依赖手机号库本身，可以独立验证数据文件是否被正确生成/传输：

  python tools/validate_phonedata.py phonedata/phone.dat
  python tools/validate_phonedata.py phonedata/phone.dat --expect-count 364528 --expect-version 1707

校验项：
  1. 文件长度 >= 8，能容纳文件头
  2. 首个索引偏移落在 (8, 文件长度) 区间内
  3. 索引区长度是 9 的整数倍
  4. 索引按 7 位号段严格递增（无重复、无乱序）
  5. 每条索引的记录偏移都落在记录区内
  6. 每条记录都以 NUL 结束、且恰好 4 个 '|' 分隔字段
  7. 卡片类型编码在 1..6 之间

任一项失败时返回非 0。
"""

from __future__ import annotations

import argparse
import struct
import sys

HEADER_SIZE = 8
INDEX_SIZE = 9
RECORD_FIELDS = 4
VALID_TYPES = range(1, 7)


def fail(message: str) -> int:
    print(f"FAIL: {message}")
    return 1


def validate(path: str, expect_count: int | None, expect_version: str | None) -> int:
    try:
        with open(path, "rb") as handle:
            blob = handle.read()
    except OSError as exc:
        return fail(f"cannot read {path}: {exc}")

    if len(blob) < HEADER_SIZE:
        return fail(f"file is {len(blob)} bytes, smaller than the {HEADER_SIZE}-byte header")

    version_bytes = blob[:4]
    try:
        version = version_bytes.decode("ascii")
    except UnicodeDecodeError:
        return fail(f"version field is not ASCII: {version_bytes!r}")

    index_offset = struct.unpack("<I", blob[4:HEADER_SIZE])[0]

    if not HEADER_SIZE <= index_offset < len(blob):
        return fail(f"first index offset {index_offset} is outside ({HEADER_SIZE}, {len(blob)})")
    if (len(blob) - index_offset) % INDEX_SIZE != 0:
        return fail(
            f"index region length {len(blob) - index_offset} is not a multiple of {INDEX_SIZE}"
        )

    count = (len(blob) - index_offset) // INDEX_SIZE
    if count == 0:
        return fail("index region is empty")

    previous = -1
    checked_records = 0
    type_counts: dict[int, int] = {}

    for i in range(count):
        base = index_offset + i * INDEX_SIZE
        phone, record_offset, card_type = struct.unpack("<IIB", blob[base : base + INDEX_SIZE])

        if phone <= previous:
            return fail(f"index {i}: phone {phone} is not greater than previous {previous}")
        previous = phone

        if not HEADER_SIZE <= record_offset < index_offset:
            return fail(
                f"index {i}: record offset {record_offset} is outside "
                f"[{HEADER_SIZE}, {index_offset})"
            )

        if card_type not in VALID_TYPES:
            return fail(f"index {i}: card type {card_type} is not in {tuple(VALID_TYPES)}")
        type_counts[card_type] = type_counts.get(card_type, 0) + 1

        terminator = blob.find(b"\x00", record_offset, index_offset)
        if terminator < 0:
            return fail(f"index {i}: record at {record_offset} is not NUL-terminated")
        content = blob[record_offset:terminator]
        if content.count(b"|") != RECORD_FIELDS - 1:
            return fail(
                f"index {i}: record {content!r} has {content.count(b'|')} separators, "
                f"expected {RECORD_FIELDS - 1}"
            )
        checked_records += 1

    if expect_count is not None and count != expect_count:
        return fail(f"index count {count} != expected {expect_count}")
    if expect_version is not None and version != expect_version:
        return fail(f"version {version!r} != expected {expect_version!r}")

    print(f"OK: {path}")
    print(f"  version       : {version}")
    print(f"  index offset  : {index_offset}")
    print(f"  records       : {count}")
    print(f"  phone range   : {first_key(blob, index_offset)} .. {last_key(blob, index_offset)}")
    print(f"  record region : {index_offset - HEADER_SIZE} bytes for {checked_records} records")
    print(f"  card types    : {dict(sorted(type_counts.items()))}")
    return 0


def first_key(blob: bytes, index_offset: int) -> int:
    return struct.unpack("<I", blob[index_offset : index_offset + 4])[0]


def last_key(blob: bytes, index_offset: int) -> int:
    count = (len(blob) - index_offset) // INDEX_SIZE
    base = index_offset + (count - 1) * INDEX_SIZE
    return struct.unpack("<I", blob[base : base + 4])[0]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Validate the structure of phone.dat")
    parser.add_argument("path", help="path to phone.dat")
    parser.add_argument("--expect-count", type=int, default=None, help="expected index count")
    parser.add_argument("--expect-version", default=None, help="expected 4-byte version, e.g. 1707")
    args = parser.parse_args(argv)
    return validate(args.path, args.expect_count, args.expect_version)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
