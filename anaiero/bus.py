"""Bounded, typed, digital-only WindBus. Contains no biological instrument drivers."""
from __future__ import annotations
from dataclasses import dataclass
from collections import deque
from typing import Callable
import time,re

KINDS=frozenset({"task","experience","observation","advisory","evidence","replay","status"})
SCOPES=frozenset({"local","shareable","restricted"})
class WindError(ValueError): pass

@dataclass(frozen=True)
class WindMessage:
    event_id:str
    source:str
    recipient:str
    kind:str
    payload_digest:str
    schema_version:str="1.0.0"
    scope:str="local"
    expires_at:float=0
    trace_id:str="local-trace"

    def __post_init__(self):
        if self.kind not in KINDS or self.scope not in SCOPES: raise WindError("invalid kind or scope")
        if not re.fullmatch(r"[0-9a-f]{64}",self.payload_digest): raise WindError("invalid digest")
        if not self.event_id or not self.source or not self.recipient: raise WindError("missing route identity")
        if not re.fullmatch(r"\d+\.\d+\.\d+",self.schema_version): raise WindError("unsupported version")

class WindBus:
    def __init__(self,capacity=100):
        if capacity<1: raise ValueError("capacity must be positive")
        self.capacity=capacity
        self.routes={}
        self.queue=deque()
        self.sent=set()
        self.delivered=set()
        self.log=[]

    def register(self,module:str,allowed_sources:set[str],allowed_kinds:set[str],callback:Callable):
        if not module or not callable(callback): raise WindError("invalid subscriber")
        if not allowed_kinds.issubset(KINDS): raise WindError("unknown kind")
        self.routes[module]=(set(allowed_sources),set(allowed_kinds),callback)

    def publish(self,msg:WindMessage,now:float|None=None):
        now=time.time() if now is None else now
        if msg.expires_at<=now: raise WindError("message expired")
        if msg.recipient not in self.routes: raise WindError("unknown recipient")
        sources,kinds,_=self.routes[msg.recipient]
        if msg.source not in sources or msg.kind not in kinds: raise WindError("unauthorized route")
        if msg.event_id in self.sent: return False
        if len(self.queue)>=self.capacity: raise WindError("backpressure capacity exceeded")
        self.queue.append(msg)
        self.sent.add(msg.event_id)
        self.log.append({"event_id":msg.event_id,"status":"queued","at":now})
        return True

    def drain(self,now:float|None=None):
        now=time.time() if now is None else now
        deliveries=[]
        while self.queue:
            msg=self.queue.popleft()
            if msg.event_id in self.delivered: continue
            if msg.expires_at<=now:
                self.log.append({"event_id":msg.event_id,"status":"expired","at":now})
                continue
            callback=self.routes[msg.recipient][2]
            try: receipt=callback(msg)
            except Exception as e:
                self.log.append({"event_id":msg.event_id,"status":"handler-failed","reason":type(e).__name__,"at":now})
                raise
            self.delivered.add(msg.event_id)
            self.log.append({"event_id":msg.event_id,"status":"delivered","at":now})
            deliveries.append({"event_id":msg.event_id,"receipt":receipt})
        return deliveries
