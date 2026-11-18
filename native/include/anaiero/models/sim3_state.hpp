#pragma once
// Passive SIM3 model records shared by durable and isolated trial execution.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace anaiero::models::sim3 {
inline constexpr std::size_t kMaxCells=128;
enum class Result : std::uint8_t {
  ok, bad_config, bad_hash, invalid_state, locked, io_error,
  corruption, capacity, uncommitted, already_open
};
enum class Channel : std::uint8_t {food=1,data=2,stimubus=3};
enum class ProcessKind : std::uint8_t {support=1,teach=2,divide=3,foreign_stress=4};
struct ModelLaws {
  std::uint32_t version=1;
  std::int32_t food_per_cell=1, fed_gain=1, unfed_cost=7, upkeep_cost=1;
  std::int32_t stress_cutoff=20, stress_cost=4, maturity_gain=1;
  std::int32_t blood_step=1, learning_gain=1;
};
struct ColonyConfig {std::string id, source_path, source_sha;};
struct Seed {std::string colony, kind;std::uint32_t id=0,parent=0;
  std::int32_t x=0,y=0,z=0;};
struct Link {std::string from,to;Channel channel=Channel::food;std::uint32_t units=0;};
struct Process {std::string colony;ProcessKind kind=ProcessKind::support;
  std::uint32_t subject=0,value=0,at=0;};
struct ConfigurationData {
  std::string name, material_role, material_path, material_sha, hash;
  ModelLaws laws{};
  std::vector<ColonyConfig> colonies;std::vector<Seed> seeds;
  std::vector<Link> links;std::vector<Process> processes;
};
struct Colony {std::string id;std::int32_t food=80,data=0,signal=0,stress=0;};
struct Cell {std::string colony,kind;std::uint32_t id=0,parent=0;
  std::int32_t x=0,y=0,z=0,energy=40,maturity=0,learning=0;
  std::uint64_t birth=0,death=0;bool alive=true;};
struct Snapshot {std::string config_hash;std::uint64_t tick=0;
  std::vector<Colony> colonies;std::vector<Cell> cells;};
} // namespace anaiero::models::sim3
