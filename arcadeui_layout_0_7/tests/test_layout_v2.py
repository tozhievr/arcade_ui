import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import arcuxc


def packed_style(source, index=0):
    data = arcuxc.compile_ui(source, ROOT)
    header = arcuxc.HEADER.unpack_from(data)
    assert header[1] == 2
    start = arcuxc.HEADER.size + header[3] + arcuxc.STYLE.size * index
    return arcuxc.STYLE.unpack_from(data, start)


class LayoutV2Tests(unittest.TestCase):
    def test_explicit_grid_columns(self):
        s = packed_style('<div style="display:grid;grid-template-columns:repeat(3, 1fr)"><span>a</span></div>')
        self.assertEqual(s[-1], 3)
        self.assertEqual(s[0], 2)

    def test_grid_direct_fraction_list(self):
        s = packed_style('<div style="display:grid;grid-template-columns:1fr 1fr"></div>')
        self.assertEqual(s[-1], 2)

    def test_flex_align_and_justify(self):
        s = packed_style('<div style="display:flex;justify-content:space-between;align-items:center"></div>')
        self.assertEqual(s[-4:], (3, 2, 0, 0))

    def test_flex_wrap(self):
        s = packed_style('<div style="display:flex;flex-wrap:wrap"></div>')
        self.assertEqual(s[-2], 1)

    def test_invalid_grid_tracks_diagnosed(self):
        for css in ['repeat(2, 100px)', 'minmax(0, 1fr)', 'repeat(0, 1fr)']:
            with self.subTest(css=css), self.assertRaisesRegex(ValueError, 'grid-template-columns'):
                packed_style('<div style="display:grid;grid-template-columns:'+css+'"></div>')

    def test_properties_in_wrong_display_diagnosed(self):
        for css in ['grid-template-columns:1fr 1fr', 'flex-wrap:wrap']:
            with self.subTest(css=css), self.assertRaises(ValueError):
                packed_style('<div style="'+css+'"></div>')

    def test_unsupported_alignment_diagnosed(self):
        with self.assertRaisesRegex(ValueError, 'justify-content'):
            packed_style('<div style="display:flex;justify-content:space-evenly"></div>')

    def test_binary_v2_header(self):
        blob = arcuxc.compile_ui('<div></div>', ROOT)
        h = arcuxc.HEADER.unpack_from(blob)
        self.assertEqual((h[1], h[4]), (2, 68))


if __name__ == '__main__':
    unittest.main()
