#!/usr/bin/env python3
"""ArcUX AOT compiler: deterministic packed UI image, Python 3.11+."""
import argparse
import hashlib
import html.parser
import xml.etree.ElementTree as ET
import tinycss2
import cssselect2
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

# Exact CSS syntax is parsed with tinycss2, not by splitting at semicolons.
# We compile a deliberately explicit CSS Snapshot 2026 rendering profile.
# CSS syntactic constructs outside the profile are diagnosed at build time.
VOID={'area','base','br','col','embed','hr','img','input','link','meta','source','track','wbr'}
METADATA={'title','meta','link','base'}
FORBIDDEN={'script','iframe','object','embed','canvas','template','noscript','frame','frameset'}
ELEMENTS={'div','span','p','b','strong','i','em','small','label','button','section',
          'header','footer','main','nav','article','aside','ul','ol','li','h1','h2',
          'h3','h4','h5','h6','a','img','input','br','hr','form','fieldset',
          'legend','output','progress','details','summary','figure','figcaption'}

def declarations(source):
    out={}
    parsed=tinycss2.parse_declaration_list(source,skip_whitespace=True,skip_comments=True)
    for item in parsed:
        if item.type=='error':raise ValueError(f'CSS declaration syntax: {item.message}')
        if item.type!='declaration':raise ValueError('CSS at-rules are not supported inside declarations')
        key=item.lower_name
        if key not in PROPS:raise ValueError('Unsupported CSS property: '+key)
        value=tinycss2.serialize(item.value).strip()
        if not value:raise ValueError('Empty CSS value for '+key)
        out[key]=(value, bool(item.important))
    return out

def stylesheet(text):
    rules=[]
    for item in tinycss2.parse_stylesheet(text,skip_whitespace=True,skip_comments=True):
        if item.type=='error':raise ValueError('CSS stylesheet syntax: '+item.message)
        if item.type!='qualified-rule':raise ValueError('Unsupported CSS at-rule: '+tinycss2.serialize([item]))
        selector=tinycss2.serialize(item.prelude).strip()
        try: selectors=cssselect2.compile_selector_list(selector)
        except cssselect2.SelectorError as e:raise ValueError('Invalid CSS selector: '+str(e)) from e
        decl=declarations(tinycss2.serialize(item.content))
        for selected in selectors:
            if selected.pseudo_element is not None:raise ValueError('Pseudo-elements are not implemented: '+selector)
            rules.append((selected,decl,selector))
    return rules

class Parser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.nodes=[];self.stack=[];self.styles=[];self.in_style=False
        self.in_head=False;self.in_title=False;self.head_seen=False
        self.et_root=ET.Element('_document');self.et_stack=[]
    def handle_decl(self,decl):
        if decl.lower()!='doctype html':raise ValueError('Only <!doctype html> is supported')
    def handle_starttag(self,tag,attrs):
        if tag in FORBIDDEN:raise ValueError(f'HTML element <{tag}> is forbidden by ArcUX profile')
        if tag=='html' or tag=='body':return
        if tag=='head':self.in_head=True;return
        if tag=='style':
            if self.in_style:raise ValueError('Nested style element')
            self.in_style=True;return
        if tag in METADATA:
            if tag=='title':self.in_title=True
            return
        if tag not in ELEMENTS:raise ValueError('Unsupported HTML element: '+tag)
        a=dict(attrs)
        if len(a)!=len(attrs):raise ValueError('Duplicate HTML attributes on '+tag)
        if any(k.startswith('on') for k in a):raise ValueError('Inline JavaScript event attributes are forbidden; use data-action')
        if 'href' in a and re.match(r'(?i)\s*(javascript|data):',a['href'] or ''):raise ValueError('Forbidden href scheme')
        idx=len(self.nodes)
        self.nodes.append(dict(tag=tag,attrs=a,parent=self.stack[-1] if self.stack else 0xffffffff,children=[],text=''))
        if self.stack:self.nodes[self.stack[-1]]['children'].append(idx)
        parent=self.et_stack[-1] if self.et_stack else self.et_root
        elt=ET.SubElement(parent,tag,a)
        if tag not in VOID:self.stack.append(idx);self.et_stack.append(elt)
    def handle_startendtag(self,tag,attrs):
        self.handle_starttag(tag,attrs)
        if tag not in VOID:self.handle_endtag(tag)
    def handle_endtag(self,tag):
        if tag in ('html','body','head'):
            if tag=='head':self.in_head=False
            return
        if tag=='style':
            if not self.in_style:raise ValueError('Unexpected </style>')
            self.in_style=False;return
        if tag=='title':self.in_title=False;return
        if tag in VOID:raise ValueError('Void HTML element cannot have an end tag: '+tag)
        if not self.stack or self.nodes[self.stack[-1]]['tag']!=tag:
            raise ValueError('Unbalanced tag: '+tag)
        self.stack.pop();self.et_stack.pop()
    def handle_data(self,data):
        if self.in_style:self.styles.append(data)
        elif self.in_title or (self.in_head and not self.stack):pass
        elif self.stack:self.nodes[self.stack[-1]]['text']+=data.strip()
        elif data.strip():raise ValueError('Text outside the UI root')
    def handle_entityref(self,name):
        self.handle_data('&'+name+';')

def compile_ui(source,asset_root):
    parser=Parser();parser.feed(source)
    if parser.stack or parser.in_style:raise ValueError('Unclosed element')
    if not parser.nodes:raise ValueError('No UI elements')
    if sum(n['parent']==0xffffffff for n in parser.nodes)!=1:raise ValueError('Exactly one root element is required')
    rules=stylesheet('\n'.join(parser.styles))
    wrappers=list(cssselect2.ElementWrapper.from_xml_root(parser.et_root).iter_subtree())
    # Synthetic _document is excluded, wrappers match element creation order.
    wrappers=[w for w in wrappers if w.etree_element.tag!='_document']
    if len(wrappers)!=len(parser.nodes):raise ValueError('Internal HTML topology mismatch')
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
        # CSS Cascade: importance > origin (single author origin) > specificity > source order.
        # Full cascade layers, inheritance, and custom properties are not implemented yet.
        winners={}
        element=wrappers[old]
        for ri,(selector,rule,raw_selector) in enumerate(rules):
            if selector.test(element):
                for key,(value,important) in rule.items():
                    priority=(int(important),*selector.specificity,ri)
                    if key not in winners or priority>=winners[key][0]:
                        winners[key]=(priority,value)
        for key,(value,important) in declarations(a.get('style','')).items():
            priority=(int(important),1,0,0,len(rules)+1)
            if key not in winners or priority>=winners[key][0]:winners[key]=(priority,value)
        decl={key:value for key,(_,value) in winners.items()}
        display={'block':0,'flex':1,'grid':2,'none':3}.get(decl.get('display','block'))
        if display is None:raise ValueError('Unsupported display: '+decl['display'])
        direction={'row':0,'column':1}.get(decl.get('flex-direction','row'))
        if direction is None:raise ValueError('Only flex-direction row/column supported')
        def number(key,default):return length(decl[key]) if key in decl else default
        bg=color(decl.get('background','transparent'));fg=color(decl.get('color','white'))
        # 32-bit flags contain text/bg colors? colors are stored in the 2 final uints.
        styles.append(STYLE.pack(display,direction,number('width',-1),number('height',-1),number('flex-grow',0),number('flex-shrink',1),number('gap',0),number('padding',0),number('margin',0),number('font-size',16),bg,fg,0))
        to,tl=intern(n['text']);ev=a.get('data-action','');eo,el=intern(ev)
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
