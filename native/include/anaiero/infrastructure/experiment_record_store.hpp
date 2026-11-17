#pragma once
// WindStore CAS experiment records. A record references immutable definition
// and source-checkpoint objects; journal publication is the commit boundary.
#include "anaiero/infrastructure/experiment_definition_file.hpp"
#include "christeye/living/durable_registry.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace anaiero::experiments::files {
inline constexpr std::size_t kMaxRecordBytes=128*1024;
inline std::string_view digest_view(const engine::Snapshot& s)noexcept {
  const auto end=std::find(s.state_sha256.begin(),s.state_sha256.end(),'\0');
  return {s.state_sha256.data(),static_cast<std::size_t>(end-s.state_sha256.begin())};
}
inline bool snapshot_shape(const engine::Snapshot& s)noexcept {
  const auto sha=digest_view(s);
  if(sha.empty())return !s.id&&!s.source_epoch&&!s.tick&&!s.revision&&!s.participants&&!s.connections&&
    !s.energy&&!s.learning_proxy&&std::all_of(s.state_sha256.begin(),s.state_sha256.end(),[](char c){return c=='\0';});
  return s.id&&s.source_epoch&&s.state_sha256.back()=='\0'&&db::valid_digest(sha)&&
    s.participants<=1000000000&&s.connections<=1000000000&&s.energy>=-1000000000&&s.energy<=1000000000&&
    s.learning_proxy>=-1000000000&&s.learning_proxy<=1000000000;
}
inline bool forecast_shape(const engine::Forecast& f)noexcept {
  if(!snapshot_shape(f.baseline)||!snapshot_shape(f.trial))return false;
  if(f.valid)return f.baseline.id&&f.trial.id&&db::valid_digest(f.source_state_sha256);
  return !f.baseline.id&&!f.trial.id&&!f.source_revision&&f.source_state_sha256.empty();
}
inline bool ranking_shape(const engine::Candidate& c)noexcept {
  return c.id?(c.policy_token&&c.cost<=1000&&c.risk<=1000&&c.uncertainty<=1000):
    (!c.policy_token&&!c.cost&&!c.risk&&!c.uncertainty);
}
inline bool record_shape(const Record& r) {
  const auto state=static_cast<unsigned>(r.state),failure=static_cast<unsigned>(r.failure);
  const auto decision=static_cast<unsigned>(r.outcome.decision);
  const auto& c=r.context;
  if(!db::valid_id(r.operation_id)||r.operation_id.size()>64||!r.revision||state<1||state>5||failure>9||
     !accepts_definition(r.definition)||r.next_candidate>r.definition.candidate_count||
     !c.engine_id||!c.source||!c.source_epoch||!c.metric_schema||!c.clock_domain||
     !db::valid_digest(c.source_state_sha256)||!db::valid_digest(c.model_sha256)||!db::valid_digest(c.input_schedule_sha256)||
     c.behavior_id.empty()||c.behavior_id.size()>96||!ranking_shape(r.selection.candidate)||
     !forecast_shape(r.selection.forecast)||decision>2||!snapshot_shape(r.outcome.observed))return false;
  const auto& d=r.definition;
  if(c.engine_id!=d.engine_id||c.model_sha256!=d.model_sha256||c.behavior_id!=d.behavior_id||
     c.metric_schema!=d.metric_schema||c.clock_domain!=d.clock_domain||c.tick_ns!=d.tick_ns||c.seed!=d.seed||
     c.input_schedule_sha256!=d.input_schedule_sha256||c.start_tick>UINT64_MAX-d.optimization.horizon)return false;
  for(std::size_t i=0;i<r.definition.candidate_count;++i){const auto& t=r.trials[i];
    const auto ts=static_cast<unsigned>(t.state),tc=static_cast<unsigned>(t.status);
    if(ts>2||tc>9||!ranking_shape(t.candidate)||!forecast_shape(t.forecast))return false;
    if(t.state==TrialState::pending){if(t.candidate.id||t.forecast.valid||!t.receipt_sha256.empty()||t.status!=Code::ok)return false;}
    else if(!t.candidate.id||!db::valid_digest(t.receipt_sha256)||
      (t.state==TrialState::completed&&(!t.forecast.valid||t.status!=Code::ok))||
      (t.state==TrialState::failed&&(t.forecast.valid||t.status==Code::ok)))return false;
  }
  return true;
}
inline void write_snapshot(std::ostream& out,std::string_view tag,const engine::Snapshot& s) {
  out<<tag<<' '<<s.id<<' '<<s.source_epoch<<' '<<s.tick<<' '<<s.revision<<' '<<s.participants<<' '
    <<s.connections<<' '<<s.energy<<' '<<s.learning_proxy<<' '<<std::quoted(std::string(digest_view(s)))<<'\n';
}
inline bool read_snapshot(Lines& in,std::string_view tag,engine::Snapshot& s) {
  std::string sha;
  if(!in.row(tag,s.id,s.source_epoch,s.tick,s.revision,s.participants,s.connections,s.energy,s.learning_proxy,std::quoted(sha))||sha.size()>64)return false;
  std::copy(sha.begin(),sha.end(),s.state_sha256.begin());return snapshot_shape(s);
}
inline void write_forecast(std::ostream& out,const engine::Forecast& f) {
  out<<"forecast "<<f.valid<<' '<<f.source_revision<<' '<<std::quoted(f.source_state_sha256)<<'\n';
  write_snapshot(out,"baseline",f.baseline);write_snapshot(out,"trial_result",f.trial);
}
inline bool read_forecast(Lines& in,engine::Forecast& f) {
  unsigned valid=0;
  if(!in.row("forecast",valid,f.source_revision,std::quoted(f.source_state_sha256))||valid>1||
     !read_snapshot(in,"baseline",f.baseline)||!read_snapshot(in,"trial_result",f.trial))return false;
  f.valid=valid!=0;return forecast_shape(f);
}
inline std::string serialize_record(const Record& r) {
  if(!record_shape(r))return {};
  const auto definition=serialize_definition(r.definition);
  std::ostringstream out;
  out<<"CEVEXPERIMENTSTATE/1\noperation "<<r.operation_id<<' '<<r.revision<<' '<<static_cast<unsigned>(r.state)<<' '
    <<static_cast<unsigned>(r.failure)<<' '<<static_cast<unsigned>(r.next_candidate)<<"\ndefinition_bytes "<<definition.size()<<'\n'
    <<definition;
  const auto& c=r.context;
  out<<"context "<<c.engine_id<<' '<<c.source<<' '<<c.source_epoch<<' '<<c.source_revision<<' '<<c.start_tick<<' '
     <<c.metric_schema<<' '<<c.clock_domain<<' '<<c.tick_ns<<' '<<c.seed<<' '<<c.source_state_sha256<<' '
     <<c.model_sha256<<' '<<std::quoted(c.behavior_id)<<' '<<c.input_schedule_sha256<<'\n';
  for(std::size_t i=0;i<r.definition.candidate_count;++i){const auto& t=r.trials[i];const auto& candidate=t.candidate;
    out<<"receipt "<<static_cast<unsigned>(t.state)<<' '<<static_cast<unsigned>(t.status)<<' '<<candidate.id<<' '
       <<candidate.policy_token<<' '<<candidate.cost<<' '<<candidate.risk<<' '<<candidate.uncertainty<<' '<<std::quoted(t.receipt_sha256)<<'\n';
    write_forecast(out,t.forecast);
  }
  const auto& s=r.selection;const auto& candidate=s.candidate;
  out<<"selection "<<s.approved<<' '<<s.score<<' '<<s.commit_id<<' '<<candidate.id<<' '<<candidate.policy_token<<' '
     <<candidate.cost<<' '<<candidate.risk<<' '<<candidate.uncertainty<<'\n';
  write_forecast(out,s.forecast);
  const auto& o=r.outcome;
  out<<"outcome "<<static_cast<unsigned>(o.decision)<<' '<<o.change_id<<' '<<o.applied_tick<<'\n';
  write_snapshot(out,"observed",o.observed);
  out<<"residuals";for(const auto residual:o.residuals)out<<' '<<residual;
  out<<'\n';const auto digest=pl::sha256_hex(out.str());out<<"seal "<<digest<<'\n';return out.str();
}
inline bool deserialize_record(std::string_view raw,Record& output) {
  if(raw.size()>kMaxRecordBytes)return false;
  // Stored records are canonical LF bytes. A digest binds the exact byte stream.
  const auto marker=raw.find("\ndefinition_bytes ");
  if(marker==std::string_view::npos)return false;
  const auto start=raw.find('\n',marker+1);
  if(start==std::string_view::npos)return false;
  Lines header(raw.substr(0,start+1));Record r{};unsigned state=0,failure=0,next=0;std::size_t length=0;
  if(!header.row("CEVEXPERIMENTSTATE/1")||!header.row("operation",r.operation_id,r.revision,state,failure,next)||
     state<1||state>5||failure>9||next>kMaxCandidates||!header.row("definition_bytes",length)||!header.end()||
     !length||length>kMaxDefinitionBytes||length>raw.size()-start-1)return false;
  r.state=static_cast<RunState>(state);r.failure=static_cast<Code>(failure);r.next_candidate=static_cast<std::uint8_t>(next);
  std::string error;
  if(!parse_definition(raw.substr(start+1,length),r.definition,error))return false;
  Lines in(raw.substr(start+1+length));auto& c=r.context;
  if(!in.row("context",c.engine_id,c.source,c.source_epoch,c.source_revision,c.start_tick,c.metric_schema,c.clock_domain,c.tick_ns,c.seed,
      c.source_state_sha256,c.model_sha256,std::quoted(c.behavior_id),c.input_schedule_sha256))return false;
  for(std::size_t i=0;i<r.definition.candidate_count;++i){auto& t=r.trials[i];auto& candidate=t.candidate;unsigned ts=0,tc=0;
    if(!in.row("receipt",ts,tc,candidate.id,candidate.policy_token,candidate.cost,candidate.risk,candidate.uncertainty,
       std::quoted(t.receipt_sha256))||ts>2||tc>9||!read_forecast(in,t.forecast))return false;
    t.state=static_cast<TrialState>(ts);t.status=static_cast<Code>(tc);
  }
  auto& s=r.selection;auto& candidate=s.candidate;unsigned approved=0;
  if(!in.row("selection",approved,s.score,s.commit_id,candidate.id,candidate.policy_token,candidate.cost,candidate.risk,candidate.uncertainty)||
     approved>1||!read_forecast(in,s.forecast))return false;
  s.approved=approved!=0;auto& o=r.outcome;unsigned decision=0;std::string seal;
  if(!in.row("outcome",decision,o.change_id,o.applied_tick)||decision>2||!read_snapshot(in,"observed",o.observed)||
     !in.row("residuals",o.residuals[0],o.residuals[1],o.residuals[2],o.residuals[3])||!in.row("seal",seal)||!in.end())return false;
  o.decision=static_cast<OutcomeDecision>(decision);
  if(!record_shape(r)||serialize_record(r)!=raw)return false;
  output=std::move(r);return true;
}
inline bool register_definition(db::Registry& registry,const fs::path& path,Definition& output,std::string& error) {
  std::string bytes;Definition d{};std::vector<PinnedArtifact> artifacts;
  if(!pl::read_bounded(path,kMaxDefinitionBytes,bytes)){error="cannot read experiment definition";return false;}
  if(!parse_definition(bytes,d,error)||!verify_artifacts(d,path.parent_path(),artifacts,error))return false;
  std::vector<std::string> references;
  for(const auto& artifact:artifacts){const auto result=registry.put(artifact.bytes);
    if(result.code!=db::Code::ok||result.hash!=artifact.sha256){error="cannot persist experiment artifact";return false;}
    if(std::find(references.begin(),references.end(),result.hash)==references.end())references.push_back(result.hash);
  }
  const auto object=registry.put_referenced(serialize_definition(d),references);
  if(object.code!=db::Code::ok||registry.publish("PLAN_REGISTERED",d.sha256,object.hash,"SIMULATED",0,
      "experiment-plan","anaiero-experiment").code!=db::Code::ok){error="cannot publish experiment definition";return false;}
  output=std::move(d);error.clear();return true;
}
inline bool lookup_definition(db::Registry& registry,std::string_view name,std::string_view sha256,Definition& output) {
  if(!db::valid_id(name)||!db::valid_digest(sha256))return false;
  db::Event event;bool found=false;std::string bytes,error;Definition next{};
  if(!registry.latest_event("PLAN_REGISTERED",sha256,"experiment-plan",event,found,"anaiero-experiment")||!found||
     !registry.get(event.object_hash,bytes)||!parse_definition(bytes,next,error)||next.name!=name||next.sha256!=sha256)return false;
  output=std::move(next);return true;
}
class WindStoreExperimentStore final:public IRecordStore {
  db::Registry& registry_;
  static bool progress_allows(const Record& old,const Record& next) {
    if(old.definition.sha256!=next.definition.sha256||old.context.source_state_sha256!=next.context.source_state_sha256||
       old.context.source_epoch!=next.context.source_epoch||old.context.source_revision!=next.context.source_revision||
       old.context.source!=next.context.source||old.context.engine_id!=next.context.engine_id||
       old.context.start_tick!=next.context.start_tick||
       old.next_candidate>next.next_candidate||next.next_candidate>old.next_candidate+1)return false;
    for(std::size_t i=0;i<old.next_candidate;++i)if(old.trials[i].receipt_sha256!=next.trials[i].receipt_sha256)return false;
    switch(old.state){
      case RunState::prepared:return (next.state==RunState::running||next.state==RunState::paused)&&!next.next_candidate;
      case RunState::running:return next.state==RunState::running||next.state==RunState::paused||next.state==RunState::completed||next.state==RunState::failed;
      case RunState::paused:return next.state==RunState::running&&next.next_candidate==old.next_candidate;
      case RunState::completed:return next.state==RunState::completed&&next.next_candidate==old.next_candidate&&
        (!old.outcome.change_id||old.outcome.change_id==next.outcome.change_id)&&
        (old.outcome.decision==OutcomeDecision::pending||old.outcome.decision==next.outcome.decision);
      case RunState::failed:return false;
    }
    return false;
  }
public:
  explicit WindStoreExperimentStore(db::Registry& registry):registry_(registry){}
  bool accepts_definition(const Definition& d)const noexcept override{return files::accepts_definition(d);}
  bool load(std::string_view id,Record& output,bool& exists)noexcept override {
    exists=false;if(!db::valid_id(id)||id.size()>64)return false;
    db::Event event;bool found=false;
    if(!registry_.latest_event("MODEL_EVALUATION",id,"experiment",event,found,"anaiero-experiment"))return false;
    if(!found)return true;
    std::string bytes;Record next{};
    if(!registry_.get(event.object_hash,bytes)||!deserialize_record(bytes,next)||next.operation_id!=id||next.revision!=event.source_tick||
       !registry_.get(next.context.source_state_sha256,next.source_checkpoint)||next.source_checkpoint.empty()||
       next.source_checkpoint.size()>kMaxCheckpointBytes||pl::sha256_hex(next.source_checkpoint)!=next.context.source_state_sha256)return false;
    output=std::move(next);exists=true;return true;
  }
  bool save(Record& record)noexcept override {
    if(record.revision==UINT64_MAX||record.source_checkpoint.empty()||record.source_checkpoint.size()>kMaxCheckpointBytes||
       pl::sha256_hex(record.source_checkpoint)!=record.context.source_state_sha256||!accepts_definition(record.definition))return false;
    Record current{};bool exists=false;
    if(!load(record.operation_id,current,exists))return false;
    Record next=record;++next.revision;
    const auto bytes=serialize_record(next);if(bytes.empty()||bytes.size()>kMaxRecordBytes)return false;
    if(exists&&current.revision==next.revision&&serialize_record(current)==bytes){record.revision=next.revision;return true;}
    if((exists&&(current.revision!=record.revision||!progress_allows(current,next)))||
       (!exists&&(record.revision||record.next_candidate||record.state!=RunState::prepared)))return false;
    std::vector<std::string> references;
    for(std::size_t i=0;i<record.definition.artifact_count;++i)references.push_back(record.definition.artifacts[i].sha256);
    if(!record.definition.learning_plan_sha256.empty()){
      references.push_back(record.definition.learning_plan_sha256);references.push_back(record.definition.learning_base_sha256);
      std::string plan_bytes;pl::Plan plan;
      if(!registry_.get(record.definition.learning_plan_sha256,plan_bytes)||pl::Plan::parse(plan_bytes,plan).error!=pl::Error::ok)return false;
      for(const auto& artifact:plan.artifacts())references.push_back(artifact.sha256);
    }
    std::sort(references.begin(),references.end());references.erase(std::unique(references.begin(),references.end()),references.end());
    const auto definition=registry_.put_referenced(serialize_definition(record.definition),references);
    if(definition.code!=db::Code::ok)return false;
    const auto checkpoint=registry_.put(record.source_checkpoint);if(checkpoint.code!=db::Code::ok)return false;
    const std::array<std::string,2> children{{definition.hash,checkpoint.hash}};
    const auto object=registry_.put_referenced(bytes,children);
    if(object.code!=db::Code::ok||registry_.publish("MODEL_EVALUATION",record.operation_id,object.hash,"SIMULATED",next.revision,
      "experiment","anaiero-experiment").code!=db::Code::ok)return false;
    record.revision=next.revision;return true;
  }
};
} // namespace anaiero::experiments::files
