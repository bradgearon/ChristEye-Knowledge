"""Publication and documentation quality gates for the GitHub release."""
from __future__ import annotations
import json
from pathlib import Path
import re
import unittest
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]

class PublicationIntegrityTests(unittest.TestCase):
    def test_phase1_original_six_complete(self):
        docs=sorted((ROOT/"phases"/"P01"/"docs").glob("*.md"))
        self.assertEqual(len(docs),6)
        for path in docs:
            text=path.read_text(encoding="utf-8")
            self.assertIn("ChristEye Anaiero",text)
            self.assertGreater(len(text),6000)
            self.assertIn("WDS research property",text)

    def test_complete_original_word_archive_conversion(self):
        manifest=json.loads((ROOT/"specifications"/"conversion-manifest.json").read_text())
        entries=manifest["documents"]
        self.assertEqual(len(entries),174)
        for item in entries:
            path=ROOT/item["path"]
            self.assertTrue(path.is_file(),str(path))
            self.assertGreater(len(path.read_text(encoding="utf-8")),1000)
            self.assertRegex(item["sha256"],r"^[a-f0-9]{64}$")

    def test_current_pairs_and_historical_isolation(self):
        designs=list((ROOT/"specifications"/"v3"/"design").glob("*.md"))
        companions=list((ROOT/"specifications"/"v3"/"companion").glob("*.md"))
        self.assertEqual(len(designs),24)
        self.assertEqual(len(companions),24)
        self.assertEqual(len(list((ROOT/"archive"/"v2").glob("*.md"))),83)

    def test_phase_specific_originals(self):
        phase2=list((ROOT/"phases"/"P02"/"docs").rglob("*.md"))
        phase2b=list((ROOT/"phases"/"P02B"/"docs").rglob("*.md"))
        self.assertEqual(len(phase2),14)
        self.assertEqual(len(phase2b),21)

    def test_json_schemas_parse(self):
        files=sorted((ROOT/"schemas"/"v6").rglob("*.schema.json"))
        # v6 contains 24 module-record schemas plus cross-system contracts.
        self.assertGreaterEqual(len(files),37)
        for path in files:
            obj=json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(obj.get("type"),"object",path.name)

    def test_reference_register_and_cell_count_origin(self):
        sources=json.loads((ROOT/"phases"/"P01"/"registers"/"source_register.json").read_text())
        self.assertEqual(len(sources),16)
        physical=json.loads((ROOT/"phases"/"P01"/"registers"/"module_cell_count_registry.json").read_text())
        self.assertTrue(physical["counts_are_vendor_reference"])
        cells={m["module_id"]:m for m in physical["module_count_records"]}
        self.assertEqual(cells["C"]["ref_live_neurons_per_unit"],80000)
        self.assertEqual(cells["C"]["ref_live_astrocytes_per_unit"],20000)

    def test_physical_patent_document_is_not_published(self):
        for path in ROOT.rglob("*"):
            if not path.is_file():continue
            self.assertFalse(path.suffix.lower()==".docx" and "patent" in path.name.lower(),
                             "Patent DOCX must not be published")

    def test_core_documentation_links_resolve(self):
        files=[ROOT/"README.md",ROOT/"docs"/"index.md",ROOT/"docs"/"phase-1.md",
               ROOT/"docs"/"full-document-library.md",ROOT/"docs"/"schema-reference.md",
               ROOT/"docs"/"document-standard.md",ROOT/"schemas"/"README.md",
               ROOT/"phases"/"P01"/"README.md"]
        pattern=re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
        for src in files:
            for href in pattern.findall(src.read_text(encoding="utf-8")):
                if href.startswith(("https:","http:","mailto:","#")):continue
                target=unquote(href.split("#")[0])
                if not target:continue
                self.assertTrue((src.parent/target).exists(),f"{src}: {href}")

if __name__=="__main__":
    unittest.main()
