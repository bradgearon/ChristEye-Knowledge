#pragma once
#include <filesystem>
#include <string_view>
namespace anaiero::qualification {
inline std::filesystem::path native_path(std::string_view value){
  return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()),value.size()));
}
}
