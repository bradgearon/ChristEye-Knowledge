# Anaiero Native API Reference — Generator v2

Reproducible source-pinned Tree-sitter C++20 documentation and static browser.
The native source is currently on feature/anaiero-environment-calibration, not main.

Local generation:
  python -m pip install -r tools/native_docs/requirements.txt
  python tools/native_docs/generate.py --source ../native-checkout --strict
  python tools/native_docs/verify_output.py --site docs/native-reference
  python tools/native_docs/generate.py --source ../native-checkout --strict --check

Only Git-checked-out source can be indexed; every output records the full source commit.
Data snapshots are immutable under docs/native-reference/data/<sha12>.
The browser fetches only a compact symbol index at startup, then declaration and source details lazily.
Stable symbol IDs derive from qualified identifiers and signatures; URLs support version selection.
The previous page and offline edition are preserved at docs/native-reference/legacy.html.

Tree-sitter is a syntactic parser. A native CMake build is separately required to validate compiled C++.
It is not a complete compiler AST, macro-expansion, or semantically verified dependency/call graph.
Retained legacy-reviewed descriptions are explicitly labeled and require signature identity.

GitHub Actions: PR validation, main tool changes, daily source synchronization, and manual workflow dispatch.
Successful source sync commits a complete validated snapshot; the old page remains intact on failure.
Direct Pages deployment requires Settings > Pages > Source to be configured for GitHub Actions.
Commits made using GITHUB_TOKEN do not independently trigger branch-based GitHub Pages builds.

Python dependencies are pinned; output serialization is sorted, timestamp-free and reproducible.
