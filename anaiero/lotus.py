"""Purely digital advisory coordinator with a hard action allowlist."""
from dataclasses import dataclass

SAFE_DIGITAL_ACTIONS={"replay_digital","refresh_index","train_digital","snapshot_digital"}

@dataclass(frozen=True)
class ActionRequest:
    action:str
    target:str
    evidence:str
    expected_gain:float
    protected_delta:float
    cost:float

class LimitController:
    def permit(self,req:ActionRequest):
        if req.action not in SAFE_DIGITAL_ACTIONS: return False,"physical-or-unknown-action-not-implemented"
        if not req.evidence: return False,"missing-evidence"
        return True,"digital-permitted"

class Lotus:
    def __init__(self,limit=None,noninferiority_margin=.05):
        self.limit=limit or LimitController()
        self.margin=noninferiority_margin

    def evaluate(self,req:ActionRequest):
        permit,reason=self.limit.permit(req)
        if not permit: return {"state":"VETO","reason":reason}
        if req.protected_delta < -self.margin: return {"state":"HOLD","reason":"protected-root-regression"}
        score=req.expected_gain-req.cost
        if score>0: return {"state":"STAGE_DIGITAL","reason":"positive-expected-net-gain","score":score}
        return {"state":"UNRESOLVED","reason":"no-positive-established-value","score":score}
