#include "mixed_qualification.hpp"
#include "anaiero/sdk/engines.hpp"
#include <charconv>
#include <iomanip>
#include <map>
#include <set>
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
void histogram(std::ostream& out,const Histogram& value){
  out<<"{\"samples\":"<<value.count<<",\"minimum_ns\":"<<value.minimum_ns<<",\"maximum_ns\":"<<value.maximum_ns
     <<",\"p50_upper_ns\":"<<value.percentile_upper_ns(50)<<",\"p95_upper_ns\":"<<value.percentile_upper_ns(95)
     <<",\"p99_upper_ns\":"<<value.percentile_upper_ns(99)<<'}';
}
void resources(std::ostream& out,const Resources& value){
  out<<"{\"resident_bytes\":"<<value.resident_bytes<<",\"peak_resident_bytes\":"<<value.peak_resident_bytes
     <<",\"cpu_ns\":"<<value.cpu_ns<<",\"context_switches_available\":"<<(value.context_switches_available?"true":"false")
     <<",\"voluntary_context_switches\":"<<value.voluntary_context_switches
     <<",\"involuntary_context_switches\":"<<value.involuntary_context_switches<<'}';
}
}
sdk::Result parse_mixed_profile(std::string_view bytes,MixedProfile& output){
  if(bytes.size()>16384||bytes.find('\0')!=std::string_view::npos)return {sdk::Code::invalid,"mixed profile exceeds text bounds"};
  std::istringstream input{std::string(bytes)};std::string line;
  if(!std::getline(input,line))return {sdk::Code::invalid,"missing mixed profile version"};
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIEROMIXEDQUALIFICATION/1")return {sdk::Code::invalid,"unsupported mixed profile version"};
  std::map<std::string,std::string> fields;
  while(std::getline(input,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line.front()=='#')continue;
    const auto at=line.find('=');
    if(at==std::string::npos||at==0||at+1==line.size()||!fields.emplace(line.substr(0,at),line.substr(at+1)).second)
      return {sdk::Code::invalid,"mixed profile contains malformed or duplicate fields"};
  }
  MixedProfile p;
  const std::map<std::string,std::uint64_t*> numbers={
    {"cycles",&p.cycles},{"period_ms",&p.period_ms},{"restart_after_cycle",&p.restart_after_cycle},
    {"checkpoint_cycles",&p.checkpoint_cycles},{"blocked_lane_ms",&p.blocked_lane_ms},
    {"minimum_run_ms",&p.minimum_run_ms},{"maximum_run_ms",&p.maximum_run_ms},
    {"maximum_store_bytes",&p.maximum_store_bytes},{"maximum_disk_bytes",&p.maximum_disk_bytes},
    {"maximum_files",&p.maximum_files},{"maximum_resident_bytes",&p.maximum_resident_bytes},
    {"maximum_status_p99_us",&p.maximum_status_p99_us},{"maximum_control_p99_us",&p.maximum_control_p99_us},
    {"maximum_probe_p99_us",&p.maximum_probe_p99_us},{"maximum_reopen_us",&p.maximum_reopen_us},
    {"maximum_lookup_nodes",&p.maximum_lookup_nodes}};
  if(fields.size()!=numbers.size()+2)return {sdk::Code::invalid,"mixed profile requires all fields and rejects unknown fields"};
  for(const auto& [key,destination]:numbers){
    const auto found=fields.find(key);
    if(found==fields.end()||!integer(found->second,*destination)||(*destination==0&&key!="minimum_run_ms"))
      return {sdk::Code::invalid,"invalid mixed profile integer: "+key};
  }
  const auto name=fields.find("name"),storage=fields.find("storage_label");
  if(name==fields.end()||storage==fields.end()||!sdk::valid_token(name->second,96)||storage->second.size()>160)
    return {sdk::Code::invalid,"invalid mixed profile identity"};
  p.name=name->second;p.storage_label=storage->second;
  if(p.cycles<24||p.cycles>10000||p.period_ms>3600000||p.restart_after_cycle<12||p.restart_after_cycle>=p.cycles||
     p.checkpoint_cycles>p.cycles||p.blocked_lane_ms>5000||p.maximum_run_ms>14ULL*24*60*60*1000||
     p.minimum_run_ms>p.maximum_run_ms||p.period_ms*p.cycles>p.maximum_run_ms||p.maximum_store_bytes<1048576||
     p.maximum_store_bytes>(1ULL<<44)||p.maximum_disk_bytes<p.maximum_store_bytes||p.maximum_files>100000000||
     p.maximum_resident_bytes<1048576||p.maximum_status_p99_us>1000000000||p.maximum_control_p99_us>1000000000||
     p.maximum_probe_p99_us>1000000000||p.maximum_reopen_us>1000000000||p.maximum_lookup_nodes>4096)
    return {sdk::Code::invalid,"mixed profile exceeds supported horizon or resource bounds"};
  output=std::move(p);return {};
}
sdk::Result validate_mixed_definition(const sdk::SessionDefinition& definition,const MixedProfile& profile){
  if(definition.engines.size()!=5)return {sdk::Code::invalid,"mixed qualification requires exactly five engine families"};
  std::set<std::string> types;
  for(const auto& engine:definition.engines){
    const auto& type=engine.identity.type_id;
    if(type!=engines::cell_behavior&&type!=engines::multi_colony&&type!=engines::vlm_plan&&
       type!=engines::passive_membrane&&type!=engines::environment)
      return {sdk::Code::unsupported,"mixed qualification accepts the five compiled engine families"};
    if(!types.insert(type).second)return {sdk::Code::invalid,"mixed qualification requires distinct engine families"};
    const auto cadence=type==engines::multi_colony?2U:1U;
    if(engine.cadence!=cadence)return {sdk::Code::invalid,"mixed qualification requires cadence two for SIM3 and one for the other engines"};
    if(type==engines::multi_colony&&profile.cycles/engine.cadence>10000)
      return {sdk::Code::capacity,"mixed profile exceeds the supported SIM3 horizon"};
  }
  return {};
}
std::string mixed_report_json(const MixedReport& r){
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"{\"schema\":\"anaiero.mixed-qualification/1\",\"profile\":"<<quote(r.profile.name)
     <<",\"profile_sha256\":"<<quote(r.profile_sha256)<<",\"session_text_sha256\":"<<quote(r.session_text_sha256)
     <<",\"session_definition_sha256\":"<<quote(r.session_definition_sha256)
     <<",\"scope\":\"five compiled engines through NativeSession and NativeContinuousHost; no IPC or TLS transport\""
     <<",\"durability_level\":\"orderly host shutdown and store reopen; no process crash or power loss\""
     <<",\"latency_scope\":\"cached snapshot call, durable engine-control completion, and queued historical probe completion\""
     <<",\"disk_scope\":\"sampled regular-file allocation in the session store; excludes filesystem metadata\""
     <<",\"percentiles\":\"bounded log2 histogram upper bounds in nanoseconds\""
     <<",\"started_utc\":"<<quote(r.started_utc)<<",\"finished_utc\":"<<quote(r.finished_utc)
     <<",\"passed\":"<<(r.passed?"true":"false")<<",\"store_id\":"<<quote(r.store_id)
     <<",\"environment\":{\"os\":"<<quote(r.environment.operating_system)<<",\"architecture\":"<<quote(r.environment.architecture)
     <<",\"filesystem\":"<<quote(r.environment.filesystem)<<",\"storage_label\":"<<quote(r.profile.storage_label)
     <<",\"compiler\":"<<quote(r.environment.compiler)<<",\"build_identity\":"<<quote(r.environment.build_identity)
     <<",\"logical_processors\":"<<r.environment.logical_processors<<'}'
     <<",\"workload\":{\"cycles\":"<<r.cycles<<",\"target_cycles\":"<<r.profile.cycles<<",\"period_ms\":"<<r.profile.period_ms
     <<",\"restart_after_cycle\":"<<r.profile.restart_after_cycle<<",\"checkpoint_cycles\":"<<r.profile.checkpoint_cycles
     <<",\"blocked_lane_ms\":"<<r.profile.blocked_lane_ms<<",\"wall_ns\":"<<r.wall_ns<<",\"workload_ns\":"<<r.workload_ns
     <<",\"probes\":"<<r.probes<<",\"owner_wakeups\":"<<r.owner_wakeups<<",\"completed_tasks\":"<<r.completed_tasks
     <<",\"maximum_queued_commands\":"<<r.maximum_queued_commands<<",\"maximum_queued_tasks\":"<<r.maximum_queued_tasks
     <<",\"task_rejections\":"<<r.task_rejections<<",\"command_rejections\":"<<r.command_rejections<<'}'
     <<",\"budgets\":{\"minimum_run_ms\":"<<r.profile.minimum_run_ms<<",\"maximum_run_ms\":"<<r.profile.maximum_run_ms
     <<",\"maximum_store_bytes\":"<<r.profile.maximum_store_bytes<<",\"maximum_disk_bytes\":"<<r.profile.maximum_disk_bytes
     <<",\"maximum_files\":"<<r.profile.maximum_files<<",\"maximum_resident_bytes\":"<<r.profile.maximum_resident_bytes
     <<",\"maximum_status_p99_us\":"<<r.profile.maximum_status_p99_us<<",\"maximum_control_p99_us\":"<<r.profile.maximum_control_p99_us
     <<",\"maximum_probe_p99_us\":"<<r.profile.maximum_probe_p99_us<<",\"maximum_reopen_us\":"<<r.profile.maximum_reopen_us
     <<",\"maximum_lookup_nodes\":"<<r.profile.maximum_lookup_nodes<<'}'
     <<",\"snapshot_latency\":";histogram(out,r.status);out<<",\"control_latency\":";histogram(out,r.control);
  out<<",\"probe_latency\":";histogram(out,r.probe);out<<",\"resources_before\":";resources(out,r.before);
  out<<",\"resources_after\":";resources(out,r.after);
  out<<",\"final_store\":{\"generation\":"<<r.final_generation<<",\"accounted_bytes\":"<<r.final_accounted_bytes
     <<",\"files\":"<<r.disk.files<<",\"logical_bytes\":"<<r.disk.logical_bytes<<",\"allocated_bytes\":"<<r.disk.allocated_bytes
     <<",\"allocated_bytes_available\":"<<(r.disk.allocated_bytes_available?"true":"false")
     <<",\"sampled_peak_disk_bytes\":"<<r.sampled_peak_disk_bytes<<",\"sampled_peak_files\":"<<r.sampled_peak_files<<'}'
     <<",\"checks\":{\"queue_pressure_verified\":"<<(r.pressure_verified?"true":"false")
     <<",\"exact_control_retry_verified\":"<<(r.control_retry_verified?"true":"false")
     <<",\"quota_rejection_verified\":"<<(r.quota_rejection_verified?"true":"false")
     <<",\"restart_identity_verified\":"<<(r.restart_identity_verified?"true":"false")
     <<",\"finite_plan_completed\":"<<(r.finite_plan_completed?"true":"false")
     <<",\"audit_passed\":"<<(r.audit_passed?"true":"false")<<",\"reopen_ns\":"<<r.reopen_ns
     <<",\"audit_ns\":"<<r.audit_ns<<",\"lookup_maximum_nodes\":"<<r.lookup_maximum_nodes<<"},\"engines\":[";
  bool comma=false;
  for(const auto& engine:r.engines){
    if(comma)out<<',';
    comma=true;
    out<<"{\"instance_id\":"<<quote(engine.identity.instance_id)<<",\"type_id\":"<<quote(engine.identity.type_id)
       <<",\"source_uid\":"<<quote(engine.identity.source_uid)<<",\"epoch\":"<<engine.identity.epoch
       <<",\"module_sha256\":"<<quote(engine.identity.module_sha256)<<",\"model_sha256\":"<<quote(engine.identity.model_sha256)
       <<",\"configuration_sha256\":"<<quote(engine.configuration_sha256)<<",\"input_sha256\":"<<quote(engine.input_sha256)
       <<",\"state_sha256\":"<<quote(engine.state_sha256)<<",\"cadence\":"<<engine.cadence<<",\"tick\":"<<engine.tick
       <<",\"revision\":"<<engine.revision<<",\"phase\":"<<static_cast<std::uint32_t>(engine.phase)<<'}';
  }
  out<<"],\"violations\":[";comma=false;
  for(const auto& value:r.violations){if(comma)out<<',';comma=true;out<<quote(value);}out<<"]}\n";return out.str();
}
}
