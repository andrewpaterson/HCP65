"""Which pages code_examples.py hands hcp65index -examples, and the errors it reads back (DocsPlan.txt F1).

How a page's blocks are compiled is tested with the C++ that does it (HCP65-IDE's TestIndexLib,
TestPageExamples.cpp).  Run by web.sh's checks: .venv/bin/python tools/test_code_examples.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import code_examples  # noqa: E402


class TestCodeExamples(unittest.TestCase):
    def test_has_examples(self):
        self.assertTrue(code_examples.has_examples('# Page\n\n```cpp\nx = 1;\n```\n'))
        self.assertTrue(code_examples.has_examples('Text\n  ~~~ {.cpp .numbers}\nx\n~~~\n'))
        self.assertTrue(code_examples.has_examples('```{.cpp .no-check}\nx\n```\n'))     # hcp65index skips it
        self.assertFalse(code_examples.has_examples('```text\ncpp\n```\n'))
        self.assertFalse(code_examples.has_examples('```cppx\n```\n'))
        self.assertFalse(code_examples.has_examples('A `cpp` span and ```` ```cpp ```` written out.\n'))

    def test_errors_of(self):
        output = ('Process (assembler.md.1.cpp)\n'
                  '/r/HCP65/Docs/a.md:12:3: error: The class has no member function of that name [cAssembler]\n'
                  '/r/C: d/b.md:1:1: error: The page isn\'t an item of a project of the solutions, so ...\n')
        self.assertEqual([('/r/HCP65/Docs/a.md', 12, 3, 'The class has no member function of that name [cAssembler]'),
                          ('/r/C: d/b.md', 1, 1, 'The page isn\'t an item of a project of the solutions, so ...')],
                         code_examples.errors_of(output))


if __name__ == '__main__':
    unittest.main()
