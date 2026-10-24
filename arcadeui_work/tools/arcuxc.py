#!/usr/bin/env python3
"""ArcUX AOT compiler: deterministic packed UI image, Python 3.11+."""
import argparse
import hashlib
import html.parser
import pathlib
import re
import struct
import sys

MAGIC = b'ARCUXI\x00\x01'
HEADER = struct.Struct('<8sIIIIIIII32s')
NODE = struct.Struct('<IIIIIIII')
STYLE = struct.Struct('<II8fIII')
# node: parent,first_child,child_count,style,text_offset,text_len,event_offset,event_len
# style: display, direction, width,height,grow,shrink,gap,padding,margin,font_size,background,text_color,flags
COLORS={'black':0xff000000,'white':0xffffffff,'transparent':0x00000000,'red':0xffff0000,'blue':0xff0000ff,'green':0xff008000,'gray':0xff808080,'grey':0xff808080,'purple':0xff800080,'navy':0xff000080}
PROPS=['display','flex-direction','width','height','flex-grow','flex-shrink','gap','padding','margin','font-size','background','color']

def color(s):
    s=s.strip().lower()
    if s in COLORS:return COLORS[s]
    if re.fullmatch(r'#[0-9a-f]{6}',s):return 0xff000000 | int(s[1:],16)
    if re.fullmatch(r'#[0-9a-f]{8}',s):return int(s[7:9]+s[1:7],16)
    raise ValueError('Unsupported color: '+s)

def length(s):
    s=s.strip()
    if s=='auto':return -1.0
    if re.fullmatch(r'-?(?:\d+(?:\.\d*)?|\.\d+)(px)?',s):return float(s.removesuffix('px'))
    raise ValueError('Expected px, unitless number or auto, got '+s)

def declarations(s):
    out={}
    for part in s.split(';'):
        if not part.strip():continue
        if ':' not in part:raise ValueError('Malformed declaration: '+part)
        k,v=part.split(':',1);k=k.strip().lower();v=v.strip()
        if k not in PROPS:raise ValueError('Unsupported CSS property: '+k)
        out[k]=v
    return out

def stylesheet(text):
    text=re.sub(r'/\*.*?\*/','',text,flags=re.S)
    rules=[]
    for selector,body in re.findall(r'([^{}]+)\{([^{}]*)\}',text):
        decl=declarations(body)
        for sel in selector.split(','):
            sel=sel.strip()
            if not re.fullmatch(r'(\*|[a-zA-Z][\w-]*|\.[\w-]+|#[\w-]+)',sel):raise ValueError('Unsupported CSS selector: '+sel)
            rules.append((sel,decl))
    return rules

class Parser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.nodes=[];self.stack=[];self.styles=[];self.in_style=False
    def handle_starttag(self,tag,attrs):
        if tag=='style':self.in_style=True;return
        if tag in ('html','head','body'):return
        a=dict(attrs);idx=len(self.nodes)
        self.nodes.append(dict(tag=tag,attrs=a,parent=self.stack[-1] if self.stack else 0xffffffff,children=[],text=''))
        if self.stack:self.nodes[self.stack[-1]]['children'].append(idx)
        self.stack.append(idx)
    def handle_startendtag(self,tag,attrs):
        self.handle_starttag(tag,attrs);self.handle_endtag(tag)
    def handle_endtag(self,tag):
        if tag=='style':self.in_style=False;return
        if tag in ('html','head','body'):return
        if not self.stack or self.nodes[self.stack[-1]]['tag']!=tag:raise ValueError('Unbalanced tag: '+tag)
        self.stack.pop()
    def handle_data(self,data):
        if self.in_style:self.styles.append(data)
        elif self.stack:self.nodes[self.stack[-1]]['text']+=data.strip()

def compile_ui(source,asset_root):
    parser=Parser();parser.feed(source)
    if parser.stack:raise ValueError('Unclosed element')
    if not parser.nodes:raise ValueError('No UI elements')
    if sum(n['parent']==0xffffffff for n in parser.nodes)!=1:raise ValueError('Exactly one root element is required')
    rules=stylesheet('\n'.join(parser.styles))
    # BFS produces contiguous siblings and cache-friendly layout layers.
    order=[];queue=[0]
    for idx in queue:
        order.append(idx);queue.extend(parser.nodes[idx]['children'])
    mapping={old:new for new,old in enumerate(order)}
    strings=bytearray();entries=[];styles=[]
    def intern(s):
        data=s.encode('utf-8');o=len(strings);strings.extend(data);return o,len(data)
    for old in order:
        n=parser.nodes[old];a=n['attrs'];decl={}
        # selector cascade: tag, class, id, inline; ties retain source order
        matches=[]
        for ri,(sel,rule) in enumerate(rules):
            matched=(sel=='*' or sel==n['tag'] or (sel.startswith('.') and sel[1:] in a.get('class','').split()) or (sel.startswith('#') and sel[1:]==a.get('id','')))
            if matched:matches.append((0 if sel=='*' else 1 if not sel.startswith(('.', '#')) else 10 if sel.startswith('.') else 100,ri,rule))
        for _,_,rule in sorted(matches):decl.update(rule)
        decl.update(declarations(a.get('style','')))
        display={'block':0,'flex':1,'grid':2,'none':3}.get(decl.get('display','block'))
        if display is None:raise ValueError('Unsupported display: '+decl['display'])
        direction={'row':0,'column':1}.get(decl.get('flex-direction','row'))
        if direction is None:raise ValueError('Only flex-direction row/column supported')
        def number(key,default):return length(decl[key]) if key in decl else default
        bg=color(decl.get('background','transparent'));fg=color(decl.get('color','white'))
        # 32-bit flags contain text/bg colors? colors are stored in the 2 final uints.
        styles.append(STYLE.pack(display,direction,number('width',-1),number('height',-1),number('flex-grow',0),number('flex-shrink',1),number('gap',0),number('padding',0),number('margin',0),number('font-size',16),bg,fg,0))
        to,tl=intern(n['text']);ev=a.get('onclick','');eo,el=intern(ev)
        kids=n['children'];first=mapping[kids[0]] if kids else 0xffffffff
        entries.append(NODE.pack(mapping[n['parent']] if n['parent']!=0xffffffff else 0xffffffff,first,len(kids),len(styles)-1,to,tl,eo,el))
    nodes_blob=b''.join(entries);styles_blob=b''.join(styles)
    # resource data section: inline images referenced by src path; offsets and contents packed.
    assets=bytearray();seen=set()
    for n in parser.nodes:
        src=n['attrs'].get('src')
        if not src:continue
        if pathlib.PurePosixPath(src).is_absolute() or '..' in pathlib.PurePosixPath(src).parts:raise ValueError('Asset path escape')
        if src in seen:continue
        seen.add(src);root=asset_root.resolve();path=(root/src).resolve()
        if not path.is_relative_to(root):raise ValueError('Asset path escape')
        data=path.read_bytes();name=src.encode('utf8');assets.extend(struct.pack('<II',len(name),len(data)));assets.extend(name);assets.extend(data)
    payload=nodes_blob+styles_blob+strings+assets
    digest=hashlib.sha256(payload).digest()
    header=HEADER.pack(MAGIC,1,len(entries),len(nodes_blob),len(styles_blob),len(strings),len(assets),0,0,digest)
    return header+payload

def main():
    ap=argparse.ArgumentParser();ap.add_argument('input',type=pathlib.Path);ap.add_argument('-o','--output',required=True,type=pathlib.Path);args=ap.parse_args()
    try:
        data=compile_ui(args.input.read_text(encoding='utf8'),args.input.parent);args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(data)
        print(f'arcuxc: {args.output} ({len(data)} bytes)')
    except (ValueError,OSError,struct.error) as e:print('arcuxc:',e,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
