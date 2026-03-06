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
RE_WS = re.compile(r"\s+")


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


def normalize_ws(text: str) -> str:
  return RE_WS.sub(" ", text).strip()


def main() -> int:
  repo_root = pathlib.Path(__file__).resolve().parents[1]
  header = repo_root / "include" / "zr" / "zr_version.h"
  readme = repo_root / "README.md"
  docs_pins = repo_root / "docs" / "VERSION_PINS.md"
  docs_versioning = repo_root / "docs" / "abi" / "versioning.md"
  docs_abi_reference = repo_root / "docs" / "ABI_REFERENCE.md"
  docs_config_module = repo_root / "docs" / "modules" / "CONFIG_AND_ABI_VERSIONING.md"
  docs_drawlist_module = repo_root / "docs" / "modules" / "DRAWLIST_FORMAT_AND_PARSER.md"

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
    "ZR_DRAWLIST_VERSION_V2",
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
  if "Zireael is alpha." not in pins_text:
    print(f"{docs_pins}: missing alpha lifecycle status text", file=sys.stderr)
    return 1
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
  if "Lifecycle: alpha" not in versioning_text:
    print(f"{docs_versioning}: missing alpha lifecycle status text", file=sys.stderr)
    return 1
  if "Drawlist formats: v1, v2" not in versioning_text:
    print(f"{docs_versioning}: missing drawlist version snapshot for v1, v2", file=sys.stderr)
    return 1

  # --- README.md must reflect the current lifecycle and drawlist snapshot ---
  readme_text = readme.read_text(encoding="utf-8")
  if "status-alpha" not in readme_text:
    print(f"{readme}: missing alpha status badge", file=sys.stderr)
    return 1
  if "Zireael is currently **alpha**." not in readme_text:
    print(f"{readme}: missing alpha lifecycle status text", file=sys.stderr)
    return 1
  if "Drawlist formats: v1, v2" not in readme_text:
    print(f"{readme}: missing drawlist version snapshot for v1, v2", file=sys.stderr)
    return 1

  # --- docs/ABI_REFERENCE.md must describe the current public drawlist header scope ---
  abi_reference_text = docs_abi_reference.read_text(encoding="utf-8")
  expected_abi_reference_snippets = [
    "`include/zr/zr_drawlist.h` (drawlist v1/v2)",
    "Drawlist v1/v2 and event batch v1 are specified by:",
  ]
  for snippet in expected_abi_reference_snippets:
    if snippet not in abi_reference_text:
      print(f"{docs_abi_reference}: missing current ABI drawlist wording: {snippet!r}", file=sys.stderr)
      return 1

  # --- docs/modules/CONFIG_AND_ABI_VERSIONING.md must describe current drawlist pins ---
  config_module_text = docs_config_module.read_text(encoding="utf-8")
  expected_config_snippets = [
    "drawlist format (`ZR_DRAWLIST_VERSION_V1` or `ZR_DRAWLIST_VERSION_V2`)",
    "Drawlist version MUST be one of the supported pinned versions (`ZR_DRAWLIST_VERSION_V1` or\n  `ZR_DRAWLIST_VERSION_V2`).",
  ]
  config_module_text_normalized = normalize_ws(config_module_text)
  for snippet in expected_config_snippets:
    if normalize_ws(snippet) not in config_module_text_normalized:
      print(f"{docs_config_module}: missing current drawlist-version wording: {snippet!r}", file=sys.stderr)
      return 1

  # --- docs/modules/DRAWLIST_FORMAT_AND_PARSER.md must describe current parser support ---
  drawlist_module_text = docs_drawlist_module.read_text(encoding="utf-8")
  expected_drawlist_snippets = [
    "`ZR_DRAWLIST_VERSION_V1` and `ZR_DRAWLIST_VERSION_V2` are accepted.",
    "`ZR_DRAWLIST_VERSION_V2`\nis additive and only gates `ZR_DL_OP_BLIT_RECT`;",
  ]
  drawlist_module_text_normalized = normalize_ws(drawlist_module_text)
  for snippet in expected_drawlist_snippets:
    if normalize_ws(snippet) not in drawlist_module_text_normalized:
      print(f"{docs_drawlist_module}: missing current parser-version wording: {snippet!r}", file=sys.stderr)
      return 1

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
