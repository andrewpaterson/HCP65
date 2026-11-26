"""How kicad_references.py reads a page's KiCad references (DocsPlan.txt Id).

Run by web.sh's checks: .venv/bin/python tools/test_kicad_references.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kicad_references  # noqa: E402


class TestKicadReferences(unittest.TestCase):
    def test_references(self):
        text = ('# Clock\n\n<!-- kicad Audio Clock -->\n\nThe DAC\'s `#!kicad ~WR_B` and `#!kicad Main Memory Channel:IC2`.\n'
                '```text\n`#!kicad IC99` in a fence\n```\n')
        found, board = kicad_references.references(text)
        self.assertEqual('Audio Clock', board)
        self.assertEqual([(5, 11, None, '~WR_B'), (5, 31, 'Main Memory Channel', 'IC2')],
                         [entry[:4] for entry in found])

    def test_plain(self):
        self.assertEqual('~WR_B', kicad_references.plain('~{WR}_{B}'))
        self.assertEqual('~Main Access', kicad_references.plain('~{Main Access}'))

    def test_find_board(self):
        known = ['Computer/Audio/Audio Clock/Audio Clock', 'Computer/A/LED Display/LED Display',
                 'Computer/B/LED Display/LED Display']
        path, error = kicad_references.find_board('Audio Clock', known)
        self.assertIsNone(error)
        self.assertTrue(path.endswith('Audio Clock.kicad_sch'))
        self.assertIn('is 2 schematics', kicad_references.find_board('LED Display', known)[1])
        self.assertIsNone(kicad_references.find_board('B/LED Display', known)[1])
        self.assertIn('no schematic', kicad_references.find_board('Nothing', known)[1])


if __name__ == '__main__':
    unittest.main()
