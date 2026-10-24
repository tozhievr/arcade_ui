import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from arcuxc import compile_ui
from diagnostics import DiagnosticError,Source,render,render_json

ROOT=pathlib.Path(__file__).resolve().parents[1]

class DiagnosticsTests(unittest.TestCase):
    def check_error(self,text,code,token):
        src=Source(text,'sample.html')
        with self.assertRaises(DiagnosticError) as caught:
            compile_ui(text,ROOT,src)
        d=caught.exception.diagnostic
        self.assertEqual(d.code,code)
        self.assertIn(token,src.lines[d.span.line-1])
        self.assertEqual(src.lines[d.span.line-1][d.span.column-1:d.span.column-1+len(token)],token)
        self.assertIn('sample.html',render(d,src))
        self.assertEqual(json.loads(render_json(d))['code'],code)
        return d
    def test_inline_property_location_and_suggestion(self):
        d=self.check_error('<div\n style="widht: 40px"></div>','AUX-CSS-101','widht')
        self.assertIn('width',d.hint)
    def test_stylesheet_property_location(self):
        self.check_error('<style>\n  div { transfrom: none; }\n</style><div></div>', 'AUX-CSS-101','transfrom')
    def test_forbidden_script(self):
        self.check_error('<div>\n<script>alert(1)</script>\n</div>','AUX-HTML-101','<script>')
    def test_bad_closing_tag(self):
        self.check_error('<div>\n</span>','AUX-HTML-106','</span>')
    def test_unclosed_element(self):
        self.check_error('<main><section></section>','AUX-HTML-107','<main>')
    def test_cli_json_and_nonzero_return(self):
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/'bad.html';path.write_text('<div style="widht: 1px"></div>')
            out=pathlib.Path(temp)/'out.arcuxi'
            proc=subprocess.run([sys.executable,str(ROOT/'tools/arcuxc.py'),str(path),'-o',str(out),
                 '--diagnostic-format','json'],capture_output=True,text=True)
            self.assertEqual(proc.returncode,1)
            self.assertEqual(json.loads(proc.stderr)['location']['column'],13)
            self.assertFalse(out.exists())

if __name__=='__main__':unittest.main()
