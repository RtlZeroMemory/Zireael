#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

GO_VERSION="${GO_VERSION:-1.22.5}"
GO_DIR="${ROOT}/out/tools/go/${GO_VERSION}"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "error: missing required command: $1" >&2
    exit 1
  fi
}

ensure_go() {
  if command -v go >/dev/null 2>&1; then
    return
  fi

  need_cmd curl
  need_cmd tar

  local uname_s uname_m goos goarch
  uname_s="$(uname -s)"
  uname_m="$(uname -m)"

  case "${uname_s}" in
    Linux) goos="linux" ;;
    Darwin) goos="darwin" ;;
    *)
      echo "error: unsupported OS for Go bootstrap: ${uname_s}" >&2
      exit 1
      ;;
  esac

  case "${uname_m}" in
    x86_64|amd64) goarch="amd64" ;;
    arm64|aarch64) goarch="arm64" ;;
    *)
      echo "error: unsupported arch for Go bootstrap: ${uname_m}" >&2
      exit 1
      ;;
  esac

  if [[ -x "${GO_DIR}/go/bin/go" ]]; then
    export PATH="${GO_DIR}/go/bin:${PATH}"
    return
  fi

  mkdir -p "${GO_DIR}"

  local tgz="go${GO_VERSION}.${goos}-${goarch}.tar.gz"
  local url="https://go.dev/dl/${tgz}"
  local tmp="${GO_DIR}/${tgz}"

  echo "Bootstrapping Go ${GO_VERSION} (${goos}/${goarch})..."
  curl -fsSL "${url}" -o "${tmp}"
  tar -C "${GO_DIR}" -xzf "${tmp}"
  rm -f "${tmp}"

  export PATH="${GO_DIR}/go/bin:${PATH}"
}

ensure_engine_build() {
  need_cmd cmake

  local preset="${ZIREAEL_PRESET:-posix-clang-release}"
  local build_dir="${ROOT}/out/build/${preset}"
  if [[ ! -d "${build_dir}" ]]; then
    echo "Configuring Zireael (${preset})..."
    cmake --preset "${preset}"
  fi

  echo "Building Zireael (${preset})..."
  cmake --build --preset "${preset}"
}

ensure_go
ensure_engine_build

cd "${ROOT}/poc/go-codex-tui"
exec go run -a . "$@"
