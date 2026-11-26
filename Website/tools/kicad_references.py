"""MkDocs hook: a page's references to KiCad parts and nets are checked against the board's schematic (DocsPlan.txt Id).

A page names a part or a net as `#!kicad IC9` or `#!kicad ~Main Access`, on the board it names once in a hidden
comment (<!-- kicad Main Memory Channel -->), or on another board as `#!kicad Audio Clock:~WR_B`.  A board is a
schematic under HCP65/Kicad (not Old) named by its file name (Audio Clock.kicad_sch), or by more of its folder's path
where two have that name (W65C816 Debug Display/LED Display).  A name is compared without KiCad's markup braces:
~{WR}_{B} is ~WR_B.

The DeviceROM program (device_tables.program()) lists a board's parts and named nets (DeviceROM --names).  A reference
the board hasn't is printed as "<real path>:<line>:<column>: error: ..." and fails the strict build; the page shows
the name without its board.
"""
import logging
import os
import re
import subprocess
import sys

from mkdocs.exceptions import PluginError

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import device_tables  # noqa: E402
import site_sources  # noqa: E402
import symbol_table  # noqa: E402

log = logging.getLogger('mkdocs.hooks.kicad_references')

KICAD = os.path.join(site_sources.ROOT, 'HCP65', 'Kicad')
SPAN = re.compile(r'`#!kicad ([^`]+)`')
BOARD = re.compile(r'<!--\s*kicad\s+(.+?)\s*-->')
FENCE = re.compile(r'^\s*(```+|~~~+)')


def plain(name):
    """A name without KiCad's markup braces: ~{WR}_{B} is ~WR_B."""
    return name.replace('{', '').replace('}', '')


def schematics():
    """Every schematic under Kicad but Old, by path relative to Kicad without its extension."""
    found = []
    for directory, directories, names in os.walk(KICAD):
        directories[:] = sorted(name for name in directories if name != 'Old')
        for name in sorted(names):
            if name.endswith('.kicad_sch'):
                found.append(os.path.relpath(os.path.join(directory, name), KICAD)[:-len('.kicad_sch')])
    return found


def find_board(board, known):
    """The schematic a board name means, or an error."""
    matches = [path for path in known
               if any(name == board or name.endswith('/' + board) for name in (path, os.path.dirname(path)))]
    if len(matches) == 1:
        return os.path.join(KICAD, matches[0] + '.kicad_sch'), None
    if not matches:
        return None, 'no schematic under HCP65/Kicad is named %s' % board
    return None, '%s is %d schematics (%s): name more of its path' % (board, len(matches), ', '.join(matches))


def names_of(path):
    """A schematic's parts and named nets (DeviceROM --names), without their braces."""
    executable = device_tables.program()
    if not os.path.isfile(executable):
        raise PluginError('%s is not built: the KiCad references need it (see tools/device_tables.py)' % executable)
    result = subprocess.run([executable, '--names', path], capture_output=True, text=True)
    if result.returncode != 0:
        raise PluginError('DeviceROM --names %s failed: %s' % (path, result.stderr.strip()))
    return {plain(line.split('\t', 1)[1]) for line in result.stdout.splitlines() if '\t' in line}


def references(text):
    """A page's references, (line, column, board or None, name, the span's start and end in its line), and the board
    its hidden comment names; fenced code is left out."""
    found = []
    page_board = None
    fence = None
    for number, line in enumerate(text.split('\n'), 1):
        match = FENCE.match(line)
        if match:
            fence = None if fence and match.group(1)[0] == fence else (fence or match.group(1)[0])
            continue
        if fence:
            continue
        comment = BOARD.search(line)
        if comment and page_board is None:
            page_board = comment.group(1)
        for span in SPAN.finditer(line):
            board, _, name = span.group(1).rpartition(':') if ':' in span.group(1) else (None, None, span.group(1))
            found.append((number, span.start() + 1, board, name, span.start(), span.end()))
    return found, page_board


class State:
    errors = 0
    known = None
    names = {}      # schematic path -> its names
    real_of = {}    # site path -> real path


def check_page(real, text):
    """The page's problems, as (line, column, message)."""
    problems = []
    found, page_board = references(text)
    for line, column, board, name, start, end in found:
        board = board or page_board
        if board is None:
            problems.append((line, column, '`#!kicad %s` names no board: write <!-- kicad <board> --> once in the page, or '
                                            '`#!kicad <board>:%s`' % (name, name)))
            continue
        path, error = find_board(board, State.known)
        if error:
            problems.append((line, column, error))
            continue
        if path not in State.names:
            State.names[path] = names_of(path)
        if plain(name) not in State.names[path]:
            problems.append((line, column, '%s has no part or net named %s' % (board, name)))
    return problems


def on_config(config):
    try:
        State.real_of = site_sources.site_files(site_sources.sources(config.extra.get('sources')))
    except site_sources.SourcesError as error:
        raise PluginError(str(error))
    State.errors = 0
    State.known = schematics()
    State.names = {}
    return config


def on_page_markdown(markdown, page, config, files):
    real = State.real_of.get(os.path.normpath(page.file.src_uri), page.file.abs_src_path)
    text = symbol_table.read_text(real) or markdown
    if '#!kicad' not in text:
        return markdown
    for line, column, message in check_page(real, text):
        sys.stderr.write('%s:%d:%d: error: %s\n' % (real, line, column, message))
        State.errors += 1
    return SPAN.sub(lambda span: '`#!kicad %s`' % span.group(1).rpartition(':')[2], markdown)


def on_post_build(config):
    if State.errors:
        log.error('%d reference%s to KiCad name nothing: see the errors above' % (State.errors, '' if State.errors == 1 else 's'))
