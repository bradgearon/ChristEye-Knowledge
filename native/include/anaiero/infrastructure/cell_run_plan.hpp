#pragma once
// CEVCELLRUN/1: bounded, digital-only reproducible developmental timeline.
// No physical feeding, differentiation, media or electrical stimulation.
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
namespace anaiero::cells::files {
struct StagePlan {
  std::string name;
  std::uint32_t ticks=0;
  Context context{};
};
struct RunPlan {
  std::string name, sha256;
  std::array<StagePlan,16> stages{};
  std::size_t count=0;
  std::uint32_t total_ticks=0;
};
inline Status parse_run_plan(std::string_view data,const Atlas& atlas,RunPlan& output){
  if(data.empty()||data.size()>16384||!data.ends_with('\n'))return Status::bad_profile;
  const auto seal=data.rfind("seal ");
  if(seal==data.npos||seal==0||data[seal-1]!='\n'||data.size()-seal!=70||
     christeye::living::plans::sha256_hex(data.substr(0,seal))!=data.substr(seal+5,64))
    return Status::bad_profile;
  RunPlan plan;plan.sha256=christeye::living::plans::sha256_hex(data);
  std::istringstream input{std::string(data.substr(0,seal))};std::string line;
  auto next=[&](){return bool(std::getline(input,line))&&!line.empty()&&line.back()!='\r';};
  if(!next()||line!="CEVCELLRUN/1")return Status::bad_profile;
  if(!next()||!line.starts_with("name ")||!ident(line.substr(5)))return Status::bad_profile;
  plan.name=line.substr(5);
  if(!next()||line!="domain SYNTHETIC_ONLY")return Status::bad_profile;
  if(!next()||line!="atlas_sha256 "+atlas.model_digest)return Status::bad_profile;
  while(next()){
    if(line=="end")break;
    std::istringstream entry(line);std::vector<std::string> tokens;std::string token;
    while(entry>>token){if(tokens.size()>7)return Status::bad_profile;tokens.push_back(token);}
    if(tokens.size()!=7||tokens[0]!="stage"||!ident(tokens[1])||plan.count==16)
      return Status::bad_profile;
    StagePlan stage;stage.name=tokens[1];
    std::uint32_t tick_count=0,lesson=0;
    if(!u32(tokens[2],tick_count)||tick_count==0||tick_count>512||
       plan.total_ticks>4096-tick_count||!i32(tokens[3],stage.context.support_proxy)||
       !i32(tokens[4],stage.context.stress_proxy)||
       !i32(tokens[5],stage.context.digital_signal)||!u32(tokens[6],lesson)||lesson>1||
       stage.context.support_proxy<0||stage.context.support_proxy>1000||
       stage.context.stress_proxy<0||stage.context.stress_proxy>1000||
       stage.context.digital_signal<0||stage.context.digital_signal>1000)
      return Status::bad_profile;
    for(std::size_t i=0;i<plan.count;++i)
      if(plan.stages[i].name==stage.name)return Status::duplicate;
    stage.ticks=tick_count;stage.context.lesson_active=lesson==1;
    plan.stages[plan.count++]=stage;plan.total_ticks+=tick_count;
  }
  if(line!="end"||plan.count==0||next())return Status::bad_profile;
  output=std::move(plan);return Status::ok;
}
inline Status load_run_plan(const std::filesystem::path& path,const Atlas& atlas,RunPlan& out){
  std::string bytes;
  if(!read_bounded(path,16384,bytes))return Status::io_failure;
  return parse_run_plan(bytes,atlas,out);
}
inline const StagePlan* stage_for_tick(const RunPlan& plan,std::uint64_t tick) noexcept {
  std::uint64_t cursor=0;
  for(std::size_t i=0;i<plan.count;++i){
    cursor+=plan.stages[i].ticks;
    if(tick<cursor)return &plan.stages[i];
  }
  return nullptr;
}
} // namespace anaiero::cells::files
