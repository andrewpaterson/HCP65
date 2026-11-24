"""The website's pages and where they really are, from mkdocs.yml's extra.sources (DocsPlan.txt decision 13).

    extra:
      sources:
        - folder: HCP65/Website/docs    # relative to the folder holding the repositories (two up from mkdocs.yml)
          site: ""                      # where its files land in the site ("" is the top)
          branch: main                  # its repository's branch, for links to the code (step D)

Shared by the MkDocs hook that gathers the pages (gather_sources.py) and the checks, so they all read the map the
same way.  Problems raise SourcesError.
"""
import os

import yaml

WEBSITE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ROOT = os.path.normpath(os.path.join(WEBSITE, '..', '..'))


class SourcesError(Exception):
    pass


class Loader(yaml.SafeLoader):
    """mkdocs.yml with its !!python/name: tags read as text."""


Loader.add_multi_constructor('tag:yaml.org,2002:python/', lambda loader, suffix, node: suffix)


def read_extra():
    """mkdocs.yml's extra."""
    with open(os.path.join(WEBSITE, 'mkdocs.yml'), encoding='utf-8') as file:
        config = yaml.load(file, Loader=Loader)
    return config.get('extra') or {}


def read_entries():
    """extra.sources, as mkdocs.yml has it."""
    return read_extra().get('sources')


def sources(entries=None):
    """The (real folder, site folder, branch) of each extra.sources entry."""
    if entries is None:
        entries = read_entries()
    if not entries:
        raise SourcesError('mkdocs.yml has no extra.sources: nothing to build the site from')
    result = []
    for entry in entries:
        if not isinstance(entry, dict) or not {'folder', 'site', 'branch'} <= set(entry):
            raise SourcesError('extra.sources: each entry needs folder, site and branch: %r' % (entry,))
        folder = os.path.normpath(os.path.join(ROOT, entry['folder']))
        if not os.path.isdir(folder):
            raise SourcesError('extra.sources: %s is not a folder' % folder)
        result.append((folder, (entry['site'] or '').strip('/'), entry['branch']))
    return result


def site_files(folders=None):
    """Site path -> real path for every file of every mapped folder.  Two files landing in one place is an error."""
    if folders is None:
        folders = sources()
    files = {}
    for folder, site, branch in folders:
        for directory, directories, names in os.walk(folder):
            directories.sort()
            for name in sorted(names):
                real = os.path.join(directory, name)
                staged = os.path.normpath(os.path.join(site, os.path.relpath(real, folder)))
                if staged in files:
                    raise SourcesError('extra.sources: %s and %s both land at %s' % (files[staged], real, staged))
                files[staged] = real
    return files
