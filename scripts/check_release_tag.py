#!/usr/bin/env python3
"""
scripts/check_release_tag.py — Validate release tags against pinned versions.

Why: Open-source release automation must enforce SemVer tag shape, keep tags in
sync with include/zr/zr_version.h, and ensure CHANGELOG.md has a matching
release entry before publishing artifacts.
"""

from __future__ import annotations

import pathlib
import re
import sys


RE_DEFINE_U = re.compile(r"^\s*#define\s+(ZR_[A-Z0-9_]+)\s+\((\d+)u\)\s*$")
RE_SEMVER_TAG = re.compile(
    r"^v(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)"
    r"(?:-(?P<prerelease>[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+(?P<buildmeta>[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$"
)
RE_CHANGELOG_HEADING = re.compile(
    r"^\s*##\s+"
    r"(?P<version>(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?)\b"
)


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


def parse_tag(tag: str) -> tuple[str, str | None]:
  m = RE_SEMVER_TAG.fullmatch(tag)
  if not m:
    raise ValueError(f"tag '{tag}' is not valid SemVer form vMAJOR.MINOR.PATCH[-PRERELEASE][+BUILDMETA]")
  core = f"{m.group('major')}.{m.group('minor')}.{m.group('patch')}"
  prerelease = m.group("prerelease")
  return core, prerelease


def changelog_versions(changelog_text: str) -> set[str]:
  found: set[str] = set()
  for line in changelog_text.splitlines():
    m = RE_CHANGELOG_HEADING.match(line)
    if m:
      found.add(m.group("version"))
  return found


def main(argv: list[str]) -> int:
  if len(argv) != 2:
    print("usage: python3 scripts/check_release_tag.py <tag>", file=sys.stderr)
    return 2

  tag = argv[1].strip()
  try:
    tag_core, tag_prerelease = parse_tag(tag)
  except ValueError as exc:
    print(str(exc), file=sys.stderr)
    return 1

  repo_root = pathlib.Path(__file__).resolve().parents[1]
  header = repo_root / "include" / "zr" / "zr_version.h"
  changelog = repo_root / "CHANGELOG.md"

  pins = parse_pins(header.read_text(encoding="utf-8"))
  required = [
    "ZR_LIBRARY_VERSION_MAJOR",
    "ZR_LIBRARY_VERSION_MINOR",
    "ZR_LIBRARY_VERSION_PATCH",
  ]
  missing = [k for k in required if k not in pins]
  if missing:
    print(f"{header}: missing expected macros: {', '.join(missing)}", file=sys.stderr)
    return 2

  header_core = "{}.{}.{}".format(
      require(pins, "ZR_LIBRARY_VERSION_MAJOR"),
      require(pins, "ZR_LIBRARY_VERSION_MINOR"),
      require(pins, "ZR_LIBRARY_VERSION_PATCH"),
  )

  if tag_core != header_core:
    print(
        f"tag '{tag}' core version ({tag_core}) does not match include/zr/zr_version.h ({header_core})",
        file=sys.stderr,
    )
    return 1

  expected_changelog = tag_core if tag_prerelease is None else f"{tag_core}-{tag_prerelease}"
  published = changelog_versions(changelog.read_text(encoding="utf-8"))
  if expected_changelog not in published:
    print(
        f"{changelog}: missing heading for release '{expected_changelog}'",
        file=sys.stderr,
    )
    return 1

  if tag_prerelease is None:
    print(f"release tag OK: stable {tag}")
  else:
    print(f"release tag OK: prerelease {tag}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main(sys.argv))
