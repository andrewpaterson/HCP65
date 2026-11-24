#!/usr/bin/env bash
# Build the website into site/ and serve it.
#
#   ./web.sh             build, then serve site/ as plain static files (as the web server will)
#   ./web.sh live        serve with live reload while editing the pages
#   ./web.sh build       build only (Website.vcxproj's build)
#   ./web.sh rebuild     clean, then build
#   ./web.sh clean       remove site/ and .staging/
#
# A build runs the generators, then the checks, then a strict MkDocs build of the pages gathered from the folders
# mkdocs.yml's extra.sources maps (tools/gather_sources.py).  The checks print problems as "<file>:<line>:1: error:"
# lines, which the IDE's Build pane opens.  The port defaults to 8000; set PORT to change it, e.g. PORT=8080 ./web.sh

set -euo pipefail
cd "$(dirname "$0")"

PORT="${PORT:-8000}"
export NO_MKDOCS_2_WARNING=1

if [[ ! -x .venv/bin/mkdocs ]]; then
    echo "Creating the Python venv..."
    python3 -m venv .venv
    .venv/bin/pip install mkdocs-material mkdocs-glightbox mkdocs-table-reader-plugin openpyxl
fi

# Pages made from other files: the 65C64's Opcode Encoding page from its spreadsheet.
generate() {
    .venv/bin/python tools/sheet_to_html.py "../X65C64-Proposal/65C64 Instruction Architecture.xlsx" \
        "Opcode encoding" B8:AJ371 docs/65c64/opcode-encoding.md --title "Opcode Encoding" \
        --intro "How each 65C64 instruction is encoded: the 16 bit opcode on the left and the 16 bit operand word that follows it on the right.  Reproduced from the 'Opcode encoding' sheet of the 65C64 Instruction Architecture spreadsheet." \
        > /dev/null
}

check() {
    python3 tools/check_instruction_tables.py
    .venv/bin/python tools/check_project_items.py
}

build() {
    generate
    check
    .venv/bin/mkdocs build --strict
}

clean() {
    rm -rf site .staging
}

case "${1:-serve}" in
    live)
        generate
        exec .venv/bin/mkdocs serve --dev-addr "127.0.0.1:${PORT}"
        ;;
    build)
        build
        ;;
    rebuild)
        clean
        build
        ;;
    clean)
        clean
        ;;
    serve)
        build
        echo "Serving site/ on http://127.0.0.1:${PORT}/ (Ctrl+C to stop)"
        exec python3 -m http.server "${PORT}" --bind 127.0.0.1 --directory site
        ;;
    *)
        echo "Usage: $0 [serve|live|build|rebuild|clean]" >&2
        exit 1
        ;;
esac
