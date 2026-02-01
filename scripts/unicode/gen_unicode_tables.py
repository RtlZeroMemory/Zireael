#!/usr/bin/env python3
"""
scripts/unicode/gen_unicode_tables.py — Generate pinned Unicode 15.1.0 tables.

Why:
  Zireael's unicode routines must be deterministic and OS/locale independent.
  This script generates immutable range tables from the Unicode 15.1.0 UCD
  sources for:
    - Grapheme_Cluster_Break (UAX #29)
    - Extended_Pictographic + Emoji_Presentation (emoji-data.txt)
    - EastAsianWidth wide/fullwidth ranges

Output:
  src/unicode/zr_unicode_data_tables_15_1_0.inc

Notes:
  - The build does NOT run this script; generated output is checked in.
  - Network download is supported for convenience and is pinned to 15.1.0.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
import textwrap
import urllib.request


UNICODE_VERSION = "15.1.0"
BASE = f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd"
URLS = {
    "GraphemeBreakProperty.txt": f"{BASE}/auxiliary/GraphemeBreakProperty.txt",
    "emoji-data.txt": f"{BASE}/emoji/emoji-data.txt",
    "EastAsianWidth.txt": f"{BASE}/EastAsianWidth.txt",
}


def _parse_codepoint_range(field: str) -> tuple[int, int]:
    field = field.strip()
    if ".." in field:
        a, b = field.split("..", 1)
        return int(a, 16), int(b, 16)
    return int(field, 16), int(field, 16)


def _merge_ranges(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    if not ranges:
        return []
    ranges = sorted(ranges, key=lambda x: (x[0], x[1]))
    out: list[tuple[int, int]] = []
    cur_lo, cur_hi = ranges[0]
    for lo, hi in ranges[1:]:
        if lo <= cur_hi + 1:
            cur_hi = max(cur_hi, hi)
            continue
        out.append((cur_lo, cur_hi))
        cur_lo, cur_hi = lo, hi
    out.append((cur_lo, cur_hi))
    return out


def _merge_valued_ranges(ranges: list[tuple[int, int, str]]) -> list[tuple[int, int, str]]:
    if not ranges:
        return []
    ranges = sorted(ranges, key=lambda x: (x[0], x[1], x[2]))
    out: list[tuple[int, int, str]] = []
    cur_lo, cur_hi, cur_v = ranges[0]
    for lo, hi, v in ranges[1:]:
        if v == cur_v and lo <= cur_hi + 1:
            cur_hi = max(cur_hi, hi)
            continue
        out.append((cur_lo, cur_hi, cur_v))
        cur_lo, cur_hi, cur_v = lo, hi, v
    out.append((cur_lo, cur_hi, cur_v))
    return out


_DATA_RE = re.compile(r"^([0-9A-Fa-f.]+)\s*;\s*([A-Za-z_]+)\b")


def _read_lines(path: pathlib.Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="strict").splitlines()


def _parse_property_file(path: pathlib.Path, wanted: set[str] | None = None) -> list[tuple[int, int, str]]:
    rows: list[tuple[int, int, str]] = []
    for line in _read_lines(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        m = _DATA_RE.match(line)
        if not m:
            continue
        cp_field, prop = m.group(1), m.group(2)
        if wanted is not None and prop not in wanted:
            continue
        lo, hi = _parse_codepoint_range(cp_field)
        if lo < 0 or hi > 0x10FFFF or lo > hi:
            raise ValueError(f"invalid codepoint range {cp_field} in {path}")
        rows.append((lo, hi, prop))
    return rows


def _download_to(path: pathlib.Path, url: str) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  req = urllib.request.Request(
      url,
      headers={
          "User-Agent": "Zireael-UnicodeTableGen/1.0 (+https://github.com/RtlZeroMemory/Zireael)",
          "Accept": "text/plain,*/*",
      },
  )
  with urllib.request.urlopen(req) as r:
    data = r.read()
  path.write_bytes(data)


def _ensure_sources(dir_path: pathlib.Path) -> dict[str, pathlib.Path]:
    out: dict[str, pathlib.Path] = {}
    for name, url in URLS.items():
        p = dir_path / name
        if not p.exists():
            _download_to(p, url)
        out[name] = p
    return out


def _assert_disjoint_sorted(ranges: list[tuple[int, int, str]], label: str) -> None:
    ranges = sorted(ranges, key=lambda x: (x[0], x[1]))
    prev_hi = -1
    for lo, hi, _ in ranges:
        if lo <= prev_hi:
            raise ValueError(f"{label}: overlapping ranges at U+{lo:04X}")
        prev_hi = hi


def _emit_inc(
    out_path: pathlib.Path,
    gcb: list[tuple[int, int, str]],
    ep: list[tuple[int, int]],
    emoji_pres: list[tuple[int, int]],
    eaw_wide: list[tuple[int, int]],
) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)

    gcb_map = {
        "CR": "ZR_GCB_CR",
        "LF": "ZR_GCB_LF",
        "Control": "ZR_GCB_CONTROL",
        "Extend": "ZR_GCB_EXTEND",
        "ZWJ": "ZR_GCB_ZWJ",
        "Regional_Indicator": "ZR_GCB_REGIONAL_INDICATOR",
        "Prepend": "ZR_GCB_PREPEND",
        "SpacingMark": "ZR_GCB_SPACINGMARK",
        "L": "ZR_GCB_L",
        "V": "ZR_GCB_V",
        "T": "ZR_GCB_T",
        "LV": "ZR_GCB_LV",
        "LVT": "ZR_GCB_LVT",
    }

    def fmt_u32(x: int) -> str:
        return f"0x{x:04X}u" if x <= 0xFFFF else f"0x{x:X}u"

    def emit_ranges8(name: str, rows: list[tuple[int, int, str]]) -> str:
        lines = [f"static const zr_unicode_range8_t {name}[] = {{"]
        for lo, hi, prop in rows:
            enum_name = gcb_map.get(prop)
            if enum_name is None:
                raise ValueError(f"unmapped GCB prop: {prop}")
            lines.append(
                f"  {{{fmt_u32(lo)}, {fmt_u32(hi)}, (uint8_t){enum_name}, {{0u, 0u, 0u}}}},"
            )
        lines.append("};")
        return "\n".join(lines)

    def emit_ranges(name: str, rows: list[tuple[int, int]]) -> str:
        lines = [f"static const zr_unicode_range_t {name}[] = {{"]
        for lo, hi in rows:
            lines.append(f"  {{{fmt_u32(lo)}, {fmt_u32(hi)}}},")
        lines.append("};")
        return "\n".join(lines)

    hdr = textwrap.dedent(
        f"""\
        /*
          src/unicode/zr_unicode_data_tables_15_1_0.inc — Generated Unicode {UNICODE_VERSION} tables.

          Why: Provides immutable, deterministic property ranges for grapheme iteration and width.

          Generated by: scripts/unicode/gen_unicode_tables.py
        */
        """
    )

    body = "\n\n".join(
        [
            emit_ranges8("kGcbRanges", gcb),
            emit_ranges("kExtendedPictographicRanges", ep),
            emit_ranges("kEmojiPresentationRanges", emoji_pres),
            emit_ranges("kEawWideRanges", eaw_wide),
        ]
    )
    out_path.write_text(hdr + "\n" + body + "\n", encoding="utf-8", errors="strict")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ucd-dir", type=pathlib.Path, default=pathlib.Path("out/unicode-ucd-15.1.0"))
    ap.add_argument("--out", type=pathlib.Path, default=pathlib.Path("src/unicode/zr_unicode_data_tables_15_1_0.inc"))
    args = ap.parse_args(argv)

    srcs = _ensure_sources(args.ucd_dir)

    gcb_props = {
        "CR",
        "LF",
        "Control",
        "Extend",
        "ZWJ",
        "Regional_Indicator",
        "Prepend",
        "SpacingMark",
        "L",
        "V",
        "T",
        "LV",
        "LVT",
    }
    gcb_rows = _parse_property_file(srcs["GraphemeBreakProperty.txt"], wanted=gcb_props)
    gcb_rows = _merge_valued_ranges(gcb_rows)
    _assert_disjoint_sorted(gcb_rows, "GCB")

    emoji_rows = _parse_property_file(
        srcs["emoji-data.txt"],
        wanted={"Extended_Pictographic", "Emoji_Presentation"},
    )
    ep = _merge_ranges([(lo, hi) for lo, hi, prop in emoji_rows if prop == "Extended_Pictographic"])
    emoji_pres = _merge_ranges([(lo, hi) for lo, hi, prop in emoji_rows if prop == "Emoji_Presentation"])

    eaw_rows = _parse_property_file(srcs["EastAsianWidth.txt"], wanted={"W", "F"})
    eaw_wide = _merge_ranges([(lo, hi) for lo, hi, _prop in eaw_rows])

    _emit_inc(args.out, gcb_rows, ep, emoji_pres, eaw_wide)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
