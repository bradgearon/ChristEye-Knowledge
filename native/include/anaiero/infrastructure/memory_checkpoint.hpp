#pragma once
#include "anaiero/contracts/simulation_engine.hpp"
#include <string>
#include <string_view>
namespace anaiero::infrastructure {
class MemoryCheckpoint final:public engine::ICheckpointWriter,
                             public engine::ICheckpointReader {
  std::string bytes_{};
  std::size_t max_bytes_=1024*1024;
public:
  explicit MemoryCheckpoint(std::size_t capacity=1024*1024) noexcept:
    max_bytes_(capacity){}
  bool write(std::string_view bytes) noexcept override {
    if(bytes.size()>max_bytes_)return false;
    bytes_.assign(bytes);return true;
  }
  std::string_view bytes()const noexcept override{return bytes_;}
  bool empty()const noexcept{return bytes_.empty();}
};
} // namespace anaiero::infrastructure
