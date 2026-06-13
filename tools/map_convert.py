#!/usr/bin/env python3
"""
Convert OpenStreetMap PBF data to Cardputer offline JPEG/PNG tiles.

This is a clean launcher for the original renderers in this directory:
  - OSM2tile.py: full/staged renderer, better for larger extracts
  - osm2tile_small.py: lightweight renderer, good for small extracts

Examples:
  python tile_convert.py input.osm.pbf -z 10-12 -b 30.8,120.8,32.4,122.2
  python tile_convert.py input.osm.pbf --engine small -z 6-14 -l en -q 75
  python tile_convert.py
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT = "gpstoolkit"
ENGINES = {
    "standard": SCRIPT_DIR / "osm2tile.py",
    "small": SCRIPT_DIR / "osm2tile_small.py",
}


def prompt(message: str, default: str | None = None) -> str:
    suffix = f" [{default}]" if default else ""
    value = input(f"{message}{suffix}: ").strip()
    return value or (default or "")


def find_pbf_files() -> list[Path]:
    seen: set[Path] = set()
    roots = [Path.cwd(), SCRIPT_DIR]
    files: list[Path] = []
    for root in roots:
        for path in sorted(root.glob("*.osm.pbf")):
            resolved = path.resolve()
            if resolved not in seen:
                seen.add(resolved)
                files.append(path)
    return files


def choose_input() -> str:
    files = find_pbf_files()
    if files:
        print("\nFound .osm.pbf files:")
        for idx, path in enumerate(files, 1):
            print(f"  {idx}. {path}")
        print("  0. Enter another path")
        while True:
            choice = prompt("Choose input file", "1")
            if choice == "0":
                return prompt("Input .osm.pbf path")
            try:
                idx = int(choice) - 1
                if 0 <= idx < len(files):
                    return str(files[idx])
            except ValueError:
                pass
            print("Invalid choice.")
    return prompt("Input .osm.pbf path")


def choose_engine(default: str = "standard") -> str:
    print("\nRender engine:")
    print("  1. standard  Full/staged renderer for normal and large maps")
    print("  2. small     Lightweight renderer for small extracts")
    while True:
        choice = prompt("Choose engine", "1" if default == "standard" else "2")
        if choice in ("1", "standard", "s", ""):
            return "standard"
        if choice in ("2", "small"):
            return "small"
        print("Invalid choice.")


def parse_zoom(value: str) -> str:
    value = value.strip()
    if not value:
        raise ValueError("zoom is empty")
    if "-" in value:
        start_s, end_s = value.split("-", 1)
        start, end = int(start_s), int(end_s)
    else:
        start = end = int(value)
    if start > end:
        start, end = end, start
    if start < 6 or end > 18:
        raise ValueError("zoom must be within 6-18")
    return str(start) if start == end else f"{start}-{end}"


def parse_bbox(value: str | None) -> str | None:
    if not value:
        return None
    parts = [part.strip() for part in value.split(",")]
    if len(parts) != 4:
        raise ValueError("bbox must be S,W,N,E")
    south, west, north, east = [float(part) for part in parts]
    if south >= north:
        raise ValueError("bbox south must be less than north")
    if west >= east:
        raise ValueError("bbox west must be less than east")
    if not (-90 <= south <= 90 and -90 <= north <= 90):
        raise ValueError("bbox latitude must be within -90..90")
    if not (-180 <= west <= 180 and -180 <= east <= 180):
        raise ValueError("bbox longitude must be within -180..180")
    return f"{south},{west},{north},{east}"


def collect_interactive(args: argparse.Namespace) -> argparse.Namespace:
    if not sys.stdin.isatty():
        if not args.input:
            raise SystemExit("input is required in non-interactive mode.")
        if not args.zoom:
            raise SystemExit("--zoom is required in non-interactive mode.")
        return args

    print("\nCardputer tile converter")
    if not args.input:
        args.input = choose_input()
    if args.engine is None:
        args.engine = choose_engine()
    if not args.zoom:
        args.zoom = parse_zoom(prompt("Zoom level or range", "10-12"))
    if args.bbox is None:
        bbox = prompt("BBox S,W,N,E (blank = data bounds)")
        args.bbox = parse_bbox(bbox) if bbox else None
    if args.output == DEFAULT_OUTPUT:
        args.output = prompt("Output directory", DEFAULT_OUTPUT)
    if args.lang is None:
        lang = prompt("Label language, e.g. en/zh/name (blank = renderer default)")
        args.lang = lang or None
    if args.workers is None:
        workers = prompt("Parallel workers", "1")
        args.workers = int(workers)
    if args.engine == "small" and args.quality is None:
        q = prompt("JPEG quality", "75")
        args.quality = int(q)
    return args


def build_engine_command(args: argparse.Namespace, passthrough: list[str]) -> list[str]:
    engine = args.engine or "standard"
    script = ENGINES[engine]
    if not script.exists():
        raise FileNotFoundError(f"Renderer not found: {script}")

    zoom = parse_zoom(args.zoom)
    bbox = parse_bbox(args.bbox)

    cmd = [sys.executable, str(script), args.input, "-z", zoom, "-o", args.output]
    if bbox:
        cmd += ["-b", bbox]

    if args.lang:
        cmd += ["--lang" if engine == "standard" else "-l", args.lang]

    if args.workers is not None:
        cmd += ["--workers", str(args.workers)]

    if engine == "standard":
        cmd += ["--tile-size", str(args.tile_size), "-f", args.format]
        if args.single_pass:
            cmd.append("--single-pass")
        for flag in ("no_areas", "no_ways", "no_pois", "no_labels"):
            if getattr(args, flag):
                cmd.append("--" + flag.replace("_", "-"))
        if args.quality is not None:
            print("Note: --quality is only supported by the small engine; standard uses its built-in JPEG quality.")
    else:
        if args.quality is not None:
            cmd += ["-q", str(args.quality)]

    cmd += passthrough
    return cmd


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert .osm.pbf to /gpstoolkit/{z}/{x}/{y}.jpg tiles.",
        epilog="Unknown extra arguments after known options are passed to the selected renderer.",
    )
    parser.add_argument("input", nargs="?", help="Input .osm.pbf file.")
    parser.add_argument("-z", "--zoom", help="Zoom level or range, e.g. 12 or 10-14.")
    parser.add_argument("-b", "--bbox", help="Render bbox as south,west,north,east.")
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT, help="Output directory.")
    parser.add_argument(
        "--engine", choices=sorted(ENGINES),
        help="Renderer to use. Defaults to standard in interactive and CLI modes.",
    )
    parser.add_argument("-l", "--lang", help="Label language, e.g. en, zh, ja, or name.")
    parser.add_argument("-q", "--quality", type=int, help="JPEG quality for --engine small.")
    parser.add_argument("-j", "--workers", type=int, help="Parallel render workers.")
    parser.add_argument("--tile-size", type=int, default=256, help="Tile size for standard engine.")
    parser.add_argument("-f", "--format", default="JPG", help="Output format for standard engine.")
    parser.add_argument("--single-pass", action="store_true", help="Use standard renderer single-pass mode.")
    parser.add_argument("--no-areas", action="store_true")
    parser.add_argument("--no-ways", action="store_true")
    parser.add_argument("--no-pois", action="store_true")
    parser.add_argument("--no-labels", action="store_true")
    parser.add_argument("--dry-run", action="store_true", help="Print renderer command only.")
    return parser


def main() -> int:
    parser = build_parser()
    args, passthrough = parser.parse_known_args()

    try:
        args = collect_interactive(args)
        args.engine = args.engine or "standard"
        if not args.input:
            raise ValueError("input is required")
        if not Path(args.input).exists():
            raise FileNotFoundError(args.input)
        cmd = build_engine_command(args, passthrough)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    print("\nRenderer command:")
    print(" ".join(f'"{part}"' if " " in part else part for part in cmd))
    if args.dry_run:
        return 0

    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
