"""Typed records and deterministic, consent-aware usage-event ledger."""
from __future__ import annotations
import dataclasses, hashlib, json, re
from typing import Any

ALLOWED_SCOPES={"local", "shareable", "restricted"}
KINDS={"task", "correction", "demonstration", "reference", "result", "preference", "revocation"}

def canonical_bytes(obj: Any) -> bytes:
    return json.dumps(obj,sort_keys=True,ensure_ascii=False,separators=(",",":")).encode("utf-8")

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

@dataclasses.dataclass(frozen=True)
class UsageEvent:
    event_id: str
    project_id: str
    kind: str
    content: str
    source_id: str
    scope: str = "local"
    use_for_training: bool = False
    allow_export: bool = False
    timestamp: str = "2026-11-14T00:00:00Z"
    parent_id: str | None = None

    def __post_init__(self):
        if self.scope not in ALLOWED_SCOPES: raise ValueError("bad scope")
        if self.kind not in KINDS: raise ValueError("bad kind")
        if not re.fullmatch(r"[A-Za-z0-9._-]{3,90}",self.event_id): raise ValueError("bad event_id")
        if not re.fullmatch(r"[A-Za-z0-9._-]{3,90}",self.project_id): raise ValueError("bad project_id")
        if not self.source_id or not self.content: raise ValueError("missing source/content")
        if self.allow_export and self.scope!="shareable": raise ValueError("export requires shareable scope")

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)

class Ledger:
    """Memory-resident append-only event log; production requires durable storage."""
    def __init__(self):
        self._events=[]
        self._ids=set()
        self._revoked=set()
        self._last_hash="0"*64

    def append(self,event:UsageEvent):
        if event.event_id in self._ids: raise ValueError("duplicate event")
        record={"event":event.to_dict(),"prev_hash":self._last_hash}
        digest=sha256(canonical_bytes(record))
        record["entry_hash"]=digest
        self._events.append(record)
        self._ids.add(event.event_id)
        self._last_hash=digest
        return digest

    def events(self,project_id:str|None=None):
        return [UsageEvent(**r["event"]) for r in self._events
                if (project_id is None or r["event"]["project_id"]==project_id)
                and r["event"]["kind"]!="revocation"
                and r["event"]["event_id"] not in self._revoked]

    def verify(self)->bool:
        prev="0"*64
        for row in self._events:
            if row["prev_hash"]!=prev: return False
            clean={k:row[k] for k in ("event","prev_hash")}
            if sha256(canonical_bytes(clean))!=row["entry_hash"]: return False
            prev=row["entry_hash"]
        return True

    def revoke(self,event_id:str,reason:str="user-revoked"):
        if event_id not in self._ids: raise ValueError("unknown event")
        if event_id in self._revoked: return False
        existing=next(r["event"] for r in self._events if r["event"]["event_id"]==event_id)
        tomb=UsageEvent(event_id="revoke-"+event_id,project_id=existing["project_id"],
             kind="revocation",content=reason,source_id="policy:revocation",
             scope="restricted",use_for_training=False,allow_export=False,parent_id=event_id)
        self.append(tomb)
        self._revoked.add(event_id)
        return True
