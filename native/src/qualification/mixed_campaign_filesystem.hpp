#pragma once
// Campaign filesystem admission/lock and best-effort JSON cache belong to I/O.
#include "anaiero/sdk/evidence.hpp"
#include "christeye/living/durable_io.hpp"
#include <filesystem>
namespace anaiero::qualification::campaign_io {
sdk::Result read_file(const std::filesystem::path&,std::size_t maximum,sdk::Bytes&);
sdk::Result lease(const std::filesystem::path&,christeye::living::durable::os::File&);
sdk::Result publish_cache(const std::filesystem::path&,std::string_view);
}
