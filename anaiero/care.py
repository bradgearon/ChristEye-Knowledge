"""Metadata-only qualified-care schedule; never dispenses physical material."""
from __future__ import annotations
from dataclasses import dataclass

class PhysicalActionUnsupported(RuntimeError): pass

@dataclass(frozen=True)
class ServicePlan:
    service_id:str
    prep_id:str
    sop_ref:str
    operator_role:str
    due_at:float
    grace_seconds:float
    def __post_init__(self):
        if not all((self.service_id,self.prep_id,self.sop_ref,self.operator_role)):
            raise ValueError("missing review or SOP")
        if self.grace_seconds<0: raise ValueError("invalid window")

class CareScheduler:
    def __init__(self):
        self.plans={}
        self.receipts={}

    def register(self,plan:ServicePlan):
        if plan.service_id in self.plans: raise ValueError("duplicate service")
        self.plans[plan.service_id]=plan

    def due(self,now:float):
        return [p for p in self.plans.values()
                if p.service_id not in self.receipts and now>=p.due_at]

    def record_external_service(self,service_id:str,operator:str,sop_ref:str,at:float):
        plan=self.plans[service_id]
        if operator!=plan.operator_role or sop_ref!=plan.sop_ref:
            raise ValueError("operator/SOP mismatch")
        if service_id in self.receipts: raise ValueError("duplicate receipt")
        self.receipts[service_id]={"operator":operator,"sop":sop_ref,"at":at,
                                   "late":at>plan.due_at+plan.grace_seconds}
        return self.receipts[service_id]

    def dispense(self,*args,**kwargs):
        raise PhysicalActionUnsupported("No physical actuation in Anaiero reference runtime")
