#!/usr/bin/env python3
"""The HCP65 device boards' tables, made from their descriptions (DocsPlan.txt Ic).

Each device board's description (DeviceRomLib's DeviceROM.h: name, socket size, how it uses the main bus, driver,
registers) is what its device ROM is made from: written by hand in DeviceROM/Boards/<board>.xml, or worked out from
its KiCad schematic by the DeviceROM program (DeviceROMSchematic.h).  The boards, and which way each is described, are
the list DeviceROM/MakeImages.ps1 makes the ROM images from.  Written to DeviceROM/Generated, which the hardware pages
include by snippet:

    devices.txt               a row a board: its name, socket, main bus and driver
    <board>-registers.txt     a row a register: its offset, name (a schematic's are its nets') and access, for each
                              board that has registers

A file is written only when it changes, and one nothing makes any more is deleted; --check changes nothing and fails
if a file would change.  The DeviceROM program
is $DEVICEROM, or build/DeviceROMRelease/DeviceROM beside the repositories.

    .venv/bin/python tools/device_tables.py [--check]
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ElementTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import site_sources  # noqa: E402
from instruction_tables import write  # noqa: E402

DEVICE_ROM = os.path.join(site_sources.ROOT, 'HCP65', 'DeviceROM')
MAKE_IMAGES = os.path.join(DEVICE_ROM, 'MakeImages.ps1')
BOARDS = os.path.join(DEVICE_ROM, 'Boards')
KICAD = os.path.join(site_sources.ROOT, 'HCP65', 'Kicad', 'Computer')
GENERATED = os.path.join(DEVICE_ROM, 'Generated')
ENTRY = re.compile(r"@\{\s*Image\s*=\s*'([^']+)';\s*(Description|Schematic)\s*=\s*'([^']+)'\s*\}")
MARK = ('Generated from the device boards\' descriptions (DeviceROM/Boards, or worked out from the boards\' schematics) '
        'by HCP65/Website/tools/device_tables.py.  Don\'t edit it: change the description or the schematic.')
MAIN_BUS = {'Master': 'Drives it', 'WindowA WindowB': 'Windows A and B', 'WindowA WindowB Private': 'Windows A and B, private'}


class DeviceTableError(Exception):
    pass


def program():
    explicit = os.environ.get('DEVICEROM')
    if explicit:
        return explicit
    return os.path.join(site_sources.ROOT, 'build', 'DeviceROMRelease', 'DeviceROM.exe' if os.name == 'nt' else 'DeviceROM')


def boards():
    """MakeImages.ps1's boards: (image name, 'Description' or 'Schematic', its file)."""
    with open(MAKE_IMAGES, encoding='utf-8') as file:
        entries = ENTRY.findall(file.read())
    if not entries:
        raise DeviceTableError('%s lists no boards (@{ Image = ...; Description|Schematic = ... })' % MAKE_IMAGES)
    return [(image, kind, os.path.join(BOARDS if kind == 'Description' else KICAD, *path.split('\\')))
            for image, kind, path in entries]


def describe(kind, path, folder):
    """A board's description as an XML root: its file, or the DeviceROM program's from its schematic."""
    if kind == 'Schematic':
        executable = program()
        if not os.path.isfile(executable):
            raise DeviceTableError('%s is not built: the device tables need it.  Build it with\n'
                                   '  cmake -S HCP65/DeviceROM -B build/DeviceROMRelease -DCMAKE_BUILD_TYPE=Release '
                                   '-DBASELIB_64=ON\n  cmake --build build/DeviceROMRelease\nfrom the folder holding '
                                   'the repositories, or set DEVICEROM to where it is.' % executable)
        described = os.path.join(folder, 'board.xml')
        result = subprocess.run([executable, path, described, os.path.join(folder, 'board.bin')],
                                capture_output=True, text=True)
        if result.returncode != 0:
            raise DeviceTableError('%s:1:1: error: %s' % (path, result.stderr.strip()))
        path = described
    return ElementTree.parse(path).getroot()


def table(headings, rows):
    lines = ['<!-- %s -->' % MARK, '', '| %s |' % ' | '.join(headings), '|%s|' % '|'.join('---' for _ in headings)]
    lines += ['|%s|' % '|'.join(' %s ' % cell if cell else ' ' for cell in row) for row in rows]
    return '\n'.join(lines) + '\n'


def outputs():
    """Each file to write, by path, with its text."""
    files = {}
    devices = []
    with tempfile.TemporaryDirectory() as folder:
        for image, kind, path in boards():
            root = describe(kind, path, folder)
            main_bus = root.get('MainBus', '')
            if main_bus and main_bus not in MAIN_BUS:
                raise DeviceTableError('%s: MainBus="%s" isn\'t one device_tables.py knows' % (path, main_bus))
            devices.append(['`%s`' % root.get('Name'), root.get('DeviceBus', ''), MAIN_BUS.get(main_bus, 'Doesn\'t use it'),
                            '`%s`' % root.get('Driver') if root.get('Driver') else 'none'])
            registers = [[register.get('Offset').replace('0x', '$'), register.get('Name'), register.get('Access')]
                         for register in root.findall('Register')]
            if registers:
                files[os.path.join(GENERATED, '%s-registers.txt' % image)] = table(['Offset', 'Name', 'Access'], registers)
    files[os.path.join(GENERATED, 'devices.txt')] = table(['Device name', 'Socket', 'Main bus', 'Driver file'], devices)
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--check', action='store_true', help='write nothing; fail if a file would change')
    arguments = parser.parse_args()
    try:
        files = outputs()
        changed = write(files, arguments.check)
        for name in sorted(os.listdir(GENERATED)):      # a board that went, or lost its registers
            path = os.path.join(GENERATED, name)
            if path not in files:
                changed.append(path)
                if not arguments.check:
                    os.remove(path)
    except DeviceTableError as error:
        raise SystemExit(str(error) if ':1:1: error:' in str(error) else '%s:1:1: error: %s' % (MAKE_IMAGES, error))
    for path in changed:
        print('%s: %s' % ('would change' if arguments.check else 'written', os.path.relpath(path, site_sources.ROOT)))
    if arguments.check and changed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
