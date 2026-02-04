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

if [[ ! -d "${venv_dir}" ]]; then
  "${python}" -m venv "${venv_dir}"
fi

source "${venv_dir}/bin/activate"

python -m pip install --upgrade pip >/dev/null
python -m pip install -r requirements-docs.txt >/dev/null

if command -v doxygen >/dev/null 2>&1; then
  doxygen Doxyfile
fi

case "${cmd}" in
  serve)
    mkdocs serve
    ;;
  build)
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

