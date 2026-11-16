#pragma once
// CE-SW-19: passive data types; no biological actuation or growth recipes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace anaiero::cells {
enum class Kind : std::uint8_t {
  crest_progenitor=1, blood_carrier, neural_progenitor,
  peripheral_neuron, central_neuron, astrocyte, schwann, microglia, endothelial
};
enum class Stage : std::uint8_t { candidate=0, developing=1, integrated=2, historical=3 };
inline constexpr std::size_t kKinds=9;
inline constexpr std::size_t kMaxCells=64;
inline constexpr std::size_t kMaxLinks=128;
inline constexpr std::size_t kMaxIntents=32;
inline constexpr std::int32_t kValueLimit=1000;
struct Vec3 { std::int32_t x=0,y=0,z=0; };
struct Parameters {
  Kind kind=Kind::crest_progenitor;
  std::uint16_t maintenance=0, support_response=0, maturation=0;
  std::uint16_t process_extension=0, activity_response=0, plasticity=0;
  std::uint16_t mobility=0, contact_radius=0, stress_response=0;
};
struct Atlas {
  std::string name="synthetic-atlas";
  std::string model_digest, reference_digest, reference_file, reference_format="SYNTHETIC";
  std::array<Parameters,kKinds> by_kind{};
  std::uint32_t revision=1;
  bool synthetic_only=true;
};
struct Cell {
  std::uint32_t id=0,parent_id=0;
  Kind kind=Kind::crest_progenitor;
  Stage stage=Stage::candidate;
  Vec3 position{};
  std::int32_t energy=100,maturity=0,process_proxy=0;
  std::int32_t activity_proxy=0,learning_proxy=0;
  std::uint64_t birth_tick=0,death_tick=0;
  bool viable=true;
};
struct Link {
  std::uint32_t from=0,to=0;
  std::uint16_t strength=0;
  std::uint64_t established_tick=0;
};
struct RouteIntent {
  std::uint32_t from=0,to=0;
  std::uint16_t guidance_weight=0; // digital bias only; not physical routing
};
struct Context {
  std::int32_t support_proxy=0,stress_proxy=0,digital_signal=0;
  bool lesson_active=false;
};
struct World {
  std::uint64_t tick=0;
  std::array<char,65> run_plan_digest{}; // fixed-capacity timeline identity
  std::uint16_t cell_count=0,link_count=0,intent_count=0;
  std::array<Cell,kMaxCells> cells{};
  std::array<Link,kMaxLinks> links{};
  std::array<RouteIntent,kMaxIntents> intents{};
  std::uint64_t generation=0;
};
struct Metrics {
  std::uint32_t viable=0,historical=0,linked=0;
  std::int64_t total_energy=0,total_process=0,total_activity=0,total_learning=0;
};
}  // namespace anaiero::cells
