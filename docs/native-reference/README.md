# Anaiero Native Reference

Source-pinned C++20 API reference at https://bradgearon.github.io/ChristEye-Knowledge/native-reference/.

Version 2 is generated from the native source branch with Tree-sitter and a separate native CMake compilation gate.
The first generated version is expected at commit c5a4e8a04616c76decab52d237de53a3f3b09f23.

The three-pane reader loads versions.json and a compact search index at startup. Declaration buckets and full
source text are requested only when a symbol or source file is opened. Each version has immutable snapshot data
under data/<commit-short> and exact source links. Features include class-member groups, overloads, search facets,
keyboard navigation, version selection, declaration diffs, source previews, and persistent deep links.

The original supplied reference, its 24 data parts, original app.js/styles.css and its offline edition are retained.
The original index is saved as legacy.html by the generator and remains a rollback path.

Do not hand-edit generated symbols. To regenerate, update or invoke tools/native_docs/generate.py and the
GitHub Actions native-reference workflow. Its separate README documents the parser, quality threshold, and
publishing prerequisite. A failed validation leaves the previously published site unchanged.

Scientific classification is not derived from C++ symbol documentation. Digital/runtime tests are not evidence
of biological learning or physical validation.
