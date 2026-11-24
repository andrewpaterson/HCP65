"""MkDocs hook: references to code (DocsPlan.txt D2, D3; decisions 8 and 12).

A page names code explicitly, Rust style: [`CTextArea`], [`CTextArea::ReplaceRanges`], [`CShape::Init(int)`],
[`BaseLib/Chars.h`].  Each becomes a link to its line on its repository's branch on GitHub: a symbol's declaration
in its header (symbol_table.py says what a reference names), a file's top.  A reference that names nothing is an
error, and fails the strict build.  A plain code span that names a symbol declared in a header, `CTextArea`, is a
warning: it could be a reference.  Made-up names (`CTank`), code (`x = 1`), names of one or two letters, functions
only a .cpp file declares (each program's `main`) and spans in another language (`#!asm lda`, `#!text abs`) are left
alone, as is everything in fenced code blocks and HTML comments.

Both are printed as "<real path>:<line>:<column>: error|warning: ..." lines, which the IDE's Build pane opens.  The
repositories, their GitHub URLs and the solutions indexed are in mkdocs.yml's extra (symbol_table.py); each
repository's branch is its extra.sources entries'.
"""
import logging
import os
import re
import sys
import urllib.parse

from mkdocs.exceptions import PluginError

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402
import symbol_table  # noqa: E402

log = logging.getLogger('mkdocs.hooks.code_references')

FENCE = re.compile(r'^\s*(`{3,}|~{3,})')
CODE_SPAN = re.compile(r'(?<!`)(`+)(?!`)(.+?)(?<!`)\1(?!`)')


class Links:
    """Where on GitHub a repository's file is."""

    def __init__(self, repositories, branches):
        self.repositories = repositories    # folder -> GitHub URL
        self.branches = branches            # folder -> branch

    def url(self, path, line=None):
        repository, inside = path.split('/', 1)
        url = '%s/blob/%s/%s' % (self.repositories[repository], self.branches[repository], urllib.parse.quote(inside))
        return url + ('#L%d' % line if line else '')


def branches_of(repositories, folders):
    """Each repository's branch, from the extra.sources entries of its folders."""
    branches = {}
    for folder, site, branch in folders:
        repository = os.path.relpath(folder, site_sources.ROOT).split(os.sep)[0]
        if branches.setdefault(repository, branch) != branch:
            raise PluginError('extra.sources: %s has folders on two branches, %s and %s' % (repository, branches[repository], branch))
    missing = sorted(set(repositories) - set(branches))
    if missing:
        raise PluginError('extra.repositories: no extra.sources folder of %s says its branch' % ', '.join(missing))
    return branches


def code_spans(markdown):
    """(line index, match) of each code span outside fenced code blocks and HTML comments."""
    fence = None
    comment = False
    for index, line in enumerate(markdown.split('\n')):
        match = FENCE.match(line)
        if fence:
            if match and match.group(1)[0] == fence[0] and len(match.group(1)) >= len(fence) and not line[match.end():].strip():
                fence = None
            continue
        if match:
            fence = match.group(1)
            continue
        visible = line
        if comment:
            end = visible.find('-->')
            if end < 0:
                continue
            visible = ' ' * (end + 3) + visible[end + 3:]
            comment = False
        while '<!--' in visible:
            start = visible.index('<!--')
            end = visible.find('-->', start + 4)
            if end < 0:
                visible = visible[:start]
                comment = True
                break
            visible = visible[:start] + ' ' * (end + 3 - start) + visible[end + 3:]
        for span in CODE_SPAN.finditer(visible):
            yield index, span


def bracketing(line, span):
    """What the brackets around a code span make it: 'reference' for [`Name`], 'link' for a link's (or an image's)
    text [`Name`](url), [`Name`][ref] or a definition [`Name`]: url, 'escaped' for \\[`Name`] (written so as not to be
    a reference), None for a plain span."""
    start, end = span.start(), span.end()
    if start == 0 or line[start - 1] != '[' or line[end:end + 1] != ']':
        return None
    if line[end + 1:end + 2] in ('(', '[') or (start >= 2 and line[start - 2] == '!'):
        return 'link'
    if line[end + 1:end + 2] == ':' and not line[:start - 1].strip() and start - 1 <= 3:
        return 'link'
    if start >= 2 and line[start - 2] == '\\':
        return 'escaped'
    return 'reference'


def could_be_reference(text):
    """Whether a plain code span is worth a warning if it names a symbol: not a file, nor code in another language
    (inlinehilite's `#!asm lda`, or `#!text abs` for a word that isn't C++), nor a name of one or two letters (the
    registers X, Y, S, PC)."""
    if text.startswith('#!') or symbol_table.is_file_reference(text):
        return False
    return len(symbol_table.split_name(text)[0]) >= 3


def convert(markdown, table, links):
    """The page with its references linked, and its problems: (line index, column index, kind, message)."""
    lines = markdown.split('\n')
    problems = []
    replacements = {}       # line index -> [(end of the reference's ']', url)]
    for index, span in code_spans(markdown):
        line = lines[index]
        text = span.group(2).strip()
        kind = bracketing(line, span)
        if kind == 'reference':
            url, message = resolve(text, table, links)
            if url:
                replacements.setdefault(index, []).append((span.end() + 1, url))
            else:
                problems.append((index, span.start() - 1, 'error', message))
        elif kind is None and could_be_reference(text):
            symbol = table.resolve_symbol(text)
            if symbol and symbol_table.in_header(symbol):
                problems.append((index, span.start(), 'warning', '`%s` names %s %s (%s:%d): write [`%s`] to link to it'
                                 % (text, symbol.kind, symbol.key, symbol.file, symbol.line, text)))
    for index, ends in replacements.items():
        line = lines[index]
        for end, url in sorted(ends, reverse=True):
            line = line[:end] + '(' + url + ')' + line[end:]
        lines[index] = line
    return '\n'.join(lines), problems


def resolve(text, table, links):
    """The URL a reference links to, or None and why."""
    if symbol_table.is_file_reference(text):
        files = table.resolve_files(text)
        if len(files) == 1:
            return links.url(files[0]), None
        if not files:
            return None, '[`%s`] names no file in the repositories' % text
        shown = ', '.join(files[:4]) + (', ...' if len(files) > 4 else '')
        return None, '[`%s`] names %d files (%s): write more of its path' % (text, len(files), shown)
    symbol = table.resolve_symbol(text)
    if symbol:
        return links.url(symbol.file, symbol.line), None
    return None, '[`%s`] names nothing the indexed solutions declare (mkdocs.yml extra.solutions)' % text


class State:
    table = None
    links = None
    real_of = {}        # site path -> real path
    errors = 0


def on_config(config):
    try:
        folders = site_sources.sources(config.extra.get('sources'))
        repositories = symbol_table.repositories(config.extra)
        State.links = Links(repositories, branches_of(repositories, folders))
        State.table = symbol_table.load(config.extra, log=log.info)
        State.real_of = site_sources.site_files(folders)
    except (site_sources.SourcesError, symbol_table.SymbolTableError) as error:
        raise PluginError(str(error))
    State.errors = 0
    return config


def on_page_markdown(markdown, page, config, files):
    real = State.real_of.get(os.path.normpath(page.file.src_uri), page.file.abs_src_path)
    text = symbol_table.read_text(real) or ''
    start = text.find(markdown)
    offset = text.count('\n', 0, start) if start > 0 else 0       # the front matter MkDocs took off
    markdown, problems = convert(markdown, State.table, State.links)
    for index, column, kind, message in problems:
        sys.stderr.write('%s:%d:%d: %s: %s\n' % (real, index + offset + 1, column + 1, kind, message))
        if kind == 'error':
            State.errors += 1
    return markdown


def on_post_build(config):
    if State.errors:
        log.error('%d reference%s to code name nothing: see the errors above' % (State.errors, '' if State.errors == 1 else 's'))
