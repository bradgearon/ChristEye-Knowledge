#!/usr/bin/env python3
"""Deterministic, source-pinned C++20 API documentation builder.

Syntactic extraction uses Tree-sitter. Compiler verification is a separate CI gate.
No generated description is represented as experimentally verified.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
import sys
from collections import defaultdict, Counter
from pathlib import Path

from tree_sitter import Parser, Language
import tree_sitter_cpp

SCHEMA = 2
TYPE_KINDS = {"class", "struct", "union", "enum", "alias", "typedef"}
FUNCTION_KINDS = {"function", "method", "constructor", "destructor", "operator"}
KNOWN_LAYERS = {"application", "services", "contracts", "models", "infrastructure", "living", "wind"}
ROOT = Path(__file__).resolve().parents[2]


def sha(text, count=16):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:count]


def normalized(text):
    return re.sub(r"\s+", " ", text).strip()


def as_json(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()


def git_commit(root):
    p = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                       text=True, capture_output=True, check=True)
    value = p.stdout.strip()
    if not re.fullmatch("[a-f0-9]{40}", value):
        raise ValueError("Native source must be a Git checkout")
    return value


def node_text(data, node):
    return data[node.start_byte:node.end_byte].decode("utf-8", "replace")


def named_child(node, kinds):
    return next((c for c in node.named_children if c.type in kinds), None)


def find_declarator(node):
    if node.type == "function_declarator":
        return node
    for c in node.named_children:
        hit = find_declarator(c)
        if hit is not None:
            return hit
    return None


def source_comment(lines, at):
    words = []
    for line in reversed(lines[max(0, at - 7):at]):
        value = line.strip()
        if value.startswith("//"):
            words.append(value[2:].strip())
        elif value or words:
            break
    return normalized(" ".join(reversed(words)))[:600]


def layer_for(path):
    for segment in path.split("/"):
        if segment in KNOWN_LAYERS:
            return segment
    return "sources"


def load_annotations(site):
    parts = sorted((site / "data-parts").glob("*.txt"))
    if not parts:
        return {}
    try:
        packed = "".join(p.read_text(encoding="utf-8") for p in parts)
        data = json.loads(packed.split("=", 1)[1].strip().rstrip(";"))
        result = {}
        for item in data.get("entries", []):
            if item.get("descriptionSource") == "reviewed":
                result[(item.get("qualified"), item.get("kind"),
                        normalized(item.get("signature", "")))] = item
        return result
    except (ValueError, OSError, IndexError) as exc:
        raise ValueError("Legacy reviewed annotation archive invalid") from exc


class Extract:
    def __init__(self, data, path, annotations):
        self.data = data
        self.path = path
        self.annotations = annotations
        self.lines = data.decode("utf-8", "replace").splitlines()
        self.layer = layer_for(path)
        self.file_id = "f-" + sha(path)
        self.symbols = []
        self.warnings = []
        self.parser = Parser(Language(tree_sitter_cpp.language()))

    def emit(self, kind, name, scope, node, access="public", owner=None, signature=None):
        if not name or name.startswith("<"):
            return
        qualified = "::".join(scope + [name])
        if signature is None:
            signature = node_text(self.data, node).split("{", 1)[0]
        signature = normalized(signature.rstrip(";"))[:1800]
        stable = kind + "|" + qualified + "|" + (signature if kind in FUNCTION_KINDS else "")
        note = source_comment(self.lines, node.start_point.row)
        item = {
            "id": "s-" + sha(stable), "name": name.split("::")[-1], "qualified": qualified,
            "kind": kind, "layer": self.layer, "fileId": self.file_id,
            "path": self.path, "line": node.start_point.row + 1,
            "endLine": node.end_point.row + 1, "signature": signature,
            "access": access, "parent": owner, "description": note,
            "descriptionSource": "source-comment" if note else "declaration"
        }
        annotation = self.annotations.get((qualified, kind, signature))
        if annotation:
            for key in ("description", "details", "returns"):
                if annotation.get(key):
                    item[key] = annotation[key]
            item["descriptionSource"] = "legacy-reviewed-signature-match"
        self.symbols.append(item)

    def visit(self, node, scope=(), owner=None, access="public"):
        kind = node.type
        if kind == "namespace_definition":
            name_node = node.child_by_field_name("name")
            name = node_text(self.data, name_node) if name_node else ""
            if not name:
                m = re.search(r"\bnamespace\s+([a-zA-Z_][\w:]*)", node_text(self.data, node)[:200])
                name = m.group(1) if m else ""
            next_scope = scope + tuple(name.strip(":").split("::")) if name else scope
            body = named_child(node, {"declaration_list"})
            for child in body.named_children if body else node.named_children:
                if child is not name_node:
                    self.visit(child, next_scope, owner, access)
            return
        if kind in {"class_specifier", "struct_specifier", "union_specifier"}:
            label = kind.split("_", 1)[0]
            name_node = node.child_by_field_name("name") or named_child(node, {"type_identifier", "qualified_identifier"})
            name = normalized(node_text(self.data, name_node)) if name_node else ""
            if not name:
                return
            self.emit(label, name, scope, node, access, owner)
            body = named_child(node, {"field_declaration_list"})
            child_scope = scope + (name,)
            child_owner = "::".join(child_scope)
            visibility = "private" if label == "class" else "public"
            if body:
                for c in body.named_children:
                    if c.type == "access_specifier":
                        visibility = normalized(node_text(self.data, c)).rstrip(":")
                    else:
                        self.visit(c, child_scope, child_owner, visibility)
            return
        if kind == "enum_specifier":
            name_node = node.child_by_field_name("name") or named_child(node, {"type_identifier", "identifier"})
            if name_node:
                self.emit("enum", normalized(node_text(self.data, name_node)), scope, node, access, owner)
            return
        if kind in {"alias_declaration", "type_definition"}:
            value = node_text(self.data, node)
            m = re.search(r"\busing\s+([a-zA-Z_]\w*)\s*=", value)
            if not m and kind == "type_definition":
                m = re.search(r"\b([a-zA-Z_]\w*)\s*;", value)
            if m:
                self.emit("alias" if kind == "alias_declaration" else "typedef", m.group(1), scope, node, access, owner)
            return
        if kind in {"field_declaration", "declaration", "function_definition"}:
            fn = find_declarator(node)
            if fn:
                name_node = fn.child_by_field_name("declarator")
                name = normalized(node_text(self.data, name_node)) if name_node else ""
                if not name:
                    m = re.search(r"([~\w:]+|operator[^\s(]+)\s*\(", node_text(self.data, fn))
                    name = m.group(1) if m else ""
                name = name.lstrip("*& ")
                simple = name.split("::")[-1]
                own = owner.split("::")[-1] if owner else ""
                label = ("constructor" if simple == own and own else
                         "destructor" if simple == "~" + own and own else
                         "operator" if "operator" in simple else
                         "method" if owner else "function")
                text = node_text(self.data, node)
                if kind == "function_definition":
                    body = node.child_by_field_name("body")
                    if body:
                        text = self.data[node.start_byte:body.start_byte].decode("utf-8", "replace")
                if name:
                    self.emit(label, simple, scope, node, access, owner, signature=text)
                return
            if kind == "field_declaration" and owner:
                value = normalized(node_text(self.data, node))
                m = re.search(r"([a-zA-Z_]\w*)\s*(?:[;={]|$)", value)
                if m:
                    self.emit("field", m.group(1), scope, node, access, owner, signature=value)
            return
        for child in node.named_children:
            self.visit(child, scope, owner, access)

    def parse(self):
        tree = self.parser.parse(self.data)
        if tree.root_node.has_error:
            self.warnings.append(self.path)
        self.visit(tree.root_node)
        return self.symbols


def collect(source, annotations):
    directory = source / "native/include"
    paths = sorted(p for p in directory.rglob("*") if p.suffix in {".h", ".hpp"})
    if len(paths) < 80:
        raise ValueError("Invalid native checkout or too few headers")
    records, files, warnings = {}, [], []
    for p in paths:
        path = p.relative_to(source).as_posix()
        data = p.read_bytes()
        e = Extract(data, path, annotations)
        warnings += e.warnings
        for symbol in e.parse():
            original = records.get(symbol["id"])
            if original and (original["qualified"], original["kind"]) != (symbol["qualified"], symbol["kind"]):
                raise ValueError("Symbol ID hash collision")
            if original:
                original.setdefault("otherDeclarations", []).append({"path": path, "line": symbol["line"]})
            else:
                records[symbol["id"]] = symbol
        files.append({"id": e.file_id, "path": path, "name": p.name, "layer": e.layer,
                      "sha256": hashlib.sha256(data).hexdigest(),
                      "symbolCount": len(e.symbols), "lineCount": len(e.lines)})
    symbols = sorted(records.values(), key=lambda x: (x["qualified"].lower(), x["kind"], x["signature"]))
    ids = {s["qualified"]: s["id"] for s in symbols if s["kind"] in TYPE_KINDS}
    for s in symbols:
        if s["parent"]:
            s["parentId"] = ids.get(s["parent"])
    return symbols, files, warnings


def save(path, data, check=False):
    if check:
        if not path.exists() or path.read_bytes() != data:
            raise ValueError("Published generated file differs: " + str(path))
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)


def generate(source, site, source_ref, check=False, strict=False):
    commit = git_commit(source)
    symbols, files, warnings = collect(source, load_annotations(site))
    counts = Counter(s["kind"] for s in symbols)
    if strict and (len(symbols) < 1000 or len(files) < 80 or len(warnings) > 50):
        raise ValueError(f"Native coverage gate: {len(symbols)} symbols, {len(files)} files, {len(warnings)} parser errors")
    short = commit[:12]
    data_root = site / "data" / short
    compact_index = [{key:s.get(key) for key in ("id","name","qualified","kind","layer","fileId","parentId","parent","line","description","access","signature")} for s in symbols]
    buckets = defaultdict(dict)
    for s in symbols:
        buckets[s["id"][2:4]][s["id"]] = s
    manifest = {
        "schema": SCHEMA, "sourceRepo": "bradgearon/ChristEye-Knowledge",
        "sourceRef": source_ref, "commit": commit, "version": short,
        "symbols": len(symbols), "headers": len(files), "counts": dict(counts),
        "warningCount": len(warnings), "sourceValidation": "Tree-sitter source index; separate compiler-build CI gate",
        "index": f"data/{short}/search.json",
        "bucketPrefix": f"data/{short}/symbols/",
        "filesPrefix": f"data/{short}/files/"
    }
    targets = {
        data_root / "search.json": as_json({"symbols": compact_index, "files": files}),
        data_root / "warnings.json": as_json({"warningHeaders": warnings}),
        data_root / "manifest.json": as_json(manifest)
    }
    for bucket, data in buckets.items():
        targets[data_root / "symbols" / (bucket + ".json")] = as_json(data)
    for file in files:
        targets[data_root / "files" / (file["id"] + ".json")] = as_json({
            **file, "source": (source / file["path"]).read_text(encoding="utf-8", errors="replace")
        })
    version_file = site / "versions.json"
    previous = json.loads(version_file.read_text()) if version_file.exists() else {"versions":[]}
    versions = [manifest] + [m for m in previous["versions"] if m["commit"] != commit]
    targets[version_file] = as_json({"schema": SCHEMA, "current": short, "versions": versions[:20]})
    for name in ("index.html", "reference.css", "reference.js"):
        targets[site / name] = (Path(__file__).parent / "ui" / name).read_bytes()
    legacy = site / "legacy.html"
    if not legacy.exists() and (site / "index.html").exists():
        targets[legacy] = (site / "index.html").read_bytes()
    for name, data in sorted(targets.items()):
        save(name, data, check)
    print(f"Native API {commit}: {len(symbols)} symbols, {len(files)} headers, {len(warnings)} syntax-warning headers, {len(targets)} files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--site", type=Path, default=ROOT / "docs/native-reference")
    parser.add_argument("--source-ref", default="feature/anaiero-environment-calibration")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    generate(args.source.resolve(), args.site.resolve(), args.source_ref, args.check, args.strict)
