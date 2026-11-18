#pragma once
// Canonical immutable experiment files. Original CEVPLAN assets and bindings
// are validated by the original reader; VLM actions are provenance, not input.
#include "anaiero/contracts/experiment.hpp"
#include "christeye/living/learning_plan.hpp"
#include "christeye/living/durable_records.hpp"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace anaiero::experiments::files {
namespace pl=christeye::living::plans;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
inline constexpr std::size_t kMaxDefinitionBytes=32768;
inline bool definition_shape(const Definition& d)noexcept {
  if(!db::valid_id(d.name)||d.name.size()>64||!d.engine_id||!db::valid_digest(d.model_sha256)||
     d.behavior_id.empty()||d.behavior_id.size()>96||!d.metric_schema||!d.clock_domain||
     (d.kind!=engine::Kind::sw5&&d.kind!=engine::Kind::sim3)||!d.optimization.goal_count||
     d.optimization.goal_count>d.optimization.objectives.size()||!d.optimization.horizon||d.optimization.horizon>128||
     !d.stage_count||d.stage_count>kMaxStages||!d.candidate_count||d.candidate_count>kMaxCandidates||
     d.artifact_count>kMaxArtifacts||d.optimization.minimum_score<1||d.outcome_tolerance<0||d.outcome_tolerance>1000000000||
     d.outcome_ticks>128)return false;
  for(char c:d.behavior_id)if(c<'!'||c>'~'||c=='"'||c=='\\')return false;
  std::array<bool,5> metrics{};
  for(std::size_t i=0;i<d.optimization.goal_count;++i){const auto& o=d.optimization.objectives[i];
    const auto m=static_cast<unsigned>(o.metric);
    if(m<1||m>4||metrics[m]||!o.importance||o.importance>1000||o.ideal< -1000000000||o.ideal>1000000000)return false;
    metrics[m]=true;
  }
  unsigned ticks=0;
  for(std::size_t i=0;i<d.stage_count;++i){const auto& s=d.stages[i];const auto& in=s.inputs;
    if(!db::valid_id(s.id)||s.id.size()>64||!s.ticks||s.ticks>128||in.support_proxy<0||in.support_proxy>1000||
       in.stress_proxy<0||in.stress_proxy>1000||in.digital_signal<0||in.digital_signal>1000)return false;
    for(std::size_t j=0;j<i;++j)if(d.stages[j].id==s.id)return false;
    ticks+=s.ticks;
  }
  if(ticks!=d.optimization.horizon)return false;
  for(std::size_t i=0;i<d.candidate_count;++i){const auto& c=d.candidates[i];const auto& r=c.ranking;
    const auto kind=static_cast<unsigned>(c.kind);
    if(!r.id||!r.policy_token||r.cost>1000||r.risk>1000||r.uncertainty>1000||kind<1||kind>3||
       !db::valid_id(c.from)||!db::valid_id(c.to)||c.from.size()>56||c.to.size()>56||c.value>1000||c.channel>3)return false;
    for(std::size_t j=0;j<i;++j)if(d.candidates[j].ranking.id==r.id||d.candidates[j].ranking.policy_token==r.policy_token)return false;
  }
  for(std::size_t i=0;i<d.artifact_count;++i){const auto& a=d.artifacts[i];
    if(!db::valid_id(a.id)||a.id.size()>64||!pl::allowed_role(a.role)||!pl::valid_file_path(a.relative_path)||!db::valid_digest(a.sha256))return false;
    for(std::size_t j=0;j<i;++j)if(d.artifacts[j].id==a.id)return false;
  }
  const bool learning=!d.learning_plan_path.empty();
  if(learning!=!d.learning_plan_sha256.empty()||learning!=!d.learning_base_path.empty()||learning!=!d.learning_base_sha256.empty())return false;
  return !learning||(pl::valid_file_path(d.learning_plan_path)&&pl::valid_file_path(d.learning_base_path)&&
    db::valid_digest(d.learning_plan_sha256)&&db::valid_digest(d.learning_base_sha256));
}
inline std::string schedule_sha256(const Definition& d) {
  if(!d.stage_count||d.stage_count>kMaxStages)return {};
  std::ostringstream out;out<<"CEVEXPERIMENTINPUTS/1\n"<<static_cast<unsigned>(d.stage_count)<<'\n';
  for(std::size_t i=0;i<d.stage_count;++i){const auto& s=d.stages[i];
    out<<s.id<<' '<<s.ticks<<' '<<s.inputs.support_proxy<<' '<<s.inputs.stress_proxy<<' '
       <<s.inputs.digital_signal<<' '<<s.inputs.lesson<<'\n';}
  return pl::sha256_hex(out.str());
}
inline std::string serialize_definition(const Definition& d,bool seal=true) {
  if(!definition_shape(d))return {};
  std::ostringstream out;out<<"CEVEXPERIMENT/1\nname "<<d.name<<"\nengine "<<d.engine_id<<' '
    <<static_cast<unsigned>(d.kind)<<' '<<d.model_sha256<<' '<<std::quoted(d.behavior_id)
    <<"\nidentity "<<d.metric_schema<<' '<<d.clock_domain<<' '<<d.tick_ns<<' '<<d.seed<<' '<<d.input_schedule_sha256
    <<"\noptimization "<<d.optimization.horizon<<' '<<d.optimization.minimum_score<<' '<<static_cast<unsigned>(d.optimization.goal_count)<<'\n';
  for(std::size_t i=0;i<d.optimization.goal_count;++i){const auto& o=d.optimization.objectives[i];
    out<<"objective "<<static_cast<unsigned>(o.metric)<<' '<<o.ideal<<' '<<o.importance<<' '<<o.forbid_regression<<'\n';}
  out<<"outcome "<<d.outcome_ticks<<' '<<d.outcome_tolerance<<' '<<d.automatic_apply<<' '<<d.automatic_revert
    <<"\nstages "<<static_cast<unsigned>(d.stage_count)<<'\n';
  for(std::size_t i=0;i<d.stage_count;++i){const auto& s=d.stages[i];
    out<<"stage "<<s.id<<' '<<s.ticks<<' '<<s.inputs.support_proxy<<' '<<s.inputs.stress_proxy<<' '
      <<s.inputs.digital_signal<<' '<<s.inputs.lesson<<'\n';}
  out<<"candidates "<<static_cast<unsigned>(d.candidate_count)<<'\n';
  for(std::size_t i=0;i<d.candidate_count;++i){const auto& c=d.candidates[i];const auto& r=c.ranking;
    out<<"candidate "<<r.id<<' '<<r.policy_token<<' '<<r.cost<<' '<<r.risk<<' '<<r.uncertainty<<' '
      <<static_cast<unsigned>(c.kind)<<' '<<c.from<<' '<<c.to<<' '<<c.value<<' '<<static_cast<unsigned>(c.channel)<<'\n';}
  out<<"artifacts "<<static_cast<unsigned>(d.artifact_count)<<'\n';
  for(std::size_t i=0;i<d.artifact_count;++i){const auto& a=d.artifacts[i];
    out<<"artifact "<<a.id<<' '<<a.role<<' '<<a.relative_path<<' '<<a.sha256<<'\n';}
  out<<"learning "<<std::quoted(d.learning_plan_path)<<' '<<std::quoted(d.learning_plan_sha256)<<' '
     <<std::quoted(d.learning_base_path)<<' '<<std::quoted(d.learning_base_sha256)<<'\n';
  if(seal){const auto digest=pl::sha256_hex(out.str());out<<"seal "<<digest<<'\n';}
  return out.str();
}
inline bool seal_definition(Definition& d) {
  if(!definition_shape(d))return false;
  d.input_schedule_sha256=schedule_sha256(d);
  d.sha256=pl::sha256_hex(serialize_definition(d,false));return true;
}
inline bool accepts_definition(const Definition& d) {
  return definition_shape(d)&&db::valid_digest(d.sha256)&&d.input_schedule_sha256==schedule_sha256(d)&&
    d.sha256==pl::sha256_hex(serialize_definition(d,false));
}
inline bool normalize_newlines(std::string_view raw,std::string& normalized) {
  normalized.clear();normalized.reserve(raw.size());
  for(std::size_t i=0;i<raw.size();++i){
    if(raw[i]=='\0')return false;
    if(raw[i]=='\r'){if(i+1==raw.size()||raw[i+1]!='\n')return false;continue;}
    normalized+=raw[i];
  }
  return true;
}
class Lines final {
  std::istringstream input_;
public:
  explicit Lines(std::string_view text):input_(std::string(text)){}
  template<class... Args> bool row(std::string_view tag,Args&&... args) {
    std::string line,actual,extra;
    if(!std::getline(input_,line)||line.size()>1024)return false;
    std::istringstream in(line);
    if(!(in>>actual)||actual!=tag||!((in>>std::forward<Args>(args))&&...))return false;
    return !(in>>extra);
  }
  bool end(){return input_.peek()==std::char_traits<char>::eof();}
};
inline bool parse_definition(std::string_view raw,Definition& output,std::string& error) {
  const auto fail=[&](const char* why){error=why;return false;};
  if(raw.size()>kMaxDefinitionBytes)return fail("experiment definition exceeds capacity");
  std::string text;if(!normalize_newlines(raw,text))return fail("invalid experiment newline or NUL");
  Lines in(text);Definition d{};unsigned kind=0,count=0,automatic_apply=0,automatic_revert=0;
  if(!in.row("CEVEXPERIMENT/1")||!in.row("name",d.name)||
     !in.row("engine",d.engine_id,kind,d.model_sha256,std::quoted(d.behavior_id))||kind<1||kind>2||
     !in.row("identity",d.metric_schema,d.clock_domain,d.tick_ns,d.seed,d.input_schedule_sha256)||
     !in.row("optimization",d.optimization.horizon,d.optimization.minimum_score,count)||!count||count>4)
    return fail("invalid experiment identity or objective count");
  d.kind=static_cast<engine::Kind>(kind);d.optimization.goal_count=static_cast<std::uint8_t>(count);
  for(std::size_t i=0;i<count;++i){unsigned metric=0,guard=0;auto& o=d.optimization.objectives[i];
    if(!in.row("objective",metric,o.ideal,o.importance,guard)||metric<1||metric>4||guard>1)return fail("invalid experiment objective");
    o.metric=static_cast<engine::ObjectiveMetric>(metric);o.forbid_regression=guard!=0;
  }
  if(!in.row("outcome",d.outcome_ticks,d.outcome_tolerance,automatic_apply,automatic_revert)||automatic_apply>1||automatic_revert>1||
     !in.row("stages",count)||!count||count>kMaxStages)return fail("invalid experiment outcome or stages");
  d.automatic_apply=automatic_apply!=0;d.automatic_revert=automatic_revert!=0;d.stage_count=static_cast<std::uint8_t>(count);
  for(std::size_t i=0;i<count;++i){auto& s=d.stages[i];unsigned lesson=0;
    if(!in.row("stage",s.id,s.ticks,s.inputs.support_proxy,s.inputs.stress_proxy,s.inputs.digital_signal,lesson)||lesson>1)
      return fail("invalid experiment stage");
    s.inputs.lesson=lesson!=0;
  }
  if(!in.row("candidates",count)||!count||count>kMaxCandidates)return fail("invalid experiment candidate count");
  d.candidate_count=static_cast<std::uint8_t>(count);
  for(std::size_t i=0;i<count;++i){auto& c=d.candidates[i];auto& r=c.ranking;unsigned policy=0,channel=0;
    if(!in.row("candidate",r.id,r.policy_token,r.cost,r.risk,r.uncertainty,policy,c.from,c.to,c.value,channel)||
       policy<1||policy>3||channel>3)return fail("invalid experiment candidate");
    c.kind=static_cast<PolicyKind>(policy);c.channel=static_cast<std::uint8_t>(channel);
  }
  if(!in.row("artifacts",count)||count>kMaxArtifacts)return fail("invalid experiment artifact count");
  d.artifact_count=static_cast<std::uint8_t>(count);
  for(std::size_t i=0;i<count;++i){auto& a=d.artifacts[i];
    if(!in.row("artifact",a.id,a.role,a.relative_path,a.sha256))return fail("invalid experiment artifact");}
  if(!in.row("learning",std::quoted(d.learning_plan_path),std::quoted(d.learning_plan_sha256),
      std::quoted(d.learning_base_path),std::quoted(d.learning_base_sha256))||!in.row("seal",d.sha256)||!in.end()||
      !accepts_definition(d)||serialize_definition(d)!=text)return fail("invalid or noncanonical experiment seal");
  output=std::move(d);error.clear();return true;
}
struct PinnedArtifact {std::string bytes,sha256;};
inline bool verify_artifacts(const Definition& d,const fs::path& root,std::vector<PinnedArtifact>& pinned,std::string& error) {
  std::vector<PinnedArtifact> next;
  const auto pin=[&](std::string_view relative,std::string_view expected,std::string_view header,std::string& bytes)->bool {
    fs::path path;
    if(!pl::rooted_file(root,relative,path)||!pl::read_bounded(path,pl::kMaxArtifactBytes,bytes)||
       pl::sha256_hex(bytes)!=expected||(!header.empty()&&!std::string_view(bytes).starts_with(header)))return false;
    next.push_back({bytes,std::string(expected)});return true;
  };
  for(std::size_t i=0;i<d.artifact_count;++i){const auto& a=d.artifacts[i];std::string bytes;
    if(!pin(a.relative_path,a.sha256,pl::role_header(a.role),bytes)){error="experiment artifact path, role or hash mismatch";return false;}}
  if(!d.learning_plan_path.empty()){
    std::string plan_bytes,base_bytes;
    if(!pin(d.learning_plan_path,d.learning_plan_sha256,{},plan_bytes)||
       !pin(d.learning_base_path,d.learning_base_sha256,{},base_bytes)){
      error="learning provenance path or hash mismatch";return false;
    }
    pl::Plan plan;christeye::living::process::ProcessMachine base;pl::Session check;
    if(pl::Plan::parse(plan_bytes,plan).error!=pl::Error::ok||
       christeye::living::process::ProcessMachine::deserialize(base_bytes,base).error!=christeye::living::process::Error::ok||
       check.initialize(plan,base,root)!=pl::Error::ok){
      error="CEVPLAN provenance validation failed";return false;
    }
    for(const auto& a:plan.artifacts()){
      fs::path path;std::string bytes;
      if(!pl::rooted_file(root,a.relative_path,path)||!pl::read_bounded(path,pl::kMaxArtifactBytes,bytes)||
         pl::sha256_hex(bytes)!=a.sha256){error="learning artifact changed during registration";return false;}
      next.push_back({std::move(bytes),a.sha256});
    }
  }
  pinned=std::move(next);error.clear();return true;
}
inline bool load_definition(const fs::path& path,Definition& output,std::string& error) {
  std::string text;Definition d{};std::vector<PinnedArtifact> artifacts;
  if(!pl::read_bounded(path,kMaxDefinitionBytes,text)){error="cannot read experiment definition";return false;}
  if(!parse_definition(text,d,error)||!verify_artifacts(d,path.parent_path(),artifacts,error))return false;
  output=std::move(d);return true;
}
} // namespace anaiero::experiments::files
