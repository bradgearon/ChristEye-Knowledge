# Installed SDK reference

This optional documentation tool writes `native/docs/sdk-reference.md` from
`native/sdk/include`. It reuses the project's native-reference syntax-tree
declaration indexer, narrows discovery to installed SDK headers, and emits a
single Markdown file. It does not update website data, browser assets, legacy
header documentation or native build manifests.

```sh
python3 -m pip install -r tools/sdk-reference/requirements.txt
python3 tools/sdk-reference/generate_reference.py
python3 tools/sdk-reference/generate_reference.py --check
```

Python and these parser packages are documentation dependencies only. The native
controller and SDK build do not discover or depend on them.

Output order and header hashes are deterministic. There is no generation time,
branch name or self-changing commit ID. Regeneration is valid before committing
new headers: each displayed SHA-256 pins the actual header bytes in the working
tree. `--source` selects another checkout; `--output` selects another Markdown
destination.

The syntax index preserves overload-family navigation, public access and type
names. Complete original header blocks preserve the exact declarations, including
source comments, defaults, callback macros and inline implementations. The known
`ANAIERO_MODULE_CALL` qualifier is masked with equal-length whitespace only for
parsing. Parser limitations are listed in the generated audit section; they
never cause source text to be omitted or descriptions to be invented.

The declaration Indexer derives from this repository's separate
`tools/native-reference/generate_reference.py` workflow. That original workflow
indexes legacy `native/include`, supports reviewed annotations and publishes a
source-pinned browser. This SDK-specific emitter intentionally has no dependency
on that checkout or on its site integration.
