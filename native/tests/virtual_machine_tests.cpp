#include "christeye/living/virtual_machine.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace christeye::living;
namespace vlm = christeye::living::vlm;
#define REQUIRE(expr) do { if (!(expr)) { std::cerr << "FAIL: " #expr << " line " << __LINE__ << '\n'; std::exit(1); } } while (false)

static constexpr const char* kTemplate =
"# editable synthetic profile\n"
"CEVLM/1\n"
"name crest-garden\n"
"model 1\n"
"image\n"
"spawn 1 crest_progenitor 10 12 0\n"
"spawn 2 blood_carrier 3 5 0\n"
"spawn 3 neural_progenitor 16 12 0\n"
"spawn 4 neural_progenitor 19 15 0\n"
"differentiate 1 peripheral_neuron\n"
"differentiate 3 central_neuron\n"
"differentiate 4 astrocyte\n"
"molecule 100 support 85 1\n"
"molecule 101 guidance_proxy 60 1\n"
"molecule 102 rna_proxy 90 1\n"
"deposit support 2 2 400\n"
"deposit rna 2 2 150\n"
"deposit soil 2 2 100\n"
"connect 1 3 20\n"
"teach 3 17 25\n"
"endimage\n"
"journal\n"
"endjournal\n";

static vlm::VirtualMachine make_vm() {
  vlm::VirtualMachine vm;
  const auto result = vlm::VirtualMachine::deserialize(kTemplate, vm);
  REQUIRE(result.result == vlm::Result::ok);
  REQUIRE(vm.image().size() == 15);
  REQUIRE(vm.journal().empty());
  REQUIRE(vm.core().metrics().cells == 4);
  REQUIRE(vm.core().metrics().links == 1);
  REQUIRE(vm.soil().total(vlm::FieldKind::support) == 400);
  return vm;
}
static void test_round_trip_and_movement() {
  auto vm = make_vm();
  REQUIRE(vm.advance(10) == vlm::Result::ok);
  REQUIRE(vm.core().tick() == 10);
  REQUIRE(vm.core().cell(2)->x == 13);
  REQUIRE(vm.soil().total(vlm::FieldKind::support) == 400);
  REQUIRE(vm.soil().at(vlm::FieldKind::support, 2, 2) < 400);
  REQUIRE(vm.soil().at(vlm::FieldKind::support, 3, 2) > 0);
  REQUIRE(vm.core().cell(3)->neurite_proxy > 0);
  REQUIRE(vm.core().cell(3)->rna_proxy > 0);
  REQUIRE(vm.core().cell(3)->learning_proxy == 25);
  auto packed = vm.serialize(true);
  vlm::VirtualMachine restored;
  REQUIRE(vlm::VirtualMachine::deserialize(packed, restored, true).result == vlm::Result::ok);
  REQUIRE(vm.state_hash() == restored.state_hash());
  REQUIRE(vm.core().state_hash() == restored.core().state_hash());
  REQUIRE(vm.core().event_chain() == restored.core().event_chain());
  REQUIRE(vm.soil().hash() == restored.soil().hash());
  REQUIRE(vm.serialize(true) == restored.serialize(true));
  REQUIRE(restored.advance(1) == vlm::Result::ok);
  REQUIRE(vm.advance(1) == vlm::Result::ok);
  REQUIRE(restored.state_hash() == vm.state_hash());
}
static void test_clone_and_fork() {
  auto running = make_vm();
  REQUIRE(running.advance(12) == vlm::Result::ok);
  vlm::VirtualMachine child;
  REQUIRE(running.clone_image_to(child, "new-instance") == vlm::Result::ok);
  REQUIRE(child.name() == "new-instance");
  REQUIRE(child.core().tick() == 0);
  REQUIRE(child.journal().empty());
  REQUIRE(child.core().metrics().cells == running.core().metrics().cells);
  auto fork = running;
  REQUIRE(fork.set_name("forked-instance"));
  REQUIRE(fork.state_hash() == running.state_hash());
  REQUIRE(fork.advance(1) == vlm::Result::ok);
  REQUIRE(fork.state_hash() != running.state_hash());
}
static void test_negative_files_are_atomic() {
  vlm::VirtualMachine sentinel = make_vm();
  const auto old = sentinel.state_hash();
  std::string corrupt = sentinel.serialize(true);
  auto p = corrupt.find("spawn 2 blood_carrier");
  REQUIRE(p != std::string::npos);
  corrupt.replace(p, 5, "teach");
  REQUIRE(vlm::VirtualMachine::deserialize(corrupt, sentinel, true).result != vlm::Result::ok);
  REQUIRE(sentinel.state_hash() == old);
  corrupt = sentinel.serialize(true);
  p = corrupt.find("seal ");
  REQUIRE(p != std::string::npos);
  corrupt[p + 6] = 'f';
  REQUIRE(vlm::VirtualMachine::deserialize(corrupt, sentinel, true).result == vlm::Result::checksum_mismatch);
  REQUIRE(sentinel.state_hash() == old);
  auto invalid = std::string(kTemplate);
  p = invalid.find("name crest-garden");
  REQUIRE(p != std::string::npos);
  invalid.replace(p, std::string("name crest-garden").size(), "name ../unsafe");
  REQUIRE(vlm::VirtualMachine::deserialize(invalid, sentinel).result == vlm::Result::invalid_setup);
  REQUIRE(sentinel.state_hash() == old);
  REQUIRE(vlm::VirtualMachine::deserialize(kTemplate, sentinel, true).result == vlm::Result::invalid_setup);
  REQUIRE(sentinel.state_hash() == old);
  invalid = std::string(kTemplate);
  invalid.replace(invalid.find("model 1"), 7, "model 99");
  REQUIRE(vlm::VirtualMachine::deserialize(invalid, sentinel).result == vlm::Result::model_incompatible);
  REQUIRE(sentinel.state_hash() == old);
}
static void test_fields_are_conservative() {
  vlm::FieldGrid grid;
  REQUIRE(grid.deposit(vlm::FieldKind::protein, 0, 0, 9999));
  REQUIRE(!grid.deposit(vlm::FieldKind::protein, 0, 0, 100000));
  REQUIRE(!grid.deposit(vlm::FieldKind::support, 8, 1, 5));
  REQUIRE(!grid.deposit(vlm::FieldKind::support, 1, 1, -5));
  for (int i=0;i<100;++i) {
    grid.step();
    REQUIRE(grid.total(vlm::FieldKind::protein) == 9999);
  }
  REQUIRE(grid.at(vlm::FieldKind::protein, 1, 0) > 0);
}
static void test_disk_io() {
  auto vm = make_vm();
  REQUIRE(vm.advance(3) == vlm::Result::ok);
  const auto path = std::filesystem::temp_directory_path() / "christeye-vlm1-test.avlm";
  REQUIRE(vlm::write_file(path, vm) == vlm::Result::ok);
  vlm::VirtualMachine loaded;
  REQUIRE(vlm::read_file(path, loaded, true).result == vlm::Result::ok);
  REQUIRE(loaded.state_hash() == vm.state_hash());
  std::error_code ec;
  std::filesystem::remove(path, ec);
}
int main() {
  test_round_trip_and_movement();
  test_clone_and_fork();
  test_negative_files_are_atomic();
  test_fields_are_conservative();
  test_disk_io();
  std::cout << "VLM1 image/journal/soil/clone/replay tests passed\n";
}
