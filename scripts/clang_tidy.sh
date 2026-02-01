#!/usr/bin/env bash
#
# scripts/clang_tidy.sh — Run clang-tidy against the current build.
#
# Why: CI-friendly entrypoint with deterministic defaults.
#
set -euo pipefail

build_dir="${1:-out/build/posix-clang-debug}"

python3 scripts/run_clang_tidy.py --build-dir "${build_dir}"

