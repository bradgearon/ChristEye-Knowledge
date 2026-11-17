#pragma once
// CE-SW-11 SW2.1: host-only, single-writer committed-head evidence journal.
// Not an authenticated network transport, scientific proof or biological driver.
#include "christeye/living/durable_io.hpp"
#include "christeye/living/plan_hash.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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
enum class FaultSite : std::uint8_t {none, after_journal_sync, after_head_temp_sync, after_head_commit};
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
         k=="DIGITAL_ROUTE_RECEIPT";
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
  if(ec||n>limit)return false;
  std::ifstream file(path,std::ios::binary);
  if(!file)return false;
  out.assign(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
  return !file.bad()&&out.size()==n;
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
class Registry final {
  fs::path root_{};
  os::File writer_lock_{};
  std::vector<Event> events_{};
  std::uint64_t committed_bytes_=0;
  bool open_=false;
  using Callback=void(*)(void*,const Event&) noexcept;
  struct Subscriber {Callback cb;void* context;std::string entity,kind;};
  std::vector<Subscriber> subscriptions_{};
#ifdef CE_DURABLE_FAULT_TESTS
  FaultSite next_fault_=FaultSite::none;
#endif
  fs::path journal_path() const {return root_/"events.log";}
  fs::path head_path() const {return root_/"HEAD";}
  fs::path object_path(std::string_view hash) const{return root_/"objects"/std::string(hash);}
  bool object_good(std::string_view hash) const {
    std::string data;
    return valid_digest(hash)&&read_bounded(object_path(hash),kMaxObjectBytes,data)&&
           pl::sha256_hex(data)==hash;
  }
  Code replay(std::string_view data,std::vector<Event>& out) const {
    if(!data.starts_with("CEWREG/1\n")||data.back()!='\n')return Code::corrupt_journal;
    std::istringstream lines{std::string(data)};
    std::string line;
    std::getline(lines,line); // version
    std::string previous(kZeroDigest);
    while(std::getline(lines,line)) {
      Event event;
      if(out.size()>=kMaxEvents||!parse_event(line,event)||
         event.sequence!=out.size()+1||event.previous_hash!=previous||
         !object_good(event.object_hash))return Code::corrupt_journal;
      previous=event.hash;out.push_back(std::move(event));
    }
    return Code::ok;
  }
  Code read_head(Head& head) const {
    std::string bytes;
    if(!read_bounded(head_path(),1024,bytes))return Code::io_error;
    return parse_head(bytes,head)?Code::ok:Code::bad_head;
  }
  bool write_head(const Head& head) const {
    return os::atomic_write(head_path(),sealed_head(head));
  }
public:
  Registry()=default;
  Registry(const Registry&)=delete;
  Registry& operator=(const Registry&)=delete;
  void close() noexcept {
    open_=false;events_.clear();subscriptions_.clear();committed_bytes_=0;
    writer_lock_.close();root_.clear();
  }
  ~Registry(){close();}
  Code open(const fs::path& root) {
    close();root_=root;
    std::error_code ec;
    fs::create_directories(root_/"objects",ec);
    if(ec)return Code::io_error;
    fs::create_directories(root_/"cursors",ec);
    if(ec)return Code::io_error;
    if(fs::is_symlink(root_/"objects",ec)||fs::is_symlink(root_/"cursors",ec))return Code::io_error;
    if(fs::is_symlink(root_/"writer.lock",ec)||fs::is_symlink(journal_path(),ec)||
       fs::is_symlink(head_path(),ec))return Code::io_error;
    if(!writer_lock_.open(root_/"writer.lock"))return Code::io_error;
    if(!writer_lock_.lock_exclusive())return Code::locked;
    if(!fs::exists(journal_path(),ec)){
      if(!os::atomic_write(journal_path(),"CEWREG/1\n"))return Code::io_error;
    }
    std::string journal;
    if(!read_bounded(journal_path(),kMaxJournalBytes,journal))return Code::io_error;
    Head head{};
    if(fs::exists(head_path(),ec)) {
      const Code hc=read_head(head);
      if(hc!=Code::ok)return hc;
      if(head.offset>journal.size()||head.offset<9)return Code::corrupt_journal;
    } else {
      // Strict import of an existing complete legacy CEWREG/1 store. Cannot
      // guess which bytes of a truncated legacy record had been acknowledged.
      head.offset=journal.size();
    }
    std::vector<Event> verified;
    const auto result=replay(std::string_view(journal).substr(0,static_cast<std::size_t>(head.offset)),verified);
    if(result!=Code::ok)return result;
    const auto last=verified.empty()?std::string(kZeroDigest):verified.back().hash;
    if(fs::exists(head_path(),ec) &&
       (head.sequence!=verified.size()||head.hash!=last))return Code::corrupt_journal;
    if(!fs::exists(head_path(),ec)) {
      head.sequence=verified.size();head.hash=last;
      if(!write_head(head))return Code::io_error;
    }
    if(journal.size()>head.offset){
      // Only bytes beyond the last atomically committed HEAD may be discarded.
      if(!os::truncate_synced(journal_path(),head.offset))return Code::io_error;
    }
    events_=std::move(verified);committed_bytes_=head.offset;open_=true;
    return Code::ok;
  }
  bool is_open() const noexcept {return open_;}
  std::size_t event_count() const noexcept {return events_.size();}
  const std::vector<Event>& events() const noexcept {return events_;}
  Result put(std::string_view data) {
    if(!open_)return {Code::not_open,{}};
    if(data.size()>kMaxObjectBytes)return {Code::capacity,{}};
    const auto hash=pl::sha256_hex(data);
    const auto path=object_path(hash);
    std::error_code ec;
    if(fs::exists(path,ec))return object_good(hash)?Result{Code::ok,hash}:Result{Code::bad_digest,{}};
    if(ec||!os::atomic_write(path,data))return {Code::io_error,{}};
    return object_good(hash)?Result{Code::ok,hash}:Result{Code::bad_digest,{}};
  }
  bool get(std::string_view hash,std::string& out) const {
    return open_&&object_good(hash)&&read_bounded(object_path(hash),kMaxObjectBytes,out);
  }
  // Generic CEVPLAN contract: validated plan, pinned artifacts and immutable
  // plan object become visible to subscribers only after an event HEAD commit.
  // Unreferenced objects after interruption are safe CAS orphans, not evidence.
  template<class Plan>
  Result register_plan(const Plan& plan,const fs::path& artifact_root) {
    if(!open_)return {Code::not_open,{}};
    if(static_cast<unsigned>(plan.validate())!=0||
       static_cast<unsigned>(plan.verify_assets(artifact_root))!=0)
      return {Code::invalid_argument,{}};
    std::error_code ec;
    const auto canonical_root=fs::canonical(artifact_root,ec);
    if(ec)return {Code::io_error,{}};
    for(const auto& artifact:plan.artifacts()) {
      const auto target=fs::canonical(canonical_root/artifact.relative_path,ec);
      if(ec)return {Code::io_error,{}};
      const auto relative=target.lexically_relative(canonical_root);
      if(relative.empty()||relative.is_absolute())return {Code::invalid_argument,{}};
      for(const auto& part:relative)if(part=="..")return {Code::invalid_argument,{}};
      std::string content;
      if(!read_bounded(target,kMaxObjectBytes,content)||
         pl::sha256_hex(content)!=artifact.sha256)return {Code::bad_digest,{}};
      auto stored=put(content);
      if(stored.code!=Code::ok)return stored;
    }
    const auto object=put(plan.serialize(true));
    if(object.code!=Code::ok)return object;
    const auto announcement=publish("PLAN_REGISTERED",plan.name(),object.hash,
                                     "SIMULATED",0,"plan-revision");
    return announcement.code==Code::ok?object:announcement;
  }
  Result publish(std::string kind,std::string entity,std::string object_hash,
                 std::string origin,std::uint64_t tick,std::string clock,
                 std::string producer="local-simulator") {
    if(!open_)return {Code::not_open,{}};
    if(!valid_kind(kind)||!valid_id(entity)||!valid_digest(object_hash)||
       !valid_origin(origin)||!valid_id(clock)||!valid_id(producer))return {Code::invalid_argument,{}};
    if(!object_good(object_hash))return {Code::missing_object,{}};
    for(const auto& e:events_)
      if(e.kind==kind&&e.entity==entity&&e.object_hash==object_hash&&e.origin==origin&&
         e.source_tick==tick&&e.clock==clock&&e.producer==producer)return {Code::ok,e.hash};
    if(events_.size()>=kMaxEvents)return {Code::capacity,{}};
    Event e{events_.size()+1,events_.empty()?std::string(kZeroDigest):events_.back().hash,
            std::move(kind),std::move(entity),std::move(object_hash),std::move(origin),
            tick,std::move(clock),std::move(producer),{}};
    e.hash=event_hash(e);
    const auto record=serialize_event(e);
    if(committed_bytes_+record.size()>kMaxJournalBytes)return {Code::capacity,{}};
    if(!os::append_synced(journal_path(),record)){open_=false;return {Code::io_error,{}};}
#ifdef CE_DURABLE_FAULT_TESTS
    if(next_fault_==FaultSite::after_journal_sync){open_=false;return {Code::injected_fault,{}};}
#endif
    const Head next{committed_bytes_+record.size(),e.sequence,e.hash};
#ifdef CE_DURABLE_FAULT_TESTS
    if(next_fault_==FaultSite::after_head_temp_sync){
      os::File f;
      const auto tmp=head_path().string()+".tmp";
      if(!f.open(tmp,true)||!f.write_all(sealed_head(next))||!f.sync())return {Code::io_error,{}};
      open_=false;return {Code::injected_fault,{}};
    }
#endif
    if(!write_head(next)){open_=false;return {Code::io_error,{}};}
#ifdef CE_DURABLE_FAULT_TESTS
    if(next_fault_==FaultSite::after_head_commit){open_=false;return {Code::injected_fault,{}};}
#endif
    committed_bytes_=next.offset;events_.push_back(e);
    for(const auto& sub:subscriptions_)
      if((sub.entity.empty()||sub.entity==e.entity)&&
         (sub.kind.empty()||sub.kind==e.kind))sub.cb(sub.context,events_.back());
    return {Code::ok,e.hash};
  }
  Code subscribe(Callback cb,void* ctx,std::uint64_t after=0,
                 std::string entity={},std::string kind={}) {
    if(!open_)return Code::not_open;
    if(!cb||after>events_.size()||subscriptions_.size()>=32||
       (!entity.empty()&&!valid_id(entity))||(!kind.empty()&&!valid_kind(kind)))
      return Code::invalid_argument;
    for(const auto& e:events_)
      if(e.sequence>after&&(entity.empty()||entity==e.entity)&&
         (kind.empty()||kind==e.kind))cb(ctx,e);
    subscriptions_.push_back({cb,ctx,std::move(entity),std::move(kind)});
    return Code::ok;
  }
  Cursor read_cursor(std::string_view consumer) const {
    if(!open_)return {Code::not_open};
    if(!valid_id(consumer))return {Code::invalid_cursor};
    const auto path=root_/"cursors"/(std::string(consumer)+".cursor");
    std::error_code ec;
    if(!fs::exists(path,ec))return {Code::ok,0,std::string(kZeroDigest)};
    if(ec)return {Code::io_error};
    Cursor c;
    std::string data;
    if(!read_bounded(path,1024,data)||!parse_cursor(data,c))return {Code::invalid_cursor};
    if(c.sequence>events_.size()||
       c.hash!=(c.sequence?events_[static_cast<std::size_t>(c.sequence-1)].hash:kZeroDigest))
      return {Code::invalid_cursor};
    return c;
  }
  Code acknowledge(std::string_view consumer,std::uint64_t sequence,std::string_view hash) {
    if(!open_)return Code::not_open;
    if(!valid_id(consumer)||sequence>events_.size()||!valid_digest(hash))return Code::invalid_cursor;
    const auto expected=sequence?events_[static_cast<std::size_t>(sequence-1)].hash:kZeroDigest;
    if(hash!=expected)return Code::invalid_cursor;
    auto old=read_cursor(consumer);
    if(old.code!=Code::ok||sequence<old.sequence)return Code::invalid_cursor;
    if(sequence==old.sequence)return Code::ok;
    const auto file=root_/"cursors"/(std::string(consumer)+".cursor");
    return os::atomic_write(file,sealed_cursor(sequence,hash))?Code::ok:Code::io_error;
  }
  Code resume(Callback cb,void* ctx,std::string_view consumer,
              std::string entity={},std::string kind={}) {
    auto c=read_cursor(consumer);
    return c.code==Code::ok?subscribe(cb,ctx,c.sequence,std::move(entity),std::move(kind)):c.code;
  }
  // Deep audit explicitly rereads committed HEAD, journal and referenced files.
  // Unlike in-memory verify(), this detects changes to events.log after open().
  bool audit_disk() const {
    if(!open_)return false;
    std::string journal;
    Head head;
    if(read_head(head)!=Code::ok||!read_bounded(journal_path(),kMaxJournalBytes,journal)||
       journal.size()!=head.offset||head.offset!=committed_bytes_)return false;
    std::vector<Event> checked;
    if(replay(journal,checked)!=Code::ok||checked.size()!=events_.size()||
       head.sequence!=events_.size())return false;
    return head.hash==(checked.empty()?std::string(kZeroDigest):checked.back().hash);
  }
#ifdef CE_DURABLE_FAULT_TESTS
  void inject_next_publish(FaultSite site) noexcept {next_fault_=site;}
#endif
};
} // namespace christeye::living::durable
