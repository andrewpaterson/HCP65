"""How check_files.py reads a folder's README table and matches files (DocsPlan.txt I).

Run by web.sh's checks: .venv/bin/python tools/test_check_files.py
"""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_files  # noqa: E402


class TestCheckFiles(unittest.TestCase):
    def test_glob(self):
        gerbers = check_files.glob_pattern('**/*.g[bt][losp]')
        self.assertTrue(gerbers.match('Computer/Audio/Board/Board-B_Cu.gbl'))
        self.assertTrue(gerbers.match('Board-F_Cu.GTL'))     # any case
        self.assertFalse(gerbers.match('Board.gbr'))
        top = check_files.glob_pattern('*.kicad_wks')
        self.assertTrue(top.match('SchematicTemplate.kicad_wks'))
        self.assertFalse(top.match('Old/SchematicTemplate.kicad_wks'))
        self.assertTrue(check_files.glob_pattern('Old/**/*.step').match('Old/Pico/GPIO/GPIO.step'))

    def test_readme(self):
        folder = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, folder)
        path = os.path.join(folder, 'README.md')
        with open(path, 'w', encoding='utf-8') as file:
            file.write('# Fonts\n\nThe console font.\n\n| Files | Are | From |\n|---|---|---|\n'
                       '| `Make.py` | Source | |\n| `CONSOLE.SFT`, `CONSOLE.bmp` | Generated | `python3 Make.py` |\n'
                       '| `Licence.txt` | Third-party | |\n| `Odd.txt` | Borrowed | somewhere |\n')
        rows, problems = check_files.read_readme(path)
        self.assertEqual([7, 8, 9], [row[0] for row in rows])
        self.assertEqual(['Source', 'Generated', 'Third-party'], [row[2] for row in rows])
        self.assertTrue(rows[1][1][1].match('CONSOLE.bmp'))
        self.assertEqual(2, len(problems))
        self.assertIn(':9:1: error: a Third-party row says where its files come from', problems[0])
        self.assertIn(':10:1: error: a row is', problems[1])

    def test_kinds_of_file(self):
        self.assertTrue(check_files.is_test_data('Tests/TestBaseLib/TestBaseLib/Finder/Ants.txt'))
        self.assertTrue(check_files.is_test_data('HCP65-Emulator/TestHCP65Lib/TestHCP65Lib/Kicad/Board.kicad_sch'))
        self.assertFalse(check_files.is_test_data('BootTest/BootTest.cpp'))
        self.assertTrue(check_files.is_own('LICENSE'))
        self.assertTrue(check_files.is_own('Libraries/BaseLib/BaseLib.vcxproj.filters'))
        self.assertTrue(check_files.is_own('LogiCim/COPYING'))
        self.assertFalse(check_files.is_own('Libraries/BaseLib/Chars.cpp'))
        cmake = {'HCP65Emulator': 'add_executable(HCP65Emulator HCP65EmulatorLinux.cpp)'}
        self.assertTrue(check_files.in_cmake('HCP65Emulator/HCP65EmulatorLinux.cpp', cmake))
        self.assertFalse(check_files.in_cmake('HCP65Emulator/Other.cpp', cmake))


if __name__ == '__main__':
    unittest.main()
