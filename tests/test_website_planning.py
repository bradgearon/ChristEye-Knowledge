"""S01 site blueprint invariants. No web/network or biological instrumentation required."""
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
PLANS=ROOT/"website"/"planning"

class WebsitePlanningTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.content=json.loads((PLANS/"content-registry.json").read_text())
        cls.sitemap=json.loads((PLANS/"site-map.json").read_text())
        cls.demos=json.loads((PLANS/"demo-catalog.json").read_text())

    def test_exact_source_files_and_unique_routes(self):
        sources=self.content["sources"]
        self.assertGreaterEqual(len(sources),300)
        self.assertEqual(self.content["source_documents"],len(sources))
        self.assertEqual(len(set(s["source_path"] for s in sources)),len(sources))
        self.assertEqual(len(set(s["planned_route"] for s in sources)),len(sources))
        for entry in sources:
            self.assertTrue((ROOT/entry["source_path"]).is_file(),entry["source_path"])
            self.assertEqual(len(entry["source_blob_sha"]),40)

    def test_every_editorial_route_has_real_primary_sources(self):
        for r in self.sitemap["routes"]:
            self.assertTrue(r["path"].startswith("/"))
            self.assertTrue(r["path"].endswith("/"))
            self.assertTrue(r["sources"])
            for source in r["sources"]:
                self.assertTrue((ROOT/source).is_file(),source)

    def test_demo_truth_and_source_checks(self):
        self.assertEqual(len(self.demos["demos"]),9)
        self.assertTrue(self.demos["no_live_biological_interfaces"])
        count=0
        for demo in self.demos["demos"]:
            for source in demo["currentSource"]:
                self.assertTrue((ROOT/source).is_file(),source)
            if demo["state"]=="code-exists":
                count+=1
                self.assertEqual(demo["kind"] in ("digital-reference","synthetic-telemetry"),True)
            if demo["state"]=="specification-only":
                self.assertNotEqual(demo["kind"],"digital-reference")
        self.assertEqual(count,4)

    def test_metadata_schemas_parse(self):
        for name in ["page-metadata.schema.json","claim-evidence.schema.json"]:
            s=json.loads((PLANS/name).read_text())
            self.assertEqual(s["$schema"],"https://json-schema.org/draft/2020-12/schema")
            self.assertFalse(s.get("additionalProperties",True))
            self.assertTrue(set(s["required"]).issubset(s["properties"]))

    def test_design_and_roadmap_exist(self):
        for name in [
            "01-source-audit.md","02-information-architecture.md","03-design-system.md",
            "04-evidence-and-content-model.md","05-interactive-labs-and-starchips.md",
            "06-technical-architecture.md","07-release-plan.md","08-first-page-storyboards.md"
        ]:
            self.assertGreater(len((PLANS/name).read_text()),3500,name)
        assert (ROOT/"website"/"README.md").is_file()

    def test_patent_document_not_imported(self):
        for entry in self.content["sources"]:
            self.assertFalse("patent" in entry["source_path"].lower() and
                             entry["source_path"].lower().endswith((".pdf",".docx")))
            self.assertEqual(entry["publication"],"WDS-public-non-patent")

if __name__=="__main__":
    unittest.main()
