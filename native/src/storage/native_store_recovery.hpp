#pragma once
// Post-interruption reconciliation of non-authoritative staging files and
// obsolete derived index directories. Never rewrites canonical ROOT/history.
#include "sealed_index.hpp"

namespace anaiero::storage::recovery {
void recover_staging(const fs::path&,std::string_view store_id,
                     std::uint64_t generation,const Digest& root_digest);
void cleanup_inactive_index(const fs::path&,std::string_view active_directory);
}
