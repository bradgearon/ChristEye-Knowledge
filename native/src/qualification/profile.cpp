#include "qualification.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace anaiero::qualification {
namespace {
bool integer(std::string_view text,std::uint64_t& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
std::string json_quote(std::string_view text){
  std::ostringstream out;out<<'"';
  for(const unsigned char c:text){
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';return out.str();
}
void histogram(std::ostream& out,const Histogram& value){
  out<<"{\"samples\":"<<value.count<<",\"minimum_ns\":"<<value.minimum_ns
     <<",\"maximum_ns\":"<<value.maximum_ns<<",\"p50_upper_ns\":"<<value.percentile_upper_ns(50)
     <<",\"p95_upper_ns\":"<<value.percentile_upper_ns(95)
     <<",\"p99_upper_ns\":"<<value.percentile_upper_ns(99)<<'}';
}
void resources(std::ostream& out,const Resources& value){
  out<<"{\"resident_bytes\":"<<value.resident_bytes<<",\"peak_resident_bytes\":"<<value.peak_resident_bytes
     <<",\"cpu_ns\":"<<value.cpu_ns<<",\"context_switches_available\":"<<(value.context_switches_available?"true":"false")
     <<",\"voluntary_context_switches\":"<<value.voluntary_context_switches
     <<",\"involuntary_context_switches\":"<<value.involuntary_context_switches<<'}';
}
}
void Histogram::add(std::uint64_t ns)noexcept{
  const auto bucket=ns==0?0U:std::bit_width(ns-1);
  ++buckets[static_cast<std::size_t>(bucket)];
  if(count==0||ns<minimum_ns)minimum_ns=ns;
  maximum_ns=std::max(maximum_ns,ns);++count;
}
std::uint64_t Histogram::percentile_upper_ns(unsigned percent)const noexcept{
  if(count==0)return 0;
  const auto target=(count/100)*percent+((count%100)*percent+99)/100;
  std::uint64_t cumulative=0;
  for(std::size_t i=0;i<buckets.size();++i){
    cumulative+=buckets[i];
    if(cumulative>=target){
      const auto ceiling=i==64?std::numeric_limits<std::uint64_t>::max():(std::uint64_t{1}<<i);
      // Both quantities bound the selected observed percentile. The exact
      // maximum tightens a partially occupied final bucket without retaining
      // samples or changing the fixed histogram's memory bound.
      return std::min(ceiling,maximum_ns);
    }
  }
  return maximum_ns;
}
sdk::Result parse_profile(std::string_view bytes,Profile& output){
  if(bytes.size()>16384||bytes.find('\0')!=std::string_view::npos)return {sdk::Code::invalid,"qualification profile exceeds text bounds"};
  std::istringstream input{std::string(bytes)};std::string line;
  if(!std::getline(input,line))return {sdk::Code::invalid,"missing qualification profile version"};
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIEROQUALIFICATION/1")return {sdk::Code::invalid,"unsupported qualification profile version"};
  std::map<std::string,std::string> fields;
  while(std::getline(input,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line.front()=='#')continue;
    const auto separator=line.find('=');
    if(separator==std::string::npos||separator==0||separator+1==line.size()||!fields.emplace(line.substr(0,separator),line.substr(separator+1)).second)
      return {sdk::Code::invalid,"qualification profile contains malformed or duplicate fields"};
  }
  Profile p;
  const std::map<std::string,std::uint64_t*> numbers={
    {"observations",&p.observations},{"epochs",&p.epochs},{"batch_size",&p.batch_size},{"payload_bytes",&p.payload_bytes},
    {"query_samples",&p.query_samples},{"maximum_store_bytes",&p.maximum_store_bytes},{"maximum_files",&p.maximum_files},
    {"maximum_resident_bytes",&p.maximum_resident_bytes},{"maximum_disk_bytes",&p.maximum_disk_bytes},
    {"maximum_commit_p99_us",&p.maximum_commit_p99_us},{"maximum_lookup_p99_us",&p.maximum_lookup_p99_us},
    {"maximum_reopen_us",&p.maximum_reopen_us},{"maximum_lookup_nodes",&p.maximum_lookup_nodes}};
  if(fields.size()!=numbers.size()+3)return {sdk::Code::invalid,"qualification profile requires all declared budgets and no unknown fields"};
  for(const auto& [key,destination]:numbers){
    const auto found=fields.find(key);
    if(found==fields.end()||!integer(found->second,*destination)||*destination==0)
      return {sdk::Code::invalid,"qualification numeric field is missing, noncanonical or zero: "+key};
  }
  const auto name=fields.find("name"),storage=fields.find("storage_label"),checkpoints=fields.find("checkpoints");
  if(name==fields.end()||storage==fields.end()||checkpoints==fields.end()||!sdk::valid_token(name->second,96)||storage->second.size()>160)
    return {sdk::Code::invalid,"qualification profile identity or storage label is invalid"};
  p.name=name->second;p.storage_label=storage->second;
  if(checkpoints->second.back()==',')return {sdk::Code::invalid,"qualification checkpoints contain an empty trailing value"};
  std::istringstream points(checkpoints->second);std::string part;
  while(std::getline(points,part,',')){
    std::uint64_t point=0;
    if(!integer(part,point)||point==0||point>p.observations||(!p.checkpoints.empty()&&point<=p.checkpoints.back())||p.checkpoints.size()==16)
      return {sdk::Code::invalid,"qualification checkpoints must be increasing and within observation count"};
    p.checkpoints.push_back(point);
  }
  if(p.checkpoints.empty()||p.checkpoints.back()!=p.observations||p.observations>10000000||p.epochs>10000||p.batch_size>63||
     p.payload_bytes<64||p.payload_bytes>65536||p.query_samples>4096||p.maximum_store_bytes<1048576||
     p.maximum_store_bytes>(1ULL<<44)||p.maximum_files>100000000||p.maximum_disk_bytes<p.maximum_store_bytes||
     p.maximum_resident_bytes<1048576||p.maximum_commit_p99_us>1000000000||p.maximum_lookup_p99_us>1000000000||p.maximum_reopen_us>1000000000)
    return {sdk::Code::invalid,"qualification profile exceeds supported workload or budget bounds"};
  output=std::move(p);return {};
}
std::string report_json(const Report& r){
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"{\"schema\":\"anaiero.native-qualification/1\",\"profile\":"<<json_quote(r.profile.name)
     <<",\"profile_sha256\":"<<json_quote(r.profile_sha256)<<",\"scope\":\"synthetic canonical evidence and lifecycle\""
     <<",\"durability_level\":\"orderly close/reopen; this run does not exercise crash or power loss\""
     <<",\"commit_scope\":\"throughput-batch durability only; lifecycle commits are measured in total wall time\""
     <<",\"wall_time_scope\":\"ingestion, queries, reopens, disk inventories, lifecycle churn and full audit\""
     <<",\"disk_scope\":\"regular-file allocation; excludes directory and filesystem journal metadata\""
     <<",\"percentile_method\":"<<json_quote(histogram_percentile_method)
     <<",\"percentiles\":"<<json_quote(histogram_percentile_description)
     <<",\"started_utc\":"<<json_quote(r.started_utc)<<",\"finished_utc\":"<<json_quote(r.finished_utc)
     <<",\"passed\":"<<(r.passed?"true":"false")<<",\"store_id\":"<<json_quote(r.store_id)
     <<",\"environment\":{\"os\":"<<json_quote(r.environment.operating_system)<<",\"architecture\":"<<json_quote(r.environment.architecture)
     <<",\"filesystem\":"<<json_quote(r.environment.filesystem)<<",\"storage_label\":"<<json_quote(r.profile.storage_label)
     <<",\"compiler\":"<<json_quote(r.environment.compiler)<<",\"build_identity\":"<<json_quote(r.environment.build_identity)
     <<",\"logical_processors\":"<<r.environment.logical_processors<<'}'
     <<",\"workload\":{\"observations\":"<<r.observations<<",\"epoch_transitions\":"<<r.epochs
     <<",\"batch_size\":"<<r.profile.batch_size<<",\"payload_bytes\":"<<r.profile.payload_bytes
     <<",\"maximum_active_sources\":"<<r.maximum_active_sources<<",\"wall_ns\":"<<r.wall_ns<<",\"ingestion_ns\":"<<r.ingestion_ns
     <<",\"lifecycle_observations\":"<<r.epochs<<'}'
     <<",\"budgets\":{\"maximum_store_bytes\":"<<r.profile.maximum_store_bytes<<",\"maximum_disk_bytes\":"<<r.profile.maximum_disk_bytes
     <<",\"maximum_resident_bytes\":"<<r.profile.maximum_resident_bytes<<",\"maximum_files\":"<<r.profile.maximum_files
     <<",\"maximum_commit_p99_us\":"<<r.profile.maximum_commit_p99_us<<",\"maximum_lookup_p99_us\":"<<r.profile.maximum_lookup_p99_us
     <<",\"maximum_reopen_us\":"<<r.profile.maximum_reopen_us<<",\"maximum_lookup_nodes\":"<<r.profile.maximum_lookup_nodes<<'}'
     <<",\"write_path\":{\"index_nodes_read\":"<<r.write_index_reads<<",\"index_nodes_written\":"<<r.write_index_nodes
     <<",\"staged_index_nodes\":"<<r.staged_index_nodes<<",\"transient_nodes_elided\":"<<r.transient_nodes_elided
     <<",\"index_spills\":"<<r.index_spills<<",\"index_peak_cache_nodes\":"<<r.index_peak_nodes<<",\"index_peak_cache_bytes\":"<<r.index_peak_bytes<<'}'
     <<",\"commit\":";histogram(out,r.commit);out<<",\"resources_before\":";resources(out,r.before);
  out<<",\"resources_after\":";resources(out,r.after);out<<",\"checkpoints\":[";
  bool comma=false;for(const auto& p:r.checkpoints){
    if(comma)out<<',';
    comma=true;
    out<<"{\"observations\":"<<p.observations<<",\"store_generation\":"<<p.store_generation<<",\"accounted_bytes\":"<<p.accounted_bytes
       <<",\"lookup_maximum_nodes\":"<<p.lookup_maximum_nodes<<",\"reopen_ns\":"<<p.reopen_ns<<",\"reopen_nodes\":"<<p.reopen_nodes<<",\"lookup\":";
    histogram(out,p.lookup);out<<",\"resources\":";resources(out,p.resources);
    out<<",\"disk\":{\"files\":"<<p.disk.files<<",\"logical_bytes\":"<<p.disk.logical_bytes<<",\"allocated_bytes\":"<<p.disk.allocated_bytes
       <<",\"allocated_bytes_available\":"<<(p.disk.allocated_bytes_available?"true":"false")<<"}}";
  }
  out<<"],\"final_store\":{\"generation\":"<<r.final_generation<<",\"accounted_bytes\":"<<r.final_accounted_bytes
     <<",\"files\":"<<r.final_disk.files<<",\"logical_bytes\":"<<r.final_disk.logical_bytes<<",\"allocated_bytes\":"<<r.final_disk.allocated_bytes
     <<",\"allocated_bytes_available\":"<<(r.final_disk.allocated_bytes_available?"true":"false")<<'}'
     <<",\"audit\":{\"passed\":"<<(r.audit_passed?"true":"false")<<",\"duration_ns\":"<<r.audit_ns
     <<",\"generations\":"<<r.audit_generations<<",\"records\":"<<r.audit_records<<",\"canonical_records_read\":"<<r.audit_canonical_records
     <<",\"index_nodes_read\":"<<r.audit_index_nodes<<",\"restart_identity_verified\":"<<(r.restart_identity_verified?"true":"false")<<"},\"violations\":[";
  comma=false;for(const auto& violation:r.violations){if(comma)out<<',';comma=true;out<<json_quote(violation);}out<<"]}\n";return out.str();
}
}
