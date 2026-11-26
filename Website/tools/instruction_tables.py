#!/usr/bin/env python3
"""The 65C64's instruction tables, made from its spreadsheet (DocsPlan.txt C1, C3).

The spreadsheet (X65C64-Proposal/65C64 Instruction Architecture.xlsx) is the source of truth.  Its Instructions sheet
has a row a numbered instruction (column B): name, mnemonic, instruction, extend and sub codes, class, the W65C816
instructions it replaces, behaviour, flags and notes.  A merged cell's value is every row's it covers; a row whose
behaviour is merged from the row above carries on that row's W65C816 list.

X65C64-Proposal/InstructionTables.yml says which rows each of the website's tables has.  Each table is written to
X65C64-Proposal/Generated/<table>.txt, which its page includes (--8<-- "<table>.txt"), and every row to
Generated/Instructions.txt, a plain text table: readable diffs of the spreadsheet, and what TestAssembler65Lib checks
the assembler and the simulator against.  A file is written only when it changes; --check writes nothing and fails
if one would change.

It also checks (C2) that every mnemonic an instruction page's title names is in the sheet and every one the sheet
has is in a title, and that each index row links to the page whose table has its row.  Problems are printed as
"<file>:<line>:1: error:" lines, and fail it.

    .venv/bin/python tools/instruction_tables.py [--check]
"""
import argparse
import collections
import os
import re
import sys
import textwrap

import openpyxl
import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402

PROPOSAL = os.path.join(site_sources.ROOT, 'HCP65', 'X65C64-Proposal')
WORKBOOK = os.path.join(PROPOSAL, '65C64 Instruction Architecture.xlsx')
SPEC = os.path.join(PROPOSAL, 'InstructionTables.yml')
PAGES = os.path.join(PROPOSAL, 'Docs', '65c64', 'instructions')
TITLE = re.compile(r'^# ([a-z, ]+) - ')
GENERATED = os.path.join(PROPOSAL, 'Generated')
SHEET = 'Instructions'

# The sheet's columns, from B.
COLUMNS = ('number', 'name', 'mnemonic', 'code', 'extend', 'sub', 'kind', 'w65c816', 'operation', 'flags', 'notes')
FIRST_COLUMN = 2
FORMS = re.compile(r'^(.*?) \((zero extend|sign extend|unsigned|signed)\)$')
MARK = ('Generated from the Instructions sheet of "65C64 Instruction Architecture.xlsx" by '
        'HCP65/Website/tools/instruction_tables.py, as X65C64-Proposal/InstructionTables.yml says.  Don\'t edit it: '
        'change the spreadsheet.')

Instruction = collections.namedtuple('Instruction', COLUMNS)


class InstructionTableError(Exception):
    pass


def text(value):
    """A cell's text on one line, its white space runs one space; '' for an empty cell."""
    if value is None:
        return ''
    if isinstance(value, float) and value.is_integer():
        value = int(value)
    return ' '.join(str(value).split())


def read_instructions(path=WORKBOOK):
    """The sheet's numbered rows, by number, in order."""
    sheet = openpyxl.load_workbook(path, data_only=True)[SHEET]
    merged_from = {}    # (row, column) -> the merged range's first row
    for merged in sheet.merged_cells.ranges:
        for row in range(merged.min_row, merged.max_row + 1):
            for column in range(merged.min_col, merged.max_col + 1):
                merged_from[(row, column)] = merged.min_row
    last_column = FIRST_COLUMN + len(COLUMNS) - 1

    instructions = collections.OrderedDict()
    previous = None
    for row in range(1, sheet.max_row + 1):
        number = text(sheet.cell(row, FIRST_COLUMN).value)
        if not number.isdigit():
            continue
        values = []
        for column in range(FIRST_COLUMN, last_column + 1):
            first = merged_from.get((row, column), row)
            values.append(text(sheet.cell(first, column).value))
        instruction = Instruction(*values)
        operation_column = FIRST_COLUMN + COLUMNS.index('operation')
        if previous is not None and merged_from.get((row, operation_column), row) < row:
            # The row above's instruction, carried on: its W65C816 list goes on.
            joined = ' '.join(part for part in (previous.w65c816, instruction.w65c816) if part)
            previous = previous._replace(w65c816=joined)
            instructions[previous.number] = previous
            continue
        if not instruction.mnemonic:
            raise InstructionTableError('%s row %d (%s) has no mnemonic' % (SHEET, row, number))
        instructions[instruction.number] = instruction
        previous = instruction
    return instructions


def cell(instruction, kind, link, as_text, previous):
    """What a column of a table shows for an instruction."""
    match = FORMS.match(instruction.name)
    if kind == 'link':
        return '[`%s`](%s)' % (instruction.mnemonic, link)
    if kind == 'mnemonic':
        if previous is not None and previous.mnemonic == instruction.mnemonic:
            return ''
        return '`#!text %s`' % instruction.mnemonic if as_text else '`%s`' % instruction.mnemonic
    if kind == 'name':
        return match.group(1) if match else instruction.name
    if kind == 'form':
        return match.group(2).capitalize() if match else ''
    if kind in ('operation', 'flags', 'w65c816'):
        return getattr(instruction, kind)
    raise InstructionTableError('InstructionTables.yml: no column kind %s' % kind)


def table(name, spec, instructions):
    """A table as Markdown, its mark first."""
    columns = spec['columns']
    kinds = [kind for kind, _ in columns]
    lines = ['<!-- %s -->' % MARK, '',
             '| %s |' % ' | '.join(heading for _, heading in columns),
             '|%s|' % '|'.join('---' for _ in columns)]
    previous = None
    for entry in spec['rows']:
        if isinstance(entry, dict):
            number, link, as_text = entry['row'], entry.get('link'), entry.get('text', False)
        elif isinstance(entry, list):
            number, link, as_text = entry[0], entry[1], False
        else:
            number, link, as_text = entry, None, False
        instruction = instructions.get(str(number))
        if instruction is None:
            raise InstructionTableError('InstructionTables.yml: %s names row %s, which the sheet hasn\'t' % (name, number))
        shown = previous if 'form' in kinds else None
        cells = [cell(instruction, kind, link, as_text, shown) for kind in kinds]
        lines.append('|%s|' % '|'.join(' %s ' % value if value else ' ' for value in cells))
        previous = instruction
    return '\n'.join(lines) + '\n'


def instruction_list(instructions):
    """Generated/Instructions.txt: a line a row, tab separated."""
    header = ('The 65C64\'s instructions.  %s  A line a numbered row of the sheet: its number, mnemonic, instruction '
              'code, extend code, sub code (- where the sheet has none) and name, tab separated.' % MARK)
    lines = ['# ' + line for line in textwrap.wrap(header, 116)]
    for instruction in instructions.values():
        lines.append('\t'.join([instruction.number, instruction.mnemonic, instruction.code or '-',
                                instruction.extend.replace(' ', ',') or '-', instruction.sub or '-', instruction.name]))
    return '\n'.join(lines) + '\n'


def outputs(instructions, spec):
    """Each file to write, by path, with its text."""
    files = {os.path.join(GENERATED, 'Instructions.txt'): instruction_list(instructions)}
    for name, table_spec in spec.items():
        files[os.path.join(GENERATED, name + '.txt')] = table(name, table_spec, instructions)
    return files


def check_pages(instructions, spec):
    """The instruction pages' titles name the sheet's mnemonics, all of them; each index row links to the page whose
    table has its row.  The problems, as "<file>:<line>:1: error:" lines."""
    problems = []
    mnemonics = {instruction.mnemonic for instruction in instructions.values()}
    titled = set()
    for name in sorted(os.listdir(PAGES)):
        if not name.endswith('.md') or name == 'index.md':
            continue
        path = os.path.join(PAGES, name)
        with open(path, encoding='utf-8') as file:
            match = TITLE.match(file.readline())
        if match is None:
            problems.append('%s:1:1: error: the title doesn\'t name its instructions ("# cpy - Copy")' % path)
            continue
        for mnemonic in match.group(1).split(', '):
            titled.add(mnemonic)
            if mnemonic not in mnemonics:
                problems.append('%s:1:1: error: %s isn\'t an instruction of the spreadsheet\'s Instructions sheet' % (path, mnemonic))
    for mnemonic in sorted(mnemonics - titled):
        problems.append('%s:1:1: error: no instruction page\'s title names %s' % (PAGES, mnemonic))
    for name, table_spec in spec.items():
        if not name.startswith('index-'):
            continue
        for entry in table_spec['rows']:
            number, link = entry[0], entry[1]
            page = link.split('#')[0][:-len('.md')]
            rows = [row['row'] if isinstance(row, dict) else row for row in spec.get(page, {}).get('rows', [])]
            if number not in rows:
                problems.append('%s:1:1: error: %s links row %s to %s, whose table hasn\'t it' % (SPEC, name, number, link))
    return problems


def read_spec(path=SPEC):
    with open(path, encoding='utf-8') as file:
        return yaml.safe_load(file)


def write(files, check=False):
    """Writes the files that changed; returns their paths.  With check, writes nothing."""
    changed = []
    for path, content in sorted(files.items()):
        old = None
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as file:
                old = file.read()
        if old != content:
            changed.append(path)
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'w', encoding='utf-8', newline='\n') as file:
                    file.write(content)
    return changed


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--check', action='store_true', help='write nothing; fail if a file would change')
    arguments = parser.parse_args()
    try:
        instructions = read_instructions()
        spec = read_spec()
        changed = write(outputs(instructions, spec), arguments.check)
    except InstructionTableError as error:
        raise SystemExit('%s:1:1: error: %s' % (WORKBOOK, error))
    for path in changed:
        print('%s: %s' % ('would change' if arguments.check else 'written', os.path.relpath(path, site_sources.ROOT)))
    problems = check_pages(instructions, spec)
    for problem in problems:
        print(problem)
    if problems or (arguments.check and changed):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
