import pathlib
import sys
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import arcuxc

class LayoutV3Tests(unittest.TestCase):
    def test_track_parser(self):
        self.assertEqual(arcuxc.parse_tracks('100px 1fr 2fr'),[(0,100.0),(1,1.0),(1,2.0)])
        self.assertEqual(arcuxc.parse_tracks('repeat(2, minmax(80px, 1fr))'),[(2,80.0)]*2)

    def test_v3_section_bounds(self):
        binary=arcuxc.compile_ui('<div style="display:grid;grid-template-columns:repeat(2, minmax(80px, 1fr))"></div>',ROOT)
        header=arcuxc.HEADER.unpack_from(binary)
        self.assertEqual(header[1],3)
        self.assertEqual(header[7],2*arcuxc.TRACK.size)
        self.assertEqual(len(binary),72+sum(header[i] for i in (3,4,5,6,7)))

    def test_min_width_conflict(self):
        with self.assertRaisesRegex(ValueError,'min-width'):
            arcuxc.compile_ui('<div style="min-width:300px;max-width:50px"></div>',ROOT)

    def test_span_error(self):
        with self.assertRaisesRegex(ValueError,'grid-column'):
            arcuxc.compile_ui('<div style="grid-column:2/4"></div>',ROOT)

    def test_bad_minmax(self):
        with self.assertRaisesRegex(ValueError,'grid-template-columns'):
            arcuxc.compile_ui('<div style="display:grid;grid-template-columns:minmax(auto, max-content)"></div>',ROOT)

    def test_binary_determinism(self):
        x='<div style="display:grid;grid-template-columns:repeat(3, 2fr)"><span style="grid-column:span 2">x</span></div>'
        self.assertEqual(arcuxc.compile_ui(x,ROOT),arcuxc.compile_ui(x,ROOT))

if __name__=='__main__':unittest.main()
