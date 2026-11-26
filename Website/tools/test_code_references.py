"""The links code_references.py puts in a page, from what hcp65index -resolve says of it (DocsPlan.txt D, E1).

What a reference names is tested with the C++ that decides it (HCP65-IDE's TestIndexLib, TestReferences.cpp).
Run by web.sh's checks: .venv/bin/python tools/test_code_references.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import code_references  # noqa: E402
from symbol_table import Result  # noqa: E402

LINKS = code_references.Links({'Lib': 'https://github.com/someone/Lib', 'Other': 'https://github.com/someone/Other'},
                              {'Lib': 'main', 'Other': 'master'})


class TestCodeReferences(unittest.TestCase):
    def test_put_links(self):
        # The page's file has two lines of front matter MkDocs took off; columns are bytes, and → is three.
        markdown = '# Shapes\nA → [`CShape`] and [`Notes Plan.txt`], `CShape`.\n'
        results = [Result(4, 7, 'link', 16, 'Lib/Shapes/Shape.h', 5, None),
                   Result(4, 22, 'link', 39, 'Lib/Notes Plan.txt', 0, None),
                   Result(4, 42, 'warning', None, None, None, '`CShape` names class CShape')]
        self.assertEqual('# Shapes\nA → [`CShape`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L5) and '
                         '[`Notes Plan.txt`](https://github.com/someone/Lib/blob/main/Notes%20Plan.txt), `CShape`.\n',
                         code_references.put_links(markdown, results, 2, LINKS))

    def test_url(self):
        self.assertEqual('https://github.com/someone/Other/blob/master/A/B.h#L7', LINKS.url('Other/A/B.h', 7))
        self.assertEqual('https://github.com/someone/Lib/blob/main/web.sh', LINKS.url('Lib/web.sh'))


if __name__ == '__main__':
    unittest.main()
