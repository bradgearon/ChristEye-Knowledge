"""Transparent delayed-retention metrics; does not locate a biological engram."""
from __future__ import annotations
from dataclasses import dataclass

LABELS=("X","Y","HOLD")

def macro_recall(actual:list[str],predicted:list[str]):
    if len(actual)!=len(predicted) or not actual: raise ValueError("nonempty matched trials required")
    if set(actual)-set(LABELS): raise ValueError("invalid ground truth")
    out={}
    for label in LABELS:
        idx=[i for i,x in enumerate(actual) if x==label]
        if not idx: raise ValueError("all classes required")
        out[label]=sum(predicted[i]==label for i in idx)/len(idx)
    return sum(out.values())/3,out

def retention_index(immediate:float,delayed:float,chance:float=1/3,margin:float=.02):
    if not all(isinstance(x,(int,float)) and 0<=x<=1 for x in (immediate,delayed,chance)):
        raise ValueError("scores outside [0,1]")
    if immediate-chance<=margin: return None
    return (delayed-chance)/(immediate-chance)

@dataclass(frozen=True)
class RootAssessment:
    root_id:str
    immediate:float
    delayed:float
    horizon:str
    protected_deltas:dict[str,float]
    min_delayed:float
    allowed_regression:float=.05

    def evaluate(self):
        if not 0<=self.min_delayed<=1: raise ValueError("invalid target")
        reasons=[]
        if self.delayed<self.min_delayed: reasons.append("delayed-score-below-target")
        for name,delta in self.protected_deltas.items():
            if delta < -self.allowed_regression: reasons.append(f"protected-regression:{name}")
        return {"status":"CANDIDATE" if not reasons else "HOLD",
                "reasons":reasons,
                "retention_ratio":retention_index(self.immediate,self.delayed),
                "absolute_delayed":self.delayed,
                "horizon":self.horizon,
                "qualification":"demonstration only; confirm with uncertainty and independent data"}
