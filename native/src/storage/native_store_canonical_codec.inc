// Wire and canonical index record formats; compiled in the native store TU
// to preserve private invariants and exact historical byte layouts.
std::vector<std::string> fields(std::string_view bytes,std::size_t max=100000){
  std::vector<std::string> f;if(!unpack(bytes,f,max,decode_limit))throw Failure(Code::corrupt,"invalid canonical field list");return f;
}
bool text_key(std::string_view v,std::size_t maximum=1024){return !v.empty()&&v.size()<=maximum&&v.find('\0')==std::string_view::npos;}
std::string record_key(std::string_view kind,std::string_view key){return "r"+std::string(kind)+'\0'+std::string(key);}
std::string history_key(const StoredRecord& r){return "h"+r.record.kind+'\0'+r.record.key+'\0'+number(r.revision);}
std::string observation_prefix(std::string_view source,std::uint64_t epoch){return pack({std::string(source),number(epoch)});}
std::string observation_key(const ObservationClaim& c){return "o"+observation_prefix(c.source_uid,c.epoch)+number(c.sequence);}
std::string event_tuple(const EventKey& e){return pack({e.kind,e.entity,e.clock,e.producer,number(e.source_tick)});}
std::string exact_event_key(const RegistryEvent& e){return "x"+event_tuple(e.key)+pack({e.object_digest,e.origin});}
Bytes encode_record(const StoredRecord& r){return pack({r.record.kind,r.record.key,r.record.payload,pack(r.record.references),r.record.tombstone?"1":"0",number(r.revision),number(r.generation),r.digest});}
StoredRecord decode_record(std::string_view bytes){
  const auto f=fields(bytes,8);if(f.size()!=8||(f[4]!="0"&&f[4]!="1"))throw Failure(Code::corrupt,"invalid stored record");
  StoredRecord r{{f[0],f[1],f[2],fields(f[3]),f[4]=="1"},parse_number(f[5]),parse_number(f[6]),f[7]};
  if(r.revision==0||r.generation==0||r.digest!=record_digest(r.record))throw Failure(Code::corrupt,"stored record identity mismatch");
  return r;
}
Bytes encode_blob(const BlobInfo& b){return pack({b.digest,number(b.ordinal),number(b.bytes),number(b.created_generation),number(b.expired_generation),b.present?"1":"0",pack(b.references)});}
BlobInfo decode_blob(std::string_view bytes){
  const auto f=fields(bytes,7);if(f.size()!=7||(f[5]!="0"&&f[5]!="1")||!valid_digest(f[0]))throw Failure(Code::corrupt,"invalid stored blob metadata");
  BlobInfo b{f[0],parse_number(f[1]),parse_number(f[2]),parse_number(f[3]),parse_number(f[4]),f[5]=="1",fields(f[6])};
  if(b.ordinal==0||b.created_generation==0||(!b.present&&b.expired_generation==0))throw Failure(Code::corrupt,"invalid blob lifecycle metadata");
  return b;
}
Bytes encode_claim(const ObservationClaim& c){return pack({c.source_uid,number(c.epoch),number(c.sequence),c.payload_digest,c.receipt_digest});}
ObservationClaim decode_claim(std::string_view bytes){
  const auto f=fields(bytes,5);if(f.size()!=5||!text_key(f[0])||!valid_digest(f[3])||!valid_digest(f[4]))throw Failure(Code::corrupt,"invalid observation claim");
  ObservationClaim c{f[0],parse_number(f[1]),parse_number(f[2]),f[3],f[4]};if(c.sequence==0)throw Failure(Code::corrupt,"zero observation sequence");return c;
}
Bytes encode_event(const StoredEvent& e){return pack({event_tuple(e.event.key),e.event.object_digest,e.event.origin,number(e.generation),number(e.position),e.digest});}
StoredEvent decode_event(std::string_view bytes){
  const auto f=fields(bytes,6);if(f.size()!=6)throw Failure(Code::corrupt,"invalid stored event");const auto k=fields(f[0],5);if(k.size()!=5)throw Failure(Code::corrupt,"invalid event tuple");
  StoredEvent e{{{k[0],k[1],k[2],k[3],parse_number(k[4])},f[1],f[2]},parse_number(f[3]),parse_number(f[4]),f[5]};
  if(e.digest!=sha256(pack({f[0],f[1],f[2]})))throw Failure(Code::corrupt,"event identity mismatch");
  return e;
}
Bytes encode_authorizations(const std::vector<AuthorizationGuard>& authorizations){std::vector<std::string> values;for(const auto& a:authorizations)values.push_back(pack({a.principal,a.scope,number(a.credential_generation),number(a.not_after_unix_s)}));return pack(values);}
std::vector<AuthorizationGuard> decode_authorizations(std::string_view encoded){std::vector<AuthorizationGuard> result;for(const auto& raw:fields(encoded,16)){const auto f=fields(raw,4);if(f.size()!=4||!text_key(f[0],256)||!text_key(f[1],1024))throw Failure(Code::corrupt,"invalid admission authorization");result.push_back({f[0],f[1],parse_number(f[2]),parse_number(f[3])});}return result;}
Bytes encode_receipt(const CommitReceipt& r){return pack({r.operation_id,r.request_digest,r.batch_digest,r.root_digest,number(r.generation),number(r.admitted_unix_s),encode_authorizations(r.authorizations),number(static_cast<std::uint64_t>(r.commit_class))});}
CommitReceipt decode_receipt(std::string_view bytes){
  const auto f=fields(bytes,8);if((f.size()!=5&&f.size()!=7&&f.size()!=8)||!valid_digest(f[1])||!valid_digest(f[2])||!valid_digest(f[3]))throw Failure(Code::corrupt,"invalid commit receipt");CommitReceipt r{f[0],f[1],f[2],f[3],parse_number(f[4]),false};if(f.size()>=7){r.admitted_unix_s=parse_number(f[5]);r.authorizations=decode_authorizations(f[6]);}if(f.size()==8){const auto kind=parse_number(f[7]);if(kind>1)throw Failure(Code::corrupt,"invalid canonical commit class");r.commit_class=static_cast<CommitClass>(kind);}return r;
}
struct Root {
  std::string store;
  std::uint64_t generation=0,blobs=0,records=0,canonical_bytes=0,blob_bytes=0;
  Digest digest,base;
};
Bytes encode_root(const Root& r){
  std::vector<std::string> f{r.store,number(r.generation),r.digest,number(r.blobs),number(r.records),number(r.canonical_bytes),number(r.blob_bytes)};
  if(!r.base.empty())f.push_back(r.base);
  return seal(r.base.empty()?"ANAIERO-CANONICAL-ROOT/1":"ANAIERO-CANONICAL-ROOT/2",f);
}
Root decode_root(std::string_view data){
  const auto envelope=fields(data,3);if(envelope.size()!=3||(envelope[0]!="ANAIERO-CANONICAL-ROOT/1"&&envelope[0]!="ANAIERO-CANONICAL-ROOT/2"))throw Failure(Code::corrupt,"unsupported canonical root version");
  const auto f=unseal(data,envelope[0],8);if(f.size()!=(envelope[0]=="ANAIERO-CANONICAL-ROOT/1"?7U:8U))throw Failure(Code::corrupt,"invalid canonical root");
  Root r{f[0],parse_number(f[1]),parse_number(f[3]),parse_number(f[4]),parse_number(f[5]),parse_number(f[6]),f[2],f.size()==8?f[7]:Digest{}};
  if(!valid_digest(r.store)||(r.generation!=0&&!valid_digest(r.digest))||(r.generation==0&&!r.digest.empty())||(!r.base.empty()&&!valid_digest(r.base)))throw Failure(Code::corrupt,"invalid root identity");
  return r;
}
bool same_root_witness(Root a,Root b){a.base.clear();b.base.clear();return encode_root(a)==encode_root(b);}
struct BasePage {Digest digest;std::string first,last;std::uint64_t bytes=0,rows=0;};
struct CanonicalBase {
  Root anchor;
  std::string operation_id;
  Digest plan_digest,previous_base;
  std::uint64_t admitted=0,operation_epoch=0,storage_bytes=0;
  std::uint64_t summarized_records=0,expired_observations=0;
  std::uint64_t before_bytes=0,removed_transaction_bytes=0,removed_index_bytes=0;
  std::array<std::uint64_t,256> counts{};
  std::vector<BasePage> pages;
};
Bytes encode_base(const CanonicalBase& b){
  std::vector<std::string> pages,counts;for(const auto& p:b.pages)pages.push_back(pack({p.digest,p.first,p.last,number(p.bytes),number(p.rows)}));
  for(std::size_t i=0;i<b.counts.size();++i)if(b.counts[i]!=0)counts.push_back(pack({std::string(1,static_cast<char>(i)),number(b.counts[i])}));
  Root anchor=b.anchor;anchor.base.clear();
  return seal("ANAIERO-CANONICAL-BASE/1",{encode_root(anchor),b.operation_id,b.plan_digest,b.previous_base,number(b.admitted),number(b.operation_epoch),number(b.storage_bytes),number(b.summarized_records),number(b.expired_observations),number(b.before_bytes),number(b.removed_transaction_bytes),number(b.removed_index_bytes),pack(counts),pack(pages)});
}
CanonicalBase decode_base(std::string_view raw){
  const auto f=unseal(raw,"ANAIERO-CANONICAL-BASE/1",14);if(f.size()!=14)throw Failure(Code::corrupt,"invalid canonical base manifest");
  CanonicalBase b;b.anchor=decode_root(f[0]);b.operation_id=f[1];b.plan_digest=f[2];b.previous_base=f[3];b.admitted=parse_number(f[4]);b.operation_epoch=parse_number(f[5]);b.storage_bytes=parse_number(f[6]);b.summarized_records=parse_number(f[7]);b.expired_observations=parse_number(f[8]);b.before_bytes=parse_number(f[9]);b.removed_transaction_bytes=parse_number(f[10]);b.removed_index_bytes=parse_number(f[11]);
  if(!text_key(b.operation_id,256)||!valid_digest(b.plan_digest)||(!b.previous_base.empty()&&!valid_digest(b.previous_base))||!b.anchor.base.empty()||b.anchor.generation==0)throw Failure(Code::corrupt,"invalid canonical base identity");
  for(const auto& value:fields(f[12],256)){const auto c=fields(value,2);if(c.size()!=2||c[0].size()!=1||b.counts[static_cast<unsigned char>(c[0][0])]!=0)throw Failure(Code::corrupt,"invalid base row counts");b.counts[static_cast<unsigned char>(c[0][0])]=parse_number(c[1]);}
  std::string previous;std::uint64_t bytes=static_cast<std::uint64_t>(raw.size());
  for(const auto& value:fields(f[13],100000)){const auto p=fields(value,5);if(p.size()!=5||!valid_digest(p[0])||p[1].empty()||p[2]<p[1]||(!previous.empty()&&p[1]<=previous))throw Failure(Code::corrupt,"invalid ordered canonical base page");BasePage page{p[0],p[1],p[2],parse_number(p[3]),parse_number(p[4])};if(page.rows==0||page.rows>128||page.bytes>decode_limit/2)throw Failure(Code::corrupt,"invalid bounded canonical base page");previous=page.last;if(page.bytes>any_generation-bytes)throw Failure(Code::corrupt,"base size overflow");bytes+=page.bytes;b.pages.push_back(std::move(page));}
  if(bytes!=b.storage_bytes)throw Failure(Code::corrupt,"canonical base byte accounting mismatch");
  return b;
}
std::uint64_t operation_epoch(std::string_view id){
  if(!id.starts_with(operation_epoch_prefix))return 0;
  if(id.size()<=operation_logical_offset||id[operation_logical_offset-1]!='/'||id.substr(operation_logical_offset).starts_with(operation_epoch_prefix))throw Failure(Code::invalid,"invalid qualified operation identity");
  try{const auto epoch=parse_number(id.substr(operation_epoch_prefix.size(),16));if(epoch==0)throw Failure(Code::invalid,"zero qualified operation epoch");return epoch;}catch(const Failure& e){throw Failure(Code::invalid,e.what());}
}

struct Transaction {
  Root root;
  Digest predecessor;
  CommitReceipt receipt;
  std::vector<StoredRecord> records;
  std::vector<BlobInfo> blobs;
  std::vector<ObservationClaim> observations;
  std::vector<StoredEvent> events;
  std::vector<std::pair<Digest,std::uint64_t>> references;
  std::vector<RecordGuard> guards;
  std::uint64_t expected_generation=any_generation;
};
class IndexCursor {
  SealedIndex& index_;Digest root_;std::string prefix_,after_;
  std::vector<std::pair<std::string,std::string>> rows_;std::size_t offset_=0;bool more_=true;
public:
  IndexCursor(SealedIndex& index,Digest root,std::string prefix):index_(index),root_(std::move(root)),prefix_(std::move(prefix)){}
  bool next(std::pair<std::string,std::string>& row){
    if(offset_==rows_.size()){if(!more_)return false;index_.scan(root_,prefix_,after_,128,rows_,more_);offset_=0;if(rows_.empty())return false;}
    row=rows_[offset_++];after_=row.first;return true;
  }
};
Bytes encode_transaction(const Transaction& t){
  std::vector<std::string> records,blobs,claims,events,refs,guards;
  for(const auto& v:t.records)records.push_back(encode_record(v));
  for(const auto& v:t.blobs)blobs.push_back(encode_blob(v));
  for(const auto& v:t.observations)claims.push_back(encode_claim(v));
  for(const auto& v:t.events)events.push_back(encode_event(v));
  for(const auto& v:t.references)refs.push_back(pack({v.first,number(v.second)}));
  for(const auto& v:t.guards)guards.push_back(pack({v.kind,v.key,number(v.expected_revision),v.expected_digest}));
  return seal("ANAIERO-CANONICAL-TRANSACTION/4",{t.root.store,number(t.root.generation),t.predecessor,t.receipt.operation_id,t.receipt.request_digest,t.receipt.batch_digest,pack(records),pack(blobs),pack(claims),pack(events),pack(refs),number(t.root.blobs),number(t.root.records),number(t.root.canonical_bytes),number(t.root.blob_bytes),number(t.receipt.admitted_unix_s),encode_authorizations(t.receipt.authorizations),number(t.expected_generation),pack(guards),number(static_cast<std::uint64_t>(t.receipt.commit_class))});
}
Transaction decode_transaction(std::string_view raw){
  const auto envelope=fields(raw,3);if(envelope.size()!=3||(envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/1"&&envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/2"&&envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/3"&&envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/4"))throw Failure(Code::corrupt,"invalid canonical transaction schema");
  const auto f=unseal(raw,envelope[0],20);if((envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/1"&&f.size()!=15)||(envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/2"&&f.size()!=17)||(envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/3"&&f.size()!=19)||(envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/4"&&f.size()!=20))throw Failure(Code::corrupt,"invalid canonical transaction");
  Transaction t;t.root={f[0],parse_number(f[1]),parse_number(f[11]),parse_number(f[12]),parse_number(f[13]),parse_number(f[14]),sha256(raw),{}};t.predecessor=f[2];t.receipt={f[3],f[4],f[5],t.root.digest,t.root.generation,false};
  if(f.size()>=17){t.receipt.admitted_unix_s=parse_number(f[15]);t.receipt.authorizations=decode_authorizations(f[16]);for(const auto& a:t.receipt.authorizations)if(a.not_after_unix_s!=0&&t.receipt.admitted_unix_s>=a.not_after_unix_s)throw Failure(Code::corrupt,"canonical receipt was admitted after authorization expiry");}
  if(f.size()>=19){t.expected_generation=parse_number(f[17]);if(t.expected_generation!=any_generation&&t.expected_generation!=t.root.generation-1)throw Failure(Code::corrupt,"canonical generation admission fence mismatch");for(const auto& raw_guard:fields(f[18],4096)){const auto guard=fields(raw_guard,4);if(guard.size()!=4||!text_key(guard[0],128)||!text_key(guard[1])||(!guard[3].empty()&&!valid_digest(guard[3])))throw Failure(Code::corrupt,"invalid canonical record admission fence");t.guards.push_back({guard[0],guard[1],parse_number(guard[2]),guard[3]});}}
  if(f.size()==20){const auto kind=parse_number(f[19]);if(kind>1)throw Failure(Code::corrupt,"invalid canonical commit class");t.receipt.commit_class=static_cast<CommitClass>(kind);}
  if(!valid_digest(t.root.store)||!valid_digest(t.receipt.request_digest)||!valid_digest(t.receipt.batch_digest)||t.root.generation==0||(t.root.generation>1&&!valid_digest(t.predecessor)))throw Failure(Code::corrupt,"invalid transaction identity");
  for(const auto& v:fields(f[6]))t.records.push_back(decode_record(v));
  for(const auto& v:fields(f[7]))t.blobs.push_back(decode_blob(v));
  for(const auto& v:fields(f[8]))t.observations.push_back(decode_claim(v));
  for(const auto& v:fields(f[9]))t.events.push_back(decode_event(v));
  for(const auto& v:fields(f[10])){const auto pair=fields(v,2);if(pair.size()!=2||!valid_digest(pair[0]))throw Failure(Code::corrupt,"invalid reference counter");t.references.emplace_back(pair[0],parse_number(pair[1]));}
  for(const auto& r:t.records)if(r.generation!=t.root.generation)throw Failure(Code::corrupt,"record generation mismatch");
  return t;
}
