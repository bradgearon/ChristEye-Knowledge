"""Offline example: consented experience -> Crystal -> verified clone -> memory replay."""
import tempfile
from pathlib import Path
from anaiero.core import UsageEvent,Ledger
from anaiero.crystal import compile_digital_crystal,clone_digital_crystal,replay_to_memory
from anaiero.memory import MemoryBank

if __name__=="__main__":
    led=Ledger()
    led.append(UsageEvent(event_id="demo-001",project_id="anaiero-lab",kind="demonstration",
                          content="Remember the route and outcome of the world-graph test",
                          source_id="test:world-graph",scope="shareable",
                          use_for_training=True,allow_export=True))
    with tempfile.TemporaryDirectory() as tmp:
        src=Path(tmp)/"pack"
        dst=Path(tmp)/"verified-clone"
        compile_digital_crystal(led.events(),src,"anaiero.worldgraph.crystal",export=True)
        clone_digital_crystal(src,dst)
        bank=MemoryBank()
        print(replay_to_memory(dst,bank))
        print(bank.recall("world graph"))
