"""The symbol table references to code resolve against (DocsPlan.txt D1, D2), and what a reference names.

mkdocs.yml lists the solutions to index and the repositories to link to:

    extra:
      repositories:                 # each repository's folder (beside HCP65) and where it is on GitHub
        HCP65: https://github.com/andrewpaterson/HCP65
      solutions:                    # indexed together by hcp65index
        - HCP65/Kernal/HCP65Kernal.sln

hcp65index (HCP65-IDE/HCP65Index) indexes the solutions as the IDE does and writes one line a symbol: its key
("CShape::Init(int)"), kind, file (relative to the folder holding the repositories) and line.  The table is kept in
Website/.symbols and made again only when one of the repositories' HEAD, the solution list or the program changes.
The program is $HCP65INDEX, or build/HCP65IDERelease/hcp65index beside the repositories.

What a reference names (resolve):
  - A symbol by its qualified name: [`CTextArea`], [`CTextArea::ReplaceRanges`].  An overloaded function without its
    parameters is its first declaration (by file and line); with them, [`CShape::Init(int)`], the one that has them
    (spaces and where const is written don't matter, nor a trailing const).  An enumerator can be named without its
    enum, as C++ allows an unscoped one: [`E65Access::MA65_Read`] or [`MA65_Read`].
  - A file in one of the repositories by the end of its path: [`BaseLib/Chars.h`], [`tools/site_sources.py`].
"""
import hashlib
import os
import re
import subprocess

import site_sources

TABLE_FOLDER = os.path.join(site_sources.WEBSITE, '.symbols')
TABLE = os.path.join(TABLE_FOLDER, 'symbols.txt')
TABLE_STATE = os.path.join(TABLE_FOLDER, 'symbols.state')

TYPE_KINDS = ('class', 'class-template', 'enum')


class SymbolTableError(Exception):
    pass


class Symbol:
    def __init__(self, key, kind, file, line):
        self.key = key
        self.kind = kind
        self.file = file        # relative to the folder holding the repositories, '/' separated
        self.line = line        # 1 based

    def __repr__(self):
        return '%s %s %s:%d' % (self.kind, self.key, self.file, self.line)


def program():
    """The hcp65index program, which may not have been built."""
    explicit = os.environ.get('HCP65INDEX')
    if explicit:
        return explicit
    name = 'hcp65index.exe' if os.name == 'nt' else 'hcp65index'
    return os.path.join(site_sources.ROOT, 'build', 'HCP65IDERelease', name)


def split_name(key):
    """A key's name and the rest: "CShape::Init(int) const" is ("CShape::Init", "(int) const")."""
    match = re.match(r'(.*?\boperator\s*\(\s*\)|[^(]*)(.*)$', key)
    return match.group(1).strip(), match.group(2).strip()


def split_parameters(text):
    """ "(int, CArrayTemplate<M, N>*) const" is (["int", "CArrayTemplate<M, N>*"], "const"), None if no list."""
    if not text.startswith('('):
        return None
    depth = 0
    for index, char in enumerate(text):
        if char in '(<[':
            depth += 1
        elif char in ')>]':
            depth -= 1
            if depth == 0:
                inner, after = text[1:index], text[index + 1:]
                break
    else:
        return None
    parameters, depth, start = [], 0, 0
    for index, char in enumerate(inner):
        if char in '(<[':
            depth += 1
        elif char in ')>]':
            depth -= 1
        elif char == ',' and depth == 0:
            parameters.append(inner[start:index])
            start = index + 1
    parameters.append(inner[start:])
    parameters = [normal_type(parameter) for parameter in parameters if parameter.strip()]
    if parameters == ['void']:
        parameters = []
    return parameters, after.strip()


def normal_type(text):
    """A parameter's type written one way: "const char *" and "char const*" are both "charconst*"."""
    text = text.strip()
    match = re.match(r'const\s+([\w:]+(?:<.*>)?)(.*)$', text)
    if match:
        text = match.group(1) + ' const' + match.group(2)
    return re.sub(r'\s+', '', text)


class Table:
    """The symbols, by the names a reference can use, and the repositories' files."""

    def __init__(self, symbols, files):
        self.by_name = {}
        self.by_key = {}
        for symbol in symbols:
            self.by_key[symbol.key] = symbol
            name = split_name(symbol.key)[0]
            self.by_name.setdefault(name, []).append(symbol)
            if symbol.kind == 'enumerator' and '::' in name:
                parts = name.split('::')
                self.by_name.setdefault('::'.join(parts[:-2] + parts[-1:]), []).append(symbol)
        for named in self.by_name.values():
            named.sort(key=lambda symbol: (symbol.kind not in TYPE_KINDS, symbol.kind == 'typedef', symbol.file, symbol.line))
        self.files = files      # every tracked file, relative to the folder holding the repositories

    def resolve_symbol(self, text):
        """The symbol text names, None if none."""
        text = text.strip()
        if text in self.by_key:
            return self.by_key[text]
        name, rest = split_name(text)
        named = self.by_name.get(name)
        if not named:
            return None
        if not rest:
            return named[0]
        wanted = split_parameters(rest)
        if wanted is None:
            return None
        for symbol in named:
            has = split_parameters(split_name(symbol.key)[1])
            if has is not None and has[0] == wanted[0] and (not wanted[1] or wanted[1] == has[1]):
                return symbol
        return None

    def resolve_files(self, text):
        """The files whose path is or ends with text."""
        text = text.strip().strip('/')
        return [file for file in self.files if file == text or file.endswith('/' + text)]


def in_header(symbol):
    """Whether the symbol's place is in a header, not only in a .c or .cpp file (where each program's main is)."""
    return re.search(r'\.(c|cc|cxx|cpp)$', symbol.file, re.IGNORECASE) is None


def is_file_reference(text):
    """Whether a reference names a file rather than a symbol: a path, or a name with an extension."""
    return '(' not in text and '::' not in text and ('/' in text or re.search(r'\.[A-Za-z]\w*$', text) is not None)


def repositories(config_extra):
    """Each repository's folder (relative to the folder holding them) to its GitHub URL."""
    entries = config_extra.get('repositories')
    if not isinstance(entries, dict) or not entries:
        raise SymbolTableError('mkdocs.yml has no extra.repositories: the references to code have nothing to link to')
    for folder in entries:
        if not os.path.isdir(os.path.join(site_sources.ROOT, folder, '.git')):
            raise SymbolTableError('extra.repositories: %s is not a git repository beside HCP65' % folder)
    return {folder: url.rstrip('/') for folder, url in entries.items()}


def solutions(config_extra):
    """The solutions to index, as full paths."""
    entries = config_extra.get('solutions')
    if not isinstance(entries, list) or not entries:
        raise SymbolTableError('mkdocs.yml has no extra.solutions: nothing to index for the references to code')
    result = []
    for entry in entries:
        path = os.path.normpath(os.path.join(site_sources.ROOT, entry))
        if not os.path.isfile(path):
            raise SymbolTableError('extra.solutions: %s is not a file' % path)
        result.append(path)
    return result


def git(folder, *arguments):
    return subprocess.run(['git', '-C', folder] + list(arguments), check=True, capture_output=True, text=True).stdout


def tracked_files(repository_folders):
    files = []
    for folder in sorted(repository_folders):
        listed = git(os.path.join(site_sources.ROOT, folder), 'ls-files', '-z')
        files.extend(folder + '/' + name for name in listed.split('\0') if name)
    return files


def state(repository_folders, solution_paths, executable):
    """What the table was made from: when it changes the table is made again."""
    lines = ['%s %s' % (folder, git(os.path.join(site_sources.ROOT, folder), 'rev-parse', 'HEAD').strip())
             for folder in sorted(repository_folders)]
    lines += ['solution %s' % os.path.relpath(path, site_sources.ROOT) for path in solution_paths]
    if os.path.isfile(executable):
        with open(executable, 'rb') as file:
            lines.append('program %s' % hashlib.sha1(file.read()).hexdigest())
    return '\n'.join(lines) + '\n'


def read_text(path):
    try:
        with open(path, encoding='utf-8') as file:
            return file.read()
    except FileNotFoundError:
        return None


def make(solution_paths, executable, log):
    """Runs hcp65index into the table folder."""
    if not os.path.isfile(executable):
        raise SymbolTableError(
            '%s is not built: the references to code need it.  Build it with\n'
            '  cmake -S HCP65-IDE/HCP65IDE -B build/HCP65IDERelease -DCMAKE_BUILD_TYPE=Release\n'
            '  cmake --build build/HCP65IDERelease --target hcp65index\n'
            'from the folder holding the repositories, or set HCP65INDEX to where it is.' % executable)
    os.makedirs(TABLE_FOLDER, exist_ok=True)
    log('Indexing %d solutions for the references to code (hcp65index, about two minutes)...' % len(solution_paths))
    result = subprocess.run([executable, '-root', site_sources.ROOT, '-out', TABLE] + solution_paths,
                            capture_output=True, text=True)
    summary = [line for line in result.stdout.splitlines() if line.startswith('hcp65index')]
    if result.returncode != 0:
        raise SymbolTableError('hcp65index failed (%d):\n%s%s' % (result.returncode, '\n'.join(summary), result.stderr))
    for line in summary:
        log(line)


loaded = {}     # the state -> the table last loaded, for `mkdocs serve`'s rebuilds


def load(config_extra, log=print, remake=False):
    """The table for mkdocs.yml's extra, made first if what it is made from changed (or remake)."""
    repository_folders = repositories(config_extra)
    solution_paths = solutions(config_extra)
    executable = program()
    now = state(repository_folders, solution_paths, executable)
    if remake or read_text(TABLE_STATE) != now or read_text(TABLE) is None:
        if os.path.exists(TABLE_STATE):
            os.remove(TABLE_STATE)
        make(solution_paths, executable, log)
        with open(TABLE_STATE, 'w', encoding='utf-8') as file:
            file.write(now)
        loaded.clear()
    if now not in loaded:
        loaded.clear()
        loaded[now] = Table(read_table(TABLE), tracked_files(repository_folders))
    return loaded[now]


def read_table(path):
    symbols = []
    with open(path, encoding='utf-8') as file:
        for number, line in enumerate(file, 1):
            fields = line.rstrip('\n').split('\t')
            if len(fields) != 4 or not fields[3].isdigit():
                raise SymbolTableError('%s:%d:1: error: not a symbol table line' % (path, number))
            symbols.append(Symbol(fields[0], fields[1], fields[2], int(fields[3])))
    return symbols


if __name__ == '__main__':
    # web.sh index: the symbol table made again now.
    try:
        load(site_sources.read_extra(), remake=True)
    except SymbolTableError as error:
        raise SystemExit('error: %s' % error)
