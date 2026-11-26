"""MkDocs hook: every ```cpp block compiles (DocsPlan.txt F1; decision 15).

A page's ```cpp blocks are compiled, check only, by hcp65index -examples (IndexLib's CPageExamples): each as a unit
of the project the page is an item of, as that project's build configuration compiles it, after the lines of the
page's hidden comment, which names its headers and declares the names its examples use without declaring:

    <!-- cpp
    #include "Assembler65Lib/Asm65C64Assembler.h"
    char*	szSource;
    -->

A block of statements is compiled as the body of a function; one that declares (a function written out, a class,
an #include) at file scope.  A ```{.cpp .no-check} block isn't compiled.  Each error is printed as
"<real path>:<line>:<column>: error: ...", which the IDE's Build pane opens, and fails the strict build.  It takes
about a second and a half for every page, so it isn't cached.
"""
import logging
import os
import re
import subprocess
import sys

from mkdocs.exceptions import PluginError

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402
import symbol_table  # noqa: E402

log = logging.getLogger('mkdocs.hooks.code_examples')

# A fence that may open a checked block: hcp65index decides which are (cpp, or {.cpp} without .no-check).
CPP_FENCE = re.compile(r'^[ \t]*(```+|~~~+)[ \t]*(cpp\b|\{[^}\n]*\.cpp\b)', re.MULTILINE)
ERROR = re.compile(r'^(.*):(\d+):(\d+): error: (.*)$')


def has_examples(text):
    return CPP_FENCE.search(text) is not None


def errors_of(output):
    """hcp65index -examples' error lines as (page, line, column, message); its other lines (CppCompilerLib's
    "Process (unit)" lines) are left out."""
    errors = []
    for line in output.splitlines():
        match = ERROR.match(line)
        if match:
            errors.append((match.group(1), int(match.group(2)), int(match.group(3)), match.group(4)))
    return errors


def check(solution_paths, pages):
    """The pages' examples' errors (hcp65index -examples)."""
    executable = symbol_table.program()
    symbol_table.check_program(executable)
    result = subprocess.run([executable, '-examples'] + solution_paths + ['-pages'] + list(pages),
                            capture_output=True, text=True, encoding='utf-8')
    if result.returncode != 0:
        raise symbol_table.SymbolTableError('hcp65index -examples failed (%d):\n%s' % (result.returncode, result.stderr))
    return errors_of(result.stdout)


class State:
    errors = 0


def on_config(config):
    try:
        folders = site_sources.sources(config.extra.get('sources'))
        solution_paths = symbol_table.solutions(config.extra)
        pages = sorted(real for real in site_sources.site_files(folders).values()
                       if real.endswith('.md') and has_examples(symbol_table.read_text(real) or ''))
        errors = check(solution_paths, pages) if pages else []
    except (site_sources.SourcesError, symbol_table.SymbolTableError) as error:
        raise PluginError(str(error))
    for page, line, column, message in errors:
        sys.stderr.write('%s:%d:%d: error: %s\n' % (page, line, column, message))
    State.errors = len(errors)
    log.info('Compiled the ```cpp blocks of %d pages' % len(pages))
    return config


def on_post_build(config):
    if State.errors:
        log.error('%d error%s in ```cpp blocks: see the errors above' % (State.errors, '' if State.errors == 1 else 's'))
