"""A failed native budget or a smaller sample set cannot qualify a new layout."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("index_trials", Path(__file__).parents[1] / "compare-index-layouts.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def case(name, steady, coexisting):
    return {"layout": name, "directory": name, "usable": True, "build_identity": "same",
            "metrics": {"workload": {"observations": 10000},
                        "counts": {"commit_samples": 159, "lookup_samples": 100, "commit_over_budget": 1},
                        "checkpoints": [{"observations": 10000, "steady_files": steady,
                                         "coexistence_files": coexisting, "coexistence_directories": 3}]}}


class LayoutComparison(unittest.TestCase):
    def test_order_capacity_latency_and_native_gate(self):
        baseline = case("node-files", 2500, 5000)
        packed = case("packed-pages", 1400, 1800)
        self.assertTrue(module.compare(packed, baseline)["passed"])
        packed["metrics"]["counts"]["commit_over_budget"] = 2
        self.assertFalse(module.compare(baseline, packed)["passed"])
        packed["metrics"]["counts"]["commit_over_budget"] = 0
        packed["metrics"]["checkpoints"][0]["coexistence_files"] = 6000
        self.assertFalse(module.compare(baseline, packed)["passed"])
        packed["metrics"]["checkpoints"][0]["coexistence_files"] = 1800
        packed["usable"] = False
        self.assertFalse(module.compare(packed, baseline)["passed"])

    def test_comparison_requires_the_same_sample_population(self):
        baseline = case("node-files", 2500, 5000)
        packed = case("packed-pages", 1400, 1800)
        packed["metrics"]["counts"]["lookup_samples"] -= 1
        self.assertFalse(module.compare(packed, baseline)["passed"])


if __name__ == "__main__":
    unittest.main()
