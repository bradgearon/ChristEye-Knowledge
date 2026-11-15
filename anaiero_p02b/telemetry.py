"""Authenticated read-only *simulated* board sensor receipts. No equipment commands."""
from __future__ import annotations
from dataclasses import dataclass
import hashlib,hmac,json,math
from typing import Mapping,Any

SCHEMA="anaiero.p02b.telemetry/1"

def canonical(value:object)->bytes:
    return json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(",",":"),allow_nan=False).encode("utf-8")

def sign(payload:Mapping[str,Any],key:bytes)->str:
    """HMAC demonstration; hardware key lifecycle is not implemented."""
    return hmac.new(key,canonical(payload),hashlib.sha256).hexdigest()

@dataclass(frozen=True)
class Board:
    board_id:str
    revision:str
    firmware_sha256:str
    allowed_measurements:frozenset[str]
    calibration_digest:str
    channel:str="wired"
    read_only:bool=True

    def __post_init__(self):
        if not self.board_id or not self.allowed_measurements or not self.calibration_digest:
            raise ValueError("board identity, metrics and calibration provenance required")
        if not self.read_only: raise ValueError("P02B admits read-only board identities")
        if self.channel not in {"wired","simulated","ble_monitor"}: raise ValueError("unknown transport")

class TelemetryGateway:
    def __init__(self,boards:Mapping[str,Board],keys:Mapping[str,bytes],*,max_age_ms:int=120000,max_future_ms:int=2000):
        if set(boards)!=set(keys): raise ValueError("board and key registry mismatch")
        self.boards=dict(boards)
        self.keys=dict(keys)
        self.max_age_ms=max_age_ms
        self.max_future_ms=max_future_ms
        self.last_sequence={}
        self.accepted=[]
        self.quarantined=[]

    def _reject(self,reason:str,board_id:str,sequence:object)->dict:
        receipt={"outcome":"quarantined","reason":reason,"board_id":board_id,"sequence":sequence}
        self.quarantined.append(receipt)
        return receipt

    def receive(self,frame:Mapping[str,Any],*,now_ms:int)->dict:
        if not isinstance(frame,Mapping): return self._reject("not_mapping","unknown",None)
        board_id=str(frame.get("board_id","unknown"))
        seq=frame.get("sequence")
        required={"schema","board_id","sequence","observed_ms","metric","value","unit","calibration_digest","signature"}
        if set(frame)!=required: return self._reject("invalid_fields_or_command",board_id,seq)
        if frame.get("schema")!=SCHEMA: return self._reject("schema_mismatch",board_id,seq)
        board=self.boards.get(board_id)
        if board is None: return self._reject("unknown_board",board_id,seq)
        if not isinstance(seq,int) or isinstance(seq,bool) or seq<0: return self._reject("bad_sequence",board_id,seq)
        value=frame["value"]
        if not isinstance(value,(float,int)) or isinstance(value,bool) or not math.isfinite(float(value)):
            return self._reject("invalid_numeric_value",board_id,seq)
        body={k:frame[k] for k in required-{"signature"}}
        signature=frame["signature"]
        if not isinstance(signature,str) or not hmac.compare_digest(sign(body,self.keys[board_id]),signature):
            return self._reject("authentication_failed",board_id,seq)
        if seq<=self.last_sequence.get(board_id,-1): return self._reject("duplicate_or_out_of_order",board_id,seq)
        observed=frame["observed_ms"]
        if not isinstance(observed,int) or isinstance(observed,bool): return self._reject("bad_timestamp",board_id,seq)
        if observed<now_ms-self.max_age_ms or observed>now_ms+self.max_future_ms:
            return self._reject("stale_or_future_sample",board_id,seq)
        if frame["metric"] not in board.allowed_measurements: return self._reject("undeclared_measurement",board_id,seq)
        if frame["calibration_digest"]!=board.calibration_digest: return self._reject("calibration_mismatch",board_id,seq)
        if not isinstance(frame["unit"],str) or not frame["unit"].strip():
            return self._reject("unit_missing",board_id,seq)
        self.last_sequence[board_id]=seq
        receipt={"outcome":"accepted","board_id":board_id,"sequence":seq,
                 "metric":frame["metric"],"value":value,"unit":frame["unit"],
                 "observed_ms":observed,"age_ms":now_ms-observed,"channel":board.channel,
                 "source":"synthetic" if board.channel=="simulated" else "external_readonly",
                 "calibration_digest":board.calibration_digest}
        self.accepted.append(receipt)
        return receipt

def make_frame(*,board:Board,key:bytes,sequence:int,observed_ms:int,metric:str,value:float,unit:str)->dict:
    body=dict(schema=SCHEMA,board_id=board.board_id,sequence=sequence,observed_ms=observed_ms,
              metric=metric,value=value,unit=unit,calibration_digest=board.calibration_digest)
    return {**body,"signature":sign(body,key)}
