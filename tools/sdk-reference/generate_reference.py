#!/usr/bin/env python3
"""Generate the deterministic installed-SDK reference without site integrations.

The declaration Indexer is adapted from this project's tools/native-reference
emitter. Full header blocks retain original declaration text even when syntax
indexing reports a C++ parser limitation. This is a documentation-only tool.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import sys

from tree_sitter import Language, Parser
import tree_sitter_cpp

CALLABLES = {'function', 'method', 'constructor', 'destructor'}
TYPES = {'class', 'struct', 'enum', 'union'}

def compact(s):
    return re.sub(r'\s+', ' ', s).strip()

def short_id(*parts):
    return hashlib.sha256('|'.join(map(str, parts)).encode()).hexdigest()[:16]

def walk(n):
    yield n
    for child in n.named_children:
        yield from walk(child)

def walk_callable_body(n):
    """Stay in this function; lambda return statements belong to other callables."""
    yield n
    for child in n.named_children:
        if child.type not in ('lambda_expression','function_definition','class_specifier','struct_specifier'):
            yield from walk_callable_body(child)

def clean_comment(s):
    lines=[]
    for line in s.splitlines():
        line=re.sub(r'^\s*(?://[/!]?|/\*+!?|\*)\s?', '', line)
        lines.append(line.removesuffix('*/').rstrip())
    return '\n'.join(lines).strip()

def subsystem(path):
    name=Path(path).stem
    if 'mesh' in name or name == 'native_tls': return 'Authenticated mesh'
    if any(x in name for x in ['experiment', 'optimization', 'optimizer', 'scenario', 'trial', 'route_policy']): return 'Experiments & optimization'
    if any(x in name for x in ['session', 'operation', 'controller', 'host', 'ipc']): return 'Native sessions & control'
    if any(x in name for x in ['scientific', 'solver', 'electrical', 'membrane', 'environment', 'hypothesis', 'quantity']): return 'Scientific models & solvers'
    if any(x in name for x in ['cell', 'sim3', 'simulation', 'movement', 'simulator', 'virtual_machine', 'process_machine', 'culture', 'learning_plan', 'colony_runner']): return 'Simulation & development'
    if any(x in name for x in ['store', 'registry', 'journal', 'checkpoint', 'durable', 'record', 'retention']): return 'Storage & recovery'
    if any(x in name for x in ['epoch', 'observation', 'projector', 'projection', 'bus', 'runtime_events', 'colony_state']): return 'Evidence & projection'
    if any(x in name for x in ['wind', 'packet', 'queue', 'scheduler', 'patchbay', 'receipt']): return 'Messages & scheduling'
    return 'Shared primitives'

class Indexer:
    def __init__(self, root):
        self.root=root
        self.parser=Parser(Language(tree_sitter_cpp.language()))
        self.entries=[]
        self.files=[]
        self.warnings=[]
        self.discovered_functions=[]

    def txt(self, n):
        return self.raw[n.start_byte:n.end_byte].decode() if n is not None else ''

    def comment(self, n):
        # Only attach a directly preceding comment, never a previous declaration's
        # trailing comment or a distant file-wide comment.
        sibling=n.prev_named_sibling
        gathered=[]
        end_row=n.start_point.row
        while sibling and sibling.type == 'comment' and end_row-sibling.end_point.row <= 2:
            previous=sibling.prev_named_sibling
            if previous and previous.end_point.row == sibling.start_point.row: break
            gathered.insert(0,self.txt(sibling))
            end_row=sibling.start_point.row
            sibling=previous
        return clean_comment('\n'.join(gathered))

    def add(self, node, name, kind, scope, parent, access, signature, **extra):
        qualified='::'.join([*scope,name])
        e=dict(id='s-'+short_id(self.path,node.start_point.row,kind,qualified,signature),
               name=name,qualified=qualified,kind=kind,namespace=self.namespace,
               parentId=parent['id'] if parent else None,parent=parent['qualified'] if parent else None,
               layer=self.layer,subsystem=subsystem(self.path),path=self.path,
               line=node.start_point.row+1,endLine=node.end_point.row+1,
               signature=signature,returnType='',parameters=[],access=access,
               modifiers=[],description='',details=[],returns='',comment=self.comment(node),
               source=self.txt(node),memberIds=[],baseTypes=[])
        e.update(extra)
        e['internal']=bool(parent and parent.get('internal')) or access!='public' or any(
            part in ('(anonymous)','detail','details') or part.endswith(('_detail','_details')) for part in scope)
        self.entries.append(e)
        if parent: parent['memberIds'].append(e['id'])
        return e

    def name_node(self, n):
        if n is None: return None
        if n.type in ('identifier','field_identifier','type_identifier','qualified_identifier','destructor_name','operator_name'): return n
        child=n.child_by_field_name('declarator')
        if child is None and n.type in ('reference_declarator','pointer_declarator','parenthesized_declarator','attributed_declarator'):
            child=next((x for x in n.named_children if x.type not in ('type_qualifier','attribute_declaration')),None)
        return self.name_node(child) if child else None

    def find_function(self,n):
        if n is None: return None
        if n.type == 'function_declarator': return n
        if n.type == 'init_declarator': return None
        child=n.child_by_field_name('declarator')
        if child is None and n.type in ('reference_declarator','pointer_declarator','parenthesized_declarator','attributed_declarator'):
            child=next((x for x in n.named_children if x.type not in ('type_qualifier','attribute_declaration')),None)
        return self.find_function(child)

    def parameters(self,fn):
        result=[]
        params=fn.child_by_field_name('parameters')
        if not params: return result
        for p in params.named_children:
            if p.type not in ('parameter_declaration','optional_parameter_declaration','variadic_parameter_declaration','variadic_declarator'): continue
            dec=p.child_by_field_name('declarator')
            nn=self.name_node(dec)
            default=p.child_by_field_name('default_value')
            end=default.start_byte if default else p.end_byte
            raw=self.raw[p.start_byte:end].decode().rstrip(' =')
            if nn:
                a=nn.start_byte-p.start_byte
                b=nn.end_byte-p.start_byte
                raw=raw[:a]+raw[b:]
            result.append(dict(name=self.txt(nn),type=compact(raw),default=self.txt(default)))
        return result

    def process_function(self,n,fn,scope,parent,access,template):
        nn=self.name_node(fn.child_by_field_name('declarator'))
        if not nn: return
        name=self.txt(nn)
        kind='method' if parent else 'function'
        if parent and name == parent['name']: kind='constructor'
        elif name.startswith('~'): kind='destructor'
        body=n.child_by_field_name('body')
        end=body.start_byte if body else n.end_byte
        initializer=next((x for x in n.named_children if x.type=='field_initializer_list'),None)
        if initializer: end=min(end,initializer.start_byte)
        sig=self.raw[n.start_byte:end].decode().strip()
        if body: sig=sig.rstrip()+';'
        sig=(template+'\n' if template else '')+sig
        modifiers=[]
        for token in ['static','virtual','inline','constexpr','consteval','explicit','noexcept','override','final','WINAPI']:
            if re.search(r'\b'+token+r'\b',sig): modifiers.append(token)
        if re.search(r'=\s*0\s*;',sig): modifiers.append('pure virtual')
        if re.search(r'=\s*default\s*;',sig): modifiers.append('defaulted')
        if re.search(r'=\s*delete\s*;',sig): modifiers.append('deleted')
        return_type=compact(self.raw[n.start_byte:nn.start_byte].decode())
        return_type=compact(re.sub(r'\b(?:static|virtual|inline|constexpr|consteval|explicit|friend|extern|WINAPI)\b','',return_type))
        if kind in ('constructor','destructor'): return_type=''
        trailing=next((x for x in fn.named_children if x.type=='trailing_return_type'),None)
        if trailing: return_type=self.txt(trailing).removeprefix('->').strip()
        e=self.add(n,name,kind,scope,parent,access,sig,
                   returnType=return_type,parameters=self.parameters(fn),modifiers=modifiers)
        if template and n.parent.type=='template_declaration': e['comment']=self.comment(n.parent) or e['comment']
        e['hasBody']=body is not None
        e['implementationFacts']=[]
        if body:
            returned=[]
            for x in walk_callable_body(body):
                if x.type=='return_statement' and x.named_children:
                    value=compact(self.txt(x.named_children[0]))
                    if value and len(value)<160 and value not in returned: returned.append(value)
                if x.type=='if_statement':
                    consequence=x.child_by_field_name('consequence')
                    condition=x.child_by_field_name('condition')
                    if consequence and consequence.type=='return_statement' and consequence.named_children:
                        value=compact(self.txt(consequence.named_children[0]))
                        cond=compact(self.txt(condition))
                        if cond.startswith('(') and cond.endswith(')'): cond=cond[1:-1]
                        if len(cond)<200 and len(value)<130:
                            e['implementationFacts'].append(f'Early-return branch: `if ({cond}) return {value};`')
            e['implementationFacts']=e['implementationFacts'][:5]
            e['returnExpressions']=returned
            if len(returned)==1 and len(body.named_children)==1 and body.named_children[0].type=='return_statement':
                e['simpleReturn']=returned[0]
        return e

    def process_declaration(self,n,scope,parent,access,template):
        embedded=[x for x in n.named_children if x.type in ('class_specifier','struct_specifier','enum_specifier','union_specifier')]
        for child in embedded: self.process(child,scope,parent,access,template)
        declarators=[c for i,c in enumerate(n.children) if n.field_name_for_child(i)=='declarator']
        for dec in declarators:
            fn=self.find_function(dec)
            # A parenthesized pointer declarator is a callback data member,
            # not a callable declaration owned by this type.
            if fn and fn.child_by_field_name('declarator') and fn.child_by_field_name('declarator').type=='parenthesized_declarator':
                fn=None
            if fn:
                self.process_function(n,fn,scope,parent,access,template)
                continue
            nn=self.name_node(dec)
            if not nn: continue
            name=self.txt(nn)
            if not name: continue
            sig=self.txt(n)
            kind='field' if parent else 'constant'
            if parent and ('constexpr' in sig or re.search(r'\bstatic\s+(?:inline\s+)?const\b',sig)): kind='constant'
            default=dec.child_by_field_name('value')
            if not default:
                nxt=dec.next_named_sibling
                if nxt and nxt.type not in ('identifier','field_identifier','type_identifier'): default=nxt
            self.add(n,name,kind,scope,parent,access,(template+'\n' if template else '')+sig,
                     returnType=self.txt(n.child_by_field_name('type')),default=self.txt(default))

    def process(self,n,scope=(),parent=None,access='public',template=''):
        t=n.type
        if t in ('translation_unit','declaration_list','field_declaration_list'):
            current=access
            for c in n.named_children:
                if c.type=='access_specifier': current=self.txt(c)
                else: self.process(c,scope,parent,current)
        elif t=='namespace_definition':
            name=self.txt(n.child_by_field_name('name')) or '(anonymous)'
            ns=tuple(name.split('::'))
            previous=self.namespace
            self.namespace='::'.join((*scope,*ns))
            body=n.child_by_field_name('body')
            if body: self.process(body,(*scope,*ns),parent,access)
            self.namespace=previous
        elif t=='template_declaration':
            params=n.child_by_field_name('parameters')
            prefix='template'+self.txt(params)
            for c in n.named_children:
                if c!=params and c.type!='requires_clause': self.process(c,scope,parent,access,prefix)
        elif t in ('class_specifier','struct_specifier','union_specifier','enum_specifier'):
            name=self.txt(n.child_by_field_name('name'))
            if not name: return
            body=n.child_by_field_name('body')
            sig=self.raw[n.start_byte:body.start_byte if body else n.end_byte].decode().strip()
            kind=t.removesuffix('_specifier')
            base=next((x for x in n.named_children if x.type=='base_class_clause'),None)
            bases=[]
            if base:
                for b in base.named_children:
                    if b.type not in ('access_specifier','virtual'): bases.append(self.txt(b))
            e=self.add(n,name,kind,scope,parent,access,(template+'\n' if template else '')+sig,baseTypes=bases)
            if template and n.parent.type=='template_declaration': e['comment']=self.comment(n.parent) or e['comment']
            if body:
                if kind=='enum':
                    for value in body.named_children:
                        if value.type=='enumerator':
                            vn=self.txt(value.child_by_field_name('name'))
                            default=self.txt(value.child_by_field_name('value'))
                            self.add(value,vn,'enumerator',(*scope,name),e,'public',self.txt(value),default=default)
                else: self.process(body,(*scope,name),e,'private' if kind=='class' else 'public')
        elif t in ('function_definition','declaration','field_declaration'):
            self.process_declaration(n,scope,parent,access,template)
        elif t=='alias_declaration':
            name=self.txt(n.child_by_field_name('name'))
            if name: self.add(n,name,'alias',scope,parent,access,self.txt(n),returnType=self.txt(n.child_by_field_name('type')))
        elif t=='type_definition':
            dec=n.child_by_field_name('declarator')
            name=self.txt(self.name_node(dec))
            if name: self.add(n,name,'alias',scope,parent,access,self.txt(n),returnType=self.txt(n.child_by_field_name('type')))
        elif t.startswith('preproc_') and t not in ('preproc_include','preproc_def','preproc_function_def','preproc_call'):
            for c in n.named_children:
                self.process(c,scope,parent,access,template)
        elif t=='linkage_specification':
            for c in n.named_children: self.process(c,scope,parent,access,template)

    def parse_file(self,p):
        self.path=p.relative_to(self.root).as_posix()
        self.layer='sdk'
        self.namespace=''
        self.raw=p.read_bytes()
        # The C calling-convention macro has no grammar meaning to tree-sitter.
        # Preserve source offsets while masking only this known ABI qualifier.
        parsed=b''.join(line if re.match(rb'\s*#\s*(?:define|undef)\s+ANAIERO_MODULE_CALL\b',line)
                        else line.replace(b'ANAIERO_MODULE_CALL',b' '*len(b'ANAIERO_MODULE_CALL'))
                        for line in self.raw.splitlines(keepends=True))
        tree=self.parser.parse(parsed)
        for n in walk(tree.root_node):
            if n.type=='ERROR' or n.is_missing:
                self.warnings.append(dict(path=self.path,line=n.start_point.row+1,node=n.type,text=self.txt(n)[:200]))
        before=len(self.entries)
        self.process(tree.root_node)
        all_comment=[]
        for n in tree.root_node.named_children:
            if n.type=='comment': all_comment.append(clean_comment(self.txt(n)))
            elif n.type not in ('preproc_include','preproc_call'): break
        self.files.append(dict(id='f-'+short_id(self.path),path=self.path,name=p.name,layer=self.layer,
                               subsystem=subsystem(self.path),source=self.raw.decode(),
                               description='\n'.join(all_comment),symbolIds=[e['id'] for e in self.entries[before:]],
                               lineCount=self.raw.count(b'\n')+1))

def escaped(value):
    return value.replace('|', '\\|').replace('`', '\\`')

def generate(root):
    headers=sorted(p for p in (root/'native/sdk/include').rglob('*') if p.suffix in ('.hpp','.h'))
    if not headers:
        raise SystemExit('No installed SDK headers found.')
    index=Indexer(root)
    digest=hashlib.sha256()
    for path in headers:
        relative=path.relative_to(root).as_posix()
        digest.update(relative.encode()+b'\0'+path.read_bytes()+b'\0')
        index.parse_file(path)
    output=[
        '# Anaiero installed SDK reference', '',
        'Generated from the public headers under `native/sdk/include`. The header',
        'hashes identify the exact source bytes. This file has no generation timestamp',
        'or dependency on a Git commit that changes when the generated file is committed.', '',
        'Regenerate with `python3 tools/sdk-reference/generate_reference.py`; use `--check`',
        'to verify the checked-in output. Python and the parser packages are optional',
        'documentation tools and are not native runtime dependencies.', '',
        'The symbol tables provide navigation. The complete header blocks are the',
        'authoritative declaration text, including defaults, macros, annotations and',
        'inline definitions. A syntax index is not a substitute for compiling against',
        'the installed SDK. Operational rules are in [modular-lifecycle.md](modular-lifecycle.md).', '',
        f'**Public-header manifest SHA-256:** `{digest.hexdigest()}`', '',
        '| Header | SHA-256 |', '|---|---|',
    ]
    for file in index.files:
        path=root/file['path']
        relative=path.relative_to(root/'native').as_posix()
        output.append(f"| [{file['name']}](../{relative}) | `{hashlib.sha256(path.read_bytes()).hexdigest()}` |")
    for file in index.files:
        output.extend(['',f"## {file['name']}",''])
        entries=[entry for entry in index.entries if entry['path']==file['path'] and not entry['internal']
                 and entry['kind'] in CALLABLES|TYPES|{'alias'}]
        seen=set()
        if entries:
            output.extend(['| Public symbol | Kind |','|---|---|'])
            for entry in entries:
                key=(entry['qualified'],entry['kind'])
                if key in seen:
                    continue
                seen.add(key)
                output.append(f"| `{escaped(entry['qualified'])}` | {entry['kind']} |")
            output.append('')
        source=file['source']
        longest=max((len(match.group()) for match in re.finditer(r'`+',source)),default=0)
        fence='`'*max(3,longest+1)
        output.extend([fence+'cpp',source.rstrip('\n'),fence])
    output.extend(['','## Syntax-index audit','',
        'Exact header text is retained above even when the optional navigation parser',
        'cannot interpret a declaration. Known C calling-convention qualifiers are',
        'masked with equal-length whitespace for parsing only; displayed source is unchanged.',''])
    if index.warnings:
        output.extend(['| Header | Line | Parser note |','|---|---|---|'])
        for warning in index.warnings:
            text=warning['text'] or '(missing syntax node)'
            output.append(f"| `{Path(warning['path']).name}` | {warning['line']} | `{escaped(warning['node'])}: {escaped(compact(text))}` |")
    else:
        output.append('The navigation parser reported no syntax notes.')
    output.append('')
    return '\n'.join(output),len(headers),len(index.entries),len(index.warnings)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output',type=Path)
    parser.add_argument('--check',action='store_true',help='Fail if the checked-in reference differs.')
    arguments=parser.parse_args()
    root=arguments.source.resolve()
    destination=arguments.output or root/'native/docs/sdk-reference.md'
    rendered,headers,symbols,warnings=generate(root)
    if arguments.check:
        if not destination.exists() or destination.read_text()!=rendered:
            raise SystemExit('SDK reference is stale; run tools/sdk-reference/generate_reference.py.')
    else:
        destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_text(rendered)
    print(json.dumps({'headers':headers,'indexed_symbols':symbols,'parser_notes':warnings,'checked':arguments.check},sort_keys=True))

if __name__=='__main__':
    main()
