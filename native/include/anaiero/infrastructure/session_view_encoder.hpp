#pragma once
#include "anaiero/contracts/session_view.hpp"
#include "anaiero/contracts/operation.hpp"
#include <sstream>
#include <string_view>
namespace anaiero::session {
inline std::string json_string(std::string_view value){
  std::string out="\"";
  constexpr char hex[]="0123456789abcdef";
  for(const unsigned char c:value){
    if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
    else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
    else out+=static_cast<char>(c);
  }
  out+='"';return out;
}
inline const char* engine_name(engine::Kind kind)noexcept{
  switch(kind){case engine::Kind::sw5:return "sw5";case engine::Kind::sim3:return "sim3";
    case engine::Kind::scientific:return "scientific";}return "unknown";
}
inline const char* operation_state(operations::State value)noexcept{
  switch(value){case operations::State::accepted:return "accepted";case operations::State::running:return "running";
    case operations::State::completed:return "completed";case operations::State::failed:return "failed";
    case operations::State::cancelled:return "cancelled";}return "unknown";
}
inline std::string encode_operation(const operations::Entry& entry){
  std::ostringstream out;
  out<<"{\"id\":"<<entry.id<<",\"state\":\""<<operation_state(entry.state)
    <<"\",\"result\":"<<static_cast<unsigned>(entry.result)<<",\"session_revision\":"<<entry.session_revision
    <<",\"evidence_revision\":"<<entry.evidence_revision<<",\"detail\":"<<json_string(entry.detail)<<"}\n";
  return out.str();
}
inline const char* summary_name(engine::SummarySemantic value)noexcept{
  switch(value){
    case engine::SummarySemantic::unsupported:return "unsupported";
    case engine::SummarySemantic::live_cells:return "live_cells";
    case engine::SummarySemantic::active_cell_links:return "active_cell_links";
    case engine::SummarySemantic::configured_colony_links:return "configured_colony_links";
    case engine::SummarySemantic::model_energy_proxy:return "model_energy_proxy";
    case engine::SummarySemantic::learning_proxy:return "learning_proxy";
    case engine::SummarySemantic::normalized_activity:return "normalized_activity";
    case engine::SummarySemantic::membrane_voltage_microvolt:return "membrane_voltage_microvolt";
    case engine::SummarySemantic::model_entities:return "model_entities";
  }return "unsupported";
}
inline std::string encode_experiment(const ExperimentView& view){
  std::ostringstream out;out<<"{\"schema\":\"anaiero.experiment/1\",\"id\":"<<json_string(view.id)
    <<",\"definition_sha256\":"<<json_string(view.definition_sha256)<<",\"source_sha256\":"<<json_string(view.source_sha256)
    <<",\"model_sha256\":"<<json_string(view.model_sha256)<<",\"behavior\":"<<json_string(view.behavior)
    <<",\"engine\":"<<view.engine<<",\"source\":"<<view.source<<",\"epoch\":"<<view.epoch
    <<",\"start_tick\":"<<view.start_tick<<",\"seed\":"<<view.seed<<",\"state\":"<<static_cast<unsigned>(view.state)
    <<",\"failure\":"<<static_cast<unsigned>(view.failure)<<",\"completed\":"<<view.completed<<",\"trials\":[";
  for(std::size_t i=0;i<view.count;++i){
    const auto& t=view.trials[i];if(i)out<<',';
    out<<"{\"candidate\":"<<t.candidate.id<<",\"state\":"<<static_cast<unsigned>(t.state)
      <<",\"status\":"<<static_cast<unsigned>(t.status)<<",\"receipt_sha256\":"<<json_string(t.receipt_sha256);
    const std::array<const engine::Snapshot*,2> snapshots{{&t.forecast.baseline,&t.forecast.trial}};
    for(std::size_t j=0;j<snapshots.size();++j){const auto& s=*snapshots[j];
      out<<",\""<<(j?"trial":"baseline")<<"\":{\"tick\":"<<s.tick<<",\"participants\":"<<s.participants
        <<",\"connections\":"<<s.connections<<",\"energy_proxy\":"<<s.energy<<",\"learning_proxy\":"<<s.learning_proxy
        <<",\"state_sha256\":"<<json_string(s.state_sha256.data())<<'}';
    }
    out<<'}';
  }
  out<<"],\"policy_paused\":"<<(view.policy_paused?"true":"false")<<",\"selected\":"<<view.selection.candidate.id<<",\"approved\":"<<(view.selection.approved?"true":"false")
    <<",\"score\":"<<view.selection.score<<",\"change_id\":"<<view.outcome.change_id
    <<",\"outcome_decision\":"<<static_cast<unsigned>(view.outcome.decision)<<",\"observed_tick\":"<<view.outcome.observed.tick
    <<",\"observed_sha256\":"<<json_string(view.outcome.observed.state_sha256.data())<<",\"residuals\":[";
  for(std::size_t i=0;i<view.outcome.residuals.size();++i){if(i)out<<',';out<<view.outcome.residuals[i];}
  out<<"]}\n";return out.str();
}
class JsonSessionViewEncoder final:public IViewEncoder {
public:
  std::string encode(const View& view)const override {
    std::ostringstream out;
    out<<"{\"schema\":\"anaiero.session/2\",\"name\":"<<json_string(view.name)
      <<",\"revision\":"<<view.revision<<",\"cycles\":"<<view.cycles<<",\"target_cycles\":"<<view.target_cycles
      <<",\"operation_id\":"<<view.operation_id<<",\"busy\":"<<(view.busy?"true":"false")
      <<",\"error\":"<<json_string(view.error)<<",\"pending_evidence\":"<<view.pending_evidence
      <<",\"state_revision\":"<<view.projected.revision<<",\"sequence_gaps\":"<<view.projected.sequence_gaps<<",\"engines\":[";
    for(std::size_t i=0;i<view.engine_count;++i){
      const auto& engine=view.engines[i];const auto& s=engine.working;const auto& c=engine.capabilities;
      if(i)out<<',';
      out<<"{\"id\":"<<s.id<<",\"kind\":\""<<engine_name(engine.kind)<<"\",\"source\":"<<engine.source
        <<",\"epoch\":"<<s.source_epoch<<",\"cadence\":"<<static_cast<unsigned>(engine.cadence)
        <<",\"tick\":"<<s.tick<<",\"revision\":"<<s.revision<<",\"sha256\":"<<json_string(s.state_sha256.data())
        <<",\"coordinator_revision\":"<<engine.coordinator_revision<<",\"published_revision\":"<<engine.published.revision
        <<",\"published_tick\":"<<engine.published.tick<<",\"publication_pending\":"<<(engine.publication_pending?"true":"false")
        <<",\"route_active\":"<<(engine.route_active?"true":"false")<<",\"metric_schema\":"<<c.metric_schema
        <<",\"clock_domain\":"<<c.clock_domain<<",\"tick_ns\":"<<c.tick_ns<<",\"time_kind\":\""<<(c.tick_ns?"simulated":"logical")
        <<"\",\"behavior\":"<<json_string(c.behavior_id)<<",\"summary_semantics\":[";
      for(std::size_t j=0;j<c.summaries.size();++j){if(j)out<<',';out<<json_string(summary_name(c.summaries[j]));}
      out<<"]}";
    }
    out<<"],\"proposal\":{\"engine\":"<<view.proposal_engine<<",\"approved\":"<<(view.proposal_approved?"true":"false")
      <<",\"candidate\":"<<view.proposal_candidate<<",\"score\":"<<view.proposal_score<<",\"commit_id\":"<<view.proposal_commit
      <<",\"source_sha256\":"<<json_string(view.proposal_source)<<"},\"metrics\":[";
    for(std::size_t i=0;i<view.projected.count;++i){
      const auto& m=view.projected.values[i];if(i)out<<',';
      out<<"{\"engine\":"<<m.colony<<",\"metric\":"<<static_cast<unsigned>(m.metric)<<",\"value\":"<<m.value
        <<",\"source\":"<<m.source<<",\"epoch\":"<<m.source_epoch<<",\"sequence\":"<<m.sequence
        <<",\"quality\":"<<static_cast<unsigned>(m.quality)<<",\"acquired\":"<<m.acquired_ns
        <<",\"evidence_tag\":"<<m.evidence_tag<<",\"unit\":\""<<(m.metric==runtime::Metric::membrane_voltage?"microvolt":
          (m.metric==runtime::Metric::viability||m.metric==runtime::Metric::connection?"count":
          (m.metric==runtime::Metric::digital_route_active?"boolean":"model_proxy")))<<"\",\"scope\":\"engine_summary\"}";
    }
    out<<"],\"experiment\":{\"id\":"<<json_string(view.experiment_id)<<",\"state\":"<<json_string(view.experiment_state)
      <<",\"completed_trials\":"<<view.completed_trials<<",\"total_trials\":"<<view.total_trials<<"},\"storage\":{\"sustained\":"
      <<(view.storage.sustained?"true":"false")<<",\"metadata_events\":"<<view.storage.metadata_events
      <<",\"evidence_events\":"<<view.storage.evidence_events<<",\"metadata_resident\":"<<view.storage.metadata_resident
      <<",\"evidence_resident\":"<<view.storage.evidence_resident<<",\"segment_events\":"<<view.storage.segment_events
      <<",\"archived_segments\":"<<view.storage.archived_segments<<",\"available_bytes\":"<<view.storage.available_bytes<<"}}\n";
    return out.str();
  }
};
} // namespace anaiero::session
