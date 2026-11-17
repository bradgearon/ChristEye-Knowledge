"""Protect Anaiero v0.015 source layering without pretending legacy debts are fixed.

Run: python -m unittest tests.test_native_architecture -v
Existing reverse dependencies are explicitly enumerated; new ones fail CI.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "native" / "include" / "anaiero"
INCLUDE = re.compile(
    r'^\s*#\s*include\s*["<]anaiero/(models|contracts|services|infrastructure|application)/([^">]+)[">]', re.M)
LEGACY_INCLUDE = re.compile(r'^\s*#\s*include\s*["<](christeye/[^">]+)[">]', re.M)
TLS_INCLUDE = re.compile(
    r'^\s*#\s*include\s*["<]((?:openssl|gnutls|mbedtls)/[^">]+|(?:schannel|sspi)\.h)[">]',
    re.M | re.I,
)

# SHA-256 is a portable algorithm, not a host service. Every other legacy
# bridge is allowed by its exact importing header and destination, never by a
# directory wildcard that could hide filesystem/runtime dependencies.
PORTABLE_UTILITIES = {"christeye/living/plan_hash.hpp"}
LEGACY_BRIDGES = {
    ("infrastructure/session_files.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/windstore_route_journal.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/durable_route_ledger.hpp", "christeye/living/durable_io.hpp"),
    ("infrastructure/route_receipt_codec.hpp", "christeye/wind/packet.hpp"),
    ("infrastructure/sim3_ingress.hpp", "christeye/living/colony_runner.hpp"),
    ("infrastructure/windstore_sink.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/passive_model_file.hpp", "christeye/living/durable_records.hpp"),
    ("infrastructure/windbus_adapter.hpp", "christeye/wind/packet.hpp"),
    ("infrastructure/experiment_record_store.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/scientific_solver_configuration.hpp", "christeye/living/durable_records.hpp"),
    ("infrastructure/windstore_epoch_observation_journal.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/cell_checkpoint.hpp", "christeye/living/durable_io.hpp"),
    ("infrastructure/experiment_definition_file.hpp", "christeye/living/learning_plan.hpp"),
    ("infrastructure/experiment_definition_file.hpp", "christeye/living/durable_records.hpp"),
    ("infrastructure/operation_store.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/windstore_checkpoint_catalog.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/mesh_configuration.hpp", "christeye/living/durable_records.hpp"),
    ("infrastructure/mesh_enrollment_store.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/mesh_ingress_store.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/native_session_mesh_export.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/windstore_mesh_artifact.hpp", "christeye/living/durable_registry.hpp"),
    ("infrastructure/windstore_mesh_replay.hpp", "christeye/living/durable_registry.hpp"),
    ("application/sim3_controller_cycle.hpp", "christeye/living/colony_runner.hpp"),
    ("application/sim3_simulation_adapter.hpp", "christeye/living/colony_runner.hpp"),
    ("application/sim3_experiment_adapter.hpp", "christeye/living/colony_runner.hpp"),
}


def headers(layer):
    return [path for path in (ROOT / layer).rglob("*")
            if path.is_file() and path.suffix in {".h", ".hpp", ".hxx", ".ipp"}]


# Baseline from audited commit 5f5ab33a6668b712c54dc15327a568e77af67a98.
# Remove individual waivers as modules are inverted; do not add new waivers casually.
KNOWN_DEBT = set()  # original inversions removed; new ones are forbidden


def internal_includes(path: Path):
    for match in INCLUDE.finditer(path.read_text(encoding="utf-8")):
        yield match.group(1) + "/" + match.group(2)


class NativeArchitectureTests(unittest.TestCase):
    def test_passive_models(self):
        files = headers("models")
        self.assertTrue(files, "Missing native Anaiero model headers")
        for path in files:
            with self.subTest(path=path.name):
                self.assertFalse(
                    [item for item in internal_includes(path)
                     if not item.startswith("models/")],
                    "Models must not depend on contracts, services or infrastructure",
                )

    def test_contracts_do_not_depend_on_services_or_infrastructure(self):
        files = headers("contracts")
        self.assertTrue(files, "Missing native contracts")
        for path in files:
            with self.subTest(path=path.name):
                self.assertFalse(
                    [item for item in internal_includes(path)
                     if item.startswith(("services/", "infrastructure/", "application/"))],
                    "Contracts are inside the dependency boundary",
                )

    def test_only_known_reverse_dependencies_exist(self):
        observed = set()
        for folder in ("services", "infrastructure"):
            for path in headers(folder):
                for included in internal_includes(path):
                    reverse = included.startswith("application/") or (
                        folder == "services" and included.startswith("infrastructure/")) or (
                        folder == "infrastructure" and included.startswith("services/"))
                    if reverse:
                        observed.add((path.relative_to(ROOT).as_posix(), included))
        self.assertEqual(
            KNOWN_DEBT, observed,
            "Source-layer dependency waivers changed. Reduce by refactoring; "
            "new reverse dependencies require architecture review.",
        )

    def test_legacy_bridges_are_exact_and_current(self):
        observed = set()
        for layer in ("models", "contracts", "services", "infrastructure", "application"):
            for path in headers(layer):
                for included in LEGACY_INCLUDE.findall(path.read_text(encoding="utf-8")):
                    if included in PORTABLE_UTILITIES and layer != "models":
                        continue
                    observed.add((path.relative_to(ROOT).as_posix(), included))
        self.assertEqual(LEGACY_BRIDGES, observed,
                         "Legacy host bridge changed: remove stale entries or review the exact new edge")

    def test_include_scanner_covers_nested_and_angle_forms(self):
        for source in ('#include "anaiero/application/nested/host.hpp"',
                       '# include <anaiero/application/nested/host.hpp>',
                       '  #  include "anaiero/infrastructure/nested/file.h"'):
            self.assertIsNotNone(INCLUDE.search(source))

    def test_os_specific_headers_stay_out_of_portable_layers(self):
        forbidden = ("windows.h", "winsock2.h", "sys/epoll.h",
                     "sys/eventfd.h", "sys/socket.h", "sys/mman.h", "unistd.h")
        for layer in ("models", "contracts", "services"):
            for path in headers(layer):
                for line in path.read_text(encoding="utf-8").splitlines():
                    if not re.match(r"\s*#\s*include\b", line):
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
                self.assertIn(term.casefold(), adr.casefold())

    def test_models_have_no_io_or_transport_includes(self):
        forbidden = ("<filesystem>", "<fstream>", "<thread>", "<sys/socket.h>",
                     '"christeye/wind/', '"anaiero/infrastructure/')
        for path in headers("models"):
            source = path.read_text(encoding="utf-8")
            for bad in forbidden:
                with self.subTest(path=path.name, forbidden=bad):
                    self.assertNotIn(f"#include {bad}", source)

    def test_tls_implementation_headers_stay_in_infrastructure(self):
        for layer in ("models", "contracts", "services", "application"):
            for path in headers(layer):
                with self.subTest(path=path.relative_to(ROOT).as_posix()):
                    self.assertFalse(
                        TLS_INCLUDE.findall(path.read_text(encoding="utf-8")),
                        "TLS library details must stay behind infrastructure adapters",
                    )

    def test_mesh_protocol_has_no_native_mutation_dispatch(self):
        wire = ROOT / "infrastructure/mesh_wire.hpp"
        enum = re.search(r"enum\s+class\s+Command\s*\{([^}]+)\}",
                         wire.read_text(encoding="utf-8"))
        self.assertIsNotNone(enum, "Mesh commands must have an explicit bounded protocol")
        commands = {item.split("=", 1)[0].strip() for item in enum.group(1).split(",")}
        self.assertEqual(
            {"publish", "subscribe", "next", "ack", "artifact", "chunk", "ping"},
            commands,
            "Adding remote operations requires an explicit authorization-boundary review",
        )
        # The wire/gateway include closure must stay independent of the local
        # session mutation, experimental execution and solver-process owners.
        # Local administration and offline export remain separate entry points.
        forbidden = {
            "services/native_session_service.hpp",
            "services/operation_service.hpp",
            "services/experiment_service.hpp",
            "application/native_session_application.hpp",
            "application/supervised_scientific_engine.hpp",
            "infrastructure/native_child_process.hpp",
            "infrastructure/stdio_live_solver.hpp",
        }
        pending = ["infrastructure/mesh_wire.hpp", "application/native_mesh_application.hpp"]
        visited = set()
        while pending:
            relative = pending.pop()
            if relative in visited:
                continue
            visited.add(relative)
            self.assertNotIn(relative, forbidden,
                             "Remote mesh gateway reached a local mutation or process owner")
            path = ROOT / relative
            self.assertTrue(path.is_file(), f"Missing dependency: {relative}")
            pending.extend(internal_includes(path))


if __name__ == "__main__":
    unittest.main()
