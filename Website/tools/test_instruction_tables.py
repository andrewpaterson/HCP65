"""The tables instruction_tables.py makes of the 65C64's Instructions sheet (DocsPlan.txt C1, C3).

Run by web.sh's checks: .venv/bin/python tools/test_instruction_tables.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import instruction_tables  # noqa: E402
from instruction_tables import Instruction  # noqa: E402

COPY = Instruction('0', 'Copy (zero extend)', 'cpy', '0', '', '0', 'Logical', 'lda ldx', 'O2 → O1', 'n, z', '')
COPY_SIGNED = Instruction('1', 'Copy (sign extend)', 'cpy', '0', '', '1', 'Logical', '', 'O2:s → O1', 'n, z', '')
ABSOLUTE = Instruction('52', 'Absolute', 'abs', '14', '', '3', 'Arithmetic', '', 'abs(R2:s) → R1', 'n, z', '')
INSTRUCTIONS = {'0': COPY, '1': COPY_SIGNED, '52': ABSOLUTE}


class TestInstructionTables(unittest.TestCase):
    def test_form_table(self):
        # A form table leaves out a mnemonic the row above has; a text row's mnemonic isn't a reference.
        spec = {'columns': [['mnemonic', 'Mnemonic'], ['form', 'Form'], ['operation', 'Operation']],
                'rows': [0, 1, {'row': 52, 'text': True}]}
        lines = instruction_tables.table('t', spec, INSTRUCTIONS).split('\n')
        self.assertTrue(lines[0].startswith('<!-- Generated from the Instructions sheet'))
        self.assertEqual(['| Mnemonic | Form | Operation |', '|---|---|---|', '| `cpy` | Zero extend | O2 → O1 |',
                          '| | Sign extend | O2:s → O1 |', '| `#!text abs` | | abs(R2:s) → R1 |', ''], lines[2:])

    def test_index_table(self):
        # An index row links its mnemonic, and its name leaves the form out.
        spec = {'columns': [['link', 'Mnemonic'], ['name', 'Name'], ['flags', 'Flags'], ['w65c816', 'W65C816']],
                'rows': [[0, 'cpy.md']]}
        self.assertEqual('| [`cpy`](cpy.md) | Copy | n, z | lda ldx |',
                         instruction_tables.table('t', spec, INSTRUCTIONS).split('\n')[4])

    def test_missing_row(self):
        spec = {'columns': [['mnemonic', 'Mnemonic']], 'rows': [7]}
        with self.assertRaises(instruction_tables.InstructionTableError):
            instruction_tables.table('t', spec, INSTRUCTIONS)

    def test_instruction_list(self):
        lines = instruction_tables.instruction_list(INSTRUCTIONS).split('\n')
        self.assertIn('0\tcpy\t0\t-\t0\tCopy (zero extend)', lines)
        self.assertIn('52\tabs\t14\t-\t3\tAbsolute', lines)

    def test_sheet(self):
        # The real sheet: its rows read, a carried on row's W65C816 list joined to the row above's.
        instructions = instruction_tables.read_instructions()
        self.assertGreaterEqual(len(instructions), 100)
        self.assertNotIn('17', instructions)
        self.assertEqual('tax tay txa txy tya tyx tcd tdc tcs tsc tsx txs xba', instructions['16'].w65c816)
        self.assertEqual([], instruction_tables.check_pages(instructions, instruction_tables.read_spec()))


if __name__ == '__main__':
    unittest.main()
