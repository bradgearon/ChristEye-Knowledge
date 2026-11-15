"""Digital curriculum DAG with prerequisites and evaluation guards."""
from __future__ import annotations
from dataclasses import dataclass

@dataclass(frozen=True)
class Lesson:
    lesson_id:str
    level:str
    prerequisites:tuple[str,...]
    source_digest:str
    evaluation_digest:str

class Curriculum:
    def __init__(self,lessons:list[Lesson]):
        self.lessons={l.lesson_id:l for l in lessons}
        if len(self.lessons)!=len(lessons): raise ValueError("duplicate lesson")
        for lesson in lessons:
            if lesson.level not in ("BREAD","MEAT"): raise ValueError("unknown level")
            if lesson.source_digest==lesson.evaluation_digest: raise ValueError("assessment must have separate content")
        visiting=set()
        completed=set()
        def visit(key):
            if key not in self.lessons: raise ValueError("unknown prerequisite")
            if key in visiting: raise ValueError("cycle")
            if key in completed: return
            visiting.add(key)
            for dep in self.lessons[key].prerequisites: visit(dep)
            visiting.remove(key)
            completed.add(key)
        for key in self.lessons: visit(key)

    def available(self,passed:set[str]):
        return [l for l in self.lessons.values()
                if l.lesson_id not in passed and set(l.prerequisites).issubset(passed)]
