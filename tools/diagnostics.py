"""ArcUX diagnostics: codepoint-exact spans, multiple errors, stable machine output."""
from __future__ import annotations
from dataclasses import dataclass, field
import bisect
import json
import os
import sys
import unicodedata

@dataclass(frozen=True)
class Span:
    line: int
    column: int
    length: int = 1
    file: str = '<input>'
    end_line: int | None = None
    end_column: int | None = None

@dataclass
class Diagnostic:
    code: str
    message: str
    span: Span
    severity: str = 'error'
    hint: str | None = None
    notes: list[str] = field(default_factory=list)
    def as_dict(self):
        end_line = self.span.end_line or self.span.line
        end_col = self.span.end_column or (self.span.column + self.span.length)
        return {'severity':self.severity,'code':self.code,'message':self.message,
                'location':{'file':self.span.file,'line':self.span.line,'column':self.span.column,
                            'length':self.span.length,'endLine':end_line,'endColumn':end_col},
                'hint':self.hint,'notes':self.notes}

class DiagnosticError(ValueError):
    def __init__(self, diagnostic: Diagnostic):
        super().__init__(f'{diagnostic.code}: {diagnostic.message}')
        self.diagnostic=diagnostic

class DiagnosticGroup(ValueError):
    def __init__(self, diagnostics: list[Diagnostic]):
        super().__init__(f'{len(diagnostics)} compilation error(s)')
        self.diagnostics=diagnostics

class Source:
    def __init__(self,text: str,path: str='<input>'):
        self.text=text; self.path=path
        self.lines=text.splitlines() or ['']
        self.starts=[0]
        for i,c in enumerate(text):
            if c=='\n':self.starts.append(i+1)
        self.collecting=False
        self.diagnostics:list[Diagnostic]=[]
        self.max_errors=100
    def span(self,line:int,col:int,length:int=1):
        return Span(max(1,line),max(1,col),max(1,length),self.path)
    def at_offset(self,offset:int,length:int=1):
        offset=max(0,min(offset,len(self.text)))
        idx=bisect.bisect_right(self.starts,offset)-1
        end=min(len(self.text),offset+max(1,length))
        end_idx=bisect.bisect_right(self.starts,end)-1
        return Span(idx+1,offset-self.starts[idx]+1,max(1,length),self.path,
                    end_idx+1,end-self.starts[end_idx]+1)
    def offset(self,line:int,col:int):
        if line<1 or line>len(self.starts):return len(self.text)
        return min(len(self.text),self.starts[line-1]+max(0,col-1))
    def emit(self,code,message,span=None,hint=None,notes=(),severity='error'):
        d=Diagnostic(code,message,span or self.span(1,1),severity,hint,list(notes))
        self.diagnostics.append(d)
        if not self.collecting:raise DiagnosticError(d)
        if len(self.diagnostics)>=self.max_errors:raise DiagnosticGroup(self.diagnostics)
        return d
    def fail(self,code,message,span=None,hint=None,notes=()):
        return self.emit(code,message,span,hint,notes)
    def raise_if_errors(self):
        if self.diagnostics:raise DiagnosticGroup(self.diagnostics)


def render(diag:Diagnostic,source:Source,*,color:bool=False,context:int=2)->str:
    def paint(code,s):return f'\x1b[{code}m{s}\x1b[0m' if color else s
    line=max(1,diag.span.line);col=max(1,diag.span.column)
    heading=paint('1;31' if diag.severity=='error' else '1;33',f'{diag.severity}[{diag.code}]')
    out=[f'{heading}: {diag.message}',f'  --> {diag.span.file}:{line}:{col}']
    low=max(1,line-context);high=min(len(source.lines),line+context)
    width=len(str(high));out.append(' '*(width+1)+' |')
    for ln in range(low,high+1):
        raw=source.lines[ln-1];expanded=raw.expandtabs(4)
        out.append(f'{ln:>{width}} | {expanded}')
        if ln==line:
            prefix=raw[:col-1].expandtabs(4)
            marked=raw[col-1:col-1+diag.span.length].expandtabs(4)
            underline=' '*len(prefix)+'^'*max(1,min(len(marked),256))
            out.append(' '*width+' | '+paint('1;31',underline))
    if diag.hint:out.append(f'  = help: {diag.hint}')
    out.extend(f'  = note: {n}' for n in diag.notes)
    return '\n'.join(out)

def render_many(ds:list[Diagnostic],source:Source,*,color:bool=False,context:int=2)->str:
    return '\n\n'.join(render(d,source,color=color,context=context) for d in ds)+f'\n\narcuxc: {len(ds)} error(s)'

def render_json(diag:Diagnostic):return json.dumps(diag.as_dict(),ensure_ascii=False,indent=2)
def render_json_many(ds:list[Diagnostic]):
    return json.dumps({'version':1,'diagnostics':[d.as_dict() for d in ds],
                       'summary':{'errors':sum(d.severity=='error' for d in ds)}},ensure_ascii=False,indent=2)
def should_color(mode):return mode=='always' or (mode=='auto' and sys.stderr.isatty() and not os.environ.get('NO_COLOR'))
