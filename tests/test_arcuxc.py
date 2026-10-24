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
        self.assertEqual((version,count,nbytes,sbytes), (2,3,96,204))
        self.assertEqual(len(data),arcuxc.HEADER.size+nbytes+sbytes+strbytes+abytes)
        self.assertEqual(arcuxc.NODE.unpack_from(data,arcuxc.HEADER.size+32)[0],0)
    def test_determinism(self):
        html='<div style="background:#123456"><span data-action="ok">Hello</span></div>'
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

class StandardsProfileTests(unittest.TestCase):
    def test_void_html_elements(self):
        data=arcuxc.compile_ui('<!doctype html><div><img src="examples/dashboard.html"><br><span>OK</span></div>',SRC)
        self.assertEqual(arcuxc.HEADER.unpack_from(data)[2],4)
    def test_no_scripting(self):
        for html in ['<div><script>alert(1)</script></div>','<button onclick="a()">x</button>', '<div><iframe></iframe></div>']:
            with self.subTest(html=html),self.assertRaises(ValueError):arcuxc.compile_ui(html,SRC)
    def test_css_descendant_selector(self):
        source='<style>div span { color: red } div > span { color: blue }</style><div><span>hello</span></div>'
        data=arcuxc.compile_ui(source,SRC)
        offset=arcuxc.HEADER.size+2*arcuxc.NODE.size+arcuxc.STYLE.size
        self.assertEqual(arcuxc.STYLE.unpack_from(data,offset)[11],arcuxc.color('blue'))
    def test_important_over_inline(self):
        source='<style>#x { color: red !important }</style><div id="x" style="color:blue">x</div>'
        data=arcuxc.compile_ui(source,SRC)
        style=arcuxc.STYLE.unpack_from(data,arcuxc.HEADER.size+arcuxc.NODE.size)
        self.assertEqual(style[11],arcuxc.color('red'))
    def test_no_silent_unsupported_css(self):
        for source in ('<style>@media screen { div {color:red} }</style><div></div>',
                       '<style>div { display:flex; grid-template-columns:1fr 1fr }</style><div></div>'):
            with self.assertRaises(ValueError):arcuxc.compile_ui(source,SRC)
