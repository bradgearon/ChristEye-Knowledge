#pragma once
// Legacy bounded store and opt-in segmented store share the public Registry API.
#include "christeye/living/durable_records.hpp"
#include "christeye/living/segmented_store.hpp"
#include <functional>
#include <memory>
#include <span>
namespace christeye::living::durable {
class Registry final {
  std::unique_ptr<SegmentedStore> segmented_;
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
    segmented_.reset();
    open_=false;events_.clear();subscriptions_.clear();committed_bytes_=0;
    writer_lock_.close();root_.clear();
  }
  ~Registry(){close();}
  Code open(const fs::path& root) {
    std::error_code check;
    if(fs::exists(root/"MANIFEST",check))return open(root,StorageOptions::sustained());
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
  Code open(const fs::path& root,StorageOptions options) {
    if(!options.segmented)return open(root);
    close();segmented_=std::make_unique<SegmentedStore>();
    const auto status=segmented_->open(root,options);
    if(status!=Code::ok)segmented_.reset();
    return status;
  }
  bool is_segmented() const noexcept{return segmented_!=nullptr;}
  bool is_open() const noexcept {return segmented_?segmented_->is_open():open_;}
  std::size_t event_count() const noexcept {return segmented_?segmented_->event_count():events_.size();}
  // Segmented mode exposes only the bounded active tail. Use visit_events for history.
  const std::vector<Event>& events() const noexcept {return segmented_?segmented_->events():events_;}
  StorageCapacity capacity()const noexcept {
    if(segmented_)return segmented_->capacity();
    std::error_code ec;const auto space=fs::space(root_,ec);
    return {false,events_.size(),kMaxEvents,kMaxJournalBytes,events_.size(),committed_bytes_,ec?0:space.available,0,0,0};
  }
  bool can_append(std::size_t count,std::size_t bytes)const noexcept {
    return segmented_?segmented_->can_append(count,bytes):open_&&count<=kMaxEvents-events_.size()&&bytes<=kMaxJournalBytes-committed_bytes_;
  }
  bool reserve(std::size_t count,std::size_t bytes,StorageReservation& r)noexcept {
    return segmented_&&segmented_->reserve(count,bytes,r);
  }
  bool release(StorageReservation& r)noexcept{return segmented_&&segmented_->release(r);}
  bool visit_events(std::uint64_t after,const std::function<bool(const Event&)>& fn)const {
    if(segmented_)return segmented_->visit_events(after,fn);
    if(!open_||after>events_.size()||!fn)return false;
    for(const auto& e:events_)if(e.sequence>after&&!fn(e))return false;
    return true;
  }
  bool latest_event(std::string_view kind,std::string_view entity,std::string_view clock,
                    Event& out,bool& found,std::string_view producer={})const {
    if(segmented_)return segmented_->latest_event(kind,entity,clock,out,found,producer);
    found=false;if(!open_)return false;
    for(auto i=events_.rbegin();i!=events_.rend();++i)
      if(i->kind==kind&&i->entity==entity&&i->clock==clock&&(producer.empty()||i->producer==producer)){out=*i;found=true;break;}
    return true;
  }
  bool find_event(std::string_view kind,std::string_view entity,std::string_view clock,
                  std::string_view producer,std::uint64_t tick,Event& out,bool& found)const {
    if(segmented_)return segmented_->find_event(kind,entity,clock,producer,tick,out,found);
    found=false;if(!open_)return false;
    for(const auto& e:events_)if(e.kind==kind&&e.entity==entity&&e.clock==clock&&e.producer==producer&&e.source_tick==tick){out=e;found=true;}
    return true;
  }
  Result put_referenced(std::string_view bytes,std::span<const std::string> refs){
    if(segmented_)return segmented_->put_referenced(bytes,refs);
    for(const auto& ref:refs)if(!object_good(ref))return {Code::missing_object,{}};
    return put(bytes);
  }
  bool save_replay_checkpoint(std::string_view bytes,std::span<const std::string> refs={}){
    return segmented_&&segmented_->save_replay_checkpoint(bytes,refs);
  }
  bool load_replay_checkpoint(ReplayCheckpoint& record,std::string& bytes,bool& found)const {
    if(segmented_)return segmented_->load_replay_checkpoint(record,bytes,found);
    found=false;return open_;
  }
  Code archive_ready(){return segmented_?segmented_->archive_ready():Code::invalid_argument;}
  Code collect_objects(ArchiveStats& stats){return segmented_?segmented_->collect_objects(stats):Code::invalid_argument;}
  Code pin(std::string_view name,std::span<const std::string> refs){return segmented_?segmented_->pin(name,refs):Code::invalid_argument;}
  Code unpin(std::string_view name){return segmented_?segmented_->unpin(name):Code::invalid_argument;}
  Result put(std::string_view data) {
    if(segmented_)return segmented_->put(data);
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
    if(segmented_)return segmented_->get(hash,out);
    return open_&&object_good(hash)&&read_bounded(object_path(hash),kMaxObjectBytes,out);
  }
  // Generic CEVPLAN contract: validated plan, pinned artifacts and immutable
  // plan object become visible to subscribers only after an event HEAD commit.
  // Unreferenced objects after interruption are safe CAS orphans, not evidence.
  template<class Plan>
  Result register_plan(const Plan& plan,const fs::path& artifact_root) {
    if(!is_open())return {Code::not_open,{}};
    std::vector<std::string> references;
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
      references.push_back(stored.hash);
    }
    const auto object=put_referenced(plan.serialize(true),references);
    if(object.code!=Code::ok)return object;
    const auto announcement=publish("PLAN_REGISTERED",plan.name(),object.hash,
                                     "SIMULATED",0,"plan-revision");
    return announcement.code==Code::ok?object:announcement;
  }
  Result publish(std::string kind,std::string entity,std::string object_hash,
                 std::string origin,std::uint64_t tick,std::string clock,
                 std::string producer="local-simulator") {
    if(segmented_){
      const auto before=segmented_->event_count();
      const auto result=segmented_->publish(std::move(kind),std::move(entity),std::move(object_hash),std::move(origin),tick,std::move(clock),std::move(producer));
      if(result.code==Code::ok&&segmented_->event_count()>before){
        const auto& e=segmented_->events().back();
        for(const auto& sub:subscriptions_)if((sub.entity.empty()||sub.entity==e.entity)&&(sub.kind.empty()||sub.kind==e.kind))sub.cb(sub.context,e);
      }
      return result;
    }
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
    if(!is_open())return Code::not_open;
    if(!cb||after>event_count()||subscriptions_.size()>=32||
       (!entity.empty()&&!valid_id(entity))||(!kind.empty()&&!valid_kind(kind)))
      return Code::invalid_argument;
    if(!visit_events(after,[&](const Event& e){
      if((entity.empty()||entity==e.entity)&&(kind.empty()||kind==e.kind))cb(ctx,e);
      return true;
    }))return Code::corrupt_journal;
    subscriptions_.push_back({cb,ctx,std::move(entity),std::move(kind)});
    return Code::ok;
  }
  Cursor read_cursor(std::string_view consumer) const {
    if(segmented_)return segmented_->read_cursor(consumer);
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
    if(segmented_)return segmented_->acknowledge(consumer,sequence,hash);
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
    if(segmented_)return segmented_->audit_disk();
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
  void inject_next_publish(FaultSite site) noexcept {if(segmented_)segmented_->inject_next_publish(site);else next_fault_=site;}
#endif
};
} // namespace christeye::living::durable
