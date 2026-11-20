// Canonical base and compaction implementation; kept source-separated from
// transaction commit and from interruption recovery.
void NativeStore::load_base(){
  base_={};base_pages_.clear();
  if(root_.base.empty())return;
  directory(path_/"bases");directory(path_/"bases"/"pages");
  const auto raw=read_file(path_/"bases"/(root_.base+".base"),decode_limit);
  if(sha256(raw)!=root_.base)throw Failure(Code::corrupt,"canonical base digest mismatch");
  base_=decode_base(raw);
  if(base_.anchor.store!=root_.store||base_.anchor.generation>root_.generation||base_.anchor.canonical_bytes>root_.canonical_bytes)throw Failure(Code::corrupt,"canonical base scope mismatch");
  for(std::size_t i=0;i<base_.pages.size();++i)if(!base_pages_.emplace(base_.pages[i].digest,i).second)throw Failure(Code::corrupt,"repeated canonical base page");
  if(base_.anchor.generation==root_.generation&&!same_root_witness(base_.anchor,root_))throw Failure(Code::corrupt,"canonical base terminal witness mismatch");
}
std::vector<std::pair<std::string,std::string>> NativeStore::base_page(const BasePage& page){
  const auto raw=read_file(path_/"bases"/"pages"/page.digest,decode_limit/2);
  if(raw.size()!=page.bytes||sha256(raw)!=page.digest)throw Failure(Code::corrupt,"canonical base page digest mismatch");
  const auto f=unseal(raw,"ANAIERO-CANONICAL-BASE-PAGE/1",2,decode_limit/2);
  if(f.size()!=2||f[0]!=root_.store)throw Failure(Code::corrupt,"canonical base page scope mismatch");
  std::vector<std::pair<std::string,std::string>> result;std::string previous;
  for(const auto& value:fields(f[1],128)){const auto r=fields(value,2);if(r.size()!=2||r[0].empty()||(!previous.empty()&&r[0]<=previous))throw Failure(Code::corrupt,"canonical base page ordering mismatch");previous=r[0];result.emplace_back(r[0],r[1]);}
  if(result.empty()||result.size()!=page.rows||result.front().first!=page.first||result.back().first!=page.last)throw Failure(Code::corrupt,"canonical base page range mismatch");
  return result;
}
std::string NativeStore::derived_base_value(const BasePage& page,std::size_t slot,const std::pair<std::string,std::string>& row){
  if(row.first.front()=='r'||row.first.front()=='h'){const auto r=decode_record(row.second);return pack({"record/2",page.digest,number(slot),r.digest});}return row.second;
}
bool NativeStore::base_value(std::string_view key,std::string& out){
  const auto found=std::lower_bound(base_.pages.begin(),base_.pages.end(),key,[](const BasePage& page,std::string_view wanted){return page.last<wanted;});
  if(found==base_.pages.end()||key<found->first)return false;
  const auto rows=base_page(*found);const auto row=std::lower_bound(rows.begin(),rows.end(),key,[](const auto& value,std::string_view wanted){return value.first<wanted;});
  if(row==rows.end()||row->first!=key)return false;
  out=derived_base_value(*found,static_cast<std::size_t>(row-rows.begin()),*row);return true;
}
void NativeStore::initialize_index_from_base(){
  index_root_.clear();index_generation_=0;index_canonical_.clear();
  if(root_.base.empty())return;
  index_root_=build_base_index(*index_,base_);
  index_generation_=base_.anchor.generation;index_canonical_=base_.anchor.digest;
}
Digest NativeStore::build_base_index(SealedIndex& target,const CanonicalBase& base,bool compaction_build){
  std::vector<std::uint64_t> ends;ends.reserve(base.pages.size());std::uint64_t rows=0;
  for(const auto& page:base.pages){rows+=page.rows;ends.push_back(rows);}
  std::size_t loaded=base.pages.size();std::vector<std::pair<std::string,std::string>> cached;
  return target.build_sorted(rows,[&](std::uint64_t ordinal){
    if(compaction_build&&ordinal==rows/2)inject(FaultPoint::compaction_during_index_build);
    const auto index=static_cast<std::size_t>(std::upper_bound(ends.begin(),ends.end(),ordinal)-ends.begin());
    if(index>=base.pages.size())throw Failure(Code::corrupt,"bulk index lookup exceeded verified base");
    const auto& page=base.pages[index];if(index!=loaded){cached=base_page(page);loaded=index;}
    const auto slot=static_cast<std::size_t>(ordinal-(index?ends[index-1]:0));const auto& row=cached.at(slot);
    return std::make_pair(row.first,derived_base_value(page,slot,row));
  });
}
void NativeStore::fill_compaction_report(CompactionReport& report,bool already)const{
  report={};report.plan_digest=base_.plan_digest;report.canonical_base_digest=root_.base;report.generation=base_.anchor.generation;report.operation_epoch=base_.operation_epoch;report.accounted_bytes_before=base_.before_bytes;report.accounted_bytes_after=usage();report.removed_transaction_bytes=base_.removed_transaction_bytes;report.removed_index_bytes=base_.removed_index_bytes;report.summarized_record_versions=base_.summarized_records;report.expired_observations=base_.expired_observations;report.activated=!root_.base.empty();report.cleanup_complete=pending_reclamation_bytes_==0;report.already_complete=already;
}
void NativeStore::cleanup_compaction(){
  const auto marker=path_/"COMPACTION";
  if(!regular_exists(marker)){pending_reclamation_bytes_=0;return;}
  const auto f=compaction_intent(marker);
  if(f[0]!=root_.store||!valid_digest(f[1])||(!f[2].empty()&&!valid_digest(f[2]))||(!f[3].empty()&&!valid_digest(f[3])))throw Failure(Code::corrupt,"invalid compaction cleanup intent");
  const auto through=parse_number(f[4]),first=parse_number(f[5]);pending_reclamation_bytes_=parse_number(f[6]);if(first==0||first>through+1)throw Failure(Code::corrupt,"invalid compaction cleanup range");
  const bool activated=!f[3].empty()&&root_.base==f[3];
  if(activated&&(base_.plan_digest!=f[1]||base_.anchor.generation!=through))throw Failure(Code::corrupt,"compaction cleanup authority mismatch");
  if(!activated&&root_.base!=f[2])throw Failure(Code::corrupt,"compaction cleanup root diverged");
  // Canonical files are removed only after ROOT names the verified replacement.
  // A prepublication interruption only removes unreferenced staging objects.
  if(activated){for(std::uint64_t g=first;g<=through;++g){remove_file(path_/"transactions"/(number(g)+".txn"));inject(FaultPoint::compaction_during_cleanup);if(g==through)break;}}
  const auto bases=path_/"bases",pages=bases/"pages";
  if(fs::exists(bases))directory(bases);
  if(fs::exists(pages))directory(pages);
  if(fs::exists(pages)){for(const auto& entry:fs::directory_iterator(pages)){const auto name=entry.path().filename().string();if(!valid_digest(name))throw Failure(Code::corrupt,"unexpected canonical base page path");if(base_pages_.count(name)==0)remove_file(entry.path());}}
  if(fs::exists(bases)){for(const auto& entry:fs::directory_iterator(bases)){if(entry.path().filename()=="pages")continue;const auto name=entry.path().filename().string();if(name.size()!=69||!name.ends_with(".base")||!valid_digest(std::string_view(name).substr(0,64)))throw Failure(Code::corrupt,"unexpected canonical base manifest path");if(name!=root_.base+".base")remove_file(entry.path());}}
  cleanup_inactive_index();remove_file(marker);pending_reclamation_bytes_=0;
}
void NativeStore::compacted_rows(bool expire_operations,const std::function<void(const std::pair<std::string,std::string>&)>& emit,std::uint64_t& summarized,std::uint64_t& expired){
  summarized=base_.summarized_records;expired=0;
  const auto each=[&](std::string_view prefix,const auto& callback){IndexCursor cursor(*index_,index_root_,std::string(prefix));std::pair<std::string,std::string> row;while(cursor.next(row))callback(row);};
  for(const auto family:{"b","e","f"})each(family,emit);
  each("h",[&](const auto& row){const auto r=indexed_record(row.second);StoredRecord latest;if(!record(r.record.kind,r.record.key,latest))throw Failure(Code::corrupt,"record history has no latest authority");if(r.revision==latest.revision){if(r.digest!=latest.digest)throw Failure(Code::corrupt,"record history/latest identity mismatch");emit({row.first,encode_record(r)});}else ++summarized;});
  const auto is_expired=[&](const ObservationClaim& c){BlobInfo payload,receipt;if(!blob(c.payload_digest,payload)||!blob(c.receipt_digest,receipt))throw Failure(Code::corrupt,"observation has no blob identity fence");return payload.expired_generation!=0&&receipt.expired_generation!=0;};
  std::string group;std::pair<std::string,std::string> boundary;bool pruning=true;
  const auto flush_boundary=[&]{if(!boundary.first.empty()){emit(boundary);boundary={};}};
  each("o",[&](const auto& row){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"invalid observation row during compaction");const auto c=decode_claim(f[1]);const auto next=observation_prefix(c.source_uid,c.epoch);if(next!=group){flush_boundary();group=next;pruning=true;}if(pruning&&is_expired(c)){boundary=row;return;}flush_boundary();pruning=false;emit(row);});flush_boundary();
  if(!expire_operations)each("p",emit);
  // A second bounded pass emits one exact closed-through fence per stream,
  // retaining its last original claim as a boundary witness. No event is made up.
  group.clear();boundary={};pruning=true;std::uint64_t floor=0;
  const auto flush_floor=[&]{if(floor!=0){emit({"q"+group,boundary.second});if(floor>any_generation-expired)throw Failure(Code::capacity,"observation summary counter exhausted");expired+=floor;}floor=0;boundary={};};
  each("o",[&](const auto& row){const auto f=fields(row.second,2);const auto c=decode_claim(f[1]);const auto next=observation_prefix(c.source_uid,c.epoch);if(next!=group){flush_floor();group=next;pruning=true;}if(pruning&&is_expired(c)){floor=c.sequence;boundary=row;}else pruning=false;});flush_floor();
  each("r",[&](const auto& row){emit({row.first,encode_record(indexed_record(row.second))});});
  for(const auto family:{"t","v","x"})each(family,emit);
}
void NativeStore::preview_compaction_locked(const CompactionOptions& options,CompactionPlan& plan,std::uint64_t* working_files){
  require_ready();if(!writable_)throw Failure(Code::busy,"canonical admission is stopped");
  if(!text_key(options.operation_id,256)||options.operation_id.starts_with("__storage/")||options.working_budget_bytes==0)throw Failure(Code::invalid,"invalid compaction operation/budget");
  if(root_.generation==0)throw Failure(Code::invalid,"empty canonical history has no prefix to compact");
  if(options.expected_generation!=any_generation&&options.expected_generation!=root_.generation)throw Failure(Code::conflict,"compaction generation fence changed");
  if(!options.expected_root.empty()&&options.expected_root!=root_.digest)throw Failure(Code::conflict,"compaction root fence changed");
  if(options.expire_operation_receipts&&base_.operation_epoch==any_generation)throw Failure(Code::capacity,"operation epoch exhausted");
  plan={};plan.options=options;plan.options.expected_generation=root_.generation;plan.options.expected_root=root_.digest;plan.generation=root_.generation;plan.source_root=root_.digest;plan.source_base=root_.base;plan.next_operation_epoch=base_.operation_epoch+(options.expire_operation_receipts?1:0);plan.accounted_bytes_before=usage();plan.canonical_transactions_removed=root_.generation-base_.anchor.generation;
  if(!valid_layout(options.target_index_layout))throw Failure(Code::invalid,"unknown target index layout");
  const auto target_layout=options.target_index_layout==IndexLayout::automatic?index_layout_:options.target_index_layout;
  std::uint64_t row_bytes=0,index_bytes=0,largest_node=4096,rows=0;
  std::uint64_t base_page_files=0,base_page_rows=0,base_page_wire=0;
  compacted_rows(options.expire_operation_receipts,[&](const auto& row){++rows;const auto type=row.first.front();if(type=='r')++plan.retained_records;if(type=='b')++plan.retained_blobs;if(type=='p')++plan.retained_operations;const auto row_wire=static_cast<std::uint64_t>(pack({row.first,row.second}).size());
    if(base_page_rows==0||base_page_rows==128||base_page_wire+row_wire>8U*1024U*1024U){
      ++base_page_files;base_page_rows=0;base_page_wire=0;
    }
    ++base_page_rows;base_page_wire+=row_wire;
    const auto canonical=row_wire+128U;const auto indexed=static_cast<std::uint64_t>(row.first.size()+((type=='r'||type=='h')?256U:row.second.size())+512U);if(canonical>any_generation-row_bytes||indexed>any_generation-index_bytes)throw Failure(Code::capacity,"compaction size overflow");row_bytes+=canonical;index_bytes+=indexed;largest_node=std::max(largest_node,indexed);},plan.summarized_record_versions,plan.expired_observations);
  std::uint64_t height=2;for(auto n=rows;n>1;n=(n+1)/2)++height;
  const auto workspace=std::min<std::uint64_t>(rows,128U)*(height+4U)*largest_node*4U+65536U;
  if(row_bytes>any_generation-index_bytes||row_bytes+index_bytes>any_generation-workspace)throw Failure(Code::capacity,"compaction working size overflow");
  plan.required_working_bytes=row_bytes+index_bytes+workspace;
  if(rows>(any_generation-32U)/2U)throw Failure(Code::capacity,"compaction file-node estimate exhausted");
  if(working_files){
    *working_files=target_layout==IndexLayout::packed_pages
      ?base_page_files+PackedIndex::bulk_file_bound(rows,index_bytes)+32U:rows*2U+32U;
  }
  plan.plan_digest=sha256(pack({"ANAIERO-COMPACTION-PLAN/1",options.operation_id,root_.store,number(root_.generation),root_.digest,root_.base,index_root_,number(options.working_budget_bytes),options.expire_operation_receipts?"1":"0",number(plan.next_operation_epoch),number(plan.required_working_bytes),number(plan.retained_records),number(plan.retained_blobs),number(plan.retained_operations),number(plan.summarized_record_versions),number(plan.expired_observations)}));
  if(options.target_index_layout!=IndexLayout::automatic||target_layout==IndexLayout::packed_pages)
    plan.plan_digest=sha256(pack({"ANAIERO-COMPACTION-LAYOUT/1",plan.plan_digest,layout_name(target_layout)}));
}
Result NativeStore::preview_compaction(const CompactionOptions& options,CompactionPlan& plan){std::lock_guard lock(mutex_);try{const auto stable_options=options;preview_compaction_locked(stable_options,plan);return {};}catch(const Failure& e){if(e.code==Code::corrupt){ready_=false;writable_=false;}return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}}
Result NativeStore::execute_compaction(const CompactionPlan& supplied,CompactionReport& report){
  std::lock_guard lock(mutex_);bool activated=false,staged=false;report={};
  CompactionReport sample;
  const auto finish=[&]{
    fill_compaction_report(report);
    report.coexistence_measured=sample.coexistence_measured;
    report.coexistence_files=sample.coexistence_files;report.coexistence_directories=sample.coexistence_directories;
    report.coexistence_old_index_files=sample.coexistence_old_index_files;report.coexistence_new_index_files=sample.coexistence_new_index_files;
  };
  try{
    if(!root_.base.empty()&&base_.plan_digest==supplied.plan_digest){
      const auto target=supplied.options.target_index_layout;
      if(!valid_layout(target)||(target!=IndexLayout::automatic&&target!=index_layout_))
        throw Failure(Code::conflict,"completed compaction has a different target index layout");
      cleanup_compaction();fill_compaction_report(report,true);return {};
    }
    CompactionPlan plan;std::uint64_t working_files=0;preview_compaction_locked(supplied.options,plan,&working_files);
    if(!valid_digest(supplied.plan_digest)||plan.plan_digest!=supplied.plan_digest||plan.source_root!=supplied.source_root||plan.source_base!=supplied.source_base||plan.generation!=supplied.generation||plan.next_operation_epoch!=supplied.next_operation_epoch||plan.accounted_bytes_before!=supplied.accounted_bytes_before||plan.required_working_bytes!=supplied.required_working_bytes||plan.retained_records!=supplied.retained_records||plan.retained_blobs!=supplied.retained_blobs||plan.retained_operations!=supplied.retained_operations||plan.summarized_record_versions!=supplied.summarized_record_versions||plan.expired_observations!=supplied.expired_observations||plan.canonical_transactions_removed!=supplied.canonical_transactions_removed)throw Failure(Code::conflict,"compaction reviewed plan is stale or altered");
    AuditReport verified_history;audit_locked(verified_history);
    const auto used=usage();if(plan.required_working_bytes>plan.options.working_budget_bytes||used>options_.maximum_bytes||plan.required_working_bytes>options_.maximum_bytes-used)throw Failure(Code::capacity,"compaction requires declared temporary and total quota headroom");
    std::error_code space_error;const auto space=fs::space(path_,space_error);if(space_error||space.available<options_.minimum_free_bytes||plan.required_working_bytes>space.available-options_.minimum_free_bytes)throw Failure(Code::capacity,"compaction filesystem working headroom exhausted");
    // The old index remains live until its replacement is verified. Reserve
    // one replacement node and up to one bounded page per retained row, plus
    // manifest, intent and publication files. Bulk construction has no COW spill.
    require_file_capacity(path_,working_files,options_.minimum_free_file_nodes);
    directory(path_/"bases");directory(path_/"bases"/"pages");
    const auto old_index_bytes=index_->load(index_root_).bytes;
    const auto obsolete_bytes=root_.canonical_bytes-base_.anchor.canonical_bytes+base_.storage_bytes+old_index_bytes;
    const auto target_layout=plan.options.target_index_layout==IndexLayout::automatic?index_layout_:plan.options.target_index_layout;
    const auto intent=[&](const Digest& destination){
      std::vector<std::string> fields{root_.store,plan.plan_digest,root_.base,destination,number(root_.generation),number(base_.anchor.generation+1),number(obsolete_bytes)};
      if(target_layout==IndexLayout::single_node)return seal("ANAIERO-COMPACTION-INTENT/1",fields);
      fields.push_back(layout_name(target_layout));return seal("ANAIERO-COMPACTION-INTENT/2",fields);
    };
    write_file(path_/"COMPACTION",intent({}));staged=true;
    CanonicalBase next;next.anchor=root_;next.anchor.base.clear();next.operation_id=plan.options.operation_id;next.plan_digest=plan.plan_digest;next.previous_base=root_.base;next.admitted=last_admitted_unix_s_;next.operation_epoch=plan.next_operation_epoch;next.before_bytes=used;next.removed_transaction_bytes=root_.canonical_bytes-base_.anchor.canonical_bytes;next.removed_index_bytes=old_index_bytes;
    std::vector<std::string> rows;std::string first,last;std::size_t bytes=0;
    const auto flush=[&]{if(rows.empty())return;const auto raw=seal("ANAIERO-CANONICAL-BASE-PAGE/1",{root_.store,pack(rows)});if(raw.size()>decode_limit/2)throw Failure(Code::capacity,"canonical snapshot page exceeds decoder");const auto digest=sha256(raw);const auto destination=path_/"bases"/"pages"/digest;if(regular_exists(destination)){if(read_file(destination,decode_limit/2)!=raw)throw Failure(Code::corrupt,"canonical base CAS collision");}else write_file(destination,raw);next.pages.push_back({digest,first,last,static_cast<std::uint64_t>(raw.size()),static_cast<std::uint64_t>(rows.size())});next.storage_bytes+=raw.size();rows.clear();bytes=0;first.clear();last.clear();if(next.pages.size()>100000)throw Failure(Code::capacity,"bounded canonical base manifest exhausted");};
    compacted_rows(plan.options.expire_operation_receipts,[&](const auto& row){const auto encoded=pack({row.first,row.second});if(!rows.empty()&&(rows.size()==128||bytes+encoded.size()>8U*1024U*1024U))flush();if(first.empty())first=row.first;last=row.first;rows.push_back(encoded);bytes+=encoded.size();++next.counts[static_cast<unsigned char>(row.first.front())];},next.summarized_records,next.expired_observations);flush();
    auto base_raw=encode_base(next);next.storage_bytes+=base_raw.size();base_raw=encode_base(next);const auto digest=sha256(base_raw);write_file(path_/"bases"/(digest+".base"),base_raw);
    // Verify all snapshot bytes before the new ROOT can authorize any deletion.
    auto verified=decode_base(base_raw);if(verified.storage_bytes>plan.required_working_bytes)throw Failure(Code::capacity,"snapshot exceeded admitted working bytes");
    const auto old_directory=index_directory_;const std::string fresh=old_directory=="nodes-a"?"nodes-b":"nodes-a";const auto target=path_/"index"/fresh;
    if(fs::is_symlink(target))throw Failure(Code::corrupt,"compaction index directory symlink rejected");
    std::error_code ec;fs::remove_all(target,ec);if(ec)throw Failure(Code::io_error,"cannot clear compaction index staging");directory(target);
    auto fresh_index=std::make_unique<SealedIndex>(target,target_layout);Digest fresh_root;
    fresh_root=build_base_index(*fresh_index,verified,true);
    inject(FaultPoint::compaction_after_index);
    if(options_.measure_compaction_files)measure_coexistence(path_,index_->directory(),target,sample);
    if(verified.storage_bytes+fresh_index->load(fresh_root).bytes>plan.required_working_bytes)throw Failure(Code::capacity,"compaction exceeded admitted working bytes");
    write_file(path_/"COMPACTION",intent(digest));inject(FaultPoint::compaction_after_base);
    Root published=root_;published.base=digest;write_file(path_/"ROOT",encode_root(published));root_=published;base_=std::move(verified);base_pages_.clear();for(std::size_t i=0;i<base_.pages.size();++i)base_pages_.emplace(base_.pages[i].digest,i);pending_reclamation_bytes_=obsolete_bytes;activated=true;
    index_directory_=fresh;index_layout_=target_layout;index_=std::move(fresh_index);index_root_=fresh_root;index_generation_=root_.generation;index_canonical_=root_.digest;
    inject(FaultPoint::compaction_after_root);save_index();cleanup_compaction();ready_=true;writable_=true;finish();return {};
  }catch(const Failure& e){if(staged||e.code==Code::corrupt){ready_=false;writable_=false;}if(activated)finish();return {activated?Code::committed_degraded:e.code,e.what()};}catch(const std::exception& e){if(staged){ready_=false;writable_=false;}if(activated)finish();return {activated?Code::committed_degraded:Code::io_error,e.what()};}
}
Result NativeStore::resume_compaction(CompactionReport& report){
  // Rebuilding also handles an interruption after ROOT publication but before
  // the derived coverage marker. Cleanup is conditional on the canonical base.
  const auto repaired=repair_index();if(!repaired)return repaired;
  std::lock_guard lock(mutex_);try{cleanup_compaction();fill_compaction_report(report,true);return {};}catch(const Failure& e){ready_=false;writable_=false;return {e.code,e.what()};}catch(const std::exception& e){ready_=false;writable_=false;return {Code::io_error,e.what()};}
}
