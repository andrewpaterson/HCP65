"""How check_decode.py reads LogiCim's decode and compares the memory map and registers with it (DocsPlan.txt Ie).

Run by web.sh's checks: .venv/bin/python tools/test_check_decode.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_decode  # noqa: E402

KERNEL = '~{Reset}=1,Kernal Mode=1,RAM Swap=1'
BEFORE = '~{Reset}=1,Kernal Mode=1,RAM Swap=0'

# A board like the HCP65's with two devices and two registers.
DECODE = '\n'.join('\t'.join(cells) for cells in [
    [KERNEL, '~{RAM}', '$000000', '$07FFFF'],
    [KERNEL, '~{ROM}', '$080000', '$0FFFFF'],
    [KERNEL, '~{Registers}', '$100000', '$107FFF'],
    [KERNEL, '~{Register0}', '$100000', '$100000', '$000010', '$107FF0'],
    [KERNEL, '~{Register1}', '$100001', '$100001', '$000010', '$107FF1'],
    [KERNEL, '~{Device1}', '$108000', '$10FFFF'],
    [KERNEL, '~{Device2}', '$110000', '$117FFF'],
    [KERNEL, '~{Devices}', '$108000', '$1FFFFF'],
    [KERNEL, '~{Main Memory}', '$200000', '$FFFFFF'],
    [BEFORE, '~{RAM}', '$080000', '$0FFFFF'],
    [BEFORE, '~{ROM}', '$000000', '$07FFFF'],
]) + '\n'

MEMORY_MAP = '''# Memory Map

## Kernel mode

In kernel mode the bottom 2MB holds the processor board's own memory and the devices.

| Address | Size | What |
|---|---|---|
| `$000000`–`$07FFFF` | 512KB | MPU RAM |
| `$080000`–`$0FFFFF` | 512KB | MPU ROM |
| `$100000`–`$107FFF` | 32KB | [On-board registers](registers.md) (device slot 0) |
| `$108000`–`$117FFF` | 2 × 32KB | [Devices 1–2](devices.md#device-sockets) |
| `$118000`–`$1FFFFF` | 928KB | Nothing |
| `$200000`–`$FFFFFF` | 14MB | [Main memory](devices.md#main-memory) |

### RAM and ROM swap at boot

| | Before the first switch to native mode | After it |
|---|---|---|
| `$000000`–`$07FFFF` | MPU ROM | MPU RAM |
| `$080000`–`$0FFFFF` | MPU RAM | MPU ROM |

## User mode
'''

REGISTERS = '''# On-board Registers

Only address bits A0–A3 are decoded, so the 16 registers repeat every 16 bytes through to `$107FFF`.

| Register | Address | Name | Access |
|---|---|---|---|
| 0 | `$100000` | Device Interrupt | Read |
| 1 | `$100001` | Not connected | |
'''


class TestCheckDecode(unittest.TestCase):
    def setUp(self):
        self.ranges = check_decode.parse_ranges(DECODE)

    def test_parse(self):
        registers = check_decode.active(self.ranges, check_decode.KERNEL, '~{Register1}')
        self.assertEqual(2048, len(registers))
        self.assertEqual([(0x100001, 0x100001), (0x100011, 0x100011)], registers[:2])
        self.assertEqual((0x107FF1, 0x107FF1), registers[-1])
        self.assertEqual([(0x080000, 0x0FFFFF)], check_decode.active(self.ranges, check_decode.BEFORE_LATCH, '~{RAM}'))
        with self.assertRaises(check_decode.DecodeError):
            check_decode.parse_ranges('%s\t~{RAM}\t$000000\n' % KERNEL)

    def test_intervals(self):
        self.assertEqual([(5, 7)], check_decode.within([(0, 7), (9, 12)], 5, 8))
        self.assertEqual([(0, 1), (6, 7), (9, 12)], check_decode.outside([(0, 7), (9, 12)], [(2, 5)]))
        self.assertEqual([], check_decode.outside([(2, 5)], [(0, 9)]))

    def test_selects(self):
        self.assertEqual(['~{Device16}', '~{Device17}'],
                         check_decode.selects('[Devices 16–17](devices.md#device-sockets)'))
        self.assertEqual([], check_decode.selects('Nothing'))
        self.assertIsNone(check_decode.selects('Video RAM'))

    def test_pages_agree(self):
        self.assertEqual(([], 8), check_decode.check_memory_map(self.ranges, MEMORY_MAP))
        self.assertEqual(([], 2), check_decode.check_registers(self.ranges, REGISTERS))

    def test_row_differs(self):
        # Device 2 isn't where the row puts it, and ~{Device1} is in the Nothing row's addresses instead.
        page = MEMORY_MAP.replace('`$108000`–`$117FFF` | 2 × 32KB', '`$110000`–`$11FFFF` | 2 × 32KB')
        problems, _ = check_decode.check_memory_map(self.ranges, page)
        lines = [line for line, _ in problems]
        self.assertIn(12, lines)
        self.assertTrue(any('~{Device1} at $108000–$10FFFF too' in message for _, message in problems))
        self.assertTrue(any('the decode has ~{Device2} at $110000–$117FFF, not $118000–$11FFFF' in message
                            for _, message in problems))

    def test_select_not_in_table(self):
        page = MEMORY_MAP.replace('| `$200000`–`$FFFFFF` | 14MB | [Main memory](devices.md#main-memory) |\n', '')
        problems, _ = check_decode.check_memory_map(self.ranges, page)
        self.assertEqual([(7, 'the decode has ~{Main Memory} at $200000–$FFFFFF too, which the table doesn\'t '
                              'give it')], problems)
        page = page.replace('| `$118000`–`$1FFFFF` | 928KB | Nothing |', '| `$118000`–`$FFFFFF` | 15MB | Nothing |')
        problems, _ = check_decode.check_memory_map(self.ranges, page)
        self.assertTrue(any('also has ~{Main Memory} at $200000–$FFFFFF' in message for _, message in problems))

    def test_swap_differs(self):
        page = MEMORY_MAP.replace('| `$000000`–`$07FFFF` | MPU ROM | MPU RAM |',
                                  '| `$000000`–`$07FFFF` | MPU RAM | MPU RAM |')
        problems, _ = check_decode.check_memory_map(self.ranges, page)
        self.assertEqual([20], sorted({line for line, _ in problems}))

    def test_register_differs(self):
        page = REGISTERS.replace('| 1 | `$100001`', '| 1 | `$100002`')
        problems, _ = check_decode.check_registers(self.ranges, page)
        self.assertEqual([8], [line for line, _ in problems])
        self.assertIn('the decode has ~{Register1} at $100001, $100011', problems[0][1])
        problems, _ = check_decode.check_registers(self.ranges, REGISTERS.replace('`$107FFF`', '`$10FFFF`'))
        self.assertEqual([7, 8], [line for line, _ in problems])


if __name__ == '__main__':
    unittest.main()
