#!/usr/bin/env python3
"""Check the memory map and the registers' addresses against the HCP65's address decode board (DocsPlan.txt Ie).

LogiCim simulates the HCP65 MPU Address Decode board's schematic (LogiCim --decode, KicadImporterLib's
CKicadSchemLogicimDecode): it drives every combination of the board's address lines and modes and prints each chip
select's ranges, a line each:

    <mode>\t<output>\t<first>\t<last>[\t<every>\t<through>]

The mode is the board's mode pins and their levels ("~{Reset}=1,Kernal Mode=1,RAM Swap=1"); the second form is ranges
as long as the first, every <every> bytes through <through>.  This checks against them:

  - memory-map.md's kernel mode table (Address | Size | What): each row's chip selects are active over its addresses,
    split evenly between them ("Devices 1-7" is seven 32KB slots in order), a row of Nothing has none, and no chip
    select is active anywhere the table doesn't give it.  The table is kernel mode out of reset after the Native Latch
    (RAM Swap high).
  - memory-map.md's RAM and ROM swap table: its columns before and after the Native Latch (RAM Swap low and high).
  - registers.md's registers (Register | Address | Name | Access): register n is ~{Register<n>}, first at its address,
    and repeating as the page says ("repeat every 16 bytes through to `$107FFF`").

A difference is printed as "<page>:<line>:1: error: ..." (which the IDE's Build pane opens) and fails it.  LogiCim is
$LOGICIM, or build/LogiCimRelease/LogiCim beside the repositories (Linux only: --decode needs no window there).

    .venv/bin/python tools/check_decode.py
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402

BOARD = os.path.join(site_sources.ROOT, 'HCP65', 'Kicad', 'Computer', 'Processor', 'HCP65 MPU Address Decode',
                     'HCP65 MPU Address Decode.kicad_sch')
LOGICIM_FOLDER = os.path.join(site_sources.ROOT, 'LogiCim', 'LogiCim', 'LogiCim')     # LogiCim's resources are here
PAGES = os.path.join(site_sources.ROOT, 'HCP65', 'HCP65-Emulator', 'HCP65Lib', 'Docs', 'hcp65')
MEMORY_MAP = os.path.join(PAGES, 'memory-map.md')
REGISTERS = os.path.join(PAGES, 'registers.md')

KERNEL = {'~{Reset}': '1', 'Kernal Mode': '1', 'RAM Swap': '1'}
BEFORE_LATCH = {'~{Reset}': '1', 'Kernal Mode': '1', 'RAM Swap': '0'}

# What a memory map row says, as the chip selects that are it (a row of several is split evenly between them).
WHAT = [(re.compile(r'^MPU RAM$'), lambda match: ['~{RAM}']),
        (re.compile(r'^MPU ROM$'), lambda match: ['~{ROM}']),
        (re.compile(r'^On-board registers\b'), lambda match: ['~{Registers}']),
        (re.compile(r'^Devices (\d+)–(\d+)$'),
         lambda match: ['~{Device%d}' % n for n in range(int(match.group(1)), int(match.group(2)) + 1)]),
        (re.compile(r'^Main memory$'), lambda match: ['~{Main Memory}']),
        (re.compile(r'^Nothing$'), lambda match: [])]
# The chip selects a row can name: any of them active where no row gives it is a difference.
CHIP_SELECT = re.compile(r'^~\{(RAM|ROM|Registers|Device\d+|Main Memory)\}$')
ADDRESS = re.compile(r'`\$([0-9A-Fa-f]+)`(?:–`\$([0-9A-Fa-f]+)`)?')
LINK = re.compile(r'\[([^\]]*)\]\([^)]*\)')
REPEAT = re.compile(r'repeat\s+every\s+(\d+)\s+bytes\s+through\s+to\s+`\$([0-9A-Fa-f]+)`')


class DecodeError(Exception):
    pass


def program():
    explicit = os.environ.get('LOGICIM')
    if explicit:
        return explicit
    return os.path.join(site_sources.ROOT, 'build', 'LogiCimRelease', 'LogiCim')


def decode():
    """LogiCim --decode's lines for the board."""
    executable = program()
    if not os.path.isfile(executable):
        raise DecodeError('%s is not built: the decode check needs it.  Build it with\n'
                          '  cmake -S LogiCim/LogiCim -B build/LogiCimRelease -DCMAKE_BUILD_TYPE=Release\n'
                          '  cmake --build build/LogiCimRelease\nfrom the folder holding the repositories, or set '
                          'LOGICIM to where it is.' % executable)
    try:
        result = subprocess.run([executable, '--decode', BOARD], cwd=LOGICIM_FOLDER, capture_output=True, text=True,
                                timeout=300)
    except subprocess.TimeoutExpired:
        raise DecodeError('%s --decode didn\'t finish: is it older than its --decode?  Build it again.' % executable)
    if result.returncode != 0:
        raise DecodeError('%s:1:1: error: LogiCim --decode failed: %s' % (BOARD, result.stderr.strip()))
    return result.stdout


def parse_ranges(text):
    """The decode's lines as {(mode, output): [(first, last), ...]}, the mode a frozenset of (pin, level), each
    repeating range expanded."""
    ranges = {}
    for line in text.splitlines():
        cells = line.split('\t')
        if len(cells) not in (4, 6):
            raise DecodeError('LogiCim --decode printed a line check_decode.py can\'t read: %r' % line)
        mode = frozenset(tuple(pin.split('=', 1)) for pin in cells[0].split(',') if pin)
        first, last = int(cells[2].lstrip('$'), 16), int(cells[3].lstrip('$'), 16)
        found = ranges.setdefault((mode, cells[1]), [])
        every, through = (0, last)
        if len(cells) == 6:
            every, through = int(cells[4].lstrip('$'), 16), int(cells[5].lstrip('$'), 16)
        while last <= through:
            found.append((first, last))
            if not every:
                break
            first, last = first + every, last + every
    return ranges


def active(ranges, mode, output):
    return ranges.get((frozenset(mode.items()), output), [])


def within(intervals, first, last):
    """The parts of the intervals between first and last."""
    return [(max(a, first), min(b, last)) for a, b in intervals if a <= last and b >= first]


def outside(intervals, allowed):
    """The parts of the intervals that none of the allowed intervals cover."""
    left = []
    for a, b in intervals:
        for c, d in sorted(allowed):
            if d < a or c > b:
                continue
            if c > a:
                left.append((a, c - 1))
            a = d + 1
            if a > b:
                break
        if a <= b:
            left.append((a, b))
    return left


def show(intervals):
    shown = ', '.join('$%06X' % a if a == b else '$%06X–$%06X' % (a, b) for a, b in intervals[:4])
    if not shown:
        return 'nowhere'
    return 'at ' + ((shown + ', ...') if len(intervals) > 4 else shown)


def table(text, heading):
    """The rows of the first table under the heading (a line of the page), as (line number, cells)."""
    lines = text.split('\n')
    start = next((i for i, line in enumerate(lines) if line.strip() == heading), None)
    if start is None:
        return []
    rows = []
    for number in range(start + 1, len(lines)):
        line = lines[number].strip()
        if line.startswith('#'):
            break
        if line.startswith('|'):
            cells = [cell.strip() for cell in line.strip('|').split('|')]
            if not all(set(cell) <= set('-: ') for cell in cells):
                rows.append((number + 1, cells))
        elif rows:
            break
    return rows


def addresses(cell):
    match = ADDRESS.search(cell)
    if not match:
        return None
    return int(match.group(1), 16), int(match.group(2) or match.group(1), 16)


def selects(what):
    """The chip selects a memory map row's What is, or None for one the check doesn't know."""
    plain = LINK.sub(r'\1', what).strip()
    for pattern, outputs in WHAT:
        match = pattern.search(plain)
        if match:
            return outputs(match)
    return None


def check_row(ranges, mode, first, last, outputs, known):
    """A row's differences: each of its chip selects is active over its share of the row, and no other is active in
    the row.  Returns (problems, each select's share)."""
    problems = []
    shares = {}
    size = (last - first + 1) // len(outputs) if outputs else 0
    for index, output in enumerate(outputs):
        share = (first + index * size, first + (index + 1) * size - 1)
        shares[output] = share
        got = within(active(ranges, mode, output), first, last)
        if got != [share]:
            problems.append('the decode has %s %s, not $%06X–$%06X' % (output, show(got), share[0], share[1]))
    for output in sorted(known - set(outputs)):
        got = within(active(ranges, mode, output), first, last)
        if got:
            problems.append('the decode also has %s %s' % (output, show(got)))
    return problems, shares


def check_memory_map(ranges, text):
    """memory-map.md's problems, as (line, message), and how many rows were checked."""
    problems = []
    rows = table(text, '## Kernel mode')
    if not rows:
        return [(1, 'there is no table under "## Kernel mode"')], 0
    known = {output for mode, output in ranges if mode == frozenset(KERNEL.items()) and CHIP_SELECT.match(output)}
    given = {}
    for line, cells in rows[1:]:
        span, outputs = addresses(cells[0]), selects(cells[-1])
        if span is None or outputs is None:
            problems.append((line, 'check_decode.py can\'t read this row: what %s is on the board' % cells[-1]))
            continue
        found, shares = check_row(ranges, KERNEL, span[0], span[1], outputs, known)
        problems += [(line, '`$%06X`–`$%06X` %s: %s' % (span[0], span[1], cells[-1], problem)) for problem in found]
        for output, share in shares.items():
            given.setdefault(output, []).append(share)
    for output in sorted(known):
        extra = outside(active(ranges, KERNEL, output), given.get(output, []))
        if extra:
            problems.append((rows[0][0], 'the decode has %s %s too, which the table doesn\'t give it' %
                             (output, show(extra))))

    swap = table(text, '### RAM and ROM swap at boot')
    for line, cells in swap[1:]:
        span = addresses(cells[0])
        for mode, what in ((BEFORE_LATCH, cells[1]), (KERNEL, cells[2])):
            outputs = selects(what)
            if span is None or outputs is None:
                problems.append((line, 'check_decode.py can\'t read this row: what %s is on the board' % what))
                continue
            found, _ = check_row(ranges, mode, span[0], span[1], outputs, {'~{RAM}', '~{ROM}'})
            problems += [(line, '`$%06X`–`$%06X` %s with RAM Swap=%s: %s' %
                          (span[0], span[1], what, mode['RAM Swap'], problem)) for problem in found]
    return problems, len(rows) - 1 + max(len(swap) - 1, 0)


def check_registers(ranges, text):
    """registers.md's problems, as (line, message): register n is ~{Register<n>}, at its address first and repeating
    as the page says; and how many rows were checked."""
    problems = []
    checked = 0
    lines = text.split('\n')
    repeat = next(((number + 1, match) for number, line in enumerate(lines)
                   for match in [REPEAT.search(line)] if match), None)
    if repeat is None:
        return [(1, 'the page doesn\'t say how the registers repeat ("repeat every N bytes through to `$...`")')], 0
    every, through = int(repeat[1].group(1)), int(repeat[1].group(2), 16)
    for line, cells in table(text, '| Register | Address | Name | Access |'):
        numbers = re.match(r'^(\d+)(?:–(\d+))?$', cells[0])
        span = addresses(cells[1])
        if not numbers or span is None:
            continue
        low, high = int(numbers.group(1)), int(numbers.group(2) or numbers.group(1))
        checked += 1
        if span[1] - span[0] != high - low:
            problems.append((line, 'registers %d–%d are %d addresses' % (low, high, span[1] - span[0] + 1)))
            continue
        for register in range(low, high + 1):
            address = span[0] + register - low
            output = '~{Register%d}' % register
            got = active(ranges, KERNEL, output)
            expected = [(a, a) for a in range(address, through + 1, every)]
            if got != expected:
                problems.append((line, 'register %d is `$%06X`, repeating every %d bytes through `$%06X` (line %d); '
                                       'the decode has %s %s' % (register, address, every, through, repeat[0],
                                                                    output, show(got))))
    return problems, checked


def main():
    try:
        ranges = parse_ranges(decode())
    except DecodeError as error:
        print(error)
        return 1
    problems = []
    checked = 0
    for path, check in ((MEMORY_MAP, check_memory_map), (REGISTERS, check_registers)):
        with open(path, encoding='utf-8') as file:
            found, rows = check(ranges, file.read())
        problems += ['%s:%d:1: error: %s' % (path, line, message) for line, message in found]
        checked += rows
    for problem in problems:
        print(problem)
    print('%d rows checked against the address decode board, %d differences' % (checked, len(problems)))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
