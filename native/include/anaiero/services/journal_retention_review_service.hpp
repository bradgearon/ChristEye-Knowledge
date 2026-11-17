#pragma once
// Non-destructive retention gate: never prune unacknowledged evidence.
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::engine {
struct RetentionDecision {
  bool eligible=false;
  std::uint64_t safe_through=0;
};
class JournalRetentionReviewService final {
public:
  template<std::size_t MaxReaders>
  RetentionDecision review(std::uint64_t last_snapshot_event,
      const std::array<std::uint64_t,MaxReaders>& acknowledged_cursors,
      std::size_t readers) const noexcept {
    RetentionDecision result{};
    if(!last_snapshot_event||!readers||readers>MaxReaders)return result;
    auto oldest=last_snapshot_event;
    for(std::size_t i=0;i<readers;++i){
      if(acknowledged_cursors[i]==0)return result;
      if(acknowledged_cursors[i]<oldest)oldest=acknowledged_cursors[i];
    }
    // A snapshot is required before any candidate event is considered
    // for future archival; this service does not delete data.
    result.eligible=true;result.safe_through=oldest;return result;
  }
};
} // namespace anaiero::engine
