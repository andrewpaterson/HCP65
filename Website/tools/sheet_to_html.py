#!/usr/bin/env python3
"""Render one sheet of an .xlsx workbook as an HTML table inside a Markdown page.

Keeps the sheet's merged cells, fill colours, thin borders, alignment and line breaks, so a
spreadsheet layout (such as the 65C64 opcode encoding) can be reproduced on the website.  Formula
cells show the value the spreadsheet last calculated.

    .venv/bin/python tools/sheet_to_html.py "../W65C64/65C64 Instruction Architecture.xlsx" \
        "Opcode encoding" B8:AJ364 docs/65c64/opcode-encoding.md --title "Opcode Encoding"

Requires openpyxl (.venv/bin/pip install openpyxl).
"""

import argparse
import html
import sys

import openpyxl
from openpyxl.utils import range_boundaries

# Spreadsheet fill colours mapped to CSS classes (styled in docs/stylesheets/extra.css).
FILL_CLASSES = {
    "FFDDDDDD": "x-f3",  # Field heading.
    "FFEEEEEE": "x-f2",  # Field body.
    "FFF2F2F2": "x-f1",  # Operand access body.
    "FFFFFFA6": "x-fy",  # Highlight.
    "FFFFFFD7": "x-fy",
}


def cell_text(value):
    if value is None:
        return ""
    if isinstance(value, float) and value.is_integer():
        value = int(value)
    return "<br>".join(html.escape(line) for line in str(value).strip("\n").split("\n"))


def column_widths(sheet, first_col, last_col):
    default = sheet.sheet_format.defaultColWidth or 8.43
    widths = {c: default for c in range(first_col, last_col + 1)}
    for dimension in sheet.column_dimensions.values():
        if dimension.width:
            for c in range(dimension.min or 0, (dimension.max or 0) + 1):
                if c in widths:
                    widths[c] = dimension.width
    return widths


def render(workbook_path, sheet_name, cell_range):
    values = openpyxl.load_workbook(workbook_path, data_only=True)[sheet_name]
    sheet = openpyxl.load_workbook(workbook_path)[sheet_name]
    first_col, first_row, last_col, last_row = range_boundaries(cell_range)

    # Merged ranges, keyed by their top left cell, and the cells they hide.
    anchors, hidden = {}, set()
    for merged in sheet.merged_cells.ranges:
        if merged.min_row > last_row or merged.max_row < first_row:
            continue
        if merged.min_col > last_col or merged.max_col < first_col:
            continue
        anchors[(merged.min_row, merged.min_col)] = merged
        for r in range(merged.min_row, merged.max_row + 1):
            for c in range(merged.min_col, merged.max_col + 1):
                if (r, c) != (merged.min_row, merged.min_col):
                    hidden.add((r, c))

    widths = column_widths(sheet, first_col, last_col)
    total = sum(widths.values())
    out = ['<div class="x-sheet-scroll">', '<table class="x-sheet">', "<colgroup>"]
    for c in range(first_col, last_col + 1):
        out.append(f'<col style="width:{widths[c] / total * 100:.2f}%">')
    out.append("</colgroup>")

    for r in range(first_row, last_row + 1):
        cells = []
        for c in range(first_col, last_col + 1):
            if (r, c) in hidden:
                continue
            cell = sheet.cell(r, c)
            merged = anchors.get((r, c))
            bottom_row, right_col = (merged.max_row, merged.max_col) if merged else (r, c)
            span_rows = min(bottom_row, last_row) - r + 1
            span_cols = min(right_col, last_col) - c + 1

            classes = []
            fill = cell.fill.fgColor.rgb if cell.fill and cell.fill.fill_type else None
            if isinstance(fill, str) and fill in FILL_CLASSES:
                classes.append(FILL_CLASSES[fill])
            # A merged range's right and bottom borders live on its right and bottom cells.
            if cell.border.left.style:
                classes.append("x-bl")
            if cell.border.top.style:
                classes.append("x-bt")
            if sheet.cell(r, right_col).border.right.style:
                classes.append("x-br")
            if sheet.cell(bottom_row, c).border.bottom.style:
                classes.append("x-bb")
            horizontal = cell.alignment.horizontal
            if horizontal == "center":
                classes.append("x-c")
            elif horizontal == "right":
                classes.append("x-r")
            if cell.alignment.vertical == "center":
                classes.append("x-m")
            if cell.font.b:
                classes.append("x-b")
            if cell.font.color is not None and cell.font.color.rgb == "FF666666":
                classes.append("x-muted")

            attributes = ""
            if span_rows > 1:
                attributes += f' rowspan="{span_rows}"'
            if span_cols > 1:
                attributes += f' colspan="{span_cols}"'
            if classes:
                attributes += f' class="{" ".join(classes)}"'
            cells.append(f"<td{attributes}>{cell_text(values.cell(r, c).value)}</td>")
        out.append("<tr>" + "".join(cells) + "</tr>")

    out += ["</table>", "</div>"]
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("workbook")
    parser.add_argument("sheet")
    parser.add_argument("range", help="cell range to render, for example B8:AJ364")
    parser.add_argument("output", help="Markdown page to write")
    parser.add_argument("--title", required=True)
    parser.add_argument("--intro", default="", help="Markdown placed between the title and the table")
    arguments = parser.parse_args()

    table = render(arguments.workbook, arguments.sheet, arguments.range)
    source = f"{arguments.sheet}' sheet of '{arguments.workbook.rsplit('/', 1)[-1]}"
    page = [
        "---",
        "hide:",
        "  - navigation",
        "  - toc",
        "---",
        "",
        f"<!-- Generated by tools/sheet_to_html.py from the '{source}'.  Do not edit by hand. -->",
        "",
        f"# {arguments.title}",
        "",
    ]
    if arguments.intro:
        page += [arguments.intro, ""]
    page += [table, ""]
    with open(arguments.output, "w", encoding="utf-8") as output:
        output.write("\n".join(page))
    print(f"Wrote {arguments.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
