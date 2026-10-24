import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from arcuxc import compile_ui

class IntrinsicTextCompilerTests(unittest.TestCase):
    def test_unicode_text_is_preserved_in_binary(self):
        html = '<div style="display:flex"><span>你好 世界</span></div>'
        data = compile_ui(html, ROOT)
        self.assertIn('你好 世界'.encode('utf-8'), data)

    def test_word_break_text_is_preserved(self):
        data = compile_ui('<div>abc def</div>', ROOT)
        self.assertIn(b'abc def', data)

if __name__ == '__main__':
    unittest.main()
