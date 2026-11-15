"""Read-only supervisor state machine and hash-chained event audit."""
from __future__ import annotations
from dataclasses import dataclass
import hashlib,json
from pathlib import Path
from .telemetry import canonical

class ForbiddenActuation(PermissionError): pass

class ReadOnlySupervisor:
    def __init__(self):
        self.mode="BOOT"
        self.faults=[]
        self.last_seen={}

    def enter_observation(self):
        if self.mode=="FAULT_LATCHED": raise ForbiddenActuation("fault review required")
        self.mode="OBSERVE_ONLY"

    def receive_receipt(self,receipt:dict):
        if receipt.get("outcome")!="accepted":
            self.faults.append(str(receipt.get("reason","invalid_receipt")))
            self.mode="FAULT_LATCHED"
        else:
            self.last_seen[receipt["board_id"]]=receipt["observed_ms"]

    def request(self,action:str,**kwargs):
        raise ForbiddenActuation("P02B has no physical actuation authority")

    def acknowledge_fault(self,*,human_reviewed:bool):
        if not human_reviewed: raise ForbiddenActuation("review required")
        self.faults.clear()
        self.mode="OBSERVE_ONLY"

@dataclass
class HashChainedLedger:
    path:Path
    prior:str="0"*64

    def append(self,record:dict)->str:
        payload={"previous_sha256":self.prior,"record":record}
        digest=hashlib.sha256(canonical(payload)).hexdigest()
        with self.path.open("a",encoding="utf-8") as f:
            f.write(json.dumps({**payload,"sha256":digest},ensure_ascii=False,sort_keys=True)+"\n")
        self.prior=digest
        return digest

    @staticmethod
    def verify(path:Path)->bool:
        prev="0"*64
        for line in path.read_text(encoding="utf-8").splitlines():
            row=json.loads(line)
            if row.get("previous_sha256")!=prev: return False
            digest=hashlib.sha256(canonical({"previous_sha256":prev,"record":row["record"]})).hexdigest()
            if row.get("sha256")!=digest: return False
            prev=digest
        return True
