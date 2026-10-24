import importlib.util
import pathlib
import struct
import sys
import tempfile
import unittest

SRC=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(SRC/'tools'))
import arcuxc

class CompilerTests(unittest.TestCase):
    def test_header(self):
        data=arcuxc.compile_ui('<div style="display:flex;width:200px"><span>abc</span><span>xyz</span></div>',SRC)
        magic,version,count,nbytes,sbytes,strbytes,abytes,_,_,sha=arcuxc.HEADER.unpack_from(data)
        self.assertEqual(magic,arcuxc.MAGIC)
        self.assertEqual((version,count,nbytes,sbytes), (1,3,96,156))
        self.assertEqual(len(data),arcuxc.HEADER.size+nbytes+sbytes+strbytes+abytes)
        self.assertEqual(arcuxc.NODE.unpack_from(data,arcuxc.HEADER.size+32)[0],0)
    def test_determinism(self):
        html='<div style="background:#123456"><span onclick="ok">Hello</span></div>'
        self.assertEqual(arcuxc.compile_ui(html,SRC),arcuxc.compile_ui(html,SRC))
    def test_reject_unknown_css(self):
        with self.assertRaises(ValueError):arcuxc.compile_ui('<div style="transform:rotate(1deg)"></div>',SRC)
    def test_traversal(self):
        data=arcuxc.compile_ui('<div><div><span>deep</span></div><span>near</span></div>',SRC)
        entries=[arcuxc.NODE.unpack_from(data,arcuxc.HEADER.size+i*32) for i in range(4)]
        self.assertEqual([e[0] for e in entries],[0xffffffff,0,0,1])
        self.assertEqual(entries[0][1:3],(1,2))
    def test_invalid_assets(self):
        with self.assertRaises(ValueError):arcuxc.compile_ui('<img src="../secret.txt"/>',SRC)

if __name__=='__main__':unittest.main()
