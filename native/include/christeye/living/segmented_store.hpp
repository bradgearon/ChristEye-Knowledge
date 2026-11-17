#pragma once
// Immutable segment storage with a small atomic root, immutable index buckets,
// source-qualified lookup, verified cold archives and bounded working memory.
#include "christeye/living/durable_records.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iomanip>
#include <span>
#include <unordered_set>
namespace christeye::living::durable {
struct StorageOptions {
  bool segmented=false;
  std::size_t segment_events=256,segment_bytes=256*1024;
  std::uint64_t minimum_free_bytes=8*1024*1024;
  static StorageOptions sustained(std::size_t events=256)noexcept {
    StorageOptions o;o.segmented=true;o.segment_events=events;return o;
  }
};
struct StorageCapacity {
  bool segmented=false;
  std::size_t resident_events=0,event_limit=0,byte_limit=0;
  std::uint64_t total_events=0,tail_bytes=0,available_disk_bytes=0;
  std::uint64_t archived_segments=0,reserved_events=0,reserved_bytes=0;
  std::uint64_t minimum_free_bytes=0;
};
struct ReplayCheckpoint {
  std::uint64_t sequence=0;
  std::string event_hash=std::string(kZeroDigest),object_hash=std::string(kZeroDigest);
};
struct ArchiveStats {std::size_t moved_objects=0,removed_orphans=0;std::uint64_t archived_segments=0;};
struct StorageReservation {std::uint64_t id=0,events=0,bytes=0;};
class SegmentedStore final {
  static constexpr std::size_t kBuckets=64,kBucketEntries=256,kReachabilityLimit=65536;
  struct Root {
    std::uint64_t generation=1,segment=1,first=1,offset=0,sequence=0,archived=0;
    std::string previous=std::string(kZeroDigest),last=std::string(kZeroDigest),closed=std::string(kZeroDigest);
    ReplayCheckpoint checkpoint{};
    std::array<std::string,kBuckets> indexes{};
    Root(){for(auto& h:indexes)h=std::string(kZeroDigest);}
  } root_state_;
  struct Segment {
    std::uint64_t id=0,first=0,last=0,bytes=0;
    std::string previous,last_hash,body_hash,previous_segment;
  };
  struct IndexEntry {Event latest;std::uint64_t maximum_tick=0;};
  fs::path root_;
  os::File lock_;
  StorageOptions options_{};
  std::vector<Event> tail_;
  bool open_=false,importing_=false;
  std::uint64_t next_reservation_=1,reserved_events_=0,reserved_bytes_=0;
  std::array<StorageReservation,32> reservations_{};
#ifdef CE_DURABLE_FAULT_TESTS
  FaultSite fault_=FaultSite::none;
#endif
  static std::string number(std::uint64_t n){return std::to_string(n);}
  static std::string filename(std::uint64_t n,std::string_view suffix){
    std::ostringstream s;s<<std::setw(20)<<std::setfill('0')<<n<<suffix;return s.str();
  }
  fs::path segment_path(std::uint64_t id,bool metadata=false)const {
    const auto directory=id<=root_state_.archived?"archive":"segments";
    return root_/directory/filename(id,metadata?".meta":".log");
  }
  fs::path object_path(std::string_view hash,bool cold=false)const{return root_/(cold?"archive_objects":"objects")/std::string(hash);}
  fs::path reference_path(std::string_view hash)const{return root_/"references"/(std::string(hash)+".refs");}
  static std::string seal(std::string body){return body+"seal "+pl::sha256_hex(body)+'\n';}
  static bool unseal(std::string_view wire,std::string& body){
    const auto position=wire.rfind("seal ");
    if(position==std::string_view::npos||wire.size()!=position+70||wire.back()!='\n')return false;
    body=wire.substr(0,position);return valid_digest(wire.substr(position+5,64))&&pl::sha256_hex(body)==wire.substr(position+5,64);
  }
  static std::string journal_header(std::uint64_t id,std::uint64_t first,std::string_view previous){
    return "CEWSEG/1\nsegment "+number(id)+"\nfirst "+number(first)+"\nprevious "+std::string(previous)+"\nend\n";
  }
  static std::string root_wire(const Root& r,const StorageOptions& o){
    std::ostringstream s;s<<"CEWSTORE/2\nlimits "<<o.segment_events<<' '<<o.segment_bytes<<' '<<o.minimum_free_bytes
      <<"\nroot "<<r.generation<<' '<<r.segment<<' '<<r.first<<' '<<r.offset<<' '<<r.sequence<<' '<<r.archived
      <<"\nchain "<<r.previous<<' '<<r.last<<' '<<r.closed<<"\ncheckpoint "<<r.checkpoint.sequence<<' '
      <<r.checkpoint.event_hash<<' '<<r.checkpoint.object_hash<<"\nindexes\n";
    for(const auto& h:r.indexes)s<<h<<'\n';
    s<<"end\n";return seal(s.str());
  }
  static bool parse_root(std::string_view raw,Root& r,StorageOptions& o){
    std::string body;if(!unseal(raw,body))return false;
    std::istringstream in(body);std::string magic,tag,extra;
    if(!(in>>magic>>tag)||magic!="CEWSTORE/2"||tag!="limits"||
       !(in>>o.segment_events>>o.segment_bytes>>o.minimum_free_bytes>>tag)||tag!="root"||
       !(in>>r.generation>>r.segment>>r.first>>r.offset>>r.sequence>>r.archived>>tag)||tag!="chain"||
       !(in>>r.previous>>r.last>>r.closed>>tag)||tag!="checkpoint"||
       !(in>>r.checkpoint.sequence>>r.checkpoint.event_hash>>r.checkpoint.object_hash>>tag)||tag!="indexes")return false;
    for(auto& h:r.indexes)if(!(in>>h)||!valid_digest(h))return false;
    if(!(in>>tag)||tag!="end"||in>>extra)return false;
    o.segmented=true;
    return r.generation&&r.segment&&r.first&&r.sequence<UINT64_MAX&&r.first<=r.sequence+1&&r.archived<r.segment&&
      r.checkpoint.sequence<=r.sequence&&valid_digest(r.previous)&&valid_digest(r.last)&&valid_digest(r.closed)&&
      valid_digest(r.checkpoint.event_hash)&&valid_digest(r.checkpoint.object_hash)&&
      o.segment_events>=8&&o.segment_events<=kMaxEvents&&o.segment_bytes>=8192&&o.segment_bytes<=kMaxJournalBytes&&
      root_wire(r,o)==raw;
  }
  static std::string segment_wire(const Segment& m){
    std::ostringstream s;s<<"CEWSEGMENT/1\nsegment "<<m.id<<' '<<m.first<<' '<<m.last<<' '<<m.bytes
      <<"\nchain "<<m.previous<<' '<<m.last_hash<<' '<<m.previous_segment<<"\nbody "<<m.body_hash<<"\nend\n";
    return seal(s.str());
  }
  bool metadata(std::uint64_t id,Segment& m,std::string* wire=nullptr)const {
    std::string raw,body;if(!read_bounded(segment_path(id,true),2048,raw)||!unseal(raw,body))return false;
    std::istringstream in(body);std::string magic,tag,extra;
    if(!(in>>magic>>tag)||magic!="CEWSEGMENT/1"||tag!="segment"||
      !(in>>m.id>>m.first>>m.last>>m.bytes>>tag)||tag!="chain"||
      !(in>>m.previous>>m.last_hash>>m.previous_segment>>tag)||tag!="body"||
      !(in>>m.body_hash>>tag)||tag!="end"||in>>extra)return false;
    if(m.id!=id||!m.first||m.last<m.first||m.last-m.first+1>options_.segment_events||
       m.bytes>options_.segment_bytes||!valid_digest(m.previous)||!valid_digest(m.last_hash)||
       !valid_digest(m.previous_segment)||!valid_digest(m.body_hash)||segment_wire(m)!=raw)return false;
    if(wire)*wire=raw;
    return true;
  }
  bool write_root(const Root& r){return importing_||os::atomic_write(root_/"MANIFEST",root_wire(r,options_));}
  bool injected(FaultSite at){
#ifdef CE_DURABLE_FAULT_TESTS
    if(fault_==at){fault_=FaultSite::none;open_=false;return true;}
#else
    (void)at;
#endif
    return false;
  }
  static std::string stream_key(std::string_view kind,std::string_view entity,std::string_view clock,std::string_view producer){
    return std::string(kind)+'\n'+std::string(entity)+'\n'+std::string(clock)+'\n'+std::string(producer);
  }
  static std::size_t bucket(std::string_view kind,std::string_view entity,std::string_view clock){
    const auto h=pl::sha256_hex(stream_key(kind,entity,clock,{}));
    auto digit=[](char c){return static_cast<unsigned>(c<='9'?c-'0':c-'a'+10);};
    return ((digit(h[0])<<4)|digit(h[1]))%kBuckets;
  }
  bool read_bucket(std::size_t b,std::vector<IndexEntry>& out)const {
    out.clear();const auto& hash=root_state_.indexes[b];if(hash==kZeroDigest)return true;
    std::string raw;if(!read_bounded(root_/"indexes"/hash,kMaxObjectBytes,raw)||pl::sha256_hex(raw)!=hash)return false;
    std::istringstream in(raw);std::string line;
    if(!std::getline(in,line)||line!="CEWINDEX/2")return false;
    while(std::getline(in,line)){
      if(line=="end")return !std::getline(in,line);
      const auto split=line.find(' ');std::uint64_t maximum=0;Event e;
      if(out.size()==kBucketEntries||split==std::string::npos||!parse_u64(std::string_view(line).substr(0,split),maximum)||
         !parse_event(std::string_view(line).substr(split+1),e)||!e.sequence||e.sequence>root_state_.sequence||
         e.source_tick>maximum||bucket(e.kind,e.entity,e.clock)!=b)return false;
      for(const auto& prior:out)if(stream_key(prior.latest.kind,prior.latest.entity,prior.latest.clock,prior.latest.producer)==
        stream_key(e.kind,e.entity,e.clock,e.producer))return false;
      out.push_back({std::move(e),maximum});
    }
    return false;
  }
  bool write_bucket(std::vector<IndexEntry>& entries,std::string& digest){
    std::sort(entries.begin(),entries.end(),[](const IndexEntry& a,const IndexEntry& b){
      return stream_key(a.latest.kind,a.latest.entity,a.latest.clock,a.latest.producer)<stream_key(b.latest.kind,b.latest.entity,b.latest.clock,b.latest.producer);
    });
    std::string wire="CEWINDEX/2\n";
    for(const auto& entry:entries)wire+=number(entry.maximum_tick)+' '+serialize_event(entry.latest);
    wire+="end\n";digest=pl::sha256_hex(wire);
    return os::atomic_write(root_/"indexes"/digest,wire);
  }
  bool references(std::string_view hash,std::vector<std::string>& refs)const {
    refs.clear();std::string raw,body;
    if(!read_bounded(reference_path(hash),64*1024,raw)||!unseal(raw,body))return false;
    std::istringstream in(body);std::string magic,parent,line;
    if(!(in>>magic>>parent)||magic!="CEWREFS/1"||parent!=hash)return false;
    while(in>>line){if(line=="end")return !(in>>line);
      if(refs.size()==512||!valid_digest(line)||line==hash)return false;
      refs.push_back(line);}
    return false;
  }
  bool write_references(std::string_view hash,std::span<const std::string> references_in){
    if(references_in.size()>512)return false;
    std::vector<std::string> refs(references_in.begin(),references_in.end());
    std::sort(refs.begin(),refs.end());refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
    std::string body="CEWREFS/1\n"+std::string(hash)+'\n';
    for(const auto& ref:refs){std::string ignored;if(!valid_digest(ref)||ref==hash||!get(ref,ignored))return false;body+=ref+'\n';}
    body+="end\n";const auto wire=seal(body);std::error_code ec;
    if(fs::exists(reference_path(hash),ec)){std::string old;return read_bounded(reference_path(hash),64*1024,old)&&old==wire;}
    return !ec&&os::atomic_write(reference_path(hash),wire);
  }
  bool read_segment(std::uint64_t id,std::vector<Event>& out,bool verify_objects=true)const {
    out.clear();std::string raw,header,expected_last;std::uint64_t first=0,limit=0;
    if(id==root_state_.segment){
      header=journal_header(id,root_state_.first,root_state_.previous);first=root_state_.first;limit=root_state_.offset;
      expected_last=root_state_.last;
      if(!read_bounded(segment_path(id),options_.segment_bytes,raw)||raw.size()<limit)return false;
      raw.resize(static_cast<std::size_t>(limit));
    }else{
      Segment meta;if(!metadata(id,meta)||!read_bounded(segment_path(id),options_.segment_bytes,raw)||
        raw.size()!=meta.bytes||pl::sha256_hex(raw)!=meta.body_hash)return false;
      first=meta.first;limit=meta.bytes;expected_last=meta.last_hash;header=journal_header(id,first,meta.previous);
    }
    if(raw.size()<header.size()||!raw.starts_with(header)||raw.back()!='\n')return false;
    std::istringstream in(raw.substr(header.size()));std::string line,previous;
    if(id==root_state_.segment)previous=root_state_.previous;
    else {Segment meta;if(!metadata(id,meta))return false;previous=meta.previous;}
    while(std::getline(in,line)){
      Event event;std::string object;
      if(out.size()==options_.segment_events||!parse_event(line,event)||event.sequence!=first+out.size()||
        event.previous_hash!=previous||(verify_objects&&!get(event.object_hash,object)))return false;
      previous=event.hash;out.push_back(std::move(event));
    }
    const auto expected_count=id==root_state_.segment?root_state_.sequence-first+1:[&]{Segment m;return metadata(id,m)?m.last-first+1:UINT64_MAX;}();
    return out.size()==expected_count&&previous==expected_last;
  }
  bool event_at(std::uint64_t seq,Event& out)const {
    if(!seq||seq>root_state_.sequence)return false;
    std::uint64_t low=1,high=root_state_.segment;
    while(low<high){const auto middle=low+(high-low)/2;Segment m;if(!metadata(middle,m))return false;
      if(m.last<seq)low=middle+1;else high=middle;}
    std::vector<Event> part;if(!read_segment(low,part))return false;
    for(const auto& e:part)if(e.sequence==seq){out=e;return true;}
    return false;
  }
  Code rotate(){
    if(tail_.empty())return Code::capacity;
    std::string raw;if(!read_bounded(segment_path(root_state_.segment),options_.segment_bytes,raw)||raw.size()!=root_state_.offset)
      return Code::corrupt_journal;
    Segment meta{root_state_.segment,root_state_.first,root_state_.sequence,root_state_.offset,
      root_state_.previous,root_state_.last,pl::sha256_hex(raw),root_state_.closed};
    const auto wire=segment_wire(meta);Root next=root_state_;
    if(next.segment==UINT64_MAX||next.generation==UINT64_MAX)return Code::capacity;
    ++next.segment;++next.generation;next.first=next.sequence+1;next.previous=next.last;
    next.closed=pl::sha256_hex(wire);const auto header=journal_header(next.segment,next.first,next.previous);
    next.offset=header.size();
    if(!os::atomic_write(segment_path(meta.id,true),wire)||
       !os::atomic_write(root_/"segments"/filename(next.segment,".log"),header))return Code::io_error;
    if(injected(FaultSite::after_segment_prepare))return Code::injected_fault;
    if(!write_root(next)){open_=false;return Code::io_error;}
    root_state_=std::move(next);tail_.clear();
    if(injected(FaultSite::after_rotation_commit))return Code::injected_fault;
    return Code::ok;
  }
  bool add_reachable(std::string_view hash,std::unordered_set<std::string>& live,std::size_t depth=0)const {
    if(hash==kZeroDigest)return true;
    if(depth>64||!valid_digest(hash))return false;
    if(live.contains(std::string(hash)))return true;
    if(live.size()==kReachabilityLimit)return false;
    {std::string data;if(!get(hash,data))return false;}
    live.insert(std::string(hash));
    std::vector<std::string> refs;if(!references(hash,refs))return false;
    for(const auto& r:refs)if(!add_reachable(r,live,depth+1))return false;
    return true;
  }
  // Disk marks are disposable derived work, rebuilt before each collection.
  // Their size follows retained evidence while resident memory remains bounded.
  bool mark_history(std::string_view hash,const fs::path& marks,std::size_t depth=0)const {
    if(depth>64||!valid_digest(hash))return false;
    const auto marker=marks/std::string(hash);std::error_code ec;
    if(fs::exists(marker,ec))return !ec&&!fs::is_symlink(marker,ec)&&!ec;
    if(ec)return false;
    {std::string raw;if(!get(hash,raw))return false;}
    std::vector<std::string> refs;if(!references(hash,refs))return false;
    for(const auto& r:refs)if(!mark_history(r,marks,depth+1))return false;
    std::ofstream output(marker,std::ios::binary|std::ios::trunc);output.put('1');output.close();return !output.fail();
  }
  bool remove_archived_hot_copies(){
    std::error_code ec;
    for(const auto& file:fs::directory_iterator(root_/"segments",ec)){
      if(ec||file.is_symlink())return false;
      const auto name=file.path().filename().string();std::uint64_t id=0;
      if(name.size()!=24&&name.size()!=25)continue;
      if(!parse_u64(std::string_view(name).substr(0,20),id)||!id||id>root_state_.archived)continue;
      const auto hot=root_/"segments"/filename(id,".log");
      const auto meta=root_/"segments"/filename(id,".meta");
      // Only remove redundant hot files once the authoritative archive verifies.
      std::vector<Event> verified;if(!read_segment(id,verified))return false;
      fs::remove(hot,ec);if(ec)return false;fs::remove(meta,ec);if(ec)return false;
    }
    if(ec)return false;
    return os::sync_directory(root_/"segments");
  }

  Result commit_event(Event e){
    if(!can_append(1,options_.segment_bytes+16384)||root_state_.generation==UINT64_MAX)return {Code::capacity,{}};
    if(e.sequence!=root_state_.sequence+1||e.previous_hash!=root_state_.last||e.hash!=event_hash(e))return {Code::corrupt_journal,{}};
    const auto record=serialize_event(e);
    if(tail_.size()==options_.segment_events||root_state_.offset+record.size()>options_.segment_bytes){
      const auto status=rotate();if(status!=Code::ok)return {status,{}};
    }
    const auto b=bucket(e.kind,e.entity,e.clock);std::vector<IndexEntry> entries;
    if(!read_bucket(b,entries))return {Code::corrupt_journal,{}};
    auto it=std::find_if(entries.begin(),entries.end(),[&](const IndexEntry& entry){const auto& old=entry.latest;
      return old.kind==e.kind&&old.entity==e.entity&&old.clock==e.clock&&old.producer==e.producer;});
    if(it==entries.end()){if(entries.size()==kBucketEntries)return {Code::capacity,{}};entries.push_back({e,e.source_tick});}
    else {it->latest=e;it->maximum_tick=std::max(it->maximum_tick,e.source_tick);}
    std::string index_hash;if(!write_bucket(entries,index_hash))return {Code::io_error,{}};
    if(!os::append_synced(segment_path(root_state_.segment),record)){open_=false;return {Code::io_error,{}};}
    if(injected(FaultSite::after_journal_sync))return {Code::injected_fault,{}};
    Root next=root_state_;++next.generation;next.offset+=record.size();next.sequence=e.sequence;next.last=e.hash;next.indexes[b]=index_hash;
#ifdef CE_DURABLE_FAULT_TESTS
    if(fault_==FaultSite::after_head_temp_sync){
      os::File temp;const auto wire=root_wire(next,options_);
      if(!temp.open(root_/"MANIFEST.tmp",true)||!temp.write_all(wire)||!temp.sync())return {Code::io_error,{}};
      if(injected(FaultSite::after_head_temp_sync))return {Code::injected_fault,{}};
    }
#endif
    if(!write_root(next)){open_=false;return {Code::io_error,{}};}
    const auto previous_index=root_state_.indexes[b];
    root_state_=std::move(next);tail_.push_back(e);
    if(injected(FaultSite::after_head_commit))return {Code::injected_fault,{}};
    if(previous_index!=kZeroDigest&&previous_index!=index_hash){
      std::error_code ec;fs::remove(root_/"indexes"/previous_index,ec);
      // Failed cleanup leaves a harmless old generation for restart maintenance.
    }
    return {Code::ok,e.hash};
  }
public:
  SegmentedStore()=default;
  SegmentedStore(const SegmentedStore&)=delete;
  SegmentedStore& operator=(const SegmentedStore&)=delete;
  ~SegmentedStore(){close();}
  void close()noexcept{open_=false;importing_=false;tail_.clear();lock_.close();root_.clear();reserved_bytes_=reserved_events_=0;reservations_={};}
  Code open(const fs::path& path,StorageOptions options){
    close();root_=path;options_=options;root_state_=Root{};
    if(!options.segmented||options.segment_events<8||options.segment_events>kMaxEvents||
       options.segment_bytes<8192||options.segment_bytes>kMaxJournalBytes)return Code::invalid_argument;
    std::error_code ec;
    for(const auto* dir:{"objects","segments","archive","archive_objects","references","cursors","pins","indexes"}){
      fs::create_directories(root_/dir,ec);if(ec||fs::is_symlink(root_/dir,ec)||ec)return Code::io_error;
    }
    if(fs::is_symlink(root_/"MANIFEST",ec)||fs::is_symlink(root_/"writer.lock",ec))return Code::io_error;
    if(!lock_.open(root_/"writer.lock"))return Code::io_error;
    if(!lock_.lock_exclusive())return Code::locked;
    if(!fs::exists(root_/"MANIFEST",ec)){
      // Opt-in migration validates the entire bounded legacy history before
      // publishing the new root. A crash before publication leaves legacy HEAD
      // authoritative; imported segment/index files are harmless uncommitted work.
      std::vector<Event> imported;
      if(fs::exists(root_/"events.log",ec)){
        std::string journal;if(!read_bounded(root_/"events.log",kMaxJournalBytes,journal))return Code::io_error;
        Head head{};bool has_head=fs::exists(root_/"HEAD",ec);
        if(has_head){std::string raw;if(!read_bounded(root_/"HEAD",1024,raw)||!parse_head(raw,head)||head.offset>journal.size())return Code::bad_head;
          journal.resize(static_cast<std::size_t>(head.offset));}
        if(!journal.starts_with("CEWREG/1\n")||journal.back()!='\n')return Code::corrupt_journal;
        std::istringstream in(journal);std::string line,previous(kZeroDigest);std::getline(in,line);
        while(std::getline(in,line)){Event e;std::string object;
          if(imported.size()==kMaxEvents||!parse_event(line,e)||e.sequence!=imported.size()+1||e.previous_hash!=previous||
            !read_bounded(root_/"objects"/e.object_hash,kMaxObjectBytes,object)||pl::sha256_hex(object)!=e.object_hash)return Code::corrupt_journal;
          previous=e.hash;imported.push_back(std::move(e));
        }
        if(has_head&&(head.sequence!=imported.size()||head.hash!=previous))return Code::corrupt_journal;
      }
      const auto header=journal_header(1,1,kZeroDigest);root_state_.offset=header.size();
      if(!os::atomic_write(root_/"segments"/filename(1,".log"),header))return Code::io_error;
      if(!imported.empty()){
        importing_=true;open_=true;
        // Legacy opaque objects may contain application references. Pin every
        // imported object conservatively; explicit migration can later replace
        // these pins after application-level reference verification.
        std::vector<std::string> roots;std::size_t pin_number=0;
        for(const auto& file:fs::directory_iterator(root_/"objects",ec)){
          if(ec||file.is_symlink())return Code::io_error;
          const auto hash=file.path().filename().string();if(!valid_digest(hash))continue;
          std::string bytes;if(!get(hash,bytes))return Code::bad_digest;
          std::vector<std::string> prior;
          if(fs::exists(reference_path(hash),ec)){if(ec||!references(hash,prior))return Code::io_error;}
          else if(ec||!write_references(hash,{}))return Code::io_error;
          roots.push_back(hash);
          if(roots.size()==256){if(pin("legacy-import-"+number(pin_number++),roots)!=Code::ok)return Code::io_error;roots.clear();}
        }
        if(!roots.empty()&&pin("legacy-import-"+number(pin_number),roots)!=Code::ok)return Code::io_error;
        for(const auto& e:imported){const auto result=commit_event(e);
          if(result.code!=Code::ok||result.hash!=e.hash)return Code::corrupt_journal;}
        importing_=false;
      }
      if(!write_root(root_state_))return Code::io_error;
    }else{
      std::string raw;if(!read_bounded(root_/"MANIFEST",16384,raw)||!parse_root(raw,root_state_,options_))return Code::bad_head;
    }
    open_=true;
    if(root_state_.segment>1){Segment previous;std::string raw;
      if(!metadata(root_state_.segment-1,previous,&raw)||pl::sha256_hex(raw)!=root_state_.closed||
        previous.last+1!=root_state_.first||previous.last_hash!=root_state_.previous){open_=false;return Code::corrupt_journal;}}
    if(!read_segment(root_state_.segment,tail_)){open_=false;return Code::corrupt_journal;}
    const auto size=fs::file_size(segment_path(root_state_.segment),ec);
    if(ec||size<root_state_.offset||(size>root_state_.offset&&!os::truncate_synced(segment_path(root_state_.segment),root_state_.offset))){open_=false;return Code::io_error;}
    for(std::size_t b=0;b<kBuckets;++b){std::vector<IndexEntry> entries;if(!read_bucket(b,entries)){open_=false;return Code::corrupt_journal;}}
    if(root_state_.checkpoint.object_hash!=kZeroDigest){std::string bytes;if(!get(root_state_.checkpoint.object_hash,bytes)){open_=false;return Code::missing_object;}}
    // Index buckets are private content-addressed metadata, never application
    // CAS roots. Old generations can be removed after the committed root is
    // verified, including files left by an interrupted root publication.
    for(const auto& file:fs::directory_iterator(root_/"indexes",ec)){
      if(ec||file.is_symlink())return Code::io_error;
      const auto hash=file.path().filename().string();
      if(valid_digest(hash)&&std::find(root_state_.indexes.begin(),root_state_.indexes.end(),hash)==root_state_.indexes.end()){
        fs::remove(file.path(),ec);if(ec)return Code::io_error;
      }
    }
    if(ec||!os::sync_directory(root_/"indexes"))return Code::io_error;
    return Code::ok;
  }
  bool is_open()const noexcept{return open_;}
  std::size_t event_count()const noexcept{return static_cast<std::size_t>(root_state_.sequence);}
  const std::vector<Event>& events()const noexcept{return tail_;}
  StorageCapacity capacity()const noexcept {
    std::error_code ec;const auto space=fs::space(root_,ec);
    return {true,tail_.size(),options_.segment_events,options_.segment_bytes,root_state_.sequence,root_state_.offset,
      ec?0:space.available,root_state_.archived,reserved_events_,reserved_bytes_,options_.minimum_free_bytes};
  }
  bool can_append(std::size_t events,std::size_t bytes)const noexcept {
    if(!open_||events==0||events>=UINT64_MAX-root_state_.sequence||
      reserved_events_>UINT64_MAX-root_state_.sequence-events||reserved_bytes_>UINT64_MAX-bytes)return false;
    const auto c=capacity();return c.available_disk_bytes>=options_.minimum_free_bytes&&
      c.available_disk_bytes-options_.minimum_free_bytes>=reserved_bytes_+bytes;
  }
  bool reserve(std::size_t events,std::size_t bytes,StorageReservation& out)noexcept {
    if(out.id||!can_append(events,bytes)||next_reservation_==0)return false;
    for(auto& r:reservations_)if(!r.id){r={next_reservation_++,events,bytes};out=r;reserved_events_+=events;reserved_bytes_+=bytes;return true;}
    return false;
  }
  bool release(StorageReservation& value)noexcept {
    for(auto& r:reservations_)if(r.id&&r.id==value.id&&r.events==value.events&&r.bytes==value.bytes){
      reserved_events_-=r.events;reserved_bytes_-=r.bytes;r={};value={};return true;}return false;
  }
  bool get(std::string_view hash,std::string& out)const {
    if(!open_||!valid_digest(hash))return false;
    std::error_code ec;const auto hot=object_path(hash);
    const auto path=fs::exists(hot,ec)?hot:object_path(hash,true);
    return !ec&&read_bounded(path,kMaxObjectBytes,out)&&pl::sha256_hex(out)==hash;
  }
  Result put_referenced(std::string_view bytes,std::span<const std::string> refs){
    if(!open_)return {Code::not_open,{}};
    if(bytes.size()>kMaxObjectBytes)return {Code::capacity,{}};
    const auto hash=pl::sha256_hex(bytes);std::string old;std::error_code ec;
    if(fs::exists(object_path(hash),ec)||fs::exists(object_path(hash,true),ec)){
      if(ec||!get(hash,old)||old!=bytes||!write_references(hash,refs))return {Code::bad_digest,{}};
      return {Code::ok,hash};
    }
    if(!os::atomic_write(object_path(hash),bytes)||!write_references(hash,refs))return {Code::io_error,{}};
    return {Code::ok,hash};
  }
  Result put(std::string_view bytes){return put_referenced(bytes,{});}
  bool latest_event(std::string_view kind,std::string_view entity,std::string_view clock,
                    Event& out,bool& found,std::string_view producer={})const {
    found=false;if(!open_||!valid_kind(kind)||!valid_id(entity)||!valid_id(clock)||(!producer.empty()&&!valid_id(producer)))return false;
    std::vector<IndexEntry> entries;if(!read_bucket(bucket(kind,entity,clock),entries))return false;
    for(const auto& entry:entries){const auto& e=entry.latest;if(e.kind==kind&&e.entity==entity&&e.clock==clock&&
      (producer.empty()||e.producer==producer)&&(!found||e.sequence>out.sequence)){out=e;found=true;}}
    return true;
  }
  bool visit_events(std::uint64_t after,const std::function<bool(const Event&)>& callback)const {
    if(!open_||after>root_state_.sequence||!callback)return false;
    if(after==root_state_.sequence)return true;
    std::uint64_t low=1,high=root_state_.segment;
    while(low<high){const auto mid=low+(high-low)/2;Segment m;if(!metadata(mid,m))return false;
      if(m.last<=after)low=mid+1;else high=mid;}
    for(auto id=low;id<=root_state_.segment;++id){
      std::vector<Event> part;if(!read_segment(id,part))return false;
      for(const auto& e:part)if(e.sequence>after&&!callback(e))return false;
    }
    return true;
  }
  bool find_event(std::string_view kind,std::string_view entity,std::string_view clock,
                  std::string_view producer,std::uint64_t tick,Event& out,bool& found)const {
    found=false;
    if(!open_||!valid_kind(kind)||!valid_id(entity)||!valid_id(clock)||!valid_id(producer))return false;
    std::vector<IndexEntry> entries;if(!read_bucket(bucket(kind,entity,clock),entries))return false;
    bool historical=false;
    for(const auto& entry:entries){const auto& last=entry.latest;
      if(last.kind!=kind||last.entity!=entity||last.clock!=clock||last.producer!=producer)continue;
      if(last.source_tick==tick){out=last;found=true;return true;}
      historical=tick<=entry.maximum_tick;break;
    }
    if(!historical)return true;
    // Historical retries are uncommon; lookup remains bounded per segment.
    return visit_events(0,[&](const Event& e){if(e.kind==kind&&e.entity==entity&&e.clock==clock&&
      e.producer==producer&&e.source_tick==tick){out=e;found=true;}return true;});
  }
  Result publish(std::string kind,std::string entity,std::string object,std::string origin,
                 std::uint64_t tick,std::string clock,std::string producer){
    if(!open_)return {Code::not_open,{}};
    if(!valid_kind(kind)||!valid_id(entity)||!valid_digest(object)||!valid_origin(origin)||!valid_id(clock)||!valid_id(producer))
      return {Code::invalid_argument,{}};
    std::string raw;if(!get(object,raw))return {Code::missing_object,{}};
    Event existing;bool found=false;
    if(!find_event(kind,entity,clock,producer,tick,existing,found))return {Code::corrupt_journal,{}};
    if(found&&existing.object_hash==object&&existing.origin==origin)return {Code::ok,existing.hash};
    if(found){
      bool duplicate=false;
      if(!visit_events(0,[&](const Event& prior){if(prior.kind==kind&&prior.entity==entity&&prior.clock==clock&&
        prior.producer==producer&&prior.source_tick==tick&&prior.object_hash==object&&prior.origin==origin){existing=prior;duplicate=true;}
        return true;}))return {Code::corrupt_journal,{}};
      if(duplicate)return {Code::ok,existing.hash};
    }
    Event e{root_state_.sequence+1,root_state_.last,std::move(kind),std::move(entity),std::move(object),std::move(origin),tick,std::move(clock),std::move(producer),{}};
    e.hash=event_hash(e);return commit_event(std::move(e));
  }

  bool save_replay_checkpoint(std::string_view bytes,std::span<const std::string> refs={}){
    if(!open_||root_state_.generation==UINT64_MAX)return false;
    const auto object=put_referenced(bytes,refs);if(object.code!=Code::ok)return false;
    Root next=root_state_;++next.generation;next.checkpoint={next.sequence,next.last,object.hash};
    if(!write_root(next)){open_=false;return false;}root_state_=std::move(next);
    return !injected(FaultSite::after_checkpoint_commit);
  }
  bool load_replay_checkpoint(ReplayCheckpoint& checkpoint,std::string& bytes,bool& found)const {
    found=root_state_.checkpoint.object_hash!=kZeroDigest;if(!open_)return false;
    if(!found)return true;
    checkpoint=root_state_.checkpoint;Event at;
    return (checkpoint.sequence?event_at(checkpoint.sequence,at)&&at.hash==checkpoint.event_hash:checkpoint.event_hash==kZeroDigest)&&
      get(checkpoint.object_hash,bytes);
  }
  Cursor read_cursor(std::string_view name)const {
    if(!open_)return {Code::not_open};
    if(!valid_id(name))return {Code::invalid_cursor};
    const auto file=root_/"cursors"/(std::string(name)+".cursor");std::error_code ec;
    if(!fs::exists(file,ec))return ec?Cursor{Code::io_error}:Cursor{};
    std::string raw;Cursor c;Event e;
    if(!read_bounded(file,1024,raw)||!parse_cursor(raw,c)||c.sequence>root_state_.sequence||
      (c.sequence?(!event_at(c.sequence,e)||e.hash!=c.hash):c.hash!=kZeroDigest))return {Code::invalid_cursor};
    return c;
  }
  Code acknowledge(std::string_view name,std::uint64_t sequence,std::string_view hash){
    if(!open_)return Code::not_open;
    if(!valid_id(name)||!valid_digest(hash)||sequence>root_state_.sequence)return Code::invalid_cursor;
    Event e;if(sequence?(!event_at(sequence,e)||e.hash!=hash):hash!=kZeroDigest)return Code::invalid_cursor;
    const auto old=read_cursor(name);if(old.code!=Code::ok||sequence<old.sequence)return Code::invalid_cursor;
    std::error_code ec;
    if(!fs::exists(root_/"cursors"/(std::string(name)+".cursor"),ec)){
      if(ec)return Code::io_error;
      std::size_t readers=0;for(const auto& file:fs::directory_iterator(root_/"cursors",ec)){
        if(ec||file.is_symlink())return Code::invalid_cursor;
        if(file.path().extension()==".cursor"&&++readers>=32)return Code::capacity;
      }
      if(ec)return Code::io_error;
    }
    return os::atomic_write(root_/"cursors"/(std::string(name)+".cursor"),sealed_cursor(sequence,hash))?Code::ok:Code::io_error;
  }
  Code pin(std::string_view name,std::span<const std::string> roots){
    if(!open_)return Code::not_open;
    if(!valid_id(name)||roots.size()>4096)return Code::invalid_argument;
    std::string body="CEWPIN/1\n";for(const auto& h:roots){std::string raw;if(!get(h,raw))return Code::missing_object;body+=h+'\n';}
    body+="end\n";return os::atomic_write(root_/"pins"/(std::string(name)+".pin"),seal(body))?Code::ok:Code::io_error;
  }
  Code unpin(std::string_view name){
    if(!open_)return Code::not_open;
    if(!valid_id(name))return Code::invalid_argument;
    std::error_code ec;fs::remove(root_/"pins"/(std::string(name)+".pin"),ec);
    return !ec&&os::sync_directory(root_/"pins")?Code::ok:Code::io_error;
  }
  Code archive_ready(){
    if(!open_)return Code::not_open;
    if(root_state_.checkpoint.object_hash==kZeroDigest)return Code::invalid_argument;
    if(root_state_.generation==UINT64_MAX)return Code::capacity;
    auto safe=root_state_.checkpoint.sequence;std::error_code ec;std::size_t readers=0;
    for(const auto& entry:fs::directory_iterator(root_/"cursors",ec)){
      if(ec||entry.is_symlink())return Code::invalid_cursor;
      if(entry.path().extension()==".tmp")continue; // interrupted cursor publication
      if(entry.path().extension()!=".cursor"||++readers>32)return Code::invalid_cursor;
      const auto cursor=read_cursor(entry.path().stem().string());if(cursor.code!=Code::ok)return cursor.code;safe=std::min(safe,cursor.sequence);
    }
    if(ec)return Code::io_error;
    auto through=root_state_.archived;
    for(auto id=through+1;id<root_state_.segment;++id){Segment m;std::string wire;
      if(!metadata(id,m,&wire))return Code::corrupt_journal;
      if(m.last>safe)break;
      std::vector<Event> evidence;std::unordered_set<std::string> references;
      if(!read_segment(id,evidence))return Code::corrupt_journal;
      for(const auto& event:evidence)if(!add_reachable(event.object_hash,references))return Code::corrupt_journal;
      std::string raw;if(!read_bounded(segment_path(id),options_.segment_bytes,raw)||pl::sha256_hex(raw)!=m.body_hash)return Code::corrupt_journal;
      if(!os::atomic_write(root_/"archive"/filename(id,".log"),raw)||
         !os::atomic_write(root_/"archive"/filename(id,".meta"),wire))return Code::io_error;
      std::string verified,verified_meta;
      if(!read_bounded(root_/"archive"/filename(id,".log"),options_.segment_bytes,verified)||pl::sha256_hex(verified)!=m.body_hash||
         !read_bounded(root_/"archive"/filename(id,".meta"),2048,verified_meta)||verified_meta!=wire)return Code::bad_digest;
      through=id;
    }
    if(through==root_state_.archived)return remove_archived_hot_copies()?Code::ok:Code::io_error;
    if(injected(FaultSite::after_archive_prepare))return Code::injected_fault;
    Root next=root_state_;++next.generation;next.archived=through;
    if(!write_root(next)){open_=false;return Code::io_error;}root_state_=std::move(next);
    if(injected(FaultSite::after_archive_commit))return Code::injected_fault;
    return remove_archived_hot_copies()?Code::ok:Code::io_error;
  }
  Code collect_objects(ArchiveStats& stats){
    stats={};if(!open_)return Code::not_open;stats.archived_segments=root_state_.archived;
    // Verify every retained evidence chain before making reclamation decisions.
    if(!audit_disk())return Code::corrupt_journal;
    std::unordered_set<std::string> live;
    for(std::size_t b=0;b<kBuckets;++b){std::vector<IndexEntry> entries;
      if(!read_bucket(b,entries))return Code::corrupt_journal;
      for(const auto& entry:entries)if(!add_reachable(entry.latest.object_hash,live))return Code::corrupt_journal;
    }
    if(!add_reachable(root_state_.checkpoint.object_hash,live))return Code::corrupt_journal;
    std::error_code ec;std::size_t pins=0;
    for(const auto& file:fs::directory_iterator(root_/"pins",ec)){
      if(ec||++pins>4096||file.is_symlink())return Code::capacity;
      std::string raw,body;if(!read_bounded(file.path(),512*1024,raw)||!unseal(raw,body))return Code::corrupt_journal;
      std::istringstream in(body);std::string word;if(!(in>>word)||word!="CEWPIN/1")return Code::corrupt_journal;
      bool ended=false;while(in>>word){if(word=="end"){ended=true;break;}if(!add_reachable(word,live))return Code::corrupt_journal;}
      if(!ended||in>>word)return Code::corrupt_journal;
    }
    if(ec)return Code::io_error;
    std::uint64_t after=0;if(root_state_.archived){Segment m;if(!metadata(root_state_.archived,m))return Code::corrupt_journal;after=m.last;}
    if(!visit_events(after,[&](const Event& e){return add_reachable(e.object_hash,live);}))return Code::corrupt_journal;
    const auto marks=root_/"gc_marks";
    if(fs::is_symlink(marks,ec))return Code::io_error;
    ec.clear();fs::remove_all(marks,ec);if(ec)return Code::io_error;
    fs::create_directory(marks,ec);if(ec)return Code::io_error;
    if(!visit_events(0,[&](const Event& e){return mark_history(e.object_hash,marks);}))return Code::corrupt_journal;
    for(const auto& file:fs::directory_iterator(root_/"objects",ec)){
      if(ec||file.is_symlink())return Code::io_error;
      const auto hash=file.path().filename().string();
      if(!valid_digest(hash))continue;
      if(live.contains(hash))continue;
      const bool historical=fs::exists(marks/hash,ec);if(ec)return Code::io_error;
      std::string raw;if(!get(hash,raw))return Code::bad_digest;
      if(historical){
        if(!os::atomic_write(object_path(hash,true),raw))return Code::io_error;
        std::string verify;if(!read_bounded(object_path(hash,true),kMaxObjectBytes,verify)||pl::sha256_hex(verify)!=hash)return Code::bad_digest;
        if(injected(FaultSite::after_object_archive))return Code::injected_fault;
        fs::remove(file.path(),ec);if(ec)return Code::io_error;++stats.moved_objects;
      }else{
        fs::remove(file.path(),ec);if(ec)return Code::io_error;
        fs::remove(reference_path(hash),ec);if(ec)return Code::io_error;++stats.removed_orphans;
      }
    }
    if(ec)return Code::io_error;
    for(const auto& file:fs::directory_iterator(root_/"references",ec)){
      if(ec||file.is_symlink())return Code::io_error;
      const auto name=file.path().filename().string();
      if(name.size()!=69||!name.ends_with(".refs")||!valid_digest(std::string_view(name).substr(0,64)))continue;
      const auto hash=name.substr(0,64);
      if(!fs::exists(object_path(hash),ec)&&!fs::exists(object_path(hash,true),ec)){
        if(ec)return Code::io_error;
        fs::remove(file.path(),ec);if(ec)return Code::io_error;
      }
    }
    if(ec)return Code::io_error;
    fs::remove_all(marks,ec);if(ec)return Code::io_error;
    return os::sync_directory(root_/"objects")&&os::sync_directory(root_/"references")?Code::ok:Code::io_error;
  }
  bool audit_disk()const {
    if(!open_)return false;
    Root state;StorageOptions options;std::string raw;
    if(!read_bounded(root_/"MANIFEST",16384,raw)||!parse_root(raw,state,options)||root_wire(state,options)!=root_wire(root_state_,options_))return false;
    std::string chain(kZeroDigest),last(kZeroDigest);std::uint64_t expected=1;
    for(std::uint64_t id=1;id<root_state_.segment;++id){Segment m;std::string wire;
      if(!metadata(id,m,&wire)||m.previous_segment!=chain||m.first!=expected||m.previous!=last)return false;
      std::vector<Event> part;if(!read_segment(id,part))return false;
      for(const auto& e:part){std::unordered_set<std::string> refs;if(!add_reachable(e.object_hash,refs))return false;}
      chain=pl::sha256_hex(wire);last=m.last_hash;expected=m.last+1;
    }
    if(chain!=root_state_.closed||last!=root_state_.previous||expected!=root_state_.first)return false;
    std::error_code ec;
    if(fs::file_size(segment_path(root_state_.segment),ec)!=root_state_.offset||ec)return false;
    std::vector<Event> active;if(!read_segment(root_state_.segment,active))return false;
    for(const auto& e:active){std::unordered_set<std::string> refs;if(!add_reachable(e.object_hash,refs))return false;}
    for(std::size_t b=0;b<kBuckets;++b){std::vector<IndexEntry> entries;if(!read_bucket(b,entries))return false;
      for(const auto& entry:entries){const auto& e=entry.latest;Event actual;if(!event_at(e.sequence,actual)||actual.hash!=e.hash)return false;}}
    ReplayCheckpoint checkpoint;std::string bytes;bool found=false;
    if(!load_replay_checkpoint(checkpoint,bytes,found))return false;
    std::unordered_set<std::string> checkpoint_refs;
    return !found||add_reachable(checkpoint.object_hash,checkpoint_refs);
  }
#ifdef CE_DURABLE_FAULT_TESTS
  void inject_next_publish(FaultSite site)noexcept{fault_=site;}
#endif
};
} // namespace christeye::living::durable
