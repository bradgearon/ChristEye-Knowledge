#pragma once
// Session parsing and WindStore adapters; simulation decisions stay in services.
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/infrastructure/windstore_checkpoint_catalog.hpp"
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "anaiero/infrastructure/scientific_solver_configuration.hpp"
#include "christeye/living/durable_registry.hpp"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
namespace anaiero::session::files {
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
inline bool read_configuration(const fs::path& path,Configuration& config,std::string& error) {
  std::string raw;
  if(!db::read_bounded(path,65536,raw)){error="cannot read session configuration";return false;}
  // Native Windows and Linux hosts share an identity across LF/CRLF editors.
  // Every other configuration byte, and each referenced file's bytes, is pinned.
  std::string normalized;normalized.reserve(raw.size());
  for(std::size_t i=0;i<raw.size();++i){
    if(raw[i]=='\0'){error="NUL in session configuration";return false;}
    if(raw[i]=='\r'){
      if(i+1>=raw.size()||raw[i+1]!='\n'){error="invalid configuration newline";return false;}
      continue;
    }
    normalized+=raw[i];
  }
  raw=std::move(normalized);
  std::istringstream in(raw);std::string line;
  Configuration next{};
  if(!std::getline(in,line)||(line!="CEVSESSION/1"&&line!="CEVSESSION/2")){error="expected CEVSESSION/1 or CEVSESSION/2";return false;}
  const bool version2=line=="CEVSESSION/2";
  if(!std::getline(in,line)||!line.starts_with("name ")||
     !db::valid_id(line.substr(5))||line.size()>69){error="invalid session name";return false;}
  next.name=line.substr(5);
  if(!std::getline(in,line)||!line.starts_with("period_ms ")){error="missing period_ms";return false;}
  std::uint64_t period=0;
  if(!db::parse_u64(std::string_view(line).substr(10),period)||period<10||period>60000){
    error="period_ms must be 10..60000";return false;
  }
  next.period_ms=static_cast<std::uint32_t>(period);
  if(version2){
    if(!std::getline(in,line)||!line.starts_with("storage segmented ")){error="expected storage segmented <segment-events>";return false;}
    std::uint64_t count=0;
    if(!db::parse_u64(std::string_view(line).substr(18),count)||count<8||count>4096){error="segment events must be 8..4096";return false;}
    next.sustained_storage=true;next.segment_events=static_cast<std::uint32_t>(count);
  }
  std::string identity=raw;
  bool ended=false;
  while(std::getline(in,line)) {
    if(line=="end"){ended=true;break;}
    if(next.count==next.engines.size()){error="too many engines";return false;}
    std::istringstream row(line);std::string tag,kind,extra,id_text,source_text,epoch_text;
    EngineConfig e{};unsigned cadence=0,lesson=0;
    std::uint64_t id=0,source=0;
    if(!(row>>tag>>id_text>>kind>>source_text>>epoch_text>>cadence>>
        std::quoted(e.model_path)>>std::quoted(e.initial_path)>>e.inputs.support_proxy>>
        e.inputs.stress_proxy>>e.inputs.digital_signal>>lesson)||row>>extra||tag!="engine"||
        !db::parse_u64(id_text,id)||!id||id>UINT32_MAX||
        !db::parse_u64(source_text,source)||!source||source>UINT32_MAX||
        !db::parse_u64(epoch_text,e.epoch)||!e.epoch||cadence<1||cadence>64||lesson>1||
        e.inputs.support_proxy<0||e.inputs.support_proxy>1000||e.inputs.stress_proxy<0||
        e.inputs.stress_proxy>1000||e.inputs.digital_signal<0||e.inputs.digital_signal>1000){
      error="invalid engine declaration";return false;
    }
    e.id=static_cast<std::uint32_t>(id);e.source=static_cast<std::uint32_t>(source);
    if(kind=="sw5")e.kind=engine::Kind::sw5;
    else if(kind=="sim3")e.kind=engine::Kind::sim3;
    else if(version2&&kind=="scientific")e.kind=engine::Kind::scientific;
    else {error="unsupported engine kind";return false;}
    e.cadence=static_cast<std::uint8_t>(cadence);e.inputs.lesson=lesson!=0;
    if(e.kind!=engine::Kind::sw5&&(e.initial_path!="-"||e.inputs.support_proxy||
       e.inputs.stress_proxy||e.inputs.digital_signal||e.inputs.lesson)){
      error="engine uses its declared model inputs";return false;
    }
    for(std::size_t i=0;i<next.count;++i)
      if(next.engines[i].id==e.id||next.engines[i].source==e.source){
        error="duplicate engine or source identity";return false;
      }
    auto pin=[&](std::string& value)->bool {
      if(value.empty()||value.size()>4096)return false;
      std::error_code ec;
      auto file=fs::absolute(path.parent_path()/fs::path(value),ec).lexically_normal();
      if(ec)return false;
      std::string content;
      if(!db::read_bounded(file,1024*1024,content))return false;
      identity+='\n';identity+=christeye::living::plans::sha256_hex(content);
      value=file.string();return true;
    };
    if(!pin(e.model_path)||(e.kind==engine::Kind::sw5&&!pin(e.initial_path))){
      error="cannot pin engine model or initial checkpoint";return false;
    }
    if(e.kind==engine::Kind::scientific){
      science::ScientificConfiguration solver;
      if(!science::files::read_solver_configuration(e.model_path,solver,error))return false;
      e.solver_configuration_sha256=solver.configuration_sha256;
      identity+='\n';identity+=solver.configuration_sha256;
    }
    next.engines[next.count++]=std::move(e);
  }
  if(!ended||!next.count||std::getline(in,line)){error="invalid session ending";return false;}
  next.digest=christeye::living::plans::sha256_hex(identity);
  config=std::move(next);error.clear();return true;
}
inline bool valid_inputs(const engine::Inputs& i)noexcept {
  return i.support_proxy>=0&&i.support_proxy<=1000&&i.stress_proxy>=0&&
    i.stress_proxy<=1000&&i.digital_signal>=0&&i.digital_signal<=1000;
}
inline std::string_view snapshot_digest(const engine::Snapshot& s)noexcept {
  const auto end=std::find(s.state_sha256.begin(),s.state_sha256.end(),'\0');
  return {s.state_sha256.data(),static_cast<std::size_t>(end-s.state_sha256.begin())};
}
inline bool empty_snapshot(const engine::Snapshot& s)noexcept {
  return !s.id&&!s.source_epoch&&!s.tick&&!s.revision&&!s.participants&&
    !s.connections&&!s.energy&&!s.learning_proxy&&
    std::all_of(s.state_sha256.begin(),s.state_sha256.end(),[](char c){return c=='\0';});
}
inline bool valid_snapshot(const engine::Snapshot& s,bool allow_empty=false,
                           bool forecast=false)noexcept {
  if(allow_empty&&empty_snapshot(s))return true;
  if(!s.id||!s.source_epoch||s.state_sha256.back()!='\0')return false;
  const auto sha=snapshot_digest(s);
  if(sha.empty()&&forecast)
    return std::all_of(s.state_sha256.begin(),s.state_sha256.end(),[](char c){return c=='\0';});
  return db::valid_digest(sha);
}
inline bool same_snapshot(const engine::Snapshot& a,const engine::Snapshot& b)noexcept {
  return a.id==b.id&&a.source_epoch==b.source_epoch&&a.tick==b.tick&&a.revision==b.revision&&
    a.participants==b.participants&&a.connections==b.connections&&a.energy==b.energy&&
    a.learning_proxy==b.learning_proxy&&a.state_sha256==b.state_sha256;
}
inline bool valid_candidate(const optimization::Candidate& c,bool allow_empty=true)noexcept {
  const auto kind=static_cast<unsigned>(c.kind);
  if(kind<1||kind>3||c.digital_cost>1000||c.model_risk>1000||c.guidance_weight>1000)return false;
  if(!c.id)return allow_empty&&!c.colony&&!c.from&&!c.to&&!c.digital_cost&&!c.model_risk&&!c.guidance_weight;
  return c.colony&&c.from&&c.to&&c.from!=c.to;
}
inline bool valid_record(const Record& r)noexcept {
  const auto& p=r.coordinator;
  if((r.schema!=1&&r.schema!=2)||(r.schema==1&&(r.operation_id||r.experiment_operation_id||r.experiment_pending||r.operation_cancelled||r.experiment_policy_paused))||
     (r.experiment_pending&&!r.experiment_operation_id)||
     !db::valid_digest(r.configuration_sha256)||p.schema!=1||!p.initialized||
     !p.engine_count||p.engine_count>engine::kMaxEngines||p.next_slot>p.engine_count||
     r.target_cycles<p.completed_cycles||!valid_inputs(p.default_inputs)||
     (!p.cycle_active&&(p.next_slot||p.publication_pending))||
     (p.cycle_active&&p.completed_cycles==UINT64_MAX)||
     !valid_snapshot(p.pending_snapshot,true))return false;
  for(std::size_t i=0;i<p.engine_count;++i){
    const auto& e=p.engines[i];const auto& v=r.evidence[i];const auto& route=v.route;
    const auto kind=static_cast<unsigned>(e.kind);
    if(r.schema>=2){
      const auto& c=r.contracts[i];
      if(c.behavior.empty()||c.behavior.size()>96||!c.input_schema||!c.metric_schema||!c.clock_domain||!c.checkpoint_schema)return false;
      for(const auto semantic:c.summaries)if(!engine::valid_summary_semantic(semantic))return false;
    }
    if(!e.id||!e.source||!e.source_epoch||kind<1||kind>3||!e.cadence||e.cadence>64||
       !db::valid_digest(e.model_digest)||!valid_snapshot(e.last)||
       e.last.id!=e.id||e.last.source_epoch!=e.source_epoch||!valid_inputs(e.cycle_inputs)||
       static_cast<unsigned>(e.last_status)>static_cast<unsigned>(engine::Status::publish_failed)||
       !v.next_sequence||!valid_snapshot(v.published,true)||
       (v.published.id&&(v.published.id!=e.id||v.published.source_epoch!=e.source_epoch))||
       route.prior_guidance>1000||route.change_id>r.changes||!valid_candidate(route.selected)||
       (route.selected.id&&route.selected.colony!=e.id)||
       (route.active&&(!route.change_id||!route.selected.id||e.kind!=engine::Kind::sw5))||
       (!route.selected.id&&(route.change_id||route.prior_existed||route.prior_guidance)))return false;
    for(std::size_t j=0;j<i;++j)
      if(p.engines[j].id==e.id||p.engines[j].source==e.source)return false;
  }
  if(p.publication_pending&&(p.next_slot>=p.engine_count||
     !same_snapshot(p.pending_snapshot,p.engines[p.next_slot].last)))return false;
  if(p.cycle_active&&p.next_slot<p.engine_count&&
     (p.completed_cycles+1)%p.engines[p.next_slot].cadence)return false;
  const auto& pub=r.publication;
  if(pub.engine_index>=p.engine_count||pub.metric_index>5||
     !valid_snapshot(pub.snapshot,true))return false;
  if(pub.active){
    const auto& e=p.engines[pub.engine_index];
    if(!valid_snapshot(pub.snapshot)||pub.snapshot.id!=e.id||pub.snapshot.source_epoch!=e.source_epoch||
       r.evidence[pub.engine_index].next_sequence>UINT64_MAX-5||
       pub.snapshot.tick>UINT64_MAX/1000000ULL||pub.snapshot.participants>INT32_MAX||
       pub.snapshot.connections>INT32_MAX||pub.snapshot.energy<INT32_MIN||pub.snapshot.energy>INT32_MAX||
       pub.snapshot.learning_proxy<INT32_MIN||pub.snapshot.learning_proxy>INT32_MAX)return false;
  }
  const auto& change=r.change;
  if(change.engine_index>=p.engine_count||change.target_checkpoint.size()>65536||
     change.target_checkpoint.find('\0')!=std::string::npos)return false;
  if(change.active&&(!db::valid_digest(change.source_sha256)||change.target_checkpoint.empty()||
     !change.target_checkpoint.starts_with("CEVCELLSTATE/1\n")||
     p.engines[change.engine_index].kind!=engine::Kind::sw5||p.cycle_active||
     (pub.active&&pub.engine_index!=change.engine_index)))return false;
  if(change.active){
    const auto& slot=p.engines[change.engine_index];
    const auto& route=r.evidence[change.engine_index].route;
    cells::Atlas atlas{};atlas.model_digest=slot.model_digest;cells::World target{};
    if(!route.change_id||!valid_candidate(route.selected,false)||
       cells::files::deserialize_world(change.target_checkpoint,atlas,target)!=cells::Status::ok||
       target.tick!=slot.last.tick||
       (target.generation!=slot.last.revision&&
        (slot.last.revision==UINT64_MAX||target.generation!=slot.last.revision+1)))return false;
    const auto target_sha=christeye::living::plans::sha256_hex(change.target_checkpoint);
    if(snapshot_digest(slot.last)!=change.source_sha256&&snapshot_digest(slot.last)!=target_sha)return false;
    if(pub.active&&(snapshot_digest(pub.snapshot)!=target_sha||pub.snapshot.tick!=target.tick||
                   pub.snapshot.revision!=target.generation))return false;
    bool found=false;
    for(std::size_t i=0;i<target.intent_count;++i){
      const auto& intent=target.intents[i];
      if(intent.from!=route.selected.from||intent.to!=route.selected.to)continue;
      found=true;
      if((route.active&&intent.guidance_weight!=route.selected.guidance_weight)||
         (!route.active&&(!route.prior_existed||intent.guidance_weight!=route.prior_guidance)))return false;
    }
    if((route.active||route.prior_existed)!=found)return false;
  }
  const auto& q=r.proposal;const auto& selection=q.selection;const auto& c=selection.candidate;
  const auto& forecast=selection.forecast;
  if(q.horizon>128||q.option.guidance>1000||q.option.uncertainty>1000||
     !valid_candidate(q.option.route)||c.cost>1000||c.risk>1000||c.uncertainty>1000||
     selection.commit_id>r.changes||!valid_snapshot(forecast.baseline,true,true)||
     !valid_snapshot(forecast.trial,true,true))return false;
  std::size_t proposal_index=0;
  while(proposal_index<p.engine_count&&p.engines[proposal_index].id!=q.engine_id)++proposal_index;
  if(q.engine_id&&(proposal_index==p.engine_count||!q.horizon||
                  p.engines[proposal_index].kind!=engine::Kind::sw5))return false;
  if(!q.engine_id&&(q.horizon||q.option.route.id||selection.approved||forecast.valid))return false;
  if(selection.approved){
    if(!q.engine_id||!forecast.valid||selection.score<=0||!c.id||!c.policy_token||
       c.id!=q.option.route.id||q.option.route.colony!=q.engine_id||
       !q.option.guidance||q.option.guidance!=q.option.route.guidance_weight||
       c.cost!=q.option.route.digital_cost||c.risk!=q.option.route.model_risk||
       c.uncertainty!=q.option.uncertainty)return false;
  }else if(c.id||c.policy_token||c.cost||c.risk||c.uncertainty||selection.score||selection.commit_id||
      q.option.route.id||q.option.guidance||q.option.uncertainty||forecast.valid||forecast.source_revision||
      !forecast.source_state_sha256.empty()||!empty_snapshot(forecast.baseline)||!empty_snapshot(forecast.trial)){
    return false;
  }
  if(forecast.valid){
    if(!q.engine_id||!forecast.source_revision||!db::valid_digest(forecast.source_state_sha256)||
       forecast.baseline.id!=q.engine_id||forecast.trial.id!=q.engine_id||
       forecast.baseline.source_epoch!=p.engines[proposal_index].source_epoch||
       forecast.trial.source_epoch!=forecast.baseline.source_epoch||
       forecast.baseline.revision!=forecast.source_revision||forecast.trial.revision!=forecast.source_revision||
       forecast.baseline.tick!=forecast.trial.tick||forecast.baseline.tick<q.horizon)return false;
  }else if(!forecast.source_state_sha256.empty()&&!db::valid_digest(forecast.source_state_sha256))return false;
  return true;
}
inline void put_inputs(std::ostream& out,const engine::Inputs& i){
  out<<i.support_proxy<<' '<<i.stress_proxy<<' '<<i.digital_signal<<' '<<i.lesson<<'\n';
}
inline bool get_inputs(std::istream& in,engine::Inputs& i){
  unsigned lesson=0;
  if(!(in>>i.support_proxy>>i.stress_proxy>>i.digital_signal>>lesson)||lesson>1)return false;
  i.lesson=lesson!=0;return valid_inputs(i);
}
inline void put_snapshot(std::ostream& out,const engine::Snapshot& s){
  out<<s.id<<' '<<s.source_epoch<<' '<<s.tick<<' '<<s.revision<<' '<<s.participants<<' '
     <<s.connections<<' '<<s.energy<<' '<<s.learning_proxy<<' '
     <<std::quoted(std::string(snapshot_digest(s)))<<'\n';
}
inline bool get_snapshot(std::istream& in,engine::Snapshot& s){
  std::string sha;
  if(!(in>>s.id>>s.source_epoch>>s.tick>>s.revision>>s.participants>>s.connections>>
       s.energy>>s.learning_proxy>>std::quoted(sha))||(!sha.empty()&&!db::valid_digest(sha)))return false;
  s.state_sha256={};std::copy(sha.begin(),sha.end(),s.state_sha256.begin());return true;
}
inline void put_candidate(std::ostream& out,const optimization::Candidate& c){
  out<<c.id<<' '<<c.colony<<' '<<c.from<<' '<<c.to<<' '<<static_cast<unsigned>(c.kind)
     <<' '<<c.digital_cost<<' '<<c.model_risk<<' '<<c.guidance_weight<<'\n';
}
inline bool get_candidate(std::istream& in,optimization::Candidate& c){
  unsigned kind=0;
  if(!(in>>c.id>>c.colony>>c.from>>c.to>>kind>>c.digital_cost>>c.model_risk>>c.guidance_weight)||
      kind<1||kind>3||c.digital_cost>1000||c.model_risk>1000||c.guidance_weight>1000)return false;
  c.kind=static_cast<optimization::RouteKind>(kind);return true;
}
inline std::string encode_record(const Record& r) {
  if(!valid_record(r))return {};
  std::ostringstream out;
  const auto& p=r.coordinator;
  out<<"CEVSESSIONSTATE/"<<r.schema<<'\n'<<r.revision<<' '<<r.changes<<' '<<r.target_cycles<<' '<<std::quoted(r.configuration_sha256);
  if(r.schema>=2)out<<' '<<r.operation_id<<' '<<r.experiment_operation_id<<' '<<r.experiment_pending<<' '<<r.operation_cancelled<<' '<<r.experiment_policy_paused;
  out<<'\n'<<p.schema<<' '<<p.completed_cycles<<' '<<static_cast<unsigned>(p.engine_count)<<' '<<static_cast<unsigned>(p.next_slot)<<' '
     <<p.initialized<<' '<<p.cycle_active<<' '<<p.publication_pending<<'\n';
  put_inputs(out,p.default_inputs);put_snapshot(out,p.pending_snapshot);
  for(std::size_t i=0;i<p.engine_count;++i){
    const auto& e=p.engines[i];const auto& v=r.evidence[i];const auto& route=v.route;
    out<<e.id<<' '<<e.source<<' '<<e.source_epoch<<' '<<static_cast<unsigned>(e.kind)<<' '
       <<static_cast<unsigned>(e.cadence)<<' '<<std::quoted(e.model_digest)<<' '
       <<static_cast<unsigned>(e.last_status)<<'\n';
    if(r.schema>=2){
      const auto& c=r.contracts[i];
      out<<std::quoted(c.behavior)<<' '<<c.input_schema<<' '<<c.metric_schema<<' '<<c.checkpoint_schema<<' '
        <<c.clock_domain<<' '<<c.tick_ns<<' '<<c.isolated_fork<<' '<<c.evaluate_candidates<<' '<<c.apply_changes<<' '<<c.restore;
      for(const auto semantic:c.summaries)out<<' '<<static_cast<unsigned>(semantic);
      out<<'\n';
    }
    put_snapshot(out,e.last);put_inputs(out,e.cycle_inputs);
    out<<v.next_sequence<<'\n';put_snapshot(out,v.published);
    out<<route.active<<' '<<route.prior_existed<<' '<<route.prior_guidance<<' '<<route.change_id<<'\n';
    put_candidate(out,route.selected);
  }
  out<<r.publication.active<<' '<<r.publication.engine_index<<' '<<r.publication.metric_index<<'\n';
  put_snapshot(out,r.publication.snapshot);
  out<<r.change.active<<' '<<r.change.engine_index<<' '<<std::quoted(r.change.source_sha256)<<' '
     <<std::quoted(r.change.target_checkpoint)<<'\n';
  const auto& q=r.proposal;const auto& s=q.selection;const auto& f=s.forecast;
  out<<q.engine_id<<' '<<q.horizon<<' '<<q.option.guidance<<' '<<q.option.uncertainty<<'\n';
  put_candidate(out,q.option.route);
  out<<s.candidate.id<<' '<<s.candidate.policy_token<<' '<<s.candidate.cost<<' '
     <<s.candidate.risk<<' '<<s.candidate.uncertainty<<' '<<s.score<<' '<<s.commit_id<<' '<<s.approved<<'\n'
     <<f.source_revision<<' '<<std::quoted(f.source_state_sha256)<<' '<<f.valid<<'\n';
  put_snapshot(out,f.baseline);put_snapshot(out,f.trial);
  out<<"end\n";return out.str();
}
inline bool decode_record(std::string_view wire,Record& output){
  if(wire.size()>1024*1024)return false;
  Record r{};std::istringstream in{std::string(wire)};std::string tag;
  auto& p=r.coordinator;unsigned engine_count=0,next_slot=0;
  if(!(in>>tag)||(tag!="CEVSESSIONSTATE/1"&&tag!="CEVSESSIONSTATE/2"))return false;
  r.schema=tag=="CEVSESSIONSTATE/1"?1u:2u;
  if(!(in>>r.revision>>r.changes>>r.target_cycles>>std::quoted(r.configuration_sha256))||!db::valid_digest(r.configuration_sha256))return false;
  if(r.schema>=2&&!(in>>r.operation_id>>r.experiment_operation_id>>r.experiment_pending>>r.operation_cancelled>>r.experiment_policy_paused))return false;
  if(
     !(in>>p.schema>>p.completed_cycles>>engine_count>>next_slot>>p.initialized>>p.cycle_active>>p.publication_pending)||
     p.schema!=1||!engine_count||engine_count>engine::kMaxEngines||next_slot>engine_count||
     !get_inputs(in,p.default_inputs)||!get_snapshot(in,p.pending_snapshot))return false;
  p.engine_count=static_cast<std::uint8_t>(engine_count);p.next_slot=static_cast<std::uint8_t>(next_slot);
  for(std::size_t i=0;i<p.engine_count;++i){
    auto& e=p.engines[i];auto& v=r.evidence[i];auto& route=v.route;
    unsigned kind=0,cadence=0,status=0;
    if(!(in>>e.id>>e.source>>e.source_epoch>>kind>>cadence>>std::quoted(e.model_digest)>>status)||
       !e.id||!e.source||!e.source_epoch||kind<1||kind>3||cadence<1||cadence>64||
       !db::valid_digest(e.model_digest)||status>static_cast<unsigned>(engine::Status::publish_failed))return false;
    if(r.schema>=2){
      auto& c=r.contracts[i];
      if(!(in>>std::quoted(c.behavior)>>c.input_schema>>c.metric_schema>>c.checkpoint_schema>>c.clock_domain>>c.tick_ns>>
           c.isolated_fork>>c.evaluate_candidates>>c.apply_changes>>c.restore))return false;
      for(auto& semantic:c.summaries){
        unsigned n=0;
        if(!(in>>n)||n>static_cast<unsigned>(engine::SummarySemantic::concentration_micromolar))return false;
        semantic=static_cast<engine::SummarySemantic>(n);
      }
    }
    if(!get_snapshot(in,e.last)||!get_inputs(in,e.cycle_inputs)||!(in>>v.next_sequence)||!v.next_sequence||
       !get_snapshot(in,v.published)||!(in>>route.active>>route.prior_existed>>route.prior_guidance>>route.change_id)||
       route.prior_guidance>1000||!get_candidate(in,route.selected))return false;
    e.kind=static_cast<engine::Kind>(kind);e.cadence=static_cast<std::uint8_t>(cadence);
    e.last_status=static_cast<engine::Status>(status);
  }
  if(!(in>>r.publication.active>>r.publication.engine_index>>r.publication.metric_index)||
     r.publication.engine_index>=p.engine_count||r.publication.metric_index>5||
     !get_snapshot(in,r.publication.snapshot)||
     !(in>>r.change.active>>r.change.engine_index>>std::quoted(r.change.source_sha256)>>std::quoted(r.change.target_checkpoint))||
     r.change.engine_index>=p.engine_count||r.change.target_checkpoint.size()>65536||
     (r.change.active&&(!db::valid_digest(r.change.source_sha256)||r.change.target_checkpoint.empty())))return false;
  auto& q=r.proposal;auto& s=q.selection;auto& f=s.forecast;
  if(!(in>>q.engine_id>>q.horizon>>q.option.guidance>>q.option.uncertainty)||
     q.horizon>128||q.option.guidance>1000||q.option.uncertainty>1000||!get_candidate(in,q.option.route)||
     !(in>>s.candidate.id>>s.candidate.policy_token>>s.candidate.cost>>s.candidate.risk>>s.candidate.uncertainty>>
       s.score>>s.commit_id>>s.approved>>f.source_revision>>std::quoted(f.source_state_sha256)>>f.valid)||
     !get_snapshot(in,f.baseline)||!get_snapshot(in,f.trial)||!(in>>tag)||tag!="end"||in>>tag)return false;
  // Canonical encoding rejects alternate signs, truncated booleans and trailing data.
  if(!valid_record(r)||encode_record(r)!=wire)return false;
  output=std::move(r);return true;
}
class WindStoreRecordStore final:public IRecordStore {
  db::Registry& db_;std::string name_;std::uint64_t revision_=0;
  std::uint64_t maintenance_sequence_=0;
  bool maintenance_loaded_=false;
  bool load_maintenance_cursor()noexcept {
    if(maintenance_loaded_)return true;
    db::ReplayCheckpoint checkpoint;std::string bytes;bool found=false;
    if(!db_.load_replay_checkpoint(checkpoint,bytes,found))return false;
    maintenance_sequence_=found?checkpoint.sequence:0;maintenance_loaded_=true;return true;
  }
  bool checkpoint_manifest(std::string_view wire,std::string_view object)noexcept {
    if(!db_.is_segmented())return true;
    // A distinct marker references the immutable manifest; storing its own hash
    // as a child would be a self-reference. The full manifest remains reviewable.
    const auto marker="CEVSESSIONMETADATA/1\nstore "+name_+"\nmanifest "+std::string(object)+"\n"+std::string(wire);
    const std::array<std::string,1> roots{std::string(object)};
    if(!db_.save_replay_checkpoint(marker,roots))return false;
    maintenance_sequence_=db_.event_count();maintenance_loaded_=true;
    return db_.archive_ready()==db::Code::ok;
  }
  static bool same_identity(const Record& a,const Record& b)noexcept {
    if(a.configuration_sha256!=b.configuration_sha256||
       a.coordinator.engine_count!=b.coordinator.engine_count)return false;
    for(std::size_t i=0;i<a.coordinator.engine_count;++i){
      const auto& x=a.coordinator.engines[i];const auto& y=b.coordinator.engines[i];
      if(x.id!=y.id||x.source!=y.source||x.source_epoch!=y.source_epoch||x.kind!=y.kind||
         x.cadence!=y.cadence||x.model_digest!=y.model_digest)return false;
      if(a.schema>=2&&b.schema>=2&&a.contracts[i]!=b.contracts[i])return false;
    }
    return true;
  }
  static bool progresses(const Record& prior,const Record& next)noexcept {
    if(!same_identity(prior,next)||next.changes<prior.changes||
       next.coordinator.completed_cycles<prior.coordinator.completed_cycles)return false;
    for(std::size_t i=0;i<prior.coordinator.engine_count;++i)
      if(next.evidence[i].next_sequence<prior.evidence[i].next_sequence)return false;
    return true;
  }
  bool scan(Record& output,bool& exists,std::uint64_t& revision)noexcept {
    exists=false;revision=0;
    if(!db_.is_open()||!db::valid_id(name_))return false;
    if(db_.is_segmented())return latest_manifest(output,exists,revision);
    Record staged{};
    for(const auto& e:db_.events()){
      if(e.kind!="CULTURE_CHECKPOINT"||e.entity!=name_||e.producer!="anaiero-session")continue;
      if(e.clock!="session"||e.origin!="SIMULATED"||revision==UINT64_MAX||e.source_tick!=revision+1)return false;
      std::string wire;Record next{};
      if(!db_.get(e.object_hash,wire)||!decode_record(wire,next)||next.revision!=e.source_tick||
         (exists&&!progresses(staged,next)))return false;
      staged=std::move(next);revision=staged.revision;exists=true;
    }
    if(exists)output=std::move(staged);
    return true;
  }
  bool latest_manifest(Record& output,bool& exists,std::uint64_t& revision)noexcept {
    exists=false;revision=0;
    if(!db_.is_open()||!db::valid_id(name_))return false;
    // The verified Registry owns the event chain. A save needs only its newest
    // matching manifest; replaying every prior manifest on every checkpoint
    // would make a growing session quadratic in decoding and disk reads.
    db::Event event{};
    if(!db_.latest_event("CULTURE_CHECKPOINT",name_,"session",event,exists,"anaiero-session"))return false;
    if(!exists)return true;
    std::string wire;
    if(event.origin!="SIMULATED"||!event.source_tick||!db_.get(event.object_hash,wire)||
       !decode_record(wire,output)||output.revision!=event.source_tick)return false;
    revision=output.revision;return true;
  }
public:
  WindStoreRecordStore(db::Registry& database,std::string name):db_(database),name_(std::move(name)){}
  bool load(Record& record,bool& exists)noexcept override{
    Record staged{};bool found=false;std::uint64_t revision=0;
    exists=false;
    if(!load_maintenance_cursor()||!scan(staged,found,revision))return false;
    if(found)record=std::move(staged);
    exists=found;revision_=revision;
    return true;
  }
  bool save(Record& record)noexcept override{
    if(record.revision!=revision_||revision_==UINT64_MAX||!valid_record(record)||!load_maintenance_cursor())return false;
    // The Registry has one disk writer, but multiple store adapters may share
    // that owner. Recheck its actual manifest revision to reject stale adapters.
    Record latest{};bool exists=false;std::uint64_t actual_revision=0;
    if(!latest_manifest(latest,exists,actual_revision)||actual_revision!=revision_||
       (exists&&!progresses(latest,record)))return false;
    auto next=record;++next.revision;next.schema=2;
    const auto wire=encode_record(next);if(wire.empty())return false;
    auto value=db_.put(wire);
    if(value.code!=db::Code::ok||db_.publish("CULTURE_CHECKPOINT",name_,value.hash,
       "SIMULATED",next.revision,"session","anaiero-session").code!=db::Code::ok)return false;
    // Manifest durability precedes maintenance. Preserve the committed revision
    // even if a later checkpoint/archive boundary reports failure.
    record.revision=next.revision;record.schema=next.schema;revision_=next.revision;
    if(db_.is_segmented()&&db_.event_count()-maintenance_sequence_>=db_.capacity().event_limit)
      return checkpoint_manifest(wire,value.hash);
    return true;
  }
  // Metadata owner only. Suitable for offline maintenance after worker join.
  bool checkpoint_now()noexcept {
    if(!db_.is_open()||!load_maintenance_cursor())return false;
    if(!db_.is_segmented())return true;
    db::Event latest;bool found=false;
    if(!db_.latest_event("CULTURE_CHECKPOINT",name_,"session",latest,found,"anaiero-session")||!found)return false;
    std::string wire;
    return latest.origin=="SIMULATED"&&db_.get(latest.object_hash,wire)&&checkpoint_manifest(wire,latest.object_hash);
  }
};
class CatalogCheckpointStore final:public IEngineCheckpointStore {
  engine::ICheckpointCatalog& catalog_;
public:
  explicit CatalogCheckpointStore(engine::ICheckpointCatalog& catalog):catalog_(catalog){}
  bool save(const engine::Descriptor& d,const engine::Snapshot& s,std::string_view bytes)noexcept override{
    if(!d.id||!d.source||!d.source_epoch||!db::valid_digest(d.model_digest)||
       !valid_snapshot(s)||s.id!=d.id||s.source_epoch!=d.source_epoch||bytes.empty())return false;
    const auto hash=christeye::living::plans::sha256_hex(bytes);
    if(hash!=snapshot_digest(s)||s.revision==UINT64_MAX)return false;
    // Catalog revision is one-based; a newly opened SIM3 engine has revision zero.
    engine::CheckpointRecord r{d.id,d.source,d.source_epoch,s.revision+1,std::string(d.model_digest),hash};
    engine::CheckpointRecord old{};std::string previous;
    if(catalog_.latest(d.id,d.source_epoch,old,previous)){
      if(old.engine!=r.engine||old.epoch!=r.epoch||old.source!=r.source||
         old.model_sha256!=r.model_sha256||old.revision>r.revision)return false;
      if(old.revision==r.revision)return old.checkpoint_sha256==hash&&previous==bytes;
    }
    return catalog_.commit(r,bytes);
  }
};
} // namespace anaiero::session::files
