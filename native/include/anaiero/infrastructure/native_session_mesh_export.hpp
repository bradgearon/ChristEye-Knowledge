#pragma once
// Offline bridge. Both source stores are locked before reading, and session
// identity is checked without initializing a simulator or launching a solver.
#include "anaiero/infrastructure/session_files.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include "christeye/living/durable_registry.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <string>
namespace anaiero::mesh {
template<class Sink> bool export_native_session(const std::filesystem::path& configuration,
  const std::filesystem::path& root,Sink&& sink,std::string& error,std::uint64_t& count){
  namespace db=christeye::living::durable;
  session::Configuration config;count=0;
  if(!session::files::read_configuration(configuration,config,error))return false;
  if(!std::filesystem::exists(root/"session")||!std::filesystem::exists(root/"evidence")){
    error="native session does not exist";return false;
  }
  db::Registry metadata,evidence;
  if(metadata.open(root/"session")!=db::Code::ok||evidence.open(root/"evidence")!=db::Code::ok){
    error="stop the native session host before exporting its evidence";return false;
  }
  db::Event identity;bool found=false;std::string bytes;
  if(!metadata.latest_event("CULTURE_CHECKPOINT","native-session-root","identity",identity,found,"anaiero-session-host")||
    !found||!metadata.get(identity.object_hash,bytes)||bytes!="CEVSESSIONIDENTITY/1\n"+config.name+'\n'+config.digest+'\n'){
    error="native session configuration does not match its durable identity";return false;
  }
  session::files::WindStoreRecordStore records(metadata,"session-"+config.name);
  session::Record manifest;bool has_manifest=false;
  if(!records.load(manifest,has_manifest)||!has_manifest||manifest.configuration_sha256!=config.digest||
    manifest.coordinator.engine_count!=config.count||(config.sustained_storage&&manifest.schema<2)){
    error="native session manifest does not match its configuration";return false;
  }
  std::array<std::uint64_t,engine::kMaxEngines> upper{};
  for(std::size_t i=0;i<config.count;++i){const auto& expected=config.engines[i];const auto& saved=manifest.coordinator.engines[i];
    if(saved.id!=expected.id||saved.source!=expected.source||saved.source_epoch!=expected.epoch||
      saved.kind!=expected.kind||saved.cadence!=expected.cadence||
      (config.sustained_storage&&!manifest.contracts[i].clock_domain)){
      error="native engine source or clock contract is inconsistent";return false;
    }
    upper[i]=manifest.evidence[i].next_sequence-1;
    if(manifest.publication.active&&manifest.publication.engine_index==i){
      const std::size_t metrics=expected.kind==engine::Kind::scientific?1u:5u;
      if(manifest.publication.metric_index>metrics){error="invalid native publication progress";return false;}
      // The journal may commit the current in-flight observation before its
      // completion advances the manifest. Preserve that recoverable prefix.
      // Previously acknowledged metrics and at most this one pending metric
      // are eligible; future observations are outside the durable intent.
      const auto pending=std::min(metrics,manifest.publication.metric_index+1);
      if(upper[i]>UINT64_MAX-pending){error="native publication sequence overflow";return false;}
      upper[i]+=pending;
    }
  }
  runtime::EpochObservationCodec codec;
  const bool result=evidence.visit_events(0,[&](const db::Event& event){
    if(event.kind!="OBSERVATION_RECORDED"||event.entity!=config.name)return true;
    std::string wire;runtime::Observation observation;
    if(!evidence.get(event.object_hash,wire)||wire.size()!=runtime::kEpochObservationFrame||
      !codec.decode(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wire.data()),wire.size()),observation)||
      event.source_tick!=observation.sequence||event.clock!="epoch-"+std::to_string(observation.source_epoch)||
      event.producer!="source-"+std::to_string(observation.source)||event.origin!="SIMULATED"||
      observation.origin!=runtime::Origin::simulated)return false;
    std::size_t index=0;
    for(;index<config.count;++index)if(config.engines[index].source==observation.source)break;
    if(index==config.count||observation.source_epoch!=config.engines[index].epoch||observation.colony!=config.engines[index].id||
      observation.clock_domain!=(config.sustained_storage?manifest.contracts[index].clock_domain:1u)||
      observation.sequence>upper[index]||!sink(observation))return false;
    ++count;return true;
  });
  if(!result)error="native evidence import stopped at its last durable observation; retry is idempotent";
  return result;
}
} // namespace anaiero::mesh
