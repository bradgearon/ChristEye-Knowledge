#pragma once
// Native operator I/O; no session policy, checkpoint or presentation state.
#include "anaiero/sdk/evidence.hpp"
#include <filesystem>
namespace anaiero::operator_io {
bool read_file(const std::filesystem::path&,std::string&,std::size_t maximum=8U*1024U*1024U);
anaiero::sdk::Result stored_budget(anaiero::sdk::evidence::ICommitStore&,
                                  std::uint64_t& maximum,bool& exists);
bool canonical_root_exists(const std::filesystem::path&);
std::uint64_t now();
std::filesystem::path executable(const char* argv0);
}
