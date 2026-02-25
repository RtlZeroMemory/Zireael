#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash scripts/docs.sh serve
  bash scripts/docs.sh build

Creates/uses a local venv at .venv-docs, installs requirements-docs.txt,
then runs MkDocs. For build, also runs Doxygen if available and publishes it
into out/site/api/.
EOF
}

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

cmd="$1"

python="${PYTHON:-python3}"
venv_dir=".venv-docs"

# Bootstrap local docs environment once and reuse it across runs.
if [[ ! -d "${venv_dir}" ]]; then
  "${python}" -m venv "${venv_dir}"
fi

source "${venv_dir}/bin/activate"

# Keep docs toolchain pinned to requirements-docs.txt.
python -m pip install --upgrade pip >/dev/null
python -m pip install -r requirements-docs.txt >/dev/null

# Doxygen is optional; build API HTML when present.
if command -v doxygen >/dev/null 2>&1; then
  mkdir -p out
  doxygen Doxyfile
fi

case "${cmd}" in
  serve)
    # Live-reload authoring workflow.
    mkdocs serve
    ;;
  build)
    # CI/docs publishing workflow.
    mkdocs build --strict
    if [[ -d out/doxygen/html ]]; then
      rm -rf out/site/api
      mkdir -p out/site/api
      cp -R out/doxygen/html/. out/site/api/
    fi
    ;;
  *)
    usage
    exit 2
    ;;
esac
