"""MkDocs hook: gathers the documentation folders mkdocs.yml's extra.sources maps into one staging folder laid out as
the site, and builds the site from there (DocsPlan.txt step A, decision 13; the map is read by site_sources.py).

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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402

STAGING = '.staging'

source_of = {}          # site path -> the real file it came from
filter_added = False


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

    try:
        folders = site_sources.sources(config.extra.get('sources'))
        files = site_sources.site_files(folders)
    except site_sources.SourcesError as error:
        raise PluginError(str(error))

    staging = os.path.join(site_sources.WEBSITE, STAGING)
    os.makedirs(staging, exist_ok=True)
    stage(staging, files)
    source_of.clear()
    source_of.update(files)

    config['docs_dir'] = staging
    for folder, site, branch in folders:
        if folder not in config['watch']:
            config['watch'].append(folder)

    if not filter_added:
        real_path_filter = RealPathFilter()
        for handler in logging.getLogger('mkdocs').handlers:
            handler.addFilter(real_path_filter)
        filter_added = True
    return config
