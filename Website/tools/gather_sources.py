"""MkDocs hook: gathers the documentation folders mkdocs.yml's extra.sources maps into one staging folder laid out as
the site, and builds the site from there (DocsPlan.txt step A, decision 13).

    extra:
      sources:
        - folder: HCP65/Website/docs    # relative to the folder holding the repositories (two up from mkdocs.yml)
          site: ""                      # where its files land in the site ("" is the top)
          branch: main                  # its repository's branch, for links to the code (step D)

Pages are copied, not linked (links don't work everywhere on Windows), and only when they changed, so a rebuild
under `mkdocs serve` is quiet.  `serve` watches the mapped folders themselves.  Two folders putting a file in the
same place is an error, as is a mapped folder that isn't there.  MkDocs's warnings about a page also go to stderr
as "<real path>:1:1: warning: ..." lines, which the IDE's Build pane can open.
"""
import filecmp
import logging
import os
import re
import shutil
import sys

from mkdocs.exceptions import PluginError

STAGING = '.staging'

source_of = {}          # staging-relative path -> the real file it came from
filter_added = False


def read_sources(config):
    """The website's folder and the (real folder, site folder, branch) of each extra.sources entry."""
    website = os.path.dirname(os.path.abspath(config.config_file_path))
    root = os.path.normpath(os.path.join(website, '..', '..'))
    entries = config.extra.get('sources')
    if not entries:
        raise PluginError('mkdocs.yml has no extra.sources: nothing to build the site from')
    sources = []
    for entry in entries:
        if not isinstance(entry, dict) or not {'folder', 'site', 'branch'} <= set(entry):
            raise PluginError('extra.sources: each entry needs folder, site and branch: %r' % (entry,))
        folder = os.path.normpath(os.path.join(root, entry['folder']))
        if not os.path.isdir(folder):
            raise PluginError('extra.sources: %s is not a folder' % folder)
        sources.append((folder, (entry['site'] or '').strip('/'), entry['branch']))
    return website, sources


def gather(sources):
    """Staging-relative path -> real path for every file of every mapped folder."""
    files = {}
    for folder, site, branch in sources:
        for directory, directories, names in os.walk(folder):
            directories.sort()
            for name in sorted(names):
                real = os.path.join(directory, name)
                staged = os.path.normpath(os.path.join(site, os.path.relpath(real, folder)))
                if staged in files:
                    raise PluginError('extra.sources: %s and %s both land at %s' % (files[staged], real, staged))
                files[staged] = real
    return files


def stage(staging, files):
    """Copies what changed into the staging folder and removes what no mapped folder has any more."""
    for staged, real in files.items():
        target = os.path.join(staging, staged)
        if os.path.isfile(target) and filecmp.cmp(real, target, shallow=False):
            continue
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copyfile(real, target)
    for directory, directories, names in os.walk(staging, topdown=False):
        for name in names:
            path = os.path.join(directory, name)
            if os.path.relpath(path, staging) not in files:
                os.remove(path)
        if directory != staging and not os.listdir(directory):
            os.rmdir(directory)


class RealPathFilter(logging.Filter):
    """Repeats MkDocs's warnings about a staged page as "<real path>:1:1: warning: ..." on stderr."""

    last = None     # the record last repeated: MkDocs has more than one handler, and each runs the filter

    def filter(self, record):
        if record.levelno >= logging.WARNING and record is not RealPathFilter.last:
            RealPathFilter.last = record
            message = record.getMessage()
            for quoted in re.findall(r"'([^']+\.md)'", message):
                real = source_of.get(os.path.normpath(quoted))
                if real:
                    kind = 'error' if record.levelno >= logging.ERROR else 'warning'
                    sys.stderr.write('%s:1:1: %s: %s\n' % (real, kind, message))
                    break
        return True


def on_config(config):
    global filter_added

    website, sources = read_sources(config)
    staging = os.path.join(website, STAGING)
    files = gather(sources)
    os.makedirs(staging, exist_ok=True)
    stage(staging, files)
    source_of.clear()
    source_of.update(files)

    config['docs_dir'] = staging
    for folder, site, branch in sources:
        if folder not in config['watch']:
            config['watch'].append(folder)

    if not filter_added:
        real_path_filter = RealPathFilter()
        for handler in logging.getLogger('mkdocs').handlers:
            handler.addFilter(real_path_filter)
        filter_added = True
    return config
