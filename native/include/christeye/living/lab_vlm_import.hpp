#pragma once
// SIM3.1 read-only source VLM2 projection. No automatic cell-identity merging.
#include "christeye/living/lab_core.hpp"
#include "christeye/living/process_machine.hpp"
#include <set>
#include <sstream>
namespace christeye::living::lab {
namespace vp=christeye::living::process;
namespace v=christeye::living::vlm;
struct OverlayResult {
  Code status=Code::ok;
  std::string source_sha256,projection_json;
  std::uint64_t source_tick=0;
};
inline OverlayResult read_overlay(const sim::Configuration& config,
                                  const std::filesystem::path& config_file,
                                  const sim::ColonyConfig& colony) {
  (void)config;
  std::filesystem::path source;
  if(!sim::bound_path(config_file.parent_path(),colony.source_path,source))
    return {Code::invalid_config,{},{},0};
  std::string bytes;
  if(!sim::bounded_read(source,sim::kMaxSourceBytes,bytes)||
     pl::sha256_hex(bytes)!=colony.source_sha)return {Code::invalid_config,{},{},0};
  vp::ProcessMachine machine;
  if(vp::read_file(source,machine).error!=vp::Error::ok)return {Code::invalid_config,{},{},0};
  std::set<std::uint32_t> cells,molecules;
  const auto collect=[&](const v::Action& action) {
    if(action.is_deposit)return;
    if(action.instruction.op==Op::spawn)cells.insert(action.instruction.subject);
    if(action.instruction.op==Op::define_molecule)molecules.insert(action.instruction.subject);
  };
  for(const auto& a:machine.base().image())collect(a);
  for(const auto& a:machine.base().journal())collect(a);
  for(const auto& e:machine.journal())if(e.direct)collect(e.action);
  for(const auto& t:machine.tasks())if(t.completed)collect(t.action);
  std::ostringstream out;
  out<<"{\"origin\":\"SIMULATED_UNCALIBRATED\",\"format\":\"CEVLMOVERLAY/1\",\"colony\":\""
     <<json_escape(colony.id)<<"\",\"source_sha256\":\""<<colony.source_sha
     <<"\",\"tick\":"<<machine.tick()<<",\"model_state_hash\":\""<<v::hex64(machine.state_hash())
     <<"\",\"cells\":[";
  bool first=true;
  for(auto id:cells){
    const auto* c=machine.base().core().cell(id);
    if(!c)continue;
    if(!first)out<<',';
    first=false;
    out<<"{\"id\":"<<c->id<<",\"parent\":"<<c->parent_id<<",\"kind\":\""<<v::cell_name(c->kind)
       <<"\",\"x\":"<<c->x<<",\"y\":"<<c->y<<",\"z\":0,\"energy_proxy\":"<<c->energy
       <<",\"maturity_proxy\":"<<c->maturity<<",\"neurite_proxy\":"<<c->neurite_proxy
       <<",\"rna_proxy\":"<<c->rna_proxy<<",\"learning_proxy\":"<<c->learning_proxy
       <<",\"viable\":"<<(c->viable?"true":"false")<<'}';
  }
  out<<"],\"molecules\":[";
  first=true;
  for(auto id:molecules){
    const auto* m=machine.base().core().molecule(id);
    if(!m)continue;
    if(!first)out<<',';
    first=false;
    out<<"{\"id\":"<<m->id<<",\"kind\":\""<<v::molecule_name(m->kind)
       <<"\",\"quantity_proxy\":"<<m->quantity<<'}';
  }
  out<<"],\"connections\":[";
  for(std::size_t i=0;i<machine.base().core().connection_count();++i){
    const auto* e=machine.base().core().connection(i);
    if(i)out<<',';
    out<<"{\"from\":"<<e->source<<",\"to\":"<<e->target<<",\"weight_proxy\":"<<e->weight<<'}';
  }
  out<<"],\"fields\":{\"side\":"<<v::kFieldSide<<",\"layers\":[";
  for(std::uint32_t kind=0;kind<v::kFieldTypes;++kind){
    if(kind)out<<',';
    const auto layer=static_cast<v::FieldKind>(kind);
    out<<"{\"name\":\""<<v::field_name(layer)<<"\",\"values\":[";
    for(std::uint32_t y=0;y<v::kFieldSide;++y)
      for(std::uint32_t x=0;x<v::kFieldSide;++x){
        if(x||y)out<<',';
        out<<machine.base().soil().at(layer,x,y);
      }
    out<<"]}";
  }
  out<<"]},\"active_processes\":[";
  first=true;
  for(const auto& t:machine.tasks()){
    if(!first)out<<',';
    first=false;
    out<<"{\"id\":"<<t.id<<",\"elapsed\":"<<t.elapsed<<",\"x\":"<<t.x<<",\"y\":"<<t.y
       <<",\"completed\":"<<(t.completed?"true":"false")<<'}';
  }
  out<<"]}";
  return {Code::ok,colony.source_sha,out.str(),machine.tick()};
}
} // namespace christeye::living::lab
