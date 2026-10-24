"""Structured, source-spanned diagnostics for the ArcUX AOT compiler."""
from __future__ import annotations
from dataclasses import dataclass, field
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

@dataclass
class Diagnostic:
    code: str
    message: str
    span: Span
    severity: str = 'error'
    hint: str | None = None
    notes: list[str] = field(default_factory=list)
    def as_dict(self):
        return {'severity': self.severity, 'code': self.code,
                'message': self.message,
                'location': {'file':self.span.file, 'line':self.span.line,
                             'column':self.span.column,'length':self.span.length},
                'hint':self.hint, 'notes':self.notes}

class DiagnosticError(ValueError):
    def __init__(self, diagnostic: Diagnostic):
        super().__init__(f'{diagnostic.code}: {diagnostic.message}')
        self.diagnostic = diagnostic

class Source:
    def __init__(self, text: str, path: str = '<input>'):
        self.text = text
        self.path = path
        self.lines = text.splitlines() or ['']
    def span(self, line: int, col: int, length: int = 1):
        return Span(max(1,line), max(1,col), max(1,length), self.path)
    def at_offset(self, offset: int, length: int = 1):
        offset=max(0,min(offset,len(self.text)))
        return self.span(self.text.count('\n',0,offset)+1,
                         offset-(self.text.rfind('\n',0,offset)+1)+1,length)
    def fail(self, code: str, message: str, span: Span | None = None, hint: str | None = None, notes=()):
        raise DiagnosticError(Diagnostic(code,message,span or self.span(1,1),hint=hint,notes=list(notes)))

def render(diag: Diagnostic, source: Source, *, color: bool=False, context: int=2) -> str:
    line=max(1,diag.span.line)
    col=max(1,diag.span.column)
    def paint(code,text):return f'\x1b[{code}m{text}\x1b[0m' if color else text
    heading=paint('1;31' if diag.severity=='error' else '1;33',f'{diag.severity}[{diag.code}]')
    output=[f'{heading}: {diag.message}',f'  --> {diag.span.file}:{line}:{col}']
    low=max(1,line-context)
    high=min(len(source.lines),line+context)
    width=len(str(high))
    output.append(' '*(width+1)+' |')
    for current in range(low,high+1):
        text=source.lines[current-1].expandtabs(4)
        output.append(f'{str(current).rjust(width)} | {text}')
        if current==line:
            # no terminal width assumptions, and avoid enormous accidental underlines
            prefix=source.lines[current-1][:col-1].expandtabs(4)
            caret=' '*(len(prefix))+'^'*max(1,min(diag.span.length, max(1,len(text)-len(prefix))))
            output.append(' '*width+' | '+paint('1;31',caret))
    if diag.hint:output.append(f'  = help: {diag.hint}')
    output.extend(f'  = note: {note}' for note in diag.notes)
    return '\n'.join(output)

def render_json(diag: Diagnostic):
    return json.dumps(diag.as_dict(),ensure_ascii=False,indent=2)

def should_color(mode: str):
    return mode=='always' or (mode=='auto' and sys.stderr.isatty() and not os.environ.get('NO_COLOR'))
