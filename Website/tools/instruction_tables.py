#!/usr/bin/env python3
"""The 65C64's instruction tables, made from its spreadsheet (DocsPlan.txt C1, C3).

The spreadsheet (X65C64-Proposal/65C64 Instruction Architecture.xlsx) is the source of truth.  Its Instructions sheet
has a row a numbered instruction (column B): name, mnemonic, instruction, extend and sub codes, class, the W65C816
instructions it replaces, behaviour, flags and notes.  A merged cell's value is every row's it covers; a row whose
behaviour is merged from the row above carries on that row's W65C816 list.

X65C64-Proposal/InstructionTables.yml says which rows each of the website's tables has; a table that names another
sheet (Hardware registers) is that sheet's rows, from its second, its columns from A the fields it lists.  Each table is written to
X65C64-Proposal/Generated/<table>.txt, which its page includes (--8<-- "<table>.txt"), and every row to
Generated/Instructions.txt, a plain text table: readable diffs of the spreadsheet, and what TestAssembler65Lib checks
the assembler and the simulator against.  A file is written only when it changes; --check writes nothing and fails
if one would change.

It also checks (C2) that every mnemonic an instruction page's title names is in the sheet and every one the sheet
has is in a title, and that each index row links to the page whose table has its row; and the pages the sheets drawn
as diagrams describe (the user, 2026-11-26: those pages stay hand-written and are checked): operands.md's modes (number,
name and type) are the Operand encoding sheet's and the Addressing modes sheet's, and registers.md's registers, their
widths and the status register's bits are the Registers sheet's.  Problems are printed as "<file>:<line>:1: error:"
lines, and fail it.

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
OPERANDS_PAGE = os.path.join(PROPOSAL, 'Docs', '65c64', 'operands.md')
REGISTERS_PAGE = os.path.join(PROPOSAL, 'Docs', '65c64', 'registers.md')
REGISTER = re.compile(r'^(?:Register|Stack Pointer|Status|Program Counter) (\w+) +\((\d+) bit\)')
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


def load(path=WORKBOOK):
    """The spreadsheet, with the values its formulas last had."""
    return openpyxl.load_workbook(path, data_only=True)


def read_instructions(workbook):
    """The Instructions sheet's numbered rows, by number, in order."""
    sheet = workbook[SHEET]
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


def sheet_table(name, spec, workbook):
    """A table of a plain sheet (spec's sheet): a row each of its rows from the second with a first cell, its cells
    from A the spec's fields."""
    if spec['sheet'] not in workbook.sheetnames:
        raise InstructionTableError('InstructionTables.yml: %s names sheet %s, which the spreadsheet hasn\'t' % (name, spec['sheet']))
    sheet = workbook[spec['sheet']]
    fields = spec['fields']
    columns = spec['columns']
    lines = ['<!-- %s -->' % MARK.replace('the Instructions sheet', 'the %s sheet' % spec['sheet']), '',
             '| %s |' % ' | '.join(heading for _, heading in columns),
             '|%s|' % '|'.join('---' for _ in columns)]
    for row in range(2, sheet.max_row + 1):
        values = dict(zip(fields, (text(sheet.cell(row, column + 1).value) for column in range(len(fields)))))
        if not values[fields[0]]:
            continue
        lines.append('|%s|' % '|'.join(' %s ' % values[kind] if values[kind] else ' ' for kind, _ in columns))
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


def outputs(workbook, instructions, spec):
    """Each file to write, by path, with its text."""
    files = {os.path.join(GENERATED, 'Instructions.txt'): instruction_list(instructions)}
    for name, table_spec in spec.items():
        if 'sheet' in table_spec:
            files[os.path.join(GENERATED, name + '.txt')] = sheet_table(name, table_spec, workbook)
        else:
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
        if not name.startswith('index-') or 'sheet' in table_spec:
            continue
        for entry in table_spec['rows']:
            number, link = entry[0], entry[1]
            page = link.split('#')[0][:-len('.md')]
            rows = [row['row'] if isinstance(row, dict) else row for row in spec.get(page, {}).get('rows', [])]
            if number not in rows:
                problems.append('%s:1:1: error: %s links row %s to %s, whose table hasn\'t it' % (SPEC, name, number, link))
    return problems


def merged_value(sheet, row, column):
    """A cell's text, a merged range's value for any cell of it."""
    for merged in sheet.merged_cells.ranges:
        if merged.min_row <= row <= merged.max_row and merged.min_col <= column <= merged.max_col:
            return text(sheet.cell(merged.min_row, merged.min_col).value)
    return text(sheet.cell(row, column).value)


def page_tables(path):
    """A Markdown page's tables: (the line of its heading, from 1, its cells, and its rows' cells)."""
    tables = []
    with open(path, encoding='utf-8') as file:
        lines = file.read().split('\n')
    for number, line in enumerate(lines, 1):
        if not line.startswith('|'):
            continue
        cells = [part.strip() for part in line.strip()[1:-1].split('|')]
        if number == 1 or not lines[number - 2].startswith('|'):
            tables.append((number, cells, []))
        elif not all(re.fullmatch(r':?-+:?', part) for part in cells):
            tables[-1][2].append(cells)
    return tables


def find_table(path, heading):
    """The page's table with that heading: (its line, its rows), or None."""
    for number, cells, rows in page_tables(path):
        if cells == heading:
            return number, rows
    return None


def check_operands(workbook):
    """operands.md's table of modes against the Operand encoding sheet (name, mode, type, a merged cell's value for
    each of its rows) and the Addressing modes sheet (a mode's number beside the first of its names)."""
    problems = []
    found = find_table(OPERANDS_PAGE, ['Mode', 'Name', 'Type', 'Assembly'])
    if found is None:
        return ['%s:1:1: error: the table of modes (Mode, Name, Type, Assembly) isn\'t there' % OPERANDS_PAGE]
    line, rows = found
    page = [(row[0], re.sub(r'^\[(.*)\]\(.*\)$', r'\1', row[1]), row[2]) for row in rows]

    sheet = workbook['Operand encoding']
    encoded = []
    for row in range(4, sheet.max_row + 1):
        name = text(sheet.cell(row, 1).value)
        if name:
            encoded.append((merged_value(sheet, row, 2), name, merged_value(sheet, row, 3)))
    if page != encoded:
        problems.append('%s:%d:1: error: the modes aren\'t the Operand encoding sheet\'s: %s' %
                        (OPERANDS_PAGE, line, '; '.join('%s %s %s' % entry for entry in encoded)))

    sheet = workbook['Addressing modes']
    drawn = set()
    for column in (3, 9, 15):
        mode = ''
        for row in range(1, sheet.max_row + 1):
            value = text(sheet.cell(row, column).value)
            number = text(sheet.cell(row, column - 1).value)
            if not value.endswith(':') or 'calculation' in value or 'memory' in value or value == 'Instruction:':
                continue
            mode = number or mode
            drawn.add((mode, 'Address ' + value[:-1]))
    addresses = {(mode, name) for mode, name, kind in page if name.startswith('Address ')}
    if addresses != drawn:
        problems.append('%s:%d:1: error: the address modes aren\'t the Addressing modes sheet\'s: %s' %
                        (OPERANDS_PAGE, line, '; '.join('%s %s' % entry for entry in sorted(addresses ^ drawn))))
    return problems


def check_registers(workbook):
    """registers.md's registers and widths, and the status register's bits, against the Registers sheet."""
    problems = []
    sheet = workbook['Registers']
    widths = {}
    for row in sheet.iter_rows(min_row=1, max_row=20):
        for value in row:
            match = REGISTER.match(text(value.value))
            if match and not match.group(1).startswith('D'):    # DI, D and DJ are internal.
                widths[match.group(1)] = match.group(2)
    found = find_table(REGISTERS_PAGE, ['Register', 'Width', 'Usual role'])
    if found is None:
        return ['%s:1:1: error: the table of registers (Register, Width, Usual role) isn\'t there' % REGISTERS_PAGE]
    line, rows = found
    page = {row[0]: row[1] for row in rows}
    if set(page) != set(widths) or any(not page[name].endswith('%s bits' % bits) for name, bits in widths.items()):
        problems.append('%s:%d:1: error: the registers aren\'t the Registers sheet\'s: %s' %
                        (REGISTERS_PAGE, line, ', '.join('%s %s bits' % entry for entry in sorted(widths.items()))))

    bits = {}
    for column in range(2, 18):
        letter = text(sheet.cell(18, column).value).rstrip('.')
        if letter:
            top = text(sheet.cell(17, column).value)
            bits[letter] = top
            if column < 17 and not text(sheet.cell(18, column + 1).value):    # A two bit field: 15–14.
                bits[letter] = '%s–%s' % (top, text(sheet.cell(17, column + 1).value))
    for number, cells, rows in page_tables(REGISTERS_PAGE):
        if cells and cells[0] == 'Bits' and rows and rows[0][0] == 'Flag':
            page_bits = dict(zip(rows[0][1:], cells[1:]))
            if page_bits != bits:
                problems.append('%s:%d:1: error: the status bits aren\'t the Registers sheet\'s: %s' %
                                (REGISTERS_PAGE, number, ', '.join('%s %s' % entry for entry in bits.items())))
            break
    else:
        problems.append('%s:1:1: error: the status register\'s bits (Bits, then Flag) aren\'t there' % REGISTERS_PAGE)
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
        workbook = load()
        instructions = read_instructions(workbook)
        spec = read_spec()
        changed = write(outputs(workbook, instructions, spec), arguments.check)
    except InstructionTableError as error:
        raise SystemExit('%s:1:1: error: %s' % (WORKBOOK, error))
    for path in changed:
        print('%s: %s' % ('would change' if arguments.check else 'written', os.path.relpath(path, site_sources.ROOT)))
    problems = check_pages(instructions, spec) + check_operands(workbook) + check_registers(workbook)
    for problem in problems:
        print(problem)
    if problems or (arguments.check and changed):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
