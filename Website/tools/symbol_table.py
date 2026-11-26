"""The symbol table references to code resolve against, and the program that resolves them (DocsPlan.txt D, E1).

mkdocs.yml lists the solutions to index and the repositories to link to:

    extra:
      repositories:                 # each repository's folder (beside HCP65) and where it is on GitHub
        HCP65: https://github.com/andrewpaterson/HCP65
      solutions:                    # indexed together by hcp65index
        - HCP65/Kernal/HCP65Kernal.sln

hcp65index (HCP65-IDE/HCP65Index) indexes the solutions as the IDE does and writes one line a symbol: its key
("CShape::Init(int)"), kind, file (relative to the folder holding the repositories) and line.  The table, and the
list of the repositories' tracked files a reference can name, are kept in Website/.symbols and made again only when
one of the repositories' HEAD, the solution list or the program changes.  The program is $HCP65INDEX, or
build/HCP65IDERelease/hcp65index beside the repositories.

What a reference names is decided by hcp65index -resolve, which reads the pages and the table: the same C++
(IndexLib's CReferenceResolver) the IDE resolves them with, so the two can't disagree.
"""
import collections
import hashlib
import os
import subprocess

import site_sources

TABLE_FOLDER = os.path.join(site_sources.WEBSITE, '.symbols')
TABLE = os.path.join(TABLE_FOLDER, 'symbols.txt')
TABLE_FILES = os.path.join(TABLE_FOLDER, 'files.txt')
TABLE_STATE = os.path.join(TABLE_FOLDER, 'symbols.state')

# What hcp65index -resolve says of a page's span; line and column are 1 based, end is the byte after a reference's
# ']' (where its link goes), file is relative to the folder holding the repositories, target 0 for a file.
Result = collections.namedtuple('Result', 'line column kind end file target message')


class SymbolTableError(Exception):
    pass


def program():
    """The hcp65index program, which may not have been built."""
    explicit = os.environ.get('HCP65INDEX')
    if explicit:
        return explicit
    name = 'hcp65index.exe' if os.name == 'nt' else 'hcp65index'
    return os.path.join(site_sources.ROOT, 'build', 'HCP65IDERelease', name)


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


def check_program(executable):
    if not os.path.isfile(executable):
        raise SymbolTableError(
            '%s is not built: the references to code need it.  Build it with\n'
            '  cmake -S HCP65-IDE/HCP65IDE -B build/HCP65IDERelease -DCMAKE_BUILD_TYPE=Release\n'
            '  cmake --build build/HCP65IDERelease --target hcp65index\n'
            'from the folder holding the repositories, or set HCP65INDEX to where it is.' % executable)


def make(solution_paths, repository_folders, executable, log):
    """Runs hcp65index into the table folder, and lists the files a reference can name beside it."""
    check_program(executable)
    os.makedirs(TABLE_FOLDER, exist_ok=True)
    log('Indexing %d solutions for the references to code (hcp65index, about two minutes)...' % len(solution_paths))
    result = subprocess.run([executable, '-root', site_sources.ROOT, '-out', TABLE] + solution_paths,
                            capture_output=True, text=True)
    summary = [line for line in result.stdout.splitlines() if line.startswith('hcp65index')]
    if result.returncode != 0:
        raise SymbolTableError('hcp65index failed (%d):\n%s%s' % (result.returncode, '\n'.join(summary), result.stderr))
    for line in summary:
        log(line)
    with open(TABLE_FILES, 'w', encoding='utf-8') as file:
        file.write(''.join(name + '\n' for name in tracked_files(repository_folders)))


def load(config_extra, log=print, remake=False):
    """Makes the table for mkdocs.yml's extra if what it is made from changed (or remake)."""
    repository_folders = repositories(config_extra)
    solution_paths = solutions(config_extra)
    executable = program()
    now = state(repository_folders, solution_paths, executable)
    if remake or read_text(TABLE_STATE) != now or read_text(TABLE) is None or read_text(TABLE_FILES) is None:
        if os.path.exists(TABLE_STATE):
            os.remove(TABLE_STATE)
        make(solution_paths, repository_folders, executable, log)
        with open(TABLE_STATE, 'w', encoding='utf-8') as file:
            file.write(now)


def resolve(pages):
    """Each page's references and the plain spans that could be references (hcp65index -resolve), by its path."""
    executable = program()
    check_program(executable)
    result = subprocess.run([executable, '-resolve', TABLE, '-files', TABLE_FILES] + list(pages),
                            capture_output=True, text=True, encoding='utf-8')
    if result.returncode != 0:
        raise SymbolTableError('hcp65index -resolve failed (%d):\n%s' % (result.returncode, result.stderr))
    results = {page: [] for page in pages}
    for line in result.stdout.splitlines():
        fields = line.split('\t')
        if len(fields) >= 7 and fields[3] == 'link':
            results[fields[0]].append(Result(int(fields[1]), int(fields[2]), 'link', int(fields[4]), fields[5],
                                             int(fields[6]), None))
        elif len(fields) >= 5 and fields[3] in ('error', 'warning'):
            results[fields[0]].append(Result(int(fields[1]), int(fields[2]), fields[3], None, None, None,
                                             '\t'.join(fields[4:])))
        else:
            raise SymbolTableError('hcp65index -resolve wrote a line it shouldn\'t: %r' % line)
    return results


if __name__ == '__main__':
    # web.sh index: the symbol table made again now.
    try:
        load(site_sources.read_extra(), remake=True)
    except SymbolTableError as error:
        raise SystemExit('error: %s' % error)
