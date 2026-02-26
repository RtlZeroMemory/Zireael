#!/usr/bin/env python3
"""
scripts/check_version_pins.py — Verify docs match pinned public versions.

Why: Zireael’s determinism contract relies on explicit version pins. This
script keeps wrapper-facing docs and the public header pins in sync.
"""

from __future__ import annotations

import pathlib
import re
import sys


RE_DEFINE_U = re.compile(r"^\s*#define\s+(ZR_[A-Z0-9_]+)\s+\((\d+)u\)\s*$")


def parse_pins(text: str) -> dict[str, int]:
  pins: dict[str, int] = {}
  for line in text.splitlines():
    m = RE_DEFINE_U.match(line)
    if not m:
      continue
    pins[m.group(1)] = int(m.group(2))
  return pins


def require(pins: dict[str, int], name: str) -> int:
  if name not in pins:
    raise KeyError(name)
  return pins[name]


def main() -> int:
  repo_root = pathlib.Path(__file__).resolve().parents[1]
  header = repo_root / "include" / "zr" / "zr_version.h"
  docs_pins = repo_root / "docs" / "VERSION_PINS.md"
  docs_versioning = repo_root / "docs" / "abi" / "versioning.md"

  header_text = header.read_text(encoding="utf-8")
  pins = parse_pins(header_text)

  required = [
    "ZR_LIBRARY_VERSION_MAJOR",
    "ZR_LIBRARY_VERSION_MINOR",
    "ZR_LIBRARY_VERSION_PATCH",
    "ZR_ENGINE_ABI_MAJOR",
    "ZR_ENGINE_ABI_MINOR",
    "ZR_ENGINE_ABI_PATCH",
    "ZR_DRAWLIST_VERSION_V1",
    "ZR_EVENT_BATCH_VERSION_V1",
  ]

  missing = [k for k in required if k not in pins]
  if missing:
    print(f"{header}: missing expected macros: {', '.join(missing)}", file=sys.stderr)
    return 2

  lib_ver = (
    require(pins, "ZR_LIBRARY_VERSION_MAJOR"),
    require(pins, "ZR_LIBRARY_VERSION_MINOR"),
    require(pins, "ZR_LIBRARY_VERSION_PATCH"),
  )
  abi_ver = (
    require(pins, "ZR_ENGINE_ABI_MAJOR"),
    require(pins, "ZR_ENGINE_ABI_MINOR"),
    require(pins, "ZR_ENGINE_ABI_PATCH"),
  )

  # --- docs/VERSION_PINS.md must mention exact macro values ---
  pins_text = docs_pins.read_text(encoding="utf-8")
  for k in required:
    v = pins[k]
    if re.search(rf"\b{re.escape(k)}\s*=\s*{v}\b", pins_text) is None:
      print(f"{docs_pins}: missing/incorrect pin line for {k} (expected {v})", file=sys.stderr)
      return 1

  # --- docs/abi/versioning.md must mention current versions in prose ---
  versioning_text = docs_versioning.read_text(encoding="utf-8")
  if f"Library: v{lib_ver[0]}.{lib_ver[1]}.{lib_ver[2]}" not in versioning_text:
    print(f"{docs_versioning}: missing library version v{lib_ver[0]}.{lib_ver[1]}.{lib_ver[2]}", file=sys.stderr)
    return 1
  if f"Engine ABI: v{abi_ver[0]}.{abi_ver[1]}.{abi_ver[2]}" not in versioning_text:
    print(f"{docs_versioning}: missing engine ABI version v{abi_ver[0]}.{abi_ver[1]}.{abi_ver[2]}", file=sys.stderr)
    return 1

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
