#!/usr/bin/env bash
# Build the website into site/ and serve it.
#
#   ./web.sh             build, then serve site/ as plain static files (as the web server will)
#   ./web.sh live        serve with live reload while editing docs/
#   ./web.sh build       build only
#
# The port defaults to 8000; set PORT to change it, e.g. PORT=8080 ./web.sh

set -euo pipefail
cd "$(dirname "$0")"

PORT="${PORT:-8000}"
export NO_MKDOCS_2_WARNING=1

if [[ ! -x .venv/bin/mkdocs ]]; then
    echo "Creating the Python venv..."
    python3 -m venv .venv
    .venv/bin/pip install mkdocs-material mkdocs-glightbox mkdocs-table-reader-plugin openpyxl
fi

case "${1:-serve}" in
    live)
        exec .venv/bin/mkdocs serve --dev-addr "127.0.0.1:${PORT}"
        ;;
    build)
        python3 tools/check_instruction_tables.py
        .venv/bin/mkdocs build --strict
        ;;
    serve)
        python3 tools/check_instruction_tables.py
        .venv/bin/mkdocs build --strict
        echo "Serving site/ on http://127.0.0.1:${PORT}/ (Ctrl+C to stop)"
        exec python3 -m http.server "${PORT}" --bind 127.0.0.1 --directory site
        ;;
    *)
        echo "Usage: $0 [serve|live|build]" >&2
        exit 1
        ;;
esac
