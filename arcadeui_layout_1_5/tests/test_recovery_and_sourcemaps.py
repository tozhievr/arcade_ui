import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from diagnostics import Source,DiagnosticGroup
from arcuxc import compile_ui

class RecoveryTests(unittest.TestCase):
    def gather(self,code):
        s=Source(code,'example.html');s.collecting=True
        with self.assertRaises(DiagnosticGroup) as caught:
            compile_ui(code,ROOT,s)
        return caught.exception.diagnostics
    def test_multiple_independent_css_errors(self):
        html='''<style>\n  div { widht: 5px; colro: red; color: blue }\n  span { margn: 8px; padding: 2px }\n</style><div><span>Hello</span></div>'''
        ds=self.gather(html)
        self.assertEqual([d.code for d in ds],['AUX-CSS-101']*3)
        self.assertEqual([(d.span.line,d.span.column) for d in ds],[(2,9),(2,21),(3,10)])
    def test_multiple_style_blocks_and_inline(self):
        html='''<style>div { wiidth: 1px }</style>\n<style>span { heigth: 2px }</style>\n<div style="widht: 2px"><span style="colro: red">Hi</span></div>'''
        ds=self.gather(html)
        self.assertEqual(len(ds),4)
        for d,token in zip(ds,['wiidth','heigth','widht','colro']):
            line=html.splitlines()[d.span.line-1]
            self.assertEqual(line[d.span.column-1:d.span.column-1+d.span.length],token)
    def test_html_recovery(self):
        html='<main><script>evil()</script><iframe>x</iframe><button onclick="f()">OK</button></main>'
        ds=self.gather(html)
        self.assertEqual([d.code for d in ds],['AUX-HTML-101','AUX-HTML-101','AUX-HTML-104'])
    def test_different_declarations_same_token(self):
        html='<style>div{widht:1px}div{widht:2px}</style><div></div>'
        ds=self.gather(html)
        self.assertEqual(len(ds),2)
        self.assertNotEqual(ds[0].span.column,ds[1].span.column)
    def test_utf8_position(self):
        html='<style>\n/* привет */ div { widht: 3px }\n</style><div/>'
        ds=self.gather(html)
        d=ds[0]
        self.assertEqual(html.splitlines()[d.span.line-1][d.span.column-1:d.span.column+4],'widht')
    def test_cli_multiple_json_and_no_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=pathlib.Path(tmp)/'input.html';out=pathlib.Path(tmp)/'out.arcuxi'
            path.write_text('<style>div{widht:1px;colro:red}</style><div></div>')
            run=subprocess.run([sys.executable,str(ROOT/'tools/arcuxc.py'),str(path),'-o',str(out),'--diagnostic-format=json'],capture_output=True,text=True)
            self.assertEqual(run.returncode,1,run.stderr)
            obj=json.loads(run.stderr)
            self.assertEqual(obj['summary']['errors'],2)
            self.assertFalse(out.exists())
            self.assertEqual(obj['diagnostics'][0]['location']['endColumn']-obj['diagnostics'][0]['location']['column'],5)
    def test_limit(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=pathlib.Path(tmp)/'input.html';out=pathlib.Path(tmp)/'out.arcuxi'
            path.write_text('<style>div{widh:1px;colro:red;heigth:5px}</style><div></div>')
            run=subprocess.run([sys.executable,str(ROOT/'tools/arcuxc.py'),str(path),'-o',str(out),'--max-errors=2','--diagnostic-format=json'],capture_output=True,text=True)
            self.assertEqual(json.loads(run.stderr)['summary']['errors'],2)

if __name__=='__main__':unittest.main()

class HtmlSpanTests(unittest.TestCase):
    def test_event_attribute_exact(self):
        html='<button title="onclick"\n class="hello" onclick="bad()">Hi</button>'
        s=Source(html,'x.html');s.collecting=True
        with self.assertRaises(DiagnosticGroup) as e:compile_ui(html,ROOT,s)
        d=e.exception.diagnostics[0]
        self.assertEqual(d.code,'AUX-HTML-104')
        self.assertEqual((d.span.line,d.span.column,d.span.length),(2,16,7))
    def test_two_bad_values_same_element(self):
        html='<div style="width: weird; color: fakecolor"></div>'
        s=Source(html,'x.html');s.collecting=True
        with self.assertRaises(DiagnosticGroup) as e:compile_ui(html,ROOT,s)
        ds=e.exception.diagnostics
        self.assertEqual([d.code for d in ds],['AUX-CSS-108','AUX-CSS-108'])
