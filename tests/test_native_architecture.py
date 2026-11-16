"""Protect Anaiero v0.015 source layering without pretending legacy debts are fixed.

Run: python -m unittest tests.test_native_architecture -v
Existing reverse dependencies are explicitly enumerated; new ones fail CI.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "native" / "include" / "anaiero"
INCLUDE = re.compile(r'^\s*#include\s+"anaiero/(models|contracts|services|infrastructure)/([^"]+)"', re.M)

# Baseline from audited commit 5f5ab33a6668b712c54dc15327a568e77af67a98.
# Remove individual waivers as modules are inverted; do not add new waivers casually.
KNOWN_DEBT = {
    ("infrastructure/cell_checkpoint.hpp", "services/cell_behavior_service.hpp"),
    ("infrastructure/windbus_adapter.hpp", "services/runtime_service.hpp"),
    ("services/runtime_service.hpp", "infrastructure/weighted_queue.hpp"),
}


def internal_includes(path: Path):
    for match in INCLUDE.finditer(path.read_text(encoding="utf-8")):
        yield match.group(1) + "/" + match.group(2)


class NativeArchitectureTests(unittest.TestCase):
    def test_passive_models(self):
        files = list((ROOT / "models").glob("*.hpp"))
        self.assertTrue(files, "Missing native Anaiero model headers")
        for path in files:
            with self.subTest(path=path.name):
                self.assertFalse(
                    [item for item in internal_includes(path)
                     if not item.startswith("models/")],
                    "Models must not depend on contracts, services or infrastructure",
                )

    def test_contracts_do_not_depend_on_services_or_infrastructure(self):
        files = list((ROOT / "contracts").glob("*.hpp"))
        self.assertTrue(files, "Missing native contracts")
        for path in files:
            with self.subTest(path=path.name):
                self.assertFalse(
                    [item for item in internal_includes(path)
                     if item.startswith(("services/", "infrastructure/"))],
                    "Contracts are inside the dependency boundary",
                )

    def test_only_known_reverse_dependencies_exist(self):
        observed = set()
        for folder in ("services", "infrastructure"):
            for path in (ROOT / folder).glob("*.hpp"):
                for included in internal_includes(path):
                    reverse = (folder == "services" and included.startswith("infrastructure/")) or (
                        folder == "infrastructure" and included.startswith("services/")
                    )
                    if reverse:
                        observed.add((f"{folder}/{path.name}", included))
        self.assertEqual(
            KNOWN_DEBT, observed,
            "Source-layer dependency waivers changed. Reduce by refactoring; "
            "new reverse dependencies require architecture review.",
        )

    def test_os_specific_headers_stay_out_of_portable_layers(self):
        forbidden = ("windows.h", "winsock2.h", "sys/epoll.h",
                     "sys/eventfd.h", "sys/socket.h", "sys/mman.h", "unistd.h")
        for layer in ("models", "contracts", "services"):
            for path in (ROOT / layer).rglob("*.hpp"):
                for line in path.read_text(encoding="utf-8").splitlines():
                    if not line.lstrip().startswith("#include"):
                        continue
                    for header in forbidden:
                        with self.subTest(path=path.name, header=header):
                            self.assertNotIn(header, line)

    def test_windows_linux_parity_adr(self):
        adr = (Path(__file__).resolve().parents[1] /
               "docs/software/ADR-002_native_windows_linux_parity.md").read_text(encoding="utf-8")
        for term in ("native C++20", "Linux", "Windows", "IOCP", "epoll",
                     "named pipes", "Unix-domain sockets", "SCM", "systemd"):
            with self.subTest(term=term):
                self.assertIn(term, adr)

    def test_models_have_no_io_or_transport_includes(self):
        forbidden = ("<filesystem>", "<fstream>", "<thread>", "<sys/socket.h>",
                     '"christeye/wind/', '"anaiero/infrastructure/')
        for path in (ROOT / "models").glob("*.hpp"):
            source = path.read_text(encoding="utf-8")
            for bad in forbidden:
                with self.subTest(path=path.name, forbidden=bad):
                    self.assertNotIn(f"#include {bad}", source)


if __name__ == "__main__":
    unittest.main()
