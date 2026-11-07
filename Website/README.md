# Codaphela documents

MkDocs (Material theme) site source.

    .venv/bin/mkdocs serve     # live preview on http://127.0.0.1:8000
    .venv/bin/mkdocs build     # static site into site/, copy that to the web server

Recreate the venv with:

    python3 -m venv .venv
    .venv/bin/pip install mkdocs-material mkdocs-glightbox mkdocs-table-reader-plugin openpyxl

The 65C64 Opcode Encoding page is generated from the spreadsheet; rerun this after changing the
'Opcode encoding' sheet:

    .venv/bin/python tools/sheet_to_html.py "../W65C64/65C64 Instruction Architecture.xlsx" \
        "Opcode encoding" B8:AJ364 docs/65c64/opcode-encoding.md --title "Opcode Encoding" \
        --intro "How each 65C64 instruction is encoded: the 16 bit opcode on the left and the 16 bit operand word that follows it on the right.  Reproduced from the 'Opcode encoding' sheet of the 65C64 Instruction Architecture spreadsheet."
