#!/usr/bin/env python3
"""
scripts/run_clang_tidy.py — Deterministic clang-tidy runner over compile_commands.json.

Why: Keeps CI/tooling portable without depending on run-clang-tidy being installed.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def _is_repo_file(repo_root: Path, p: Path) -> bool:
    try:
        rp = p.resolve()
    except FileNotFoundError:
        return False
    try:
        rp.relative_to(repo_root)
    except ValueError:
        return False
    return True


def _want_file(rel: Path) -> bool:
    if rel.parts[:1] not in (("src",), ("tests",)):
        return False
    return rel.suffix.lower() == ".c"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--build-dir",
        required=True,
        help="CMake build directory containing compile_commands.json",
    )
    ap.add_argument(
        "--clang-tidy",
        default="clang-tidy",
        help="clang-tidy executable (default: clang-tidy)",
    )
    ap.add_argument(
        "--warnings-as-errors",
        default="clang-analyzer-*",
        help="clang-tidy --warnings-as-errors value (default: clang-analyzer-*)",
    )
    ap.add_argument(
        "--jobs",
        type=int,
        default=max(1, os.cpu_count() or 1),
        help="parallelism (default: cpu count)",
    )
    ap.add_argument(
        "--include-tests",
        action="store_true",
        help="also lint tests/ sources (default: only src/)",
    )
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    build_dir = Path(args.build_dir).resolve()
    cc_path = build_dir / "compile_commands.json"
    if not cc_path.exists():
        print(f"error: missing {cc_path}", file=sys.stderr)
        return 2

    try:
        compile_commands = json.loads(cc_path.read_text(encoding="utf-8"))
    except Exception as e:
        print(f"error: failed to parse {cc_path}: {e}", file=sys.stderr)
        return 2

    files: list[str] = []
    seen: set[str] = set()
    for entry in compile_commands:
        file_str = entry.get("file")
        if not isinstance(file_str, str) or not file_str:
            continue

        f = Path(file_str)
        if not f.is_absolute():
            d = entry.get("directory")
            if isinstance(d, str) and d:
                f = Path(d) / f

        if not _is_repo_file(repo_root, f):
            continue
        rel = f.resolve().relative_to(repo_root)
        if rel.parts[:1] == ("tests",) and not args.include_tests:
            continue
        if not _want_file(rel):
            continue

        key = str(rel)
        if key in seen:
            continue
        seen.add(key)
        files.append(key)

    files.sort()
    if not files:
        print("clang-tidy: no files selected", file=sys.stderr)
        return 0

    cmd = [
        args.clang_tidy,
        "-p",
        str(build_dir),
        "--quiet",
        f"--warnings-as-errors={args.warnings_as_errors}",
        *files,
    ]

    # Note: clang-tidy has internal parallelism; keep external parallelism to 1 by default.
    # If desired, users can run multiple invocations themselves.
    env = os.environ.copy()
    env.setdefault("CLANG_TIDY_EXTRA_ARGS", "")

    try:
        proc = subprocess.run(cmd, cwd=str(repo_root), env=env)
    except FileNotFoundError:
        print(f"error: clang-tidy not found: {args.clang_tidy}", file=sys.stderr)
        return 127
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())

