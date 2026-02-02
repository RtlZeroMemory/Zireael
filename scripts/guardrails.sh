#!/usr/bin/env bash
set -euo pipefail

# scripts/guardrails.sh — CI guardrails for platform boundary + libc policy.
#
# Why: Enforces "no OS headers" and a forbidden-libc-call set under:
#   src/core/, src/unicode/, src/util/

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v rg >/dev/null 2>&1; then
  echo "error: ripgrep (rg) is required for guardrails" >&2
  exit 2
fi

scan_dirs=(
  "${repo_root}/src/core"
  "${repo_root}/src/unicode"
  "${repo_root}/src/util"
)

has_violations=0

print_section() {
  echo
  echo "== $1 =="
}

run_rg() {
  rg --no-heading --line-number --with-filename -S "$@"
}

print_section "Platform boundary (OS headers forbidden in core/unicode/util)"

# Matches OS headers only in #include lines. Keep this list focused to avoid false positives.
os_include_re='^\s*#\s*include\s*[<"](?:windows\.h|winsock2\.h|ws2tcpip\.h|winnt\.h|winuser\.h|winternl\.h|ntdef\.h|ntstatus\.h|io\.h|direct\.h|unistd\.h|termios\.h|fcntl\.h|poll\.h|sys/[^>"]+|arpa/[^>"]+|net/[^>"]+|netinet/[^>"]+|linux/[^>"]+|mach/[^>"]+|CoreFoundation/[^>"]+|dispatch/[^>"]+)[>"]'

os_hits="$(run_rg -g'*.{c,h}' "${os_include_re}" "${scan_dirs[@]}" || true)"
if [[ -n "${os_hits}" ]]; then
  echo "${os_hits}"
  has_violations=1
else
  echo "ok"
fi

print_section "Forbidden libc calls (core/unicode/util only)"

# Note: Match common call forms like `name(`. Word boundaries avoid false positives like `zr_printf`.
forbidden_calls=(
  # printf family
  printf fprintf sprintf snprintf vprintf vfprintf vsprintf vsnprintf dprintf vdprintf
  puts putchar getchar fputs fputc fgetc

  # scanf family
  scanf fscanf sscanf vscanf vfscanf vsscanf

  # locale / environment
  setlocale getenv putenv

  # time / clock
  time clock difftime mktime localtime gmtime asctime ctime strftime

  # randomness
  rand srand

  # process / shell
  system popen pclose
)

for name in "${forbidden_calls[@]}"; do
  hits="$(run_rg -g'*.{c,h}' "\b${name}\s*\(" "${scan_dirs[@]}" || true)"
  if [[ -n "${hits}" ]]; then
    echo "${hits}"
    has_violations=1
  fi
done

if [[ "${has_violations}" -ne 0 ]]; then
  echo
  echo "guardrails: FAILED" >&2
  exit 1
fi

echo
echo "guardrails: OK"
