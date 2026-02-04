#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash scripts/clang_format_check.sh --all
  bash scripts/clang_format_check.sh --diff <base> <head>

Runs clang-format over tracked *.c/*.h files and fails if formatting changes.
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 2
fi

mode="$1"
shift

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found in PATH" >&2
  exit 2
fi

files=()
case "${mode}" in
  --all)
    while IFS= read -r f; do
      files+=("$f")
    done < <(git ls-files '*.c' '*.h')
    ;;
  --diff)
    if [[ $# -ne 2 ]]; then
      usage
      exit 2
    fi
    base="$1"
    head="$2"
    while IFS= read -r f; do
      files+=("$f")
    done < <(git diff --name-only "${base}..${head}" -- '*.c' '*.h')
    ;;
  *)
    usage
    exit 2
    ;;
esac

if [[ ${#files[@]} -eq 0 ]]; then
  echo "No C/C header files to format."
  exit 0
fi

clang-format -i "${files[@]}"

if ! git diff --exit-code -- "${files[@]}"; then
  echo ""
  echo "Formatting differs. Run:" >&2
  echo "  bash scripts/clang_format_check.sh --all" >&2
  exit 1
fi

