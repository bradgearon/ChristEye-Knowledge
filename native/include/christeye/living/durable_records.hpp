#pragma once
// Versioned records shared by legacy and segmented native stores.
// Not an authenticated network transport, scientific proof or biological driver.
#include "christeye/living/durable_io.hpp"
#include "christeye/living/plan_hash.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace christeye::living::durable {
namespace fs=std::filesystem;
namespace pl=christeye::living::plans;
inline constexpr std::size_t kMaxObjectBytes=4*1024*1024;
inline constexpr std::size_t kMaxJournalBytes=4*1024*1024;
inline constexpr std::size_t kMaxEvents=4096;
inline constexpr std::string_view kZeroDigest="0000000000000000000000000000000000000000000000000000000000000000";
enum class Code : std::uint8_t {
  ok, locked, io_error, corrupt_journal, bad_head, invalid_argument,
  missing_object, bad_digest, capacity, not_open, injected_fault, invalid_cursor
};
struct Result {Code code=Code::ok;std::string hash;};
struct Event {
  std::uint64_t sequence=0;
  std::string previous_hash,kind,entity,object_hash,origin;
  std::uint64_t source_tick=0;
  std::string clock,producer,hash;
};
struct Cursor {Code code=Code::ok;std::uint64_t sequence=0;std::string hash=std::string(kZeroDigest);};
enum class FaultSite : std::uint8_t {none, after_journal_sync, after_head_temp_sync, after_head_commit,
  after_segment_prepare, after_rotation_commit, after_checkpoint_commit,
  after_archive_prepare, after_archive_commit, after_object_archive};
inline bool valid_digest(std::string_view hash) noexcept {
  if(hash.size()!=64)return false;
  for(char c:hash)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline bool valid_id(std::string_view id) noexcept {
  if(id.empty()||id.size()>80)return false;
  for(char c:id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                      (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
  return true;
}
inline bool valid_kind(std::string_view k) noexcept {
  return k=="PLAN_REGISTERED"||k=="RNA_OBSERVATION"||k=="LEARNING_RESULT"||
         k=="SUPPORT_OBSERVATION"||k=="CRYSTAL_EVIDENCE"||k=="SUCCESSOR_PROPOSED"||
         k=="CULTURE_CHECKPOINT"||k=="VLM_OVERLAY_IMPORTED"||k=="MODEL_EVALUATION"||
         k=="DIGITAL_ROUTE_RECEIPT"||k=="OBSERVATION_RECORDED"||
         k=="SCIENTIFIC_MODEL_SESSION"||k=="MESH_INGRESS_RECEIPT";
}
inline bool valid_origin(std::string_view o) noexcept {
  return o=="SIMULATED"||o=="INFERRED"||o=="IMPORTED_UNVERIFIED";
}
inline bool parse_u64(std::string_view s,std::uint64_t& out) noexcept {
  auto r=std::from_chars(s.data(),s.data()+s.size(),out);
  return r.ec==std::errc{}&&r.ptr==s.data()+s.size();
}
inline bool read_bounded(const fs::path& path,std::size_t limit,std::string& out) {
  std::error_code ec;
  if(fs::is_symlink(path,ec)||ec)return false;
  const auto n=fs::file_size(path,ec);
  if(ec||n>limit||n>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))return false;
  std::ifstream file(path,std::ios::binary);
  if(!file)return false;
  // Reading to EOF after a size check would allow a growing file to defeat the
  // memory bound. Read exactly that checked size and reject a trailing byte.
  out.resize(static_cast<std::size_t>(n));
  file.read(out.data(),static_cast<std::streamsize>(n));
  if(file.gcount()!=static_cast<std::streamsize>(n)||file.bad())return false;
  return file.peek()==std::char_traits<char>::eof()&&!file.bad();
}
inline std::string event_core(const Event& e) {
  std::ostringstream out;
  out<<"event "<<e.sequence<<' '<<e.previous_hash<<' '<<e.kind<<' '<<e.entity<<' '
     <<e.object_hash<<' '<<e.origin<<' '<<e.source_tick<<' '<<e.clock<<' '<<e.producer;
  return out.str();
}
inline std::string event_hash(const Event& e) {
  return pl::sha256_hex("CE/WINDREG/EVENT/1\n"+event_core(e)+'\n');
}
inline std::string serialize_event(const Event& e){return event_core(e)+' '+e.hash+'\n';}
inline bool parse_event(std::string_view text,Event& event) {
  std::istringstream in{std::string(text)};
  std::string tag,seq,tick,extra;
  Event candidate;
  if(!(in>>tag>>seq>>candidate.previous_hash>>candidate.kind>>candidate.entity>>
       candidate.object_hash>>candidate.origin>>tick>>candidate.clock>>candidate.producer>>candidate.hash)||
     (in>>extra)||tag!="event"||!parse_u64(seq,candidate.sequence)||
     !parse_u64(tick,candidate.source_tick)||!valid_digest(candidate.previous_hash)||
     !valid_digest(candidate.object_hash)||!valid_digest(candidate.hash)||
     !valid_kind(candidate.kind)||!valid_id(candidate.entity)||!valid_origin(candidate.origin)||
     !valid_id(candidate.clock)||!valid_id(candidate.producer)||event_hash(candidate)!=candidate.hash)return false;
  event=std::move(candidate);return true;
}
struct Head {std::uint64_t offset=0,sequence=0;std::string hash=std::string(kZeroDigest);};
inline std::string head_body(const Head& head) {
  return "CEWHEAD/1\noffset "+std::to_string(head.offset)+"\nsequence "+
         std::to_string(head.sequence)+"\nlast "+head.hash+'\n';
}
inline std::string sealed_head(const Head& head) {
  const auto body=head_body(head);
  return body+"seal "+pl::sha256_hex(body)+'\n';
}
inline bool parse_head(std::string_view data,Head& head) {
  std::istringstream in{std::string(data)};
  std::string magic,k1,k2,k3,k4,off,seq,digest,seal,extra;
  if(!(in>>magic>>k1>>off>>k2>>seq>>k3>>digest>>k4>>seal)||
     (in>>extra)||magic!="CEWHEAD/1"||k1!="offset"||k2!="sequence"||
     k3!="last"||k4!="seal"||!parse_u64(off,head.offset)||
     !parse_u64(seq,head.sequence)||!valid_digest(digest)||!valid_digest(seal))return false;
  head.hash=digest;
  return sealed_head(head)==data;
}
inline std::string cursor_body(std::uint64_t seq,std::string_view hash) {
  return "CEWCURSOR/1\nsequence "+std::to_string(seq)+"\nlast "+std::string(hash)+'\n';
}
inline std::string sealed_cursor(std::uint64_t seq,std::string_view hash) {
  auto body=cursor_body(seq,hash);return body+"seal "+pl::sha256_hex(body)+'\n';
}
inline bool parse_cursor(std::string_view text,Cursor& cursor) {
  std::istringstream in{std::string(text)};
  std::string magic,k1,k2,k3,seq,hash,seal,extra;
  if(!(in>>magic>>k1>>seq>>k2>>hash>>k3>>seal)||
     (in>>extra)||magic!="CEWCURSOR/1"||k1!="sequence"||k2!="last"||
     k3!="seal"||!parse_u64(seq,cursor.sequence)||!valid_digest(hash)||
     !valid_digest(seal)||sealed_cursor(cursor.sequence,hash)!=text)return false;
  cursor.hash=hash;return true;
}
} // namespace christeye::living::durable
