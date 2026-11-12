#!/usr/bin/env python3
"""Check that the 65C64 instruction summary agrees with the instruction pages.

docs/65c64/instructions/index.md repeats each instruction's operation and flags from the
instruction's own page.  This fails (exit code 1) when a row of the summary has an operation
and flags that its page doesn't give, so the two can't drift apart.

    python3 tools/check_instruction_tables.py
"""

import os
import re
import sys

DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', '65c64', 'instructions')
LINK = re.compile(r'\[`(\w+)`\]\(([\w-]+\.md)(#[\w-]+)?\)')


def tables(path):
    """Yields each Markdown table in the file as (header cells, list of row cells)."""
    header = None
    rows = []
    with open(path, encoding='utf-8') as file:
        for line in file:
            line = line.strip()
            if line.startswith('|') and line.endswith('|'):
                cells = [cell.strip() for cell in line[1:-1].split('|')]
                if header is None:
                    header = cells
                elif not all(re.fullmatch(r':?-+:?', cell) for cell in cells):
                    rows.append(cells)
            elif header is not None:
                yield header, rows
                header = None
                rows = []
    if header is not None:
        yield header, rows


def page_operations(path):
    """The (operation, flags) pairs a page gives.  A page table without a Flags column has no flags."""
    pairs = set()
    for header, rows in tables(path):
        if 'Operation' not in header:
            continue
        operation = header.index('Operation')
        flags = header.index('Flags') if 'Flags' in header else None
        for row in rows:
            pairs.add((row[operation], row[flags] if flags is not None else ''))
    return pairs


def main():
    errors = 0
    checked = 0
    pages = {}
    for header, rows in tables(os.path.join(DOCS, 'index.md')):
        if 'Operation' not in header:
            continue
        operation = header.index('Operation')
        flags = header.index('Flags') if 'Flags' in header else None
        for row in rows:
            link = LINK.search(row[0])
            if link is None:
                continue
            page = link.group(2)
            if page not in pages:
                pages[page] = page_operations(os.path.join(DOCS, page))
            pair = (row[operation], row[flags] if flags is not None else '')
            checked += 1
            if pair not in pages[page]:
                errors += 1
                print('index.md: `%s` is "%s" with flags "%s", which %s does not give' % (link.group(1), pair[0], pair[1], page))
    print('%d instruction rows checked, %d differ from their pages' % (checked, errors))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
