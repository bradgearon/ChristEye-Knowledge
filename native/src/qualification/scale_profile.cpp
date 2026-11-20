#include "scale_qualification.hpp"
#include <charconv>
#include <iomanip>
#include <map>
#include <sstream>

namespace anaiero::qualification {
namespace {
bool integer(std::string_view text,std::uint64_t& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
std::string quote(std::string_view text){
  std::ostringstream out;out<<'"';
  for(const unsigned char c:text){
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';return out.str();
}
const char* boolean(bool b){return b?"true":"false";}
void histogram(std::ostream& out,const Histogram& h){
  out<<"{\"samples\":"<<h.count<<",\"minimum_ns\":"<<h.minimum_ns<<",\"maximum_ns\":"<<h.maximum_ns
     <<",\"p50_upper_ns\":"<<h.percentile_upper_ns(50)<<",\"p95_upper_ns\":"<<h.percentile_upper_ns(95)
     <<",\"p99_upper_ns\":"<<h.percentile_upper_ns(99)<<'}';
}
void disk(std::ostream& out,const DiskUsage& d){
  out<<disk_usage_json(d);
}
void lookup(std::ostream& out,const ScaleLookup& l){
  out<<"{\"sequence\":"<<l.sequence<<",\"maximum_nodes\":"<<l.maximum_nodes<<",\"latency\":";
  histogram(out,l.latency);out<<",\"threshold\":"<<threshold_json(l.threshold,99)<<'}';
}
}
sdk::Result parse_scale_profile(std::string_view bytes,ScaleProfile& out){
  if(bytes.size()>16384||bytes.find('\0')!=std::string_view::npos)
    return {sdk::Code::invalid,"scale profile exceeds bounded text size"};
  std::istringstream input{std::string(bytes)};std::string line;
  if(!std::getline(input,line))return {sdk::Code::invalid,"missing scale profile version"};
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIEROSCALEQUALIFICATION/1")return {sdk::Code::invalid,"unsupported scale profile version"};
  ScaleProfile profile;
  const std::map<std::string,std::uint64_t*> numbers={
    {"receiver_pages_per_checkpoint",&profile.receiver_pages_per_checkpoint},
    {"receiver_page_observations",&profile.receiver_page_observations},
    {"retained_receiver_pages",&profile.retained_receiver_pages},
    {"compaction_working_bytes",&profile.compaction_working_bytes}};
  std::map<std::string,bool> seen;std::ostringstream evidence;evidence<<"ANAIEROQUALIFICATION/1\n";
  while(std::getline(input,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line.front()=='#')continue;
    const auto separator=line.find('=');
    const auto key=line.substr(0,separator);
    if(separator==std::string::npos||!seen.emplace(key,true).second)
      return {sdk::Code::invalid,"malformed or duplicate scale profile field"};
    const auto number=numbers.find(key);
    if(number!=numbers.end()){
      if(!integer(std::string_view(line).substr(separator+1),*number->second)||*number->second==0)
        return {sdk::Code::invalid,"noncanonical or zero scale profile field: "+key};
    }else evidence<<line<<'\n';
  }
  for(const auto& [key,value]:numbers){(void)value;if(!seen.contains(key))return {sdk::Code::invalid,"missing scale profile field: "+key};}
  if(auto r=parse_profile(evidence.str(),profile.evidence);!r)return r;
  if(profile.receiver_pages_per_checkpoint>4096||profile.receiver_page_observations>16||
     profile.retained_receiver_pages>64||profile.receiver_pages_per_checkpoint<=profile.retained_receiver_pages||
     profile.compaction_working_bytes>profile.evidence.maximum_store_bytes||
     profile.evidence.checkpoints.front()<6||
     profile.evidence.epochs<profile.evidence.checkpoints.size())
    return {sdk::Code::invalid,"scale workload exceeds bounded receiver, compaction or churn limits"};
  profile.evidence.latency_gate=threshold_method;
  out=std::move(profile);return {};
}
std::string scale_report_json(const ScaleReport& r){
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"{\"schema\":\"anaiero.native-scale-qualification/1\",\"profile\":"<<quote(r.profile.evidence.name)
     <<",\"profile_sha256\":"<<quote(r.profile_sha256)<<",\"passed\":"<<boolean(r.passed)
     <<",\"scope\":\"synthetic throughput with real lifecycle, relay retention and canonical compaction services\""
     <<",\"transport_scope\":\"synthetic authenticated relay fixture; no TLS or network measurement\""
     <<",\"durability_scope\":\"orderly reopen; separate native fault fixtures cover interrupted maintenance\""
     <<",\"identity_policy\":\"preserve compact authority, source, tombstone and exact retry identities within configured quota\""
     <<",\"disk_scope\":\"checkpoint inventories cover the primary store; final inventory covers the full output before report publication; directory allocation is separate when available; filesystem journals are not attributed\""
     <<",\"percentile_method\":"<<quote(histogram_percentile_method)
     <<",\"acceptance_method\":"<<quote(threshold_method)
     <<",\"started_utc\":"<<quote(r.started_utc)<<",\"finished_utc\":"<<quote(r.finished_utc)
     <<",\"index_layout\":"<<quote(layout_label(r.layout))<<",\"index_backend\":"<<quote(r.index_backend)
     <<",\"store_id\":"<<quote(r.store_id)<<",\"device_sha256\":"<<quote(r.device_sha256)
     <<",\"device\":"<<device_declaration_json(r.device)<<",\"environment\":{\"os\":"<<quote(r.environment.operating_system)
     <<",\"architecture\":"<<quote(r.environment.architecture)<<",\"filesystem\":"<<quote(r.environment.filesystem)
     <<",\"compiler\":"<<quote(r.environment.compiler)<<",\"build_identity\":"<<quote(r.environment.build_identity)
     <<",\"logical_processors\":"<<r.environment.logical_processors<<",\"storage_label\":"<<quote(r.profile.evidence.storage_label)<<'}'
     <<",\"admission\":{\"minimum_file_nodes\":"<<r.minimum_file_nodes
     <<",\"scope\":"<<quote(r.layout==sdk::evidence::IndexLayout::packed_pages
       ?"one distinct payload file per observation plus reserve; packed pages and metadata require additional headroom"
       :"distinct payload files plus old and replacement single-node indexes; minimum only")
     <<",\"disk\":"
     <<disk_usage_json(r.admission_disk)<<'}'
     <<",\"workload\":{\"observations\":"<<r.observations<<",\"epochs\":"<<r.epochs
     <<",\"distinct_local_source_uids\":"<<(r.epochs?1+r.epochs/2:0)
     <<",\"receiver_observations\":"<<r.receiver_observations<<",\"receiver_expired_observations\":"<<r.receiver_expired
     <<",\"maximum_retained_receiver_observations\":"<<r.maximum_retained_receiver_observations
     <<",\"maximum_active_sources\":"<<r.maximum_active_sources<<",\"reclaimed_payload_bytes\":"<<r.reclaimed_payload_bytes
     <<",\"wall_ns\":"<<r.wall_ns<<'}'
     <<",\"budgets\":{\"maximum_store_bytes\":"<<r.profile.evidence.maximum_store_bytes
     <<",\"maximum_disk_bytes\":"<<r.profile.evidence.maximum_disk_bytes
     <<",\"maximum_resident_bytes\":"<<r.profile.evidence.maximum_resident_bytes
     <<",\"maximum_files\":"<<r.profile.evidence.maximum_files
     <<",\"maximum_commit_p99_us\":"<<r.profile.evidence.maximum_commit_p99_us
     <<",\"maximum_lookup_p99_us\":"<<r.profile.evidence.maximum_lookup_p99_us
     <<",\"maximum_reopen_us\":"<<r.profile.evidence.maximum_reopen_us
     <<",\"maximum_lookup_nodes\":"<<r.profile.evidence.maximum_lookup_nodes
     <<",\"compaction_working_bytes\":"<<r.profile.compaction_working_bytes
     <<",\"retained_receiver_pages\":"<<r.profile.retained_receiver_pages<<'}'
     <<",\"commit\":";histogram(out,r.commit);
  out<<",\"exact_threshold_counts\":{\"commit_samples\":"<<r.commit_threshold.count<<",\"commit_over_budget\":"<<r.commit_threshold.exceeded
     <<",\"lookup_samples\":"<<r.lookup_threshold.count<<",\"lookup_over_budget\":"<<r.lookup_threshold.exceeded<<'}'
     <<",\"commit_acceptance\":"<<threshold_json(r.commit_threshold,99)
     <<",\"controls\":{\"protected_root_verified\":"<<boolean(r.protected_root_verified)
     <<",\"quota_rejection_verified\":"<<boolean(r.quota_rejection_verified)
     <<",\"maintenance_reserve_verified\":"<<boolean(r.maintenance_reserve_verified)<<'}'
     <<",\"resources\":{\"resident_before\":"<<r.before.resident_bytes<<",\"resident_after\":"<<r.after.resident_bytes
     <<",\"peak_resident_bytes\":"<<r.after.peak_resident_bytes<<",\"cpu_before_ns\":"<<r.before.cpu_ns
     <<",\"cpu_after_ns\":"<<r.after.cpu_ns<<"},\"final_output_disk\":";disk(out,r.final_disk);out<<",\"checkpoints\":[";
  bool comma=false;
  for(const auto& p:r.checkpoints){
    if(comma)out<<',';
    comma=true;
    out<<"{\"observations\":"<<p.observations<<",\"epochs\":"<<p.epochs<<",\"receiver_observations\":"<<p.receiver_observations
       <<",\"receiver_expired\":"<<p.receiver_expired<<",\"generation\":"<<p.generation<<",\"root\":"<<quote(p.root)
       <<",\"reopen_ns\":"<<p.reopen_ns<<",\"reopen_nodes\":"<<p.reopen_nodes<<",\"audit_ns\":"<<p.audit_ns
       <<",\"compaction_ns\":"<<p.compaction_ns<<",\"required_working_bytes\":"<<p.required_working_bytes
       <<",\"retained_records\":"<<p.retained_records<<",\"retained_blob_identities\":"<<p.retained_blobs
       <<",\"retained_operation_receipts\":"<<p.retained_operations
       <<",\"disk_before\":";disk(out,p.before_disk);out<<",\"disk_after\":";disk(out,p.after_disk);
    out<<",\"oldest\":";lookup(out,p.oldest);out<<",\"middle\":";lookup(out,p.middle);out<<",\"recent\":";lookup(out,p.recent);
    out<<",\"capacity\":{\"accounted_bytes\":"<<p.capacity.accounted_bytes<<",\"ordinary_limit_bytes\":"<<p.capacity.ordinary_limit_bytes
       <<",\"ordinary_remaining_bytes\":"<<p.capacity.ordinary_remaining_bytes
       <<",\"maintenance_reserve_bytes\":"<<p.capacity.maintenance_reserve_bytes
       <<",\"maintenance_remaining_bytes\":"<<p.capacity.maintenance_remaining_bytes
       <<",\"pending_reclamation_bytes\":"<<p.capacity.pending_reclamation_bytes<<'}'
       <<",\"compaction\":{\"activated\":"<<boolean(p.compaction.activated)<<",\"cleanup_complete\":"<<boolean(p.compaction.cleanup_complete)
       <<",\"coexistence_measured\":"<<boolean(p.compaction.coexistence_measured)
       <<",\"coexistence_files\":"<<p.compaction.coexistence_files
       <<",\"coexistence_directories\":"<<p.compaction.coexistence_directories
       <<",\"coexistence_old_index_files\":"<<p.compaction.coexistence_old_index_files
       <<",\"coexistence_new_index_files\":"<<p.compaction.coexistence_new_index_files
       <<",\"accounted_bytes_before\":"<<p.compaction.accounted_bytes_before<<",\"accounted_bytes_after\":"<<p.compaction.accounted_bytes_after
       <<",\"removed_transaction_bytes\":"<<p.compaction.removed_transaction_bytes<<",\"removed_index_bytes\":"<<p.compaction.removed_index_bytes
       <<",\"summarized_record_versions\":"<<p.compaction.summarized_record_versions<<",\"expired_observations\":"<<p.compaction.expired_observations<<'}'
       <<",\"audit\":{\"generations\":"<<p.audit.generations<<",\"records\":"<<p.audit.records<<",\"blobs\":"<<p.audit.blobs
       <<",\"base_pages\":"<<p.audit.base_pages<<",\"summarized_generations\":"<<p.audit.summarized_generations
       <<",\"canonical_records_read\":"<<p.audit.canonical_records_read<<",\"index_nodes_read\":"<<p.audit.index_nodes_read<<'}'
       <<",\"peak_resident_bytes\":"<<p.resources.peak_resident_bytes
       <<",\"restart_verified\":"<<boolean(p.restart_verified)<<",\"identity_fences_verified\":"<<boolean(p.identity_fences_verified)
       <<",\"exact_retry_verified\":"<<boolean(p.exact_retry_verified)<<'}';
  }
  out<<"],\"violations\":[";comma=false;for(const auto& v:r.violations){if(comma)out<<',';comma=true;out<<quote(v);}out<<"]}\n";
  return out.str();
}
}
