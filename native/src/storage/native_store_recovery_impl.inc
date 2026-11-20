// Recovery / inspection concern: load_index
void NativeStore::load_index(){
  const bool current=regular_exists(path_/index_coverage_name);
  const bool legacy=!current&&legacy_index_exists(path_);
  if(!current&&!legacy){
    index_generation_=0;index_root_.clear();index_canonical_.clear();index_directory_="nodes-a";
    index_layout_=options_.index_layout==IndexLayout::packed_pages?IndexLayout::packed_pages:IndexLayout::single_node;
    // The original writer may have published its first canonical ROOT before
    // creating any coverage marker. Replay that retained suffix on open.
  }
  else {
    const auto raw=read_file(path_/(current?index_coverage_name:"INDEX"),16384);const auto envelope=fields(raw,3);
    if(envelope.size()!=3||(envelope[0]!="ANAIERO-INDEX-COVERAGE/2"&&envelope[0]!="ANAIERO-INDEX-COVERAGE/3"&&envelope[0]!="ANAIERO-INDEX-COVERAGE/4"))throw Failure(Code::corrupt,"unsupported index coverage version");
    const auto f=unseal(raw,envelope[0],7);
    const bool packed=envelope[0]=="ANAIERO-INDEX-COVERAGE/4";
    const auto arity=packed?7U:(envelope[0]=="ANAIERO-INDEX-COVERAGE/3"?6U:5U);
    if(f.size()!=arity||f[0]!=root_.store||(f[1]!="nodes-a"&&f[1]!="nodes-b")||
       (packed&&f[6]!="native-packed-pages/1"))throw Failure(Code::corrupt,"invalid index store or layout");
    index_directory_=f[1];index_layout_=packed?IndexLayout::packed_pages:IndexLayout::single_node;
    if((arity>=6?f[5]:Digest{})!=root_.base)throw Failure(Code::corrupt,"index base identity mismatch");
    index_generation_=parse_number(f[2]);index_canonical_=f[3];index_root_=f[4];
    if(index_generation_<base_.anchor.generation||index_generation_>root_.generation||(index_generation_==0&&(!index_root_.empty()||!index_canonical_.empty())))throw Failure(Code::corrupt,"index coverage ahead of canonical root");
    if(index_generation_!=0){const auto digest=index_generation_==base_.anchor.generation?base_.anchor.digest:transaction(index_generation_).root.digest;if(digest!=index_canonical_)throw Failure(Code::corrupt,"index coverage diverges from canonical history");}
  }
  directory(path_/"index"/index_directory_);index_=std::make_unique<SealedIndex>(path_/"index"/index_directory_,index_layout_);
  if(index_generation_<base_.anchor.generation)throw Failure(Code::corrupt,"index is behind the canonical base");
  (void)index_->load(index_root_);index_->finish_cleanup(index_root_);
  if(!current&&!legacy&&index_layout_==IndexLayout::packed_pages)save_index();
  if(legacy){
    // Publish the validated derived marker durably before removing its old
    // spelling. A crash leaves either the original or the preferred new marker;
    // canonical ROOT/transactions and payloads are never renamed or rewritten.
    save_index();remove_file(path_/"INDEX");
  }
}

// Recovery / inspection concern: recover_staging
void NativeStore::recover_staging(){recovery::recover_staging(path_,root_.store,root_.generation,root_.digest);}

// Recovery / inspection concern: replay_suffix
void NativeStore::replay_suffix(){
  while(index_generation_<root_.generation){const auto next=transaction(index_generation_+1);apply_index(next);}
  if(index_generation_!=root_.generation||index_canonical_!=root_.digest)throw Failure(Code::corrupt,"index does not cover the sealed canonical root");
}

// Recovery / inspection concern: cleanup_inactive_index
void NativeStore::cleanup_inactive_index(){recovery::cleanup_inactive_index(path_,index_directory_);}

// Recovery / inspection concern: verify_canonical_history
void NativeStore::verify_canonical_history(){
  Digest previous=base_.anchor.digest;std::uint64_t bytes=base_.anchor.canonical_bytes,admitted=base_.admitted;
  for(const auto& page:base_.pages)(void)base_page(page);
  for(std::uint64_t generation=base_.anchor.generation+1;generation<=root_.generation;++generation){const auto tx=transaction(generation);if(tx.predecessor!=previous||tx.receipt.admitted_unix_s<admitted)throw Failure(Code::corrupt,"canonical history cannot be repaired by rebuilding an index");previous=tx.root.digest;admitted=tx.receipt.admitted_unix_s;bytes+=fs::file_size(path_/"transactions"/(number(generation)+".txn"));if(bytes!=tx.root.canonical_bytes)throw Failure(Code::corrupt,"canonical history byte accounting mismatch");}
  if(previous!=root_.digest||bytes!=root_.canonical_bytes)throw Failure(Code::corrupt,"canonical history does not reach the sealed root");
}

// Recovery / inspection concern: open
Result NativeStore::open(){
  try{
    if(!valid_layout(options_.index_layout)||options_.maximum_bytes<65536||options_.maximum_batch_bytes==0||options_.maximum_batch_bytes>decode_limit/2||options_.maximum_blob_bytes==0||options_.maximum_records==0||options_.maximum_records>1024||options_.maximum_blobs==0||options_.maximum_blobs>1024||options_.maximum_references>4096||options_.maximum_index_updates==0||options_.maximum_index_updates>1024)return {Code::invalid,"invalid native storage limits"};
    directory(path_);directory(path_/"transactions");directory(path_/"blobs");directory(path_/"index");
    (void)regular_exists(path_/"WRITER");if(!writer_.open(path_/"WRITER")||!writer_.lock_exclusive())return {Code::busy,"canonical store already has a writer"};
    if(!regular_exists(path_/"ROOT")){
      if(regular_exists(path_/index_coverage_name)||legacy_index_exists(path_)||regular_exists(path_/"STAGING")||!fs::is_empty(path_/"transactions")||!fs::is_empty(path_/"blobs")||(fs::exists(path_/"bases")&&!fs::is_empty(path_/"bases")))throw Failure(Code::corrupt,"canonical root missing from initialized storage");
      std::random_device random;root_.store=sha256(std::to_string(random())+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+path_.string());write_file(path_/"ROOT",encode_root(root_));
    }else root_=decode_root(read_file(path_/"ROOT",16384));
    generation_.store(root_.generation);load_base();last_admitted_unix_s_=base_.admitted;
    if(root_.generation>base_.anchor.generation){const auto t=transaction(root_.generation);if(!same_root_witness(t.root,root_))throw Failure(Code::corrupt,"canonical root/transaction mismatch");last_admitted_unix_s_=t.receipt.admitted_unix_s;}
    if(regular_exists(path_/"COMPACTION")){const auto pending=compaction_intent(path_/"COMPACTION");pending_reclamation_bytes_=parse_number(pending[6]);}
    recover_staging();
    try{load_index();replay_suffix();cleanup_compaction();cleanup_inactive_index();ready_=true;writable_=true;}
    catch(const Failure&){ready_=false;writable_=false;} // Authoritative data is retained for explicit repair.
    return {};
  }catch(const Failure& e){return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}
}

// Recovery / inspection concern: repair_index
Result NativeStore::repair_index(){
  std::lock_guard lock(mutex_);
  try{
    // A failed rename/directory flush may have made ROOT visible before the
    // caller observed completion. Reconcile it before removing staged objects.
    root_=decode_root(read_file(path_/"ROOT",16384));generation_.store(root_.generation);load_base();last_admitted_unix_s_=base_.admitted;
    if(root_.generation>base_.anchor.generation){const auto t=transaction(root_.generation);if(!same_root_witness(t.root,root_))throw Failure(Code::corrupt,"repair root witness mismatch");last_admitted_unix_s_=t.receipt.admitted_unix_s;}
    recover_staging();
    verify_canonical_history();
    bool suffix=false;
    try{load_index();index_->audit(index_root_);suffix=true;}catch(const Failure&){suffix=false;}
    if(suffix){try{replay_suffix();cleanup_compaction();cleanup_inactive_index();ready_=true;writable_=true;return {};}catch(const Failure&){suffix=false;}}
    (void)suffix;
    const auto old_directory=index_directory_;const auto fresh=old_directory=="nodes-a"?"nodes-b":"nodes-a";
    const auto target=path_/"index"/fresh;
    if(fs::is_symlink(target))throw Failure(Code::corrupt,"rebuild directory symlink rejected");
    std::error_code ec;fs::remove_all(target,ec);if(ec)throw Failure(Code::io_error,"cannot clear inactive index generation");directory(target);
    if(regular_exists(path_/"COMPACTION")){
      const auto intent=compaction_intent(path_/"COMPACTION");
      if(!intent[3].empty()&&intent[3]==root_.base)index_layout_=read_layout(intent[7]);
    }
    index_directory_=fresh;index_=std::make_unique<SealedIndex>(target,index_layout_);index_root_.clear();index_generation_=0;index_canonical_.clear();
    // Coverage is advanced after each sealed transaction. A rebuild interrupted
    // after any published prefix resumes through the same canonical suffix.
    initialize_index_from_base();save_index();replay_suffix();cleanup_compaction();ready_=true;writable_=true;
    const auto old=path_/"index"/old_directory;if(old!=target){fs::remove_all(old,ec);if(ec)throw Failure(Code::io_error,"obsolete index generation cleanup failed");}
    return {};
  }catch(const Failure& e){ready_=false;writable_=false;return {e.code,e.what()};}catch(const std::exception& e){ready_=false;writable_=false;return {Code::io_error,e.what()};}
}

// Recovery / inspection concern: audit_compacted
void NativeStore::audit_compacted(AuditReport& report){
  verify_canonical_history();report={};report.canonical_base_digest=root_.base;report.base_pages=base_.pages.size();report.summarized_generations=base_.anchor.generation;report.summarized_record_versions=base_.summarized_records;report.expired_observations=base_.expired_observations;report.records=base_.counts['h'];
  std::array<std::uint64_t,256> base_counts{};
  for(const auto& page:base_.pages)for(const auto& row:base_page(page)){++base_counts[static_cast<unsigned char>(row.first.front())];if(row.first.front()=='r'||row.first.front()=='h'){const auto r=decode_record(row.second);if(r.generation>base_.anchor.generation||row.first!=(row.first.front()=='r'?record_key(r.record.kind,r.record.key):history_key(r)))throw Failure(Code::corrupt,"canonical base record witness mismatch");++report.canonical_records_read;}}
  if(base_counts!=base_.counts||base_counts['r']!=base_.anchor.records||base_counts['h']!=base_counts['r']||base_counts['b']!=base_.anchor.blobs||base_counts['v']!=base_.anchor.blobs)throw Failure(Code::corrupt,"canonical base coverage mismatch");
  std::uint64_t claims=base_.counts['o'],events=base_.counts['x'],tips=base_.counts['t'],operations=base_.counts['p'];
  for(std::uint64_t g=base_.anchor.generation+1;g<=root_.generation;++g){const auto t=transaction(g);++report.generations;++operations;report.records+=t.records.size();report.canonical_records_read+=t.records.size()+t.observations.size()+t.events.size();claims+=t.observations.size();events+=t.events.size();for(const auto& c:t.observations)if(c.sequence==1)++tips;}
  const auto before=index_->stats.nodes_read;index_->audit(index_root_);
  std::array<std::uint64_t,256> counts{};std::array<std::uint64_t,5> expected_references{},indexed_references{};
  const auto fingerprint=[](auto& aggregate,std::string_view digest,std::uint64_t count){if(!valid_digest(digest))throw Failure(Code::corrupt,"invalid reference fingerprint identity");aggregate[0]+=count;for(std::size_t i=0;i<4;++i)aggregate[i+1]+=parse_number(digest.substr(i*16,16))*count;};
  std::size_t page_number=0;std::vector<std::pair<std::string,std::string>> cached_rows;std::size_t cached_number=base_.pages.size();
  const auto baseline=[&](const auto& row){while(page_number<base_.pages.size()&&base_.pages[page_number].last<row.first)++page_number;if(page_number==base_.pages.size()||row.first<base_.pages[page_number].first)return false;if(cached_number!=page_number){cached_rows=base_page(base_.pages[page_number]);cached_number=page_number;}const auto found=std::lower_bound(cached_rows.begin(),cached_rows.end(),row.first,[](const auto& item,const auto& wanted){return item.first<wanted;});return found!=cached_rows.end()&&found->first==row.first&&derived_base_value(base_.pages[page_number],static_cast<std::size_t>(found-cached_rows.begin()),*found)==row.second;};
  IndexCursor all(*index_,index_root_,"");std::pair<std::string,std::string> row;
  while(all.next(row)){
    const auto type=row.first.front();++counts[static_cast<unsigned char>(type)];const bool from_base=baseline(row);
    if(type=='r'||type=='h'){const auto r=indexed_record(row.second);if(row.first!=(type=='r'?record_key(r.record.kind,r.record.key):history_key(r)))throw Failure(Code::corrupt,"record index has no canonical key");if(!from_base&&r.generation<=base_.anchor.generation)throw Failure(Code::corrupt,"record index differs from canonical base");if(type=='r'&&!r.record.tombstone)for(const auto& ref:r.record.references)fingerprint(expected_references,ref,1);}
    else if(type=='p'){const auto receipt=decode_receipt(row.second);if(row.first!="p"+receipt.operation_id)throw Failure(Code::corrupt,"operation index key mismatch");if(!from_base){if(receipt.generation<=base_.anchor.generation||encode_receipt(transaction(receipt.generation).receipt)!=row.second)throw Failure(Code::corrupt,"operation index lacks retained canonical witness");}}
    else if(type=='o'||type=='t'||type=='q'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad observation/floor index");const auto c=decode_claim(f[1]);if(row.first!=(type=='o'?observation_key(c):std::string(1,type)+observation_prefix(c.source_uid,c.epoch)))throw Failure(Code::corrupt,"observation/floor key mismatch");if(type=='q'&&!from_base)throw Failure(Code::corrupt,"unwitnessed observation expiry floor");if(!from_base){const auto generation=parse_number(f[0]);if(generation<=base_.anchor.generation)throw Failure(Code::corrupt,"observation index differs from canonical base");const auto t=transaction(generation);bool match=false;for(const auto& claim:t.observations)if(encode_claim(claim)==f[1])match=true;if(!match)throw Failure(Code::corrupt,"observation index lacks retained canonical witness");}}
    else if(type=='e'||type=='x'){const auto e=decode_event(row.second);if(row.first!=(type=='e'?"e"+event_tuple(e.event.key):exact_event_key(e.event)))throw Failure(Code::corrupt,"event index key mismatch");if(!from_base){if(e.generation<=base_.anchor.generation)throw Failure(Code::corrupt,"event index differs from canonical base");const auto t=transaction(e.generation);if(e.position>=t.events.size()||encode_event(t.events[static_cast<std::size_t>(e.position)])!=row.second)throw Failure(Code::corrupt,"event index lacks retained canonical witness");}}
    else if(type=='b'){
      const auto b=decode_blob(row.second);if(row.first!="b"+b.digest)throw Failure(Code::corrupt,"blob index key mismatch");++report.blobs;
      if(!from_base){
        if(b.created_generation<=base_.anchor.generation){std::string raw;if(!base_value(row.first,raw))throw Failure(Code::corrupt,"blob has no canonical base creation witness");const auto original=decode_blob(raw);if(original.ordinal!=b.ordinal||original.bytes!=b.bytes||original.references!=b.references||original.created_generation!=b.created_generation||(original.expired_generation!=0&&original.expired_generation!=b.expired_generation)||(!original.present&&b.present))throw Failure(Code::corrupt,"blob lifecycle differs from canonical base");}
        else {const auto creation=transaction(b.created_generation);bool found=false;for(const auto& candidate:creation.blobs)if(candidate.digest==b.digest&&candidate.ordinal==b.ordinal&&candidate.bytes==b.bytes&&candidate.references==b.references&&candidate.expired_generation==0&&candidate.present)found=true;if(!found)throw Failure(Code::corrupt,"blob lacks canonical creation witness");}
        if(b.expired_generation>base_.anchor.generation){const auto expiry=transaction(b.expired_generation);bool found=false;for(const auto& candidate:expiry.blobs)if(candidate.digest==b.digest&&candidate.expired_generation==b.expired_generation)found=true;if(!found)throw Failure(Code::corrupt,"blob lacks canonical expiry witness");}
      }
      if(b.expired_generation!=0){++report.expired_blobs;continue;}
      for(const auto& ref:b.references)fingerprint(expected_references,ref,1);
      const auto raw=read_file(path_/"blobs"/b.digest,options_.maximum_blob_bytes);if(raw.size()!=b.bytes||sha256(raw)!=b.digest)throw Failure(Code::corrupt,"retained blob digest mismatch");
    }
    else if(type=='f'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad reference index");if(!from_base){const auto generation=parse_number(f[0]);if(generation<=base_.anchor.generation)throw Failure(Code::corrupt,"reference differs from canonical base");const auto t=transaction(generation);bool found=false;for(const auto& ref:t.references)if(row.first=="f"+ref.first&&f[1]==number(ref.second))found=true;if(!found)throw Failure(Code::corrupt,"reference lacks canonical witness");}fingerprint(indexed_references,std::string_view(row.first).substr(1),parse_number(f[1]));}
    else if(type=='v'){BlobInfo b;if(!blob(row.second,b)||row.first!="v"+number(b.ordinal))throw Failure(Code::corrupt,"blob inventory mismatch");}
    else throw Failure(Code::corrupt,"unknown compacted index entry type");
  }
  if(counts['r']!=root_.records||counts['h']!=report.records||counts['p']!=operations||counts['o']!=claims||counts['t']!=tips||counts['q']!=base_.counts['q']||counts['x']!=events||counts['b']!=root_.blobs||counts['v']!=root_.blobs||expected_references!=indexed_references)throw Failure(Code::corrupt,"compacted index coverage/reference mismatch");
  for(const auto& family:std::array<std::pair<std::string,std::string>,3>{{{"h","r"},{"o","t"},{"x","e"}}}){
    IndexCursor history(*index_,index_root_,family.first),current(*index_,index_root_,family.second);std::pair<std::string,std::string> item,actual;std::string group,latest_value;std::uint64_t revision=0,generation=0,position=0;
    const auto finish=[&]{if(group.empty())return;if(!current.next(actual)||actual.first!=group)throw Failure(Code::corrupt,"current index differs from compacted history");if(family.first=="h"){const auto a=indexed_record(actual.second),b=indexed_record(latest_value);if(a.digest!=b.digest||a.generation!=b.generation||a.revision!=b.revision)throw Failure(Code::corrupt,"latest record differs from compacted history");}else if(actual.second!=latest_value)throw Failure(Code::corrupt,"latest source/event differs from compacted history");};
    while(history.next(item)){
      std::string key;std::uint64_t next_revision=0,next_generation=0,next_position=0;
      if(family.first=="h"){const auto r=indexed_record(item.second);key=record_key(r.record.kind,r.record.key);next_revision=r.revision;next_generation=r.generation;}
      else if(family.first=="o"){const auto f=fields(item.second,2);const auto c=decode_claim(f[1]);key="t"+observation_prefix(c.source_uid,c.epoch);next_revision=c.sequence;next_generation=parse_number(f[0]);}
      else {const auto e=decode_event(item.second);key="e"+event_tuple(e.event.key);next_generation=e.generation;next_position=e.position;}
      if(key!=group){finish();group=key;latest_value.clear();revision=0;generation=0;position=0;if(family.first=="h"){std::string original;if(base_value(key,original))revision=indexed_record(original).revision-1;}else if(family.first=="o"){bool found=false;const auto floor=value("q"+key.substr(1),found);if(found){const auto f=fields(floor,2);revision=decode_claim(f[1]).sequence-1;}}}
      if(family.first!="x"){if(next_revision!=revision+1||next_generation<generation||(family.first=="h"&&next_generation==generation))throw Failure(Code::corrupt,"compacted history has a gap/reversal");revision=next_revision;generation=next_generation;latest_value=item.second;}
      else if(latest_value.empty()||next_generation>generation||(next_generation==generation&&next_position>position)){generation=next_generation;position=next_position;latest_value=item.second;}
    }
    finish();if(current.next(actual))throw Failure(Code::corrupt,"current index has an unwitnessed compacted identity");
  }
  report.index_nodes_read=index_->stats.nodes_read-before;
}

// Recovery / inspection concern: audit_locked
void NativeStore::audit_locked(AuditReport& report){
  if(!root_.base.empty()){audit_compacted(report);return;}
  report={};Digest previous;std::uint64_t canonical_bytes=0,claims=0,events=0,tips=0,admitted=0;
  for(std::uint64_t g=1;g<=root_.generation;++g){const auto t=transaction(g);++report.generations;report.records+=t.records.size();report.canonical_records_read+=t.records.size()+t.observations.size()+t.events.size();claims+=t.observations.size();events+=t.events.size();for(const auto& c:t.observations)if(c.sequence==1)++tips;if(t.predecessor!=previous||t.receipt.admitted_unix_s<admitted)throw Failure(Code::corrupt,"historical canonical predecessor/admission order mismatch");admitted=t.receipt.admitted_unix_s;previous=t.root.digest;canonical_bytes+=fs::file_size(path_/"transactions"/(number(g)+".txn"));if(t.root.canonical_bytes!=canonical_bytes)throw Failure(Code::corrupt,"canonical byte accounting mismatch");}
  if(previous!=root_.digest||canonical_bytes!=root_.canonical_bytes)throw Failure(Code::corrupt,"canonical audit root mismatch");
  const auto before=index_->stats.nodes_read;index_->audit(index_root_);
  std::array<std::uint64_t,256> counts{};
  std::array<std::uint64_t,5> expected_references{},indexed_references{};
  const auto reference_fingerprint=[](std::array<std::uint64_t,5>& aggregate,std::string_view digest,std::uint64_t count){if(!valid_digest(digest))throw Failure(Code::corrupt,"invalid reference fingerprint identity");aggregate[0]+=count;for(std::size_t i=0;i<4;++i)aggregate[i+1]+=parse_number(digest.substr(i*16,16))*count;};
  std::vector<std::pair<std::string,std::string>> rows;bool more=true;std::string after;
  // Each index entry points to a bounded canonical transaction. This validates
  // historical and latest indexes without scanning history per observation.
  while(more){index_->scan(index_root_,"",after,128,rows,more);for(const auto& row:rows){after=row.first;const auto type=row.first.front();++counts[static_cast<unsigned char>(type)];
    if(type=='r'||type=='h'){const auto r=indexed_record(row.second);if(row.first!=(type=='r'?record_key(r.record.kind,r.record.key):history_key(r)))throw Failure(Code::corrupt,"record index has no matching canonical key");if(type=='r'&&!r.record.tombstone)for(const auto& ref:r.record.references)reference_fingerprint(expected_references,ref,1);}
    else if(type=='p'){const auto receipt=decode_receipt(row.second);const auto t=transaction(receipt.generation);if(encode_receipt(t.receipt)!=encode_receipt(receipt)||row.first!="p"+receipt.operation_id)throw Failure(Code::corrupt,"operation index has no canonical witness");}
    else if(type=='o'||type=='t'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad observation index");const auto claim=decode_claim(f[1]);if(row.first!=(type=='o'?observation_key(claim):"t"+observation_prefix(claim.source_uid,claim.epoch)))throw Failure(Code::corrupt,"observation index key mismatch");const auto t=transaction(parse_number(f[0]));bool match=false;for(const auto& c:t.observations)if(encode_claim(c)==f[1]){match=true;break;}if(!match)throw Failure(Code::corrupt,"observation index has no canonical witness");}
    else if(type=='e'||type=='x'){const auto e=decode_event(row.second);const auto t=transaction(e.generation);if(row.first!=(type=='e'?"e"+event_tuple(e.event.key):exact_event_key(e.event))||e.position>=t.events.size()||encode_event(t.events[static_cast<std::size_t>(e.position)])!=row.second)throw Failure(Code::corrupt,"event index has no canonical witness");}
    else if(type=='b'){
      const auto b=decode_blob(row.second);if(row.first!="b"+b.digest)throw Failure(Code::corrupt,"blob index key mismatch");++report.blobs;
      const auto creation=transaction(b.created_generation);bool created=false;for(const auto& candidate:creation.blobs)if(candidate.digest==b.digest&&candidate.ordinal==b.ordinal&&candidate.bytes==b.bytes&&candidate.references==b.references&&candidate.expired_generation==0&&candidate.present){created=true;break;}if(!created)throw Failure(Code::corrupt,"blob has no canonical creation witness");
      if(b.expired_generation!=0){++report.expired_blobs;const auto expiry=transaction(b.expired_generation);bool expired=false;for(const auto& candidate:expiry.blobs)if(candidate.digest==b.digest&&candidate.expired_generation==b.expired_generation){expired=true;break;}if(!expired)throw Failure(Code::corrupt,"blob has no canonical expiry witness");continue;}
      for(const auto& ref:b.references)reference_fingerprint(expected_references,ref,1);
      const auto raw=read_file(path_/"blobs"/b.digest,options_.maximum_blob_bytes);if(raw.size()!=b.bytes||sha256(raw)!=b.digest)throw Failure(Code::corrupt,"historical audit blob digest mismatch");
    }
    else if(type=='f'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad reference index");const auto t=transaction(parse_number(f[0]));bool match=false;for(const auto& ref:t.references)if(row.first=="f"+ref.first&&f[1]==number(ref.second)){match=true;break;}if(!match)throw Failure(Code::corrupt,"reference index has no canonical witness");reference_fingerprint(indexed_references,std::string_view(row.first).substr(1),parse_number(f[1]));}
    else if(type=='v'){BlobInfo b;if(!blob(row.second,b)||row.first!="v"+number(b.ordinal))throw Failure(Code::corrupt,"blob inventory index mismatch");}
    else throw Failure(Code::corrupt,"unknown index entry type");
  }}
  if(counts['r']!=root_.records||counts['h']!=report.records||counts['p']!=root_.generation||counts['o']!=claims||counts['t']!=tips||counts['x']!=events||counts['b']!=root_.blobs||counts['v']!=root_.blobs||expected_references!=indexed_references)throw Failure(Code::corrupt,"index coverage/reference aggregate mismatch");
  // Merge the ordered immutable occurrence histories with current directories.
  // Only two fixed-size pages and one group accumulator are resident, even for
  // fully retired identities. No audit observation triggers a historical scan.
  for(const auto& family:std::array<std::pair<std::string,std::string>,3>{{{"h","r"},{"o","t"},{"x","e"}}}){
    IndexCursor history(*index_,index_root_,family.first),current(*index_,index_root_,family.second);std::pair<std::string,std::string> row,actual;std::string group,latest_value;std::uint64_t revision=0,generation=0,position=0;
    const auto finish=[&]{if(group.empty())return;if(!current.next(actual)||actual.first!=group||actual.second!=latest_value)throw Failure(Code::corrupt,"current index differs from ordered canonical history");};
    while(history.next(row)){
      std::string key;std::uint64_t next_revision=0,next_generation=0,next_position=0;
      if(family.first=="h"){const auto r=indexed_record(row.second);key=record_key(r.record.kind,r.record.key);next_revision=r.revision;next_generation=r.generation;}
      else if(family.first=="o"){const auto f=fields(row.second,2);const auto c=decode_claim(f[1]);key="t"+observation_prefix(c.source_uid,c.epoch);next_revision=c.sequence;next_generation=parse_number(f[0]);}
      else {const auto e=decode_event(row.second);key="e"+event_tuple(e.event.key);next_generation=e.generation;next_position=e.position;}
      if(key!=group){finish();group=key;latest_value.clear();revision=0;generation=0;position=0;}
      if(family.first!="x"){if(next_revision!=revision+1||next_generation<generation||(family.first=="h"&&next_generation==generation))throw Failure(Code::corrupt,"record/source history revision gap or reversal");revision=next_revision;generation=next_generation;latest_value=row.second;}
      else if(latest_value.empty()||next_generation>generation||(next_generation==generation&&next_position>position)){generation=next_generation;position=next_position;latest_value=row.second;}
    }
    finish();if(current.next(actual))throw Failure(Code::corrupt,"current index contains an unwitnessed identity");
  }
  report.index_nodes_read=index_->stats.nodes_read-before;
}

// Recovery / inspection concern: audit
Result NativeStore::audit(AuditReport& report){return read([&]{audit_locked(report);});}
