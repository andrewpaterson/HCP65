"""The references to code on a fixture page give exactly the links, errors and warnings expected (DocsPlan.txt D).

Run by web.sh's checks: .venv/bin/python tools/test_code_references.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import code_references  # noqa: E402
import symbol_table  # noqa: E402
from symbol_table import Symbol  # noqa: E402

SYMBOLS = [
    Symbol('CShape', 'class', 'Lib/Shapes/Shape.h', 5),
    Symbol('CShape::Init(int)', 'function', 'Lib/Shapes/Shape.h', 11),
    Symbol('CShape::Init(char const*, int)', 'function', 'Lib/Shapes/Shape.h', 12),
    Symbol('CShape::GetSides() const', 'function', 'Lib/Shapes/Shape.h', 13),
    Symbol('EKind::EK_Square', 'enumerator', 'Lib/Shapes/Shape.h', 20),
    Symbol('main(int, char*[])', 'function', 'Lib/Tools/Main.cpp', 3),
    Symbol('X()', 'function', 'Lib/Shapes/Shape.h', 30),
    Symbol('abs(int)', 'function', 'Lib/Shapes/Shape.h', 31),
]
FILES = ['Lib/Shapes/Shape.h', 'Lib/Shapes/Shape.cpp', 'Lib/Tools/Main.cpp', 'Other/Shapes/Shape.h', 'Lib/Notes Plan.txt']
LINKS = code_references.Links({'Lib': 'https://github.com/someone/Lib', 'Other': 'https://github.com/someone/Other'},
                              {'Lib': 'main', 'Other': 'master'})

PAGE = '''# Shapes

A shape is a [`CShape`], made by [`CShape::Init`] or [`CShape::Init(const char *, int)`] and asked [`CShape::GetSides()`].
Its kind is [`EK_Square`] or [`EKind::EK_Square`]; its header [`Shapes/Shape.h`] is in [`Lib/Shapes/Shape.h`], the plan in [`Notes Plan.txt`].
Broken: [`CShape::Kill`], [`CShape::Init(float)`], [`Shape.h`] and [`Missing.h`].
Plain: `CShape`, `CShape::Init` and `CTank`, `x = 1`, `main`, `X`, `#!text abs`, `abs`, `Shape.h`.
Already links: [`CShape`](shapes.md), [`CShape`][shape], ![`CShape`](shape.png) and \\[`CShape`].

```cpp
CShape cShape;   // [`Missing`] in a fence is code
```

<!-- [`Missing`] in a comment, `CShape`
still in it --> after it `CShape::GetSides`
'''

LINKED = '''# Shapes

A shape is a [`CShape`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L5), made by [`CShape::Init`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L11) or [`CShape::Init(const char *, int)`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L12) and asked [`CShape::GetSides()`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L13).
Its kind is [`EK_Square`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L20) or [`EKind::EK_Square`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h#L20); its header [`Shapes/Shape.h`] is in [`Lib/Shapes/Shape.h`](https://github.com/someone/Lib/blob/main/Shapes/Shape.h), the plan in [`Notes Plan.txt`](https://github.com/someone/Lib/blob/main/Notes%20Plan.txt).
'''

PROBLEMS = [
    (3, 62, 'error', '[`Shapes/Shape.h`] names 2 files (Lib/Shapes/Shape.h, Other/Shapes/Shape.h): write more of its path'),
    (4, 8, 'error', '[`CShape::Kill`] names nothing the indexed solutions declare (mkdocs.yml extra.solutions)'),
    (4, 26, 'error', '[`CShape::Init(float)`] names nothing the indexed solutions declare (mkdocs.yml extra.solutions)'),
    (4, 51, 'error', '[`Shape.h`] names 2 files (Lib/Shapes/Shape.h, Other/Shapes/Shape.h): write more of its path'),
    (4, 67, 'error', '[`Missing.h`] names no file in the repositories'),
    (5, 7, 'warning', '`CShape` names class CShape (Lib/Shapes/Shape.h:5): write [`CShape`] to link to it'),
    (5, 17, 'warning', '`CShape::Init` names function CShape::Init(int) (Lib/Shapes/Shape.h:11): write [`CShape::Init`] to link to it'),
    (5, 81, 'warning', '`abs` names function abs(int) (Lib/Shapes/Shape.h:31): write [`abs`] to link to it'),
    (13, 25, 'warning', '`CShape::GetSides` names function CShape::GetSides() const (Lib/Shapes/Shape.h:13): write [`CShape::GetSides`] to link to it'),
]


class TestCodeReferences(unittest.TestCase):
    def setUp(self):
        self.table = symbol_table.Table(SYMBOLS, FILES)

    def test_page(self):
        linked, problems = code_references.convert(PAGE, self.table, LINKS)
        self.assertEqual(PAGE.split('\n')[5:], linked.split('\n')[5:])
        self.assertEqual(LINKED.split('\n'), linked.split('\n')[:4] + [''])
        self.assertEqual(PROBLEMS, problems)

    def test_resolve(self):
        self.assertEqual('CShape::Init(int)', self.table.resolve_symbol('CShape::Init').key)
        self.assertEqual('CShape::Init(int)', self.table.resolve_symbol('CShape::Init( int )').key)
        self.assertEqual('CShape::GetSides() const', self.table.resolve_symbol('CShape::GetSides(void)').key)
        self.assertEqual('CShape::GetSides() const', self.table.resolve_symbol('CShape::GetSides() const').key)
        self.assertIsNone(self.table.resolve_symbol('CShape::GetSides() volatile'))
        self.assertIsNone(self.table.resolve_symbol('Shape'))
        self.assertTrue(symbol_table.is_file_reference('BaseLib/Chars.h'))
        self.assertTrue(symbol_table.is_file_reference('web.sh'))
        self.assertFalse(symbol_table.is_file_reference('CShape::Init'))
        self.assertFalse(symbol_table.is_file_reference('CShape'))
        self.assertEqual((['charconst*', 'int'], ''), symbol_table.split_parameters('(const char *, int)'))
        self.assertEqual((['CArrayTemplate<M,N>*'], 'const'), symbol_table.split_parameters('(CArrayTemplate<M, N>*) const'))


if __name__ == '__main__':
    unittest.main()
