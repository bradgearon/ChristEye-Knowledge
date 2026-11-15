# Runnable digital reference

The `anaiero/` Python package is a digital-only software prototype, directly browsable on GitHub. **It is not a biological device controller.**

- [Typed usage events and consent ledger](../anaiero/core.py)
- [WindBus router](../anaiero/bus.py)
- [Memory Bank](../anaiero/memory.py)
- [Roots assessment](../anaiero/roots.py)
- [Lotus and digital action veto](../anaiero/lotus.py)
- [Content-addressed filesystem](../anaiero/store.py)
- [Digital Crystal compiler, verified cloning and replay](../anaiero/crystal.py)
- [Bread/Meat curriculum DAG](../anaiero/curriculum.py)
- [Qualified-care metadata with physical actuation disabled](../anaiero/care.py)
- [Example replay script](../examples/run_digital_replay.py)

Run locally (Python 3.10+):

```bash
python -m unittest discover -s tests -v
python -m examples.run_digital_replay
```

Source references and scientific assumptions are in [Research](research.md); private patent material is not part of the code.
