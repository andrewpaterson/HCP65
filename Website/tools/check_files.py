#!/usr/bin/env python3
"""Check that every tracked file of the repositories says what it is (DocsPlan.txt I).

A file is accounted for when it is:
  - an item of a Visual Studio project (a plan or note, .txt, of exactly one: decision 10 and Ia);
  - test data: under a test's folder (Tests, Test..., Input, Output; decision 11);
  - one of a repository's own files: at its top, or a solution, project, CMake, git or editor file;
  - named in a CMakeLists.txt beside it or above it (a program's Linux entry point, which Visual Studio doesn't build);
  - under a README.md whose table says what it is: the nearest README.md above it with a table headed
    | Files | Are | From |, one row of which matches it.  Files are glob patterns in code spans, relative to the
    README's folder (** for any folders, matched without regard to case); Are is Source, Generated or Third-party;
    From is what a generated file is made from, with the command, or where a third-party file came from, with its
    version.

Anything else is printed as "<path>:1:1: error: ..." (which the IDE's Build pane opens) and fails it.

    .venv/bin/python tools/check_files.py
"""
import os
import re
import subprocess
import sys

from check_project_items import ROOT, projects, project_items

REPOSITORIES = ('CommonLibraries', 'HCP65', 'HCP65-IDE', 'LogiCim', 'HCP65-Sculpt')
TEST_FOLDERS = ('Tests', 'Input', 'Output')
OWN_NAMES = {'CMakeLists.txt', '.gitignore', '.gitattributes', 'README.md', 'Rakefile', 'LICENSE', 'COPYING'}
OWN_EXTENSIONS = {'.sln', '.vcxproj', '.filters', '.user', '.hcp65build', '.hcp65run', '.hcp65card', '.cmake', '.ruleset'}
OWN_FOLDERS = {'.idea', '.claude', '.vscode', 'CMake'}
KINDS = ('Source', 'Generated', 'Third-party')
SPAN = re.compile(r'`([^`]+)`')


def tracked(repository):
    """The repository's tracked files, relative to it."""
    listed = subprocess.run(['git', '-C', os.path.join(ROOT, repository), 'ls-files', '-z'],
                            check=True, capture_output=True, text=True).stdout
    return [name for name in listed.split('\0') if name]


def glob_pattern(pattern):
    """A glob as a regular expression: ** any folders, * and ? within a name, [...] as written; any case."""
    out = ''
    i = 0
    while i < len(pattern):
        if pattern.startswith('**/', i):
            out += '(?:.*/)?'
            i += 3
        elif pattern.startswith('**', i):
            out += '.*'
            i += 2
        elif pattern[i] == '*':
            out += '[^/]*'
            i += 1
        elif pattern[i] == '?':
            out += '[^/]'
            i += 1
        elif pattern[i] == '[':
            end = pattern.index(']', i)
            out += pattern[i:end + 1]
            i = end + 1
        else:
            out += re.escape(pattern[i])
            i += 1
    return re.compile(out + r'\Z', re.IGNORECASE)


def read_readme(path):
    """A README's rows, (line, patterns, kind, source); [] if it has no | Files | Are | From | table.  Problems with
    its rows are printed."""
    rows = []
    problems = []
    with open(path, encoding='utf-8') as file:
        lines = file.read().split('\n')
    heading = None
    for number, line in enumerate(lines, 1):
        cells = [cell.strip() for cell in line.strip()[1:-1].split('|')] if line.startswith('|') else None
        if cells == ['Files', 'Are', 'From']:
            heading = number
            continue
        if heading is None or cells is None:
            heading = None if cells is None else heading
            continue
        if all(re.fullmatch(r':?-+:?', cell) for cell in cells):
            continue
        if len(cells) != 3 or cells[1] not in KINDS or not SPAN.findall(cells[0]):
            problems.append('%s:%d:1: error: a row is | `pattern`... | Source, Generated or Third-party | where from |'
                            % (path, number))
            continue
        if cells[1] != 'Source' and not cells[2]:
            problems.append('%s:%d:1: error: a %s row says where its files come from' % (path, number, cells[1]))
        rows.append((number, [glob_pattern(span) for span in SPAN.findall(cells[0])], cells[1], cells[2]))
    return rows, problems


class Readmes:
    """The README tables of a repository, by folder (relative to the repository)."""

    def __init__(self, repository):
        self.repository = repository
        self.tables = {}    # folder -> rows, [] for a folder with no README table
        self.problems = []

    def rows(self, folder):
        """The rows of the folder's README.md (read once, tracked yet or not), [] if it has none."""
        if folder not in self.tables:
            path = os.path.join(ROOT, self.repository, folder, 'README.md')
            rows, problems = read_readme(path) if os.path.isfile(path) else ([], [])
            self.problems += problems
            self.tables[folder] = rows
        return self.tables[folder]

    def describe(self, name):
        """(README path, the rows that match name), the nearest README with a table above it; None if none."""
        folder = os.path.dirname(name)
        while True:
            if self.rows(folder):
                relative = name[len(folder) + 1:] if folder else name
                rows = [row for row in self.tables[folder] if any(pattern.match(relative) for pattern in row[1])]
                return os.path.join(ROOT, self.repository, folder, 'README.md'), rows
            if not folder:
                return None
            folder = os.path.dirname(folder)


def is_test_data(name):
    return any(part in TEST_FOLDERS or part.startswith('Test') for part in name.split('/')[:-1])


def is_own(name):
    parts = name.split('/')
    return (len(parts) == 1 or parts[-1] in OWN_NAMES or os.path.splitext(name)[1] in OWN_EXTENSIONS
            or any(part in OWN_FOLDERS for part in parts[:-1]))


def cmake_names(repository, files):
    """The file names each folder's CMakeLists.txt mentions, by folder."""
    names = {}
    for name in files:
        if os.path.basename(name) == 'CMakeLists.txt':
            with open(os.path.join(ROOT, repository, name), encoding='utf-8', errors='replace') as file:
                names[os.path.dirname(name)] = file.read()
    return names


def in_cmake(name, cmake):
    folder = os.path.dirname(name)
    base = os.path.basename(name)
    while True:
        if base in cmake.get(folder, ''):
            return True
        if not folder:
            return False
        folder = os.path.dirname(folder)


def main():
    owners = {}
    for project in projects():
        for item in project_items(project):
            owners.setdefault(item.lower(), []).append(project)

    checked = 0
    problems = []
    for repository in REPOSITORIES:
        files = tracked(repository)
        readmes = Readmes(repository)
        cmake = cmake_names(repository, files)
        for name in files:
            checked += 1
            path = os.path.join(ROOT, repository, name)
            listed = owners.get(os.path.normpath(path).lower(), [])
            if listed:
                if name.endswith('.txt') and len(listed) > 1:
                    problems.append('%s:1:1: error: an item of %d projects (%s): list it in one' %
                                    (path, len(listed), ', '.join(os.path.relpath(project, ROOT) for project in listed)))
                continue
            if is_test_data(name) or is_own(name) or in_cmake(name, cmake):
                continue
            described = readmes.describe(name)
            if described is None:
                problems.append('%s:1:1: error: nothing says what this is: make it an item of its project, or a row '
                                'of a README.md above it (| Files | Are | From |)' % path)
            elif len(described[1]) != 1:
                problems.append('%s:1:1: error: %s has %d rows for it, not one (%s)' %
                                (path, described[0], len(described[1]),
                                 ', '.join('line %d' % row[0] for row in described[1]) or 'none'))
        problems += readmes.problems
    for problem in problems:
        print(problem)
    print('%d tracked files checked, %d problems' % (checked, len(problems)))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
