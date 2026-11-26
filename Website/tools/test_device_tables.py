"""The device boards' tables device_tables.py makes (DocsPlan.txt Ic).

Run by web.sh's checks: .venv/bin/python tools/test_device_tables.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import device_tables  # noqa: E402


class TestDeviceTables(unittest.TestCase):
    def test_board_list(self):
        # MakeImages.ps1's list: each board described by hand or by its schematic.
        found = device_tables.ENTRY.findall("@{ Image = 'UARTBoard';\t\tDescription = 'UARTBoard.xml' },\n"
                                            "@{ Image = 'PS2Board';\tSchematic = 'PS2\\PS2 Board\\PS2 Board.kicad_sch' }")
        self.assertEqual([('UARTBoard', 'Description', 'UARTBoard.xml'),
                          ('PS2Board', 'Schematic', 'PS2\\PS2 Board\\PS2 Board.kicad_sch')], found)
        boards = device_tables.boards()
        self.assertGreaterEqual(len(boards), 10)
        self.assertTrue(all(os.path.isfile(path) for _, _, path in boards))

    def test_table(self):
        lines = device_tables.table(['Offset', 'Name', 'Access'], [['$400', 'Flags', 'ReadWrite'], ['$401', '', 'Write']])
        self.assertTrue(lines.startswith('<!-- Generated from the device boards'))
        self.assertEqual(['| Offset | Name | Access |', '|---|---|---|', '| $400 | Flags | ReadWrite |',
                          '| $401 | | Write |', ''], lines.split('\n')[2:])


if __name__ == '__main__':
    unittest.main()
