#!/usr/bin/env python3
"""Check that every file under a documentation folder mkdocs.yml's extra.sources maps is an item of exactly one Visual
Studio project (DocsPlan.txt decision 10), so the IDE's Rename, Find Usages and Find in Files see it.

The projects are every .vcxproj in the repositories beside this one.  Problems are printed as
"<path>:1:1: error: ..." lines, which the IDE's Build pane can open; the exit code is the number of them (at most 1).
"""
import os
import sys
import xml.etree.ElementTree as ElementTree

import yaml

WEBSITE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ROOT = os.path.normpath(os.path.join(WEBSITE, '..', '..'))
SKIPPED = {'.git', '.vs', '.venv', '.staging', '.cache', 'site', 'build', 'Output', 'node_modules', 'x64', 'Debug', 'Release'}


class Loader(yaml.SafeLoader):
    """mkdocs.yml with its !!python/name: tags read as text."""


Loader.add_multi_constructor('tag:yaml.org,2002:python/', lambda loader, suffix, node: suffix)


def mapped_folders():
    with open(os.path.join(WEBSITE, 'mkdocs.yml'), encoding='utf-8') as file:
        config = yaml.load(file, Loader=Loader)
    return [os.path.normpath(os.path.join(ROOT, entry['folder'])) for entry in config['extra']['sources']]


def projects():
    """Every .vcxproj under the folders in ROOT that are git repositories."""
    for repository in sorted(os.listdir(ROOT)):
        top = os.path.join(ROOT, repository)
        if not os.path.isdir(os.path.join(top, '.git')):
            continue
        for directory, directories, names in os.walk(top):
            directories[:] = sorted(name for name in directories if name not in SKIPPED)
            for name in sorted(names):
                if name.endswith('.vcxproj'):
                    yield os.path.join(directory, name)


def project_items(project):
    """The full paths of a project's items (any element with an Include in an ItemGroup)."""
    directory = os.path.dirname(project)
    try:
        tree = ElementTree.parse(project)
    except ElementTree.ParseError:
        return []
    items = []
    for group in tree.getroot():
        if not group.tag.endswith('ItemGroup'):
            continue
        for item in group:
            include = item.get('Include')
            if include and '*' not in include and '$(' not in include:
                items.append(os.path.normpath(os.path.join(directory, include.replace('\\', '/'))))
    return items


def main():
    owners = {}
    for project in projects():
        for item in project_items(project):
            owners.setdefault(item, []).append(project)

    checked = 0
    problems = 0
    for folder in mapped_folders():
        for directory, directories, names in os.walk(folder):
            directories.sort()
            for name in sorted(names):
                path = os.path.join(directory, name)
                checked += 1
                listed = owners.get(path, [])
                if len(listed) == 1:
                    continue
                problems += 1
                if not listed:
                    print('%s:1:1: error: not an item of any Visual Studio project: list it in the project of what it '
                          'describes' % path)
                else:
                    print('%s:1:1: error: an item of %d projects (%s): list it in one' %
                          (path, len(listed), ', '.join(os.path.relpath(project, ROOT) for project in listed)))
    print('%d documentation files checked, %d not in exactly one project' % (checked, problems))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
