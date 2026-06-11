#!/usr/bin/env python3
"""Build SD-card vector map index files.

Each .idx record is 16 bytes, little-endian:
  int16 minLat, maxLat, minLon, maxLon
  uint32 dataOffset   byte offset in the matching .bin to the first point
  uint16 pointCount   number of lat/lon point pairs in the segment
  uint16 reserved
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


BBOX = 0x7FFE
END = 0x7FFF

LAYERS = (
    "coast",
    "border",
    "state",
    "river",
    "lake",
    "coast_low",
    "border_low",
)


def read_i16(data: bytes, index: int) -> int:
    return struct.unpack_from("<h", data, index * 2)[0]


def build_index(bin_path: Path, idx_path: Path) -> int:
    data = bin_path.read_bytes()
    if len(data) % 2:
        raise ValueError(f"{bin_path} has odd byte length")

    value_count = len(data) // 2
    i = 0
    records = []

    while i < value_count:
        marker = read_i16(data, i)
        i += 1

        if marker == END:
            if i < value_count and read_i16(data, i) == END:
                break
            continue

        if marker != BBOX:
            raise ValueError(f"{bin_path}: expected bbox marker at value {i - 1}")

        if i + 4 > value_count:
            raise ValueError(f"{bin_path}: truncated bbox at value {i - 1}")

        min_lat = read_i16(data, i)
        max_lat = read_i16(data, i + 1)
        min_lon = read_i16(data, i + 2)
        max_lon = read_i16(data, i + 3)
        i += 4

        data_offset = i * 2
        point_count = 0

        while i < value_count:
            v = read_i16(data, i)
            if v == END:
                i += 1
                break
            if i + 1 >= value_count:
                raise ValueError(f"{bin_path}: truncated point at value {i}")
            point_count += 1
            i += 2

        if point_count > 0xFFFF:
            raise ValueError(f"{bin_path}: segment has {point_count} points")

        records.append(
            struct.pack(
                "<hhhhIHH",
                min_lat,
                max_lat,
                min_lon,
                max_lon,
                data_offset,
                point_count,
                0,
            )
        )

    idx_path.write_bytes(b"".join(records))
    return len(records)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "directory",
        nargs="?",
        default="vector_bin",
        help="Directory containing vector .bin files",
    )
    args = parser.parse_args()

    vector_dir = Path(args.directory)
    for layer in LAYERS:
        bin_path = vector_dir / f"{layer}.bin"
        idx_path = vector_dir / f"{layer}.idx"
        if not bin_path.exists():
            print(f"skip missing {bin_path}")
            continue
        count = build_index(bin_path, idx_path)
        print(f"{idx_path}: {count} segments, {idx_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
