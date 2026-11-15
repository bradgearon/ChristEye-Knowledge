"""Scientific documentation quality checks (v3.1 design release, no biological validation)."""
import json
from pathlib import Path
import re
import unittest
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]
MODULES=json.loads((ROOT/"specifications/v3/rigorous-module-registry.json").read_text())["modules"]
SOURCE=ROOT/"specifications/v3"
PROC=ROOT/"processes"

class RigorousDocumentationTests(unittest.TestCase):
    def test_all_24_modules_and_48_full_documents(self):
        self.assertEqual(len(MODULES),24)
        self.assertEqual(len({m["id"] for m in MODULES}),24)
        self.assertEqual(len(list((SOURCE/"design").glob("CE-A*_Design.md"))),24)
        self.assertEqual(len(list((SOURCE/"companion").glob("CE-A*_Companion.md"))),24)
        for m in MODULES:
            for kind,suffix in (("design","Design"),("companion","Companion")):
                path=SOURCE/kind/(m["id"]+"_"+m["slug"]+"_"+suffix+".md")
                self.assertTrue(path.is_file(),path)
                s=path.read_text(encoding="utf-8")
                self.assertGreater(len(s),12500,path)
                self.assertIn("revision: \"3.1\"",s)
                self.assertIn("date: \"2026-11-15\"",s)
                self.assertIn("Original v3.0",s)
                self.assertNotIn("{{ORIGINAL_V3}}",s)
                self.assertFalse(re.search(r"\{\{[A-Z_0-9]+\}\}",s),path)
                self.assertIn(m["molecular_questions"],s)
                self.assertIn(m["axon"],s)
                self.assertIn(m["training"],s)
                self.assertIn(m["growth"],s)
                if kind=="design":
                    for number in range(1,21):
                        self.assertRegex(s,rf"(?m)^## {number}\.")
                    self.assertIn("Mandatory food/peptide/growth alteration card",s)
                else:
                    for number in range(1,15):
                        self.assertRegex(s,rf"(?m)^## {number}\.")
                    self.assertIn("Food / peptide change research card",s)

    def test_scientific_process_links_resolve(self):
        expected={
            "BIO-01":"BIO-01-crest-lineage.md",
            "BIO-02":"BIO-02-axon-spinal-root.md",
            "BIO-03":"BIO-03-soil-organoid.md",
            "BIO-04":"BIO-04-sustibus-chemibus.md",
            "BIO-05":"BIO-05-measurements.md",
            "BIO-06":"BIO-06-teaching.md",
            "BIO-07":"BIO-07-joining.md",
            "BIO-08":"BIO-08-crystals.md",
            "BIO-09":"BIO-09-growth-control.md",
            "BIO-10":"BIO-10-care-health.md",
            "SYS-11":"SYS-11-module-training.md",
            "SYS-12":"SYS-12-lineage-ids.md"
        }
        self.assertEqual(len(list(PROC.glob("BIO-*.md"))),10)
        self.assertEqual(len(list(PROC.glob("SYS-*.md"))),2)
        for m in MODULES:
            for process in m["processes"]:
                self.assertTrue((PROC/expected[process]).is_file())
            s=(SOURCE/"design"/(m["id"]+"_"+m["slug"]+"_Design.md")).read_text()
            for process in m["processes"]:
                self.assertIn(f"[{process}:",s)

    def test_new_links_in_current_sections_are_valid(self):
        paths=[*list((SOURCE/"design").glob("CE-A*.md")),
               *list((SOURCE/"companion").glob("CE-A*.md")),
               *list(PROC.glob("BIO-*.md")),*list(PROC.glob("SYS-*.md")),
               PROC/"SCIENTIST_WORKFLOW.md",PROC/"MOLECULAR_INDEX.md"]
        pat=re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
        for p in paths:
            text=p.read_text(encoding="utf-8")
            # Archived original edition sometimes uses different relative bases. Only check
            # the current revision before the historical section.
            text=text.split("## Original v3.0")[0]
            for href in pat.findall(text):
                if href.startswith(("https://","http://","mailto:","#")):continue
                dest=unquote(href.split("#")[0])
                if not dest:continue
                self.assertTrue((p.parent/dest).exists(), f"{p} broken {href}")

    def test_all_ten_biological_schemas(self):
        sch=ROOT/"schemas/biology"
        a=list(sch.glob("*.schema.json"))
        self.assertEqual(len(a),10)
        for file in a:
            o=json.loads(file.read_text())
            self.assertEqual(o.get("$schema"),"https://json-schema.org/draft/2020-12/schema")
            self.assertFalse(o.get("additionalProperties"))
            self.assertEqual(o.get("type"),"object")
            self.assertTrue(set(o.get("required",[])).issubset(o["properties"]))
        self.assertEqual(json.loads((sch/"physical_crystal_candidate.schema.json").read_text())["properties"]["installable"],{"const":False})
        self.assertEqual(json.loads((sch/"change_proposal.schema.json").read_text())["properties"]["automated_physical_actuation"],{"const":False})

    def test_cell_and_axon_claim_separation(self):
        root=(PROC/"BIO-02-axon-spinal-root.md").read_text()
        crest=(PROC/"BIO-01-crest-lineage.md").read_text()
        self.assertIn("parts of neurons",root)
        self.assertIn("clone_lineage_id",crest)
        self.assertIn("cell_instance_id",crest)
        self.assertIn("globally unique",crest)
        self.assertIn("living organoid", (PROC/"BIO-03-soil-organoid.md").read_text())

    def test_original_secret_patent_document_not_added(self):
        for p in ROOT.rglob("*"):
            if p.is_file() and p.suffix.lower() in {".docx",".pdf"}:
                self.assertNotIn("patent",p.name.lower())

if __name__=="__main__":
    unittest.main()
