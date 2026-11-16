// CE-SW-13 host CLI: synthetic replay, CEVLM overlay, WindStore indexing.
#include "christeye/living/lab_vlm_import.hpp"
#include <fstream>
#include <iostream>
#include <string_view>
namespace lab=christeye::living::lab;
namespace sim=christeye::living::sim3;
namespace fs=std::filesystem;
static int fail(std::string_view label,int code){std::cerr<<label<<" error="<<code<<'\n';return 1;}
static int usage(){
  std::cerr<<"ce_colony_lab <stream|export|overlay|index|compare|compare-index> <run.cev> <state-dir> <arg> [registry-dir]\n";
  return 2;
}
int main(int argc,char**argv){
  if(argc!=5&&argc!=6)return usage();
  const std::string_view op(argv[1]);
  sim::Runner runner;
  if(const auto e=runner.open(argv[2],argv[3]);e!=sim::Result::ok)
    return fail("open",static_cast<int>(e));
  if(!runner.audit_disk())return fail("audit",1);
  const auto& cfg=runner.configuration();
  if(op=="stream"&&argc==5){
    std::uint32_t ticks=0;
    if(!sim::parse_u32(argv[4],ticks)||ticks==0||ticks>256)return usage();
    for(std::uint32_t i=0;i<ticks;++i){
      if(auto e=runner.step();e!=sim::Result::ok)return fail("tick",static_cast<int>(e));
      std::cout<<lab::frame(cfg,runner.state())<<'\n';
    }
    return 0;
  }
  if(op=="export"&&argc==5){
    if(fs::exists(argv[4]))return fail("output-exists",2);
    std::ofstream out(argv[4],std::ios::binary|std::ios::trunc);
    if(!out)return fail("output",1);
    for(std::uint64_t tick=0;tick<=runner.state().tick;++tick){
      std::string bytes;
      sim::Snapshot state;
      if(!sim::bounded_read(fs::path(argv[3])/"history"/lab::history_name(tick),sim::kMaxSnapshotBytes,bytes)||
         sim::decode_snapshot(bytes,cfg,state)!=sim::Result::ok||state.tick!=tick)
        return fail("history",1);
      out<<lab::frame(cfg,state)<<'\n';
    }
    out.flush();return out?0:fail("flush",1);
  }
  if(op=="overlay"&&argc==5){
    if(fs::exists(argv[4]))return fail("output-exists",2);
    std::ofstream out(argv[4],std::ios::binary|std::ios::trunc);
    if(!out)return fail("output",1);
    for(const auto& colony:cfg.colonies){
      auto overlay=lab::read_overlay(cfg,argv[2],colony);
      if(overlay.status!=lab::Code::ok)return fail("overlay",static_cast<int>(overlay.status));
      out<<overlay.projection_json<<'\n';
    }
    out.flush();return out?0:fail("flush",1);
  }
  if(op=="index"&&argc==5){
    christeye::living::durable::Registry store;
    if(auto e=store.open(argv[4]);e!=christeye::living::durable::Code::ok)
      return fail("store",static_cast<int>(e));
    const auto indexed=lab::index_history(cfg,argv[3],store,runner.state().tick);
    if(indexed.code!=lab::Code::ok)return fail("history-index",static_cast<int>(indexed.code));
    for(const auto& colony:cfg.colonies){
      auto overlay=lab::read_overlay(cfg,argv[2],colony);
      if(overlay.status!=lab::Code::ok)return fail("overlay",static_cast<int>(overlay.status));
      auto object=store.put(overlay.projection_json);
      if(object.code!=christeye::living::durable::Code::ok)
        return fail("overlay-put",static_cast<int>(object.code));
      auto event=store.publish("VLM_OVERLAY_IMPORTED",colony.id,object.hash,"SIMULATED",
                                overlay.source_tick,"synthetic-tick","sim3-vlm-bridge");
      if(event.code!=christeye::living::durable::Code::ok)
        return fail("overlay-index",static_cast<int>(event.code));
    }
    if(!store.audit_disk())return fail("store-audit",1);
    std::cout<<"{\"indexed\":"<<indexed.indexed<<",\"duplicates\":"<<indexed.duplicates
             <<",\"events\":"<<store.event_count()<<"}\n";
    return 0;
  }
  if((op=="compare"&&argc==5)||(op=="compare-index"&&argc==6)){
    lab::Profile profile;
    if(!lab::Profile::read(argv[4],profile)||profile.source_sha256!=cfg.material_sha)
      return fail("synthetic-profile",1);
    const auto report=lab::compare(profile,cfg,runner.state());
    if(op=="compare-index"){
      christeye::living::durable::Registry store;
      if(auto e=store.open(argv[5]);e!=christeye::living::durable::Code::ok)
        return fail("store",static_cast<int>(e));
      const auto object=store.put(report);
      if(object.code!=christeye::living::durable::Code::ok)return fail("report-put",1);
      const auto event=store.publish("MODEL_EVALUATION",profile.id,object.hash,"SIMULATED",
                                      runner.state().tick,"synthetic-tick","sim3-lab");
      if(event.code!=christeye::living::durable::Code::ok)return fail("report-publish",1);
    }
    std::cout<<report<<'\n';return 0;
  }
  return usage();
}
