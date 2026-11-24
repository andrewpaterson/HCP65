# Codaphela documents

MkDocs (Material theme) site source.  The plan for it is DocsPlan.txt.

    ./web.sh           # build, then serve site/ as static files on http://127.0.0.1:8000
    ./web.sh live      # live preview while editing the pages
    ./web.sh build     # build only: copy site/ to the web server
    ./web.sh rebuild   # clean, then build
    ./web.sh clean     # remove site/ and .staging/

PORT=8080 ./web.sh uses another port.  web.sh creates the venv if it is missing; to do it by hand:

    python3 -m venv .venv
    .venv/bin/pip install mkdocs-material mkdocs-glightbox mkdocs-table-reader-plugin openpyxl

Website.sln / Website.vcxproj is the site as a Visual Studio Makefile project: its build is `bash web.sh build`, and
its items are the site's own files.

## Where the pages come from

The pages live in folders that mkdocs.yml's `extra.sources` maps to places in the site:

    extra:
      sources:
        - folder: HCP65/Website/docs    # relative to the folder holding the repositories
          site: ""                      # where it lands in the site ("" is the top)
          branch: main                  # its repository's branch

tools/gather_sources.py (an MkDocs hook, so `mkdocs build` and `mkdocs serve` both use it) copies them into
.staging, laid out as the site, and the site is built from there.  Today every page is in docs/; pages are to move
beside the code they describe, each folder mapped here (DocsPlan.txt step B).

## Generated pages

The 65C64 Opcode Encoding page is generated from the spreadsheet's 'Opcode encoding' sheet by
tools/sheet_to_html.py; web.sh regenerates it before every build and live preview (the command is in web.sh).

## Checks

web.sh runs these before every build; problems are printed as `<file>:<line>:1: error:` lines, which the IDE's Build
pane opens, and MkDocs's own warnings about a page are repeated that way with the page's real path.

    .venv/bin/python tools/check_instruction_tables.py

fails when the instruction summary (the site's 65c64/instructions/index.md) gives an operation or flags that the
instruction's own page doesn't.  Both checks find pages through extra.sources (tools/site_sources.py).

    .venv/bin/python tools/check_project_items.py

fails when a file in a mapped folder isn't an item of exactly one Visual Studio project (DocsPlan.txt decision 10):
a page goes in the project of the code it describes, a page about nothing in particular in Website.vcxproj.

TestAssembler65Lib (TestAsm65C64Website.cpp) assembles every ```asm example in docs/65c64, so fence a syntax summary
that isn't real source as ```text.

The Worked Examples page includes the feature programs from
../Tests/TestAssembler65Lib/TestAssembler65Lib/Input (pymdownx.snippets, base_path in mkdocs.yml).
