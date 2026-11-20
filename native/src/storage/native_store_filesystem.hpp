#pragma once
// Internal native filesystem operations: capacity admission, safe directories
// and physical index coexistence inventory. No canonical record parsing.
#include "sealed_index.hpp"
#include "anaiero/sdk/storage_tools.hpp"

namespace anaiero::storage::fsops {
void require_file_capacity(const fs::path&,std::uint64_t temporary,std::uint64_t reserve);
void measure_coexistence(const fs::path& store,const fs::path& old_index,
                         const fs::path& new_index,sdk::evidence::CompactionReport&);
void directory(const fs::path&);
}
