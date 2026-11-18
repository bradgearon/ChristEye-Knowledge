"""Parser regression tests: namespaces, ownership, overloads, stable routes."""
import importlib.util
import sys
import unittest
from pathlib import Path

source = Path(__file__).resolve().parents[1] / "generate.py"
spec = importlib.util.spec_from_file_location("anaiero_generator", source)
gen = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = gen
spec.loader.exec_module(gen)


def symbols(text):
    return gen.Extract(text.encode(), "native/include/anaiero/services/demo.hpp", {}).parse()


class NativeIndexTests(unittest.TestCase):
    def test_types_access_and_overloads(self):
        sample = """#pragma once
namespace anaiero::services {
// A service that exercises the generator.
class ExampleService {
public:
  ExampleService() = default;
  int visit(int count) noexcept;
  int visit(double count) noexcept;
private:
  bool enabled_;
};
}
"""
        entries = symbols(sample)
        cls = [s for s in entries if s["kind"] == "class"]
        self.assertEqual(cls[0]["qualified"], "anaiero::services::ExampleService")
        self.assertEqual(cls[0]["description"], "A service that exercises the generator.")
        overloads = [s for s in entries if s["name"] == "visit"]
        self.assertEqual(len(overloads), 2)
        self.assertEqual(len({s["id"] for s in overloads}), 2)
        self.assertTrue(all(s["parent"] == "anaiero::services::ExampleService" for s in overloads))
        self.assertTrue(any(s["name"] == "enabled_" and s["access"] == "private" for s in entries))

    def test_ids_do_not_change_for_trailing_whitespace(self):
        text = "namespace test { struct MyType { void call(int value); }; }"
        self.assertEqual([s["id"] for s in symbols(text)], [s["id"] for s in symbols(text + "\n\n")])
        self.assertTrue(all(s["line"] > 0 for s in symbols(text)))

    def test_comma_separated_field_declarators(self):
        entries = symbols("namespace a { struct Record { int source=0,epoch=0; }; }")
        names = {s["name"] for s in entries if s["kind"] == "field"}
        self.assertEqual({"source", "epoch"}, names)

    def test_source_layers_and_json_determinism(self):
        self.assertEqual(gen.layer_for("native/include/anaiero/contracts/engine.hpp"), "contracts")
        self.assertEqual(gen.layer_for("native/include/wind/packet.hpp"), "wind")
        self.assertEqual(gen.as_json({"b":2,"a":1}), gen.as_json({"a":1,"b":2}))


if __name__ == "__main__":
    unittest.main()
