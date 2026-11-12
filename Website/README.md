# Codaphela documents

MkDocs (Material theme) site source.

    ./web.sh           # build, then serve site/ as static files on http://127.0.0.1:8000
    ./web.sh live      # live preview while editing docs/
    ./web.sh build     # build only: copy site/ to the web server

PORT=8080 ./web.sh uses another port.  web.sh creates the venv if it is missing; to do it by hand:

    python3 -m venv .venv
    .venv/bin/pip install mkdocs-material mkdocs-glightbox mkdocs-table-reader-plugin openpyxl

The 65C64 Opcode Encoding page is generated from the spreadsheet; rerun this after changing the
'Opcode encoding' sheet:

    .venv/bin/python tools/sheet_to_html.py "../W65C64/65C64 Instruction Architecture.xlsx" \
        "Opcode encoding" B8:AJ364 docs/65c64/opcode-encoding.md --title "Opcode Encoding" \
        --intro "How each 65C64 instruction is encoded: the 16 bit opcode on the left and the 16 bit operand word that follows it on the right.  Reproduced from the 'Opcode encoding' sheet of the 65C64 Instruction Architecture spreadsheet."

The 65C64 pages are checked in two ways:

    python3 tools/check_instruction_tables.py

fails when the instruction summary (docs/65c64/instructions/index.md) gives an operation or flags
that the instruction's own page doesn't; web.sh runs it before every build.  TestAssembler65Lib
(TestAsm65C64Website.cpp) assembles every ```asm example in docs/65c64, so fence a syntax
summary that isn't real source as ```text.

The Worked Examples page includes the feature programs from
../Tests/TestAssembler65Lib/TestAssembler65Lib/Input (pymdownx.snippets, base_path in mkdocs.yml).
