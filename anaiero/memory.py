"""Transparent lexical memory index, not a neural engram model."""
import re
from .core import UsageEvent

def tokens(text):
    return set(re.findall(r"[a-z0-9]+",text.lower()))

class MemoryBank:
    def __init__(self):
        self.events={}

    def ingest(self,event:UsageEvent):
        if not event.use_for_training: raise ValueError("not authorized for training")
        if event.event_id in self.events: return False
        self.events[event.event_id]=event
        return True

    def recall(self,query:str,limit:int=5):
        q=tokens(query)
        hits=[]
        for event in self.events.values():
            overlap=len(tokens(event.content)&q)
            if overlap: hits.append((overlap/(len(q) or 1),event))
        return [{"event_id":ev.event_id,"score":round(score,3),"content":ev.content,"source_id":ev.source_id}
                for score,ev in sorted(hits,key=lambda x:(-x[0],x[1].event_id))[:limit]]

    def summary(self):
        return {"stored_events":len(self.events),"grounding":"source-bound lexical index; no neural engram claims"}
