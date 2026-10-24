#!/usr/bin/env python3
"""ArcUX AOT compiler: deterministic packed UI image, Python 3.11+."""
import argparse
import difflib
from diagnostics import Source, DiagnosticError, render, render_json, should_color
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

def declarations(source, diag=None, origin=None):
    cursor = 0
    out={}
    parsed=tinycss2.parse_declaration_list(source,skip_whitespace=True,skip_comments=True)
    for item in parsed:
        if item.type=='error':
            if diag:diag.fail('AUX-CSS-001',f'CSS declaration syntax: {item.message}',origin or diag.span(1,1))
            raise ValueError(f'CSS declaration syntax: {item.message}')
        if item.type!='declaration':
            if diag:diag.fail('AUX-CSS-002','CSS at-rules are not supported inside declarations',origin)
            raise ValueError('CSS at-rules are not supported inside declarations')
        key=item.lower_name
        at=origin
        if diag and origin:
            # Resolve the declaration against its containing stylesheet/attribute.
            base=sum(len(line)+1 for line in diag.lines[:origin.line-1])+origin.column-1
            offset=diag.text.find(key, max(base,cursor))
            if offset>=0:at=diag.at_offset(offset,len(key));cursor=offset+len(key)
        if key not in PROPS:
            suggestion=difflib.get_close_matches(key,PROPS,n=1,cutoff=.6)
            if diag:diag.fail('AUX-CSS-101',f'Unsupported CSS property: {key}',at,
                ('Did you mean '+suggestion[0]+'?' if suggestion else 'This property is outside the current ArcUX CSS profile.'))
            raise ValueError('Unsupported CSS property: '+key)
        value=tinycss2.serialize(item.value).strip()
        if not value:
            if diag:diag.fail('AUX-CSS-102',f'Empty CSS value for {key}',at)
            raise ValueError('Empty CSS value for '+key)
        out[key]=(value, bool(item.important))
    return out

def stylesheet(text, diag=None, origin=None):
    rules=[]
    for item in tinycss2.parse_stylesheet(text,skip_whitespace=True,skip_comments=True):
        if item.type=='error':
            if diag:diag.fail('AUX-CSS-001','CSS stylesheet syntax: '+item.message,origin)
            raise ValueError('CSS stylesheet syntax: '+item.message)
        if item.type!='qualified-rule':
            if diag:diag.fail('AUX-CSS-103','Unsupported CSS at-rule: '+tinycss2.serialize([item]).strip(),origin)
            raise ValueError('Unsupported CSS at-rule: '+tinycss2.serialize([item]))
        selector=tinycss2.serialize(item.prelude).strip()
        try: selectors=cssselect2.compile_selector_list(selector)
        except cssselect2.SelectorError as e:
            if diag:diag.fail('AUX-CSS-104','Invalid CSS selector: '+str(e),origin)
            raise ValueError('Invalid CSS selector: '+str(e)) from e
        rule_origin=origin
        if diag and origin:
            anchor=sum(len(line)+1 for line in diag.lines[:origin.line-1])+origin.column-1
            prelude=tinycss2.serialize(item.prelude).strip()
            selector_pos=diag.text.find(prelude,anchor)
            if selector_pos>=0:
                opening=diag.text.find('{',selector_pos)
                rule_origin=diag.at_offset(opening+1) if opening>=0 else origin
        decl=declarations(tinycss2.serialize(item.content), diag, rule_origin)
        for selected in selectors:
            if selected.pseudo_element is not None:
                if diag:diag.fail('AUX-CSS-105','Pseudo-elements are not implemented: '+selector,origin)
                raise ValueError('Pseudo-elements are not implemented: '+selector)
            rules.append((selected,decl,selector))
    return rules

class Parser(html.parser.HTMLParser):
    def __init__(self, diag=None):
        super().__init__(convert_charrefs=True)
        self.diag=diag;self.style_origins=[];self.style_data_origins=[];self.last_span=None
        self.nodes=[];self.stack=[];self.styles=[];self.in_style=False
        self.in_head=False;self.in_title=False;self.head_seen=False
        self.et_root=ET.Element('_document');self.et_stack=[]
    def handle_decl(self,decl):
        if decl.lower()!='doctype html':
            if self.diag:self.diag.fail('AUX-HTML-001','Only <!doctype html> is supported',self.position())
            raise ValueError('Only <!doctype html> is supported')
    def position(self,token=None):
        line,col=self.getpos()
        if not self.diag:return None
        raw=self.get_starttag_text() or ''
        return self.diag.span(line,col+1,len(token or raw or '<'))
    def reject(self,code,message,hint=None,span=None):
        if self.diag:self.diag.fail(code,message,span or self.position(),hint)
        raise ValueError(message)
    def handle_starttag(self,tag,attrs):
        if tag in FORBIDDEN:self.reject('AUX-HTML-101',f'HTML element <{tag}> is forbidden by ArcUX profile',
            'Use data-action and a native C++ callback instead of scripting.' if tag=='script' else 'Remove this element or implement a native equivalent.')
        if tag=='html' or tag=='body':return
        if tag=='head':self.in_head=True;return
        if tag=='style':
            if self.in_style:raise ValueError('Nested style element')
            self.in_style=True;self.style_origins.append(self.position());return
        if tag in METADATA:
            if tag=='title':self.in_title=True
            return
        if tag not in ELEMENTS:self.reject('AUX-HTML-102','Unsupported HTML element: '+tag,
            ('Did you mean <'+difflib.get_close_matches(tag,sorted(ELEMENTS),n=1)[0]+'>?' if difflib.get_close_matches(tag,sorted(ELEMENTS),n=1) else 'Element is outside the ArcUX HTML profile.'))
        a=dict(attrs)
        if len(a)!=len(attrs):self.reject('AUX-HTML-103','Duplicate HTML attributes on '+tag)
        if any(k.startswith('on') for k in a):self.reject('AUX-HTML-104','Inline JavaScript event attributes are forbidden',
            'Use data-action="name" and register a native callback.')
        if 'href' in a and re.match(r'(?i)\s*(javascript|data):',a['href'] or ''):self.reject('AUX-HTML-105','Forbidden href scheme')
        idx=len(self.nodes)
        self.nodes.append(dict(tag=tag,attrs=a,parent=self.stack[-1] if self.stack else 0xffffffff,children=[],text='',span=self.position()))
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
            expected=self.nodes[self.stack[-1]]['tag'] if self.stack else None
            self.reject('AUX-HTML-106','Unbalanced closing tag: </'+tag+'>',
                ('Expected </'+expected+'>.' if expected else 'Remove the unexpected closing tag.'),
                self.diag.span(*[self.getpos()[0], self.getpos()[1]+1],len(tag)+3) if self.diag else None)
        self.stack.pop();self.et_stack.pop()
    def handle_data(self,data):
        if self.in_style:
            self.styles.append(data)
            self.style_data_origins.append(self.diag.span(self.getpos()[0],self.getpos()[1]+1) if self.diag else None)
        elif self.in_title or (self.in_head and not self.stack):pass
        elif self.stack:self.nodes[self.stack[-1]]['text']+=data.strip()
        elif data.strip():raise ValueError('Text outside the UI root')
    def handle_entityref(self,name):
        self.handle_data('&'+name+';')

def compile_ui(source,asset_root,diagnostic_source=None):
    diag=diagnostic_source or Source(source)
    parser=Parser(diag);parser.feed(source)
    if parser.stack or parser.in_style:diag.fail('AUX-HTML-107','Unclosed element',
        parser.nodes[parser.stack[-1]]['span'] if parser.stack else parser.style_origins[-1],
        'Close the element before the end of the document.')
    if not parser.nodes:diag.fail('AUX-HTML-108','No UI elements')
    if sum(n['parent']==0xffffffff for n in parser.nodes)!=1:diag.fail('AUX-HTML-109','Exactly one root element is required')
    rules=stylesheet('\n'.join(parser.styles),diag,parser.style_data_origins[0] if parser.style_data_origins else diag.span(1,1))
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
        for key,(value,important) in declarations(a.get('style',''),diag,
            diag.at_offset(diag.text.find(a.get('style',''),
                sum(len(line)+1 for line in diag.lines[:n['span'].line-1])+n['span'].column-1))
            if a.get('style') and a.get('style') in diag.text else n['span']).items():
            priority=(int(important),1,0,0,len(rules)+1)
            if key not in winners or priority>=winners[key][0]:winners[key]=(priority,value)
        decl={key:value for key,(_,value) in winners.items()}
        display={'block':0,'flex':1,'grid':2,'none':3}.get(decl.get('display','block'))
        if display is None:diag.fail('AUX-CSS-106','Unsupported display: '+decl['display'],n['span'])
        direction={'row':0,'column':1}.get(decl.get('flex-direction','row'))
        if direction is None:diag.fail('AUX-CSS-107','Only flex-direction row/column supported',n['span'])
        def number(key,default):return length(decl[key]) if key in decl else default
        try:
            bg=color(decl.get('background','transparent'));fg=color(decl.get('color','white'))
            values=[number('width',-1),number('height',-1),number('flex-grow',0),number('flex-shrink',1),number('gap',0),number('padding',0),number('margin',0),number('font-size',16)]
        except ValueError as exc:
            diag.fail('AUX-CSS-108',str(exc),n['span'], 'Use a value supported by the current layout profile.')
        # 32-bit flags contain text/bg colors? colors are stored in the 2 final uints.
        styles.append(STYLE.pack(display,direction,*values,bg,fg,0))
        to,tl=intern(n['text']);ev=a.get('data-action','');eo,el=intern(ev)
        kids=n['children'];first=mapping[kids[0]] if kids else 0xffffffff
        entries.append(NODE.pack(mapping[n['parent']] if n['parent']!=0xffffffff else 0xffffffff,first,len(kids),len(styles)-1,to,tl,eo,el))
    nodes_blob=b''.join(entries);styles_blob=b''.join(styles)
    # resource data section: inline images referenced by src path; offsets and contents packed.
    assets=bytearray();seen=set()
    for n in parser.nodes:
        src=n['attrs'].get('src')
        if not src:continue
        if pathlib.PurePosixPath(src).is_absolute() or '..' in pathlib.PurePosixPath(src).parts:
            diag.fail('AUX-ASSET-101','Asset path escapes the project root: '+src,n['span'])
        if src in seen:continue
        seen.add(src);root=asset_root.resolve();path=(root/src).resolve()
        if not path.is_relative_to(root):diag.fail('AUX-ASSET-101','Asset path escapes the project root: '+src,n['span'])
        try:data=path.read_bytes()
        except OSError as exc:diag.fail('AUX-ASSET-102','Could not read asset: '+src,n['span'],str(exc))
        name=src.encode('utf8');assets.extend(struct.pack('<II',len(name),len(data)));assets.extend(name);assets.extend(data)
    payload=nodes_blob+styles_blob+strings+assets
    digest=hashlib.sha256(payload).digest()
    header=HEADER.pack(MAGIC,1,len(entries),len(nodes_blob),len(styles_blob),len(strings),len(assets),0,0,digest)
    return header+payload

def main():
    ap=argparse.ArgumentParser(description='ArcUX HTML/CSS AOT compiler')
    ap.add_argument('input',type=pathlib.Path)
    ap.add_argument('-o','--output',required=True,type=pathlib.Path)
    ap.add_argument('--diagnostic-format',choices=('human','json'),default='human')
    ap.add_argument('--color',choices=('auto','always','never'),default='auto')
    ap.add_argument('--context-lines',type=int,default=2)
    args=ap.parse_args()
    source_text=''
    try:
        source_text=args.input.read_text(encoding='utf8')
        source=Source(source_text,str(args.input))
        data=compile_ui(source_text,args.input.parent,source)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(data)
        print(f'arcuxc: {args.output} ({len(data)} bytes)')
    except DiagnosticError as e:
        if args.diagnostic_format=='json':print(render_json(e.diagnostic),file=sys.stderr)
        else:print(render(e.diagnostic,Source(source_text,str(args.input)),
              color=should_color(args.color),context=max(0,args.context_lines)),file=sys.stderr)
        return 1
    except (ValueError,OSError,struct.error) as e:
        if args.diagnostic_format=='json':
            from diagnostics import Diagnostic
            print(render_json(Diagnostic('AUX-INTERNAL-001',str(e),Source(source_text,str(args.input)).span(1,1))),file=sys.stderr)
        else:print('arcuxc: '+str(e),file=sys.stderr)
        return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
