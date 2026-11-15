"""Digital CrystalPack compiler, validator, clone and replay. No molecular handling."""
from pathlib import Path
import json,shutil,re
from .core import UsageEvent,sha256,canonical_bytes
from .memory import MemoryBank

class CrystalError(ValueError): pass

def _manifest_path(root): return Path(root)/"manifest.json"
def _experience_path(root): return Path(root)/"assets"/"experiences.json"

def compile_digital_crystal(events:list[UsageEvent],dest:Path,pack_id:str,
                            version:str="1.0.0",export:bool=False):
    if not re.fullmatch(r"[a-z][a-z0-9.-]{4,80}",pack_id): raise CrystalError("invalid pack_id")
    if not re.fullmatch(r"\d+\.\d+\.\d+",version): raise CrystalError("invalid semver")
    if not events: raise CrystalError("no events")
    eligible=[]
    for ev in events:
        if not ev.use_for_training: continue
        if export and not (ev.allow_export and ev.scope=="shareable"): raise CrystalError("non-exportable event")
        eligible.append(ev.to_dict())
    if not eligible: raise CrystalError("no training-consented records")
    dest=Path(dest)
    if dest.exists(): raise CrystalError("destination exists")
    (dest/"assets").mkdir(parents=True)
    data=canonical_bytes({"experiences":eligible})
    _experience_path(dest).write_bytes(data)
    manifest={
        "schema_version":"2.0.0","pack_id":pack_id,"version":version,
        "kind":"digital-experience-replay","installable":True,
        "permission_mode":"shareable" if export else "local-only",
        "claims":["educational-replay-data"],"evidence_tier":"D",
        "physical_carrier":False,
        "asset":{"path":"assets/experiences.json","sha256":sha256(data)},
        "source_event_ids":[ev["event_id"] for ev in eligible],
        "readers":["anaiero.memory.v2"],
        "activation":["verify","stage","replay","independent_assessment"],
        "disclaimer":"Digital teaching artifact, not a cloned biological engram."
    }
    _manifest_path(dest).write_bytes(canonical_bytes(manifest))
    return manifest

def verify_digital_crystal(root:Path):
    root=Path(root)
    manifest=json.loads(_manifest_path(root).read_bytes())
    if manifest.get("physical_carrier") or manifest.get("kind")!="digital-experience-replay" or not manifest.get("installable"):
        raise CrystalError("not a digitally installable Crystal")
    asset=manifest.get("asset",{})
    if asset.get("path")!="assets/experiences.json": raise CrystalError("unexpected asset path")
    data=_experience_path(root).read_bytes()
    if sha256(data)!=asset.get("sha256"): raise CrystalError("tampered Crystal data")
    events=[UsageEvent(**v) for v in json.loads(data)["experiences"]]
    if manifest.get("source_event_ids")!=[e.event_id for e in events]: raise CrystalError("event lineage mismatch")
    if manifest.get("permission_mode")=="shareable" and any(not e.allow_export for e in events):
        raise CrystalError("permission mismatch")
    return manifest,events

def clone_digital_crystal(src:Path,dest:Path):
    verify_digital_crystal(src)
    if Path(dest).exists(): raise CrystalError("destination exists")
    shutil.copytree(src,dest)
    verify_digital_crystal(dest)
    return True

def replay_to_memory(root:Path,target:MemoryBank):
    manifest,events=verify_digital_crystal(root)
    count=sum(bool(target.ingest(event)) for event in events)
    return {"status":"digital-replayed","pack_id":manifest["pack_id"],"new_events":count,
            "teaching_removed":True,"biological_transfer_claim":False,
            "assessment_required":"held-out and delayed probes; not included in replay"}
