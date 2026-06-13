#!/usr/bin/env python3
"""
Download OpenStreetMap PBF data for Cardputer offline tiles.

Examples:
  python map_download.py --region asia/china
  python map_download.py --url https://download.geofabrik.de/asia/china-latest.osm.pbf
  python map_download.py
"""

from __future__ import annotations

import argparse
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


GEOFABRIK_BASE = "https://download.geofabrik.de"
DEFAULT_OUTPUT_DIR = "."

REGIONS = {
    "asia/china": "China",
    "asia/taiwan": "Taiwan",
    "asia/hong-kong": "Hong Kong",
    "asia/japan": "Japan",
    "asia/south-korea": "South Korea",
    "asia/singapore": "Singapore",
    "asia/thailand": "Thailand",
    "asia/vietnam": "Vietnam",
    "europe/germany": "Germany",
    "europe/france": "France",
    "europe/great-britain": "Great Britain",
    "europe/italy": "Italy",
    "europe/spain": "Spain",
    "north-america/us": "United States",
    "north-america/us/california": "California, US",
    "north-america/us/new-york": "New York, US",
    "north-america/canada": "Canada",
    "south-america/brazil": "Brazil",
    "australia-oceania/australia": "Australia",
}


def geofabrik_url(region: str) -> str:
    region = region.strip().strip("/")
    if region.endswith(".osm.pbf"):
        return f"{GEOFABRIK_BASE}/{region}"
    if region.endswith("-latest"):
        return f"{GEOFABRIK_BASE}/{region}.osm.pbf"
    return f"{GEOFABRIK_BASE}/{region}-latest.osm.pbf"


def filename_from_url(url: str) -> str:
    path = urllib.parse.urlparse(url).path
    name = os.path.basename(path)
    return name or "map.osm.pbf"


def human_size(num: int | None) -> str:
    if num is None:
        return "unknown"
    value = float(num)
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if value < 1024.0 or unit == "TB":
            return f"{value:.1f} {unit}" if unit != "B" else f"{int(value)} B"
        value /= 1024.0
    return f"{value:.1f} TB"


def prompt(message: str, default: str | None = None) -> str:
    suffix = f" [{default}]" if default else ""
    value = input(f"{message}{suffix}: ").strip()
    return value or (default or "")


def choose_region() -> str:
    items = list(REGIONS.items())
    print("\nCommon Geofabrik regions:")
    for idx, (slug, label) in enumerate(items, 1):
        print(f"  {idx:2d}. {label:<20} {slug}")
    print("   0. Custom URL")
    print("  99. Custom Geofabrik path, e.g. asia/china")

    while True:
        choice = prompt("Choose map source", "1")
        if choice == "0":
            return prompt("Download URL")
        if choice == "99":
            return geofabrik_url(prompt("Geofabrik path"))
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(items):
                return geofabrik_url(items[idx][0])
        except ValueError:
            pass
        print("Invalid choice.")


def resolve_download_url(args: argparse.Namespace) -> str:
    if args.url:
        return args.url
    if args.region:
        return geofabrik_url(args.region)
    if not sys.stdin.isatty():
        raise SystemExit("Either --url or --region is required in non-interactive mode.")
    return choose_region()


def open_request(url: str, start: int = 0) -> urllib.response.addinfourl:
    headers = {"User-Agent": "cardputer-gps-toolkit-maptool/1.0"}
    if start > 0:
        headers["Range"] = f"bytes={start}-"
    request = urllib.request.Request(url, headers=headers)
    return urllib.request.urlopen(request, timeout=60)


def download(url: str, target: Path, resume: bool = True, force: bool = False) -> Path:
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_suffix(target.suffix + ".part")

    if target.exists() and not force:
        print(f"Already exists: {target}")
        return target
    if force and target.exists():
        target.unlink()

    start = part.stat().st_size if resume and part.exists() else 0
    if start:
        print(f"Resuming from {human_size(start)}")

    try:
        response = open_request(url, start)
    except urllib.error.HTTPError as exc:
        if start and exc.code == 416:
            part.replace(target)
            return target
        raise

    status = getattr(response, "status", response.getcode())
    if start and status != 206:
        print("Server did not accept resume; restarting download.")
        start = 0

    mode = "ab" if start else "wb"
    total_header = response.headers.get("Content-Length")
    total = int(total_header) + start if total_header and start else (
        int(total_header) if total_header else None
    )

    print(f"URL:    {url}")
    print(f"Output: {target}")
    print(f"Size:   {human_size(total)}")

    done = start
    last_print = time.time()
    began = time.time()
    with response, part.open(mode) as fh:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            fh.write(chunk)
            done += len(chunk)
            now = time.time()
            if now - last_print >= 0.5:
                rate = (done - start) / max(now - began, 0.001)
                if total:
                    pct = done * 100.0 / total
                    print(f"\r{pct:6.2f}%  {human_size(done)} / {human_size(total)}  {human_size(int(rate))}/s", end="")
                else:
                    print(f"\r{human_size(done)}  {human_size(int(rate))}/s", end="")
                last_print = now

    print()
    part.replace(target)
    print(f"Done: {target}")
    return target


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Download .osm.pbf map data.")
    parser.add_argument("-u", "--url", help="Direct .osm.pbf download URL.")
    parser.add_argument(
        "-r", "--region",
        help="Geofabrik path such as asia/china or north-america/us/california.",
    )
    parser.add_argument("-o", "--output-dir", default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--filename", help="Save as this filename.")
    parser.add_argument("--no-resume", action="store_true", help="Do not resume .part downloads.")
    parser.add_argument("--force", action="store_true", help="Overwrite existing output file.")
    parser.add_argument("--list-regions", action="store_true", help="Print bundled region shortcuts.")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.list_regions:
        for slug, label in REGIONS.items():
            print(f"{slug:<34} {label}")
        return 0

    url = resolve_download_url(args)
    filename = args.filename or filename_from_url(url)
    target = Path(args.output_dir) / filename
    try:
        download(url, target, resume=not args.no_resume, force=args.force)
    except KeyboardInterrupt:
        print("\nInterrupted. Partial file is kept as .part for resume.")
        return 130
    except Exception as exc:
        print(f"Download failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
