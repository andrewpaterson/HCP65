"""MkDocs hook: references to code (DocsPlan.txt D2, D3, E1; decisions 8 and 12).

A page names code explicitly, Rust style: [`CTextArea`], [`CTextArea::ReplaceRanges`], [`CShape::Init(int)`],
[`BaseLib/Chars.h`].  Each becomes a link to its line on its repository's branch on GitHub: a symbol's declaration
in its header, a file's top.  A reference that names nothing is an error, and fails the strict build.  A plain code
span that names a symbol declared in a header, `CTextArea`, is a warning: it could be a reference.  Made-up names
(`CTank`), code (`x = 1`), names of one or two letters, functions only a .cpp file declares (each program's `main`)
and spans in another language (`#!asm lda`, `#!text abs`) are left alone, as is everything in fenced code blocks
and HTML comments.

What each span is, is decided by hcp65index -resolve (IndexLib's CReferenceResolver, which the IDE uses too) for
every page at once when the build starts (symbol_table.py); this hook puts the links in and prints the problems as
"<real path>:<line>:<column>: error|warning: ..." lines, which the IDE's Build pane opens.  The repositories, their
GitHub URLs and the solutions indexed are in mkdocs.yml's extra; each repository's branch is its extra.sources
entries'.
"""
import logging
import os
import sys
import urllib.parse

from mkdocs.exceptions import PluginError

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402
import symbol_table  # noqa: E402

log = logging.getLogger('mkdocs.hooks.code_references')


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


def put_links(markdown, results, offset, links):
    """The page with a link after each resolved reference.  results are hcp65index's for the page's file, whose
    lines are offset more than markdown's (the front matter MkDocs took off); their columns are bytes."""
    lines = markdown.split('\n')
    by_line = {}
    for result in results:
        if result.kind == 'link':
            by_line.setdefault(result.line - 1 - offset, []).append(result)
    for index, found in by_line.items():
        line = lines[index]
        encoded = line.encode('utf-8')
        for result in sorted(found, key=lambda result: result.end, reverse=True):
            end = len(encoded[:result.end].decode('utf-8', errors='ignore'))
            line = line[:end] + '(' + links.url(result.file, result.target) + ')' + line[end:]
        lines[index] = line
    return '\n'.join(lines)


class State:
    links = None
    results = {}        # real path -> hcp65index's results
    real_of = {}        # site path -> real path
    errors = 0


def on_config(config):
    try:
        folders = site_sources.sources(config.extra.get('sources'))
        repositories = symbol_table.repositories(config.extra)
        State.links = Links(repositories, branches_of(repositories, folders))
        symbol_table.load(config.extra, log=log.info)
        State.real_of = site_sources.site_files(folders)
        State.results = symbol_table.resolve(sorted(real for real in State.real_of.values() if real.endswith('.md')))
    except (site_sources.SourcesError, symbol_table.SymbolTableError) as error:
        raise PluginError(str(error))
    State.errors = 0
    return config


def on_page_markdown(markdown, page, config, files):
    real = State.real_of.get(os.path.normpath(page.file.src_uri), page.file.abs_src_path)
    results = State.results.get(real, [])
    text = symbol_table.read_text(real) or ''
    start = text.find(markdown)
    offset = text.count('\n', 0, start) if start > 0 else 0       # the front matter MkDocs took off
    for result in results:
        if result.kind != 'link':
            sys.stderr.write('%s:%d:%d: %s: %s\n' % (real, result.line, result.column, result.kind, result.message))
            if result.kind == 'error':
                State.errors += 1
    return put_links(markdown, results, offset, links=State.links)


def on_post_build(config):
    if State.errors:
        log.error('%d reference%s to code name nothing: see the errors above' % (State.errors, '' if State.errors == 1 else 's'))
