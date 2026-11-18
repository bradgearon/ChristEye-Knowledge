#pragma once
// Bounded immutable reference/plan files and atomic, no-clobber report output.
// SHA-256 seals establish byte identity, not measurement authenticity.
#include "anaiero/contracts/scientific_calibration.hpp"
#include "anaiero/infrastructure/environment_model_file.hpp"
#include "christeye/living/durable_records.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
namespace anaiero::science::calibration::files {
namespace fs=std::filesystem;
namespace durable=christeye::living::durable;
inline constexpr std::size_t kMaxReferenceBytes=1024*1024,kMaxRawBytes=4*1024*1024;
struct FileReference {std::string path,sha256;};
struct CandidateFile {std::string id;FileReference model;};
struct PlanFiles {
  std::string name;
  FileReference training,holdout;
  FidelityCriteria criteria;
  std::vector<CandidateFile> candidates;
};
struct ReferenceFile {Reference reference;FileReference raw;};
inline std::string_view origin_name(ReferenceOrigin origin)noexcept{
  switch(origin){
    case ReferenceOrigin::synthetic_reference:return "SYNTHETIC_REFERENCE";
    case ReferenceOrigin::external_unverified_readonly:return "EXTERNAL_UNVERIFIED_READONLY";
  }
  return "INVALID";
}
inline bool origin_value(std::string_view text,ReferenceOrigin& output)noexcept{
  if(text=="SYNTHETIC_REFERENCE")output=ReferenceOrigin::synthetic_reference;
  else if(text=="EXTERNAL_UNVERIFIED_READONLY")output=ReferenceOrigin::external_unverified_readonly;
  else return false;
  return true;
}
inline std::string seal(std::string body){return body+"seal "+christeye::living::plans::sha256_hex(body)+"\n";}
inline bool unseal(std::string_view bytes,std::string_view& body)noexcept{
  if(bytes.size()<70||bytes.back()!='\n')return false;
  const auto offset=bytes.size()-70;
  if(bytes.substr(offset,5)!="seal "||
     christeye::living::plans::sha256_hex(bytes.substr(0,offset))!=bytes.substr(offset+5,64))return false;
  body=bytes.substr(0,offset);return true;
}
inline bool valid_path(std::string_view path)noexcept{
  if(path.empty()||path.size()>4096)return false;
  for(char c:path)if(static_cast<unsigned char>(c)<32)return false;
  return true;
}
inline std::string serialize_reference(const ReferenceFile& record){
  const auto& r=record.reference;const auto& p=r.provenance;
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"CEVENVREFERENCE/1\nrun "<<p.run_id<<"\norigin "<<origin_name(p.origin)
    <<"\nsource "<<p.source<<"\nepoch "<<p.epoch<<"\nclock_domain "<<p.clock_domain
    <<"\nunit "<<static_cast<unsigned>(r.unit)<<"\nstep_ns "<<r.step_ns
    <<"\ninitial_micromolar "<<r.initial_micromolar
    <<"\ninitial_uncertainty_micromolar "<<r.initial_uncertainty_micromolar
    <<"\nproducer_sha256 "<<p.producer_sha256<<"\nprotocol_sha256 "<<p.protocol_sha256
    <<"\nraw_artifact "<<std::quoted(record.raw.path)<<' '<<record.raw.sha256
    <<"\nsamples "<<r.samples.size()<<'\n';
  for(const auto& sample:r.samples)out<<"sample "<<sample.sequence<<' '<<sample.acquired_ns<<' '
    <<sample.inlet_input<<' '<<sample.concentration_micromolar<<' '<<sample.uncertainty_micromolar<<'\n';
  out<<"end\n";return seal(out.str());
}
inline bool parse_reference(std::string_view bytes,ReferenceFile& output){
  std::string_view body;if(bytes.size()>kMaxReferenceBytes||!unseal(bytes,body))return false;
  std::istringstream in{std::string(body)};in.imbue(std::locale::classic());
  ReferenceFile next;auto& r=next.reference;auto& p=r.provenance;
  std::string header,tag,origin,extra;unsigned unit=0;std::size_t count=0;
  if(!(in>>header)||header!="CEVENVREFERENCE/1"||
     !(in>>tag>>p.run_id)||tag!="run"||!durable::valid_id(p.run_id)||
     !(in>>tag>>origin)||tag!="origin"||!origin_value(origin,p.origin)||
     !(in>>tag>>p.source)||tag!="source"||
     !(in>>tag>>p.epoch)||tag!="epoch"||
     !(in>>tag>>p.clock_domain)||tag!="clock_domain"||
     !(in>>tag>>unit)||tag!="unit"||unit!=static_cast<unsigned>(Unit::micromolar)||
     !(in>>tag>>r.step_ns)||tag!="step_ns"||
     !(in>>tag>>r.initial_micromolar)||tag!="initial_micromolar"||
     !(in>>tag>>r.initial_uncertainty_micromolar)||tag!="initial_uncertainty_micromolar"||
     !(in>>tag>>p.producer_sha256)||tag!="producer_sha256"||!durable::valid_digest(p.producer_sha256)||
     !(in>>tag>>p.protocol_sha256)||tag!="protocol_sha256"||!durable::valid_digest(p.protocol_sha256)||
     !(in>>tag>>std::quoted(next.raw.path)>>next.raw.sha256)||tag!="raw_artifact"||
       !valid_path(next.raw.path)||!durable::valid_digest(next.raw.sha256)||
     !(in>>tag>>count)||tag!="samples"||!count||count>kMaxSamples)return false;
  r.unit=static_cast<Unit>(unit);p.raw_sha256=next.raw.sha256;r.samples.reserve(count);
  for(std::size_t i=0;i<count;++i){
    Sample sample{};
    if(!(in>>tag>>sample.sequence>>sample.acquired_ns>>sample.inlet_input>>sample.concentration_micromolar>>sample.uncertainty_micromolar)||
       tag!="sample")return false;
    r.samples.push_back(sample);
  }
  if(!(in>>tag)||tag!="end"||in>>extra||serialize_reference(next)!=bytes)return false;
  r.sha256=christeye::living::plans::sha256_hex(bytes);output=std::move(next);return true;
}
inline std::string serialize_plan(const PlanFiles& plan){
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"CEVENVCALIBRATION/1\nname "<<plan.name
    <<"\ntraining "<<std::quoted(plan.training.path)<<' '<<plan.training.sha256
    <<"\nholdout "<<std::quoted(plan.holdout.path)<<' '<<plan.holdout.sha256
    <<"\nmax_absolute_error_micromolar "<<plan.criteria.max_absolute_error_micromolar
    <<"\nmax_rmse_micromolar "<<plan.criteria.max_rmse_micromolar
    <<"\ncandidates "<<plan.candidates.size()<<'\n';
  for(const auto& candidate:plan.candidates)out<<"candidate "<<candidate.id<<' '
    <<std::quoted(candidate.model.path)<<' '<<candidate.model.sha256<<'\n';
  out<<"end\n";return seal(out.str());
}
inline bool parse_plan(std::string_view bytes,PlanFiles& output){
  std::string_view body;if(bytes.size()>65536||!unseal(bytes,body))return false;
  std::istringstream in{std::string(body)};in.imbue(std::locale::classic());
  PlanFiles next;std::string header,tag,extra;std::size_t count=0;
  if(!(in>>header)||header!="CEVENVCALIBRATION/1"||
     !(in>>tag>>next.name)||tag!="name"||!durable::valid_id(next.name)||
     !(in>>tag>>std::quoted(next.training.path)>>next.training.sha256)||tag!="training"||
       !valid_path(next.training.path)||!durable::valid_digest(next.training.sha256)||
     !(in>>tag>>std::quoted(next.holdout.path)>>next.holdout.sha256)||tag!="holdout"||
       !valid_path(next.holdout.path)||!durable::valid_digest(next.holdout.sha256)||
     !(in>>tag>>next.criteria.max_absolute_error_micromolar)||tag!="max_absolute_error_micromolar"||
     !(in>>tag>>next.criteria.max_rmse_micromolar)||tag!="max_rmse_micromolar"||
     !(in>>tag>>count)||tag!="candidates"||!count||count>kMaxCandidates)return false;
  next.candidates.reserve(count);
  for(std::size_t i=0;i<count;++i){
    CandidateFile item;
    if(!(in>>tag>>item.id>>std::quoted(item.model.path)>>item.model.sha256)||tag!="candidate"||
       !durable::valid_id(item.id)||!valid_path(item.model.path)||!durable::valid_digest(item.model.sha256))return false;
    next.candidates.push_back(std::move(item));
  }
  if(!(in>>tag)||tag!="end"||in>>extra||serialize_plan(next)!=bytes)return false;
  output=std::move(next);return true;
}
inline bool read_reference(const fs::path& path,std::string_view expected,Reference& output,std::string& error){
  std::string bytes;
  if(!durable::read_bounded(path,kMaxReferenceBytes,bytes)||christeye::living::plans::sha256_hex(bytes)!=expected){
    error="reference file unavailable or digest mismatch";return false;
  }
  ReferenceFile record;
  if(!parse_reference(bytes,record)){error="invalid sealed reference file";return false;}
  if(!durable::read_bounded(path.parent_path()/record.raw.path,kMaxRawBytes,bytes)||
     christeye::living::plans::sha256_hex(bytes)!=record.raw.sha256){
    error="raw reference artifact unavailable or digest mismatch";return false;
  }
  output=std::move(record.reference);return true;
}
inline bool read_bundle(const fs::path& path,Plan& plan,Reference& training,Reference& holdout,std::string& error){
  std::string bytes;
  if(!durable::read_bounded(path,65536,bytes)){error="calibration plan unavailable";return false;}
  PlanFiles record;
  if(!parse_plan(bytes,record)){error="invalid sealed calibration plan";return false;}
  Plan next;Reference train,next_holdout;
  next.name=record.name;next.sha256=christeye::living::plans::sha256_hex(bytes);
  next.training_sha256=record.training.sha256;next.holdout_sha256=record.holdout.sha256;next.criteria=record.criteria;
  if(!read_reference(path.parent_path()/record.training.path,record.training.sha256,train,error)||
     !read_reference(path.parent_path()/record.holdout.path,record.holdout.sha256,next_holdout,error))return false;
  next.candidates.reserve(record.candidates.size());
  for(const auto& source:record.candidates){
    Candidate candidate;candidate.id=source.id;
    if(!anaiero::science::files::read_environment_model(path.parent_path()/source.model.path,candidate.model,candidate.model_sha256)||
       candidate.model_sha256!=source.model.sha256){error="candidate model unavailable or digest mismatch";return false;}
    next.candidates.push_back(std::move(candidate));
  }
  plan=std::move(next);training=std::move(train);holdout=std::move(next_holdout);error.clear();return true;
}
inline std::string json_string(std::string_view value){
  std::ostringstream out;out.imbue(std::locale::classic());out<<'"';
  for(unsigned char c:value){
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';return out.str();
}
inline void json_provenance(std::ostream& out,const Provenance& p,std::string_view digest){
  out<<"{\"dataset_sha256\":"<<json_string(digest)<<",\"origin\":"<<json_string(origin_name(p.origin))
    <<",\"run_id\":"<<json_string(p.run_id)<<",\"source\":"<<p.source<<",\"source_epoch\":"<<p.epoch
    <<",\"clock_domain\":"<<p.clock_domain<<",\"producer_sha256\":"<<json_string(p.producer_sha256)
    <<",\"protocol_sha256\":"<<json_string(p.protocol_sha256)<<",\"raw_artifact_sha256\":"<<json_string(p.raw_sha256)<<'}';
}
inline void json_error_summary(std::ostream& out,const ErrorSummary& value){
  out<<"{\"samples\":"<<value.count<<",\"sum_squared_error_micromolar_squared\":"<<value.sum_squared_error
    <<",\"rmse_micromolar\":"<<std::setprecision(17)
    <<(value.count?std::sqrt(static_cast<double>(value.sum_squared_error)/static_cast<double>(value.count)):0.0)
    <<",\"max_absolute_error_micromolar\":"<<value.max_absolute_error_micromolar
    <<",\"max_reference_uncertainty_micromolar\":"<<value.max_reference_uncertainty_micromolar<<'}';
}
inline std::string serialize_report(const Report& report){
  if(report.code!=Code::ok||!report.winner_frozen||report.selected_index>=report.candidates.size())return {};
  const auto& selected=report.candidates[report.selected_index];
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"{\n  \"schema\":\"anaiero.environment-calibration/1\",\n  \"plan_name\":"<<json_string(report.plan_name)
    <<",\n  \"plan_sha256\":"<<json_string(report.plan_sha256)
    <<",\n  \"outcome\":"<<json_string(report.passed?"PASS":"FAIL")
    <<",\n  \"classification\":"<<json_string(report.holdout_provenance.origin==ReferenceOrigin::synthetic_reference?
      "NUMERICAL_REFERENCE_COMPARISON":"DECLARED_EXTERNAL_REFERENCE_COMPARISON_UNVERIFIED")
    <<",\n  \"biological_validation\":false,\n  \"scientific_optimization_enabled\":false,"
    <<"\n  \"measurement_authenticity_verified\":false,\n  \"reference_interpretation\":"
    <<json_string("Reference origin, producer, protocol, run identity and uncertainties are declared metadata. Checked hashes bind supplied bytes; neither independent acquisition nor scientific validity is established.")
    <<",\n  \"quantity\":\"generic_scalar_environmental_concentration\",\n  \"unit\":\"micromolar\","
    <<"\n  \"step_ns\":"<<report.step_ns<<",\n  \"initial_micromolar\":"<<report.initial_micromolar
    <<",\n  \"training_initial_uncertainty_micromolar\":"<<report.training_initial_uncertainty_micromolar
    <<",\n  \"holdout_initial_uncertainty_micromolar\":"<<report.holdout_initial_uncertainty_micromolar
    <<",\n  \"behavior_sha256\":"<<json_string(report.behavior_sha256)
    <<",\n  \"selection_policy\":"<<json_string(report.selection_policy)
    <<",\n  \"fidelity_policy\":"<<json_string(report.fidelity_policy)
    <<",\n  \"winner_frozen_before_holdout\":true,\n  \"holdout_evaluations\":1,\n  \"selected_candidate\":"<<json_string(selected.id)
    <<",\n  \"selected_model_sha256\":"<<json_string(selected.model_sha256)
    <<",\n  \"frozen_selection\":{\"candidate\":"<<json_string(report.frozen_candidate_id)
    <<",\"model_sha256\":"<<json_string(report.frozen_model_sha256)<<'}'
    <<",\n  \"training_passed\":"<<(report.training_passed?"true":"false")
    <<",\n  \"holdout_passed\":"<<(report.holdout_passed?"true":"false")
    <<",\n  \"criteria\":{\"max_absolute_error_micromolar\":"<<report.criteria.max_absolute_error_micromolar
    <<",\"max_rmse_micromolar\":"<<report.criteria.max_rmse_micromolar<<",\"uncertainty_relaxes_threshold\":false},"
    <<"\n  \"training_reference\":";
  json_provenance(out,report.training_provenance,report.training_sha256);
  out<<",\n  \"holdout_reference\":";json_provenance(out,report.holdout_provenance,report.holdout_sha256);
  out<<",\n  \"training_candidates\":[";
  for(std::size_t i=0;i<report.candidates.size();++i){
    if(i)out<<',';
    const auto& candidate=report.candidates[i];out<<"\n    {\"id\":"<<json_string(candidate.id)
      <<",\"model_sha256\":"<<json_string(candidate.model_sha256)<<",\"error\":";
    json_error_summary(out,candidate.training);out<<'}';
  }
  out<<"\n  ],\n  \"holdout_error\":";json_error_summary(out,report.holdout);
  out<<"\n}\n";return out.str();
}
inline bool stage_exclusive(const fs::path& path,std::string_view bytes,bool& owned)noexcept{
  owned=false;
#ifdef _WIN32
  const auto handle=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(handle==INVALID_HANDLE_VALUE)return false;
  owned=true;bool ok=true;std::size_t offset=0;
  while(offset<bytes.size()){
    DWORD written=0;const auto count=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,1u<<20));
    if(!WriteFile(handle,bytes.data()+offset,count,&written,nullptr)||!written){ok=false;break;}
    offset+=written;
  }
  if(ok)ok=FlushFileBuffers(handle)!=0;
  CloseHandle(handle);return ok;
#else
  const int handle=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL
#ifdef O_CLOEXEC
    |O_CLOEXEC
#endif
    ,0600);
  if(handle<0)return false;
  owned=true;bool ok=true;std::size_t offset=0;
  while(offset<bytes.size()){
    const auto count=::write(handle,bytes.data()+offset,bytes.size()-offset);
    if(count<0&&errno==EINTR)continue;
    if(count<=0){ok=false;break;}
    offset+=static_cast<std::size_t>(count);
  }
  if(ok)ok=::fsync(handle)==0;
  ::close(handle);return ok;
#endif
}
inline bool write_new_report(const fs::path& requested,std::string_view bytes,std::string& error){
  if(bytes.empty()||bytes.size()>65536){error="invalid report bytes";return false;}
  std::error_code ec;const auto path=fs::absolute(requested,ec).lexically_normal();
  if(ec||path.filename().empty()){error="invalid report path";return false;}
  fs::create_directories(path.parent_path(),ec);
  if(ec){error="cannot create report directory";return false;}
  auto lock_path=path;lock_path+=".lock";
  if(fs::is_symlink(lock_path,ec)){error="report lock cannot be a symlink";return false;}
  durable::os::File lock;
  if(!lock.open(lock_path)||!lock.lock_exclusive()){error="report writer is already active";return false;}
  if(fs::exists(path,ec)||fs::is_symlink(path,ec)){error="report already exists";return false;}
  auto temporary=path;temporary+=".pending";
  bool owned=false;
  if(!stage_exclusive(temporary,bytes,owned)){
    if(owned)fs::remove(temporary,ec);
    error="cannot create and sync new report staging file (existing staging is preserved)";return false;
  }
  // Creating a hard link to the complete, synced staging file is an atomic
  // no-clobber publish on Windows and Linux. Even an unrelated competing
  // writer cannot make this operation replace an already existing report.
  fs::create_hard_link(temporary,path,ec);
  if(ec){fs::remove(temporary,ec);error="cannot publish new report (destination exists or hard links unavailable)";return false;}
  fs::remove(temporary,ec);
  if(ec||!durable::os::sync_directory(path.parent_path())){error="report published but directory durability could not be confirmed";return false;}
  error.clear();return true;
}
} // namespace anaiero::science::calibration::files
