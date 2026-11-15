"""Structured scientific handoff record, not an authorization to experiment."""
from dataclasses import dataclass

@dataclass(frozen=True)
class StudyHandoff:
    study_id:str
    question:str
    source_lot_id:str|None
    source_consent_reviewed:bool
    preparation_approved:bool
    scientific_director_signed:bool
    cell_production_lead_signed:bool
    safety_officer_signed:bool
    evaluator_sealed:bool

    def blockers(self):
        fields={
            "source_lot_id":bool(self.source_lot_id),
            "source_consent_reviewed":self.source_consent_reviewed,
            "preparation_approved":self.preparation_approved,
            "scientific_director_signed":self.scientific_director_signed,
            "cell_production_lead_signed":self.cell_production_lead_signed,
            "safety_officer_signed":self.safety_officer_signed,
            "evaluator_sealed":self.evaluator_sealed
        }
        return sorted(k for k,v in fields.items() if not v)

    def ready_for_institutional_review(self):
        return bool(self.study_id and self.question and not self.blockers())
