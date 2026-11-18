#pragma once
#include "anaiero/contracts/supervised_solver.hpp"
#include "anaiero/contracts/scientific_observable.hpp"
#include "christeye/living/durable_records.hpp"
#include <filesystem>
#include <iomanip>
#include <sstream>
namespace anaiero::science::files {
inline bool read_solver_configuration(const std::filesystem::path& path,ScientificConfiguration& output,std::string& error){
  namespace db=christeye::living::durable;namespace fs=std::filesystem;
  auto fail=[&](std::string_view message){error=message;return false;};
  std::string raw;if(!db::read_bounded(path,65536,raw))return fail("cannot read solver configuration");
  std::istringstream input(raw);std::string header,tag,model_path,model_pin,line,extra;
  ScientificConfiguration next{};std::uint32_t unit=0;
  if(!std::getline(input,header))return fail("missing solver configuration header");
  if(!header.empty()&&header.back()=='\r')header.pop_back();
  if(header!="CEVSOLVER/1"||
     !(input>>tag>>next.expected.provider)||tag!="provider"||
     !(input>>tag>>std::quoted(model_path)>>model_pin)||tag!="model"||!db::valid_digest(model_pin)||
     !(input>>tag>>next.expected.behavior_sha256)||tag!="behavior"||!db::valid_digest(next.expected.behavior_sha256)||
     !(input>>tag>>std::quoted(next.process.executable))||tag!="executable"||
     !(input>>tag>>next.expected.step_ns)||tag!="step_ns"||
     !(input>>tag>>next.expected.max_steps)||tag!="max_steps"||
     !(input>>tag>>next.timeout_ms)||tag!="timeout_ms"||
     !(input>>tag>>next.input_proxy)||tag!="input_proxy"||
     !(input>>tag>>unit)||tag!="unit"||!observable_for_unit(unit)||
     !(input>>tag>>next.expected.checkpoint_schema)||tag!="checkpoint_schema"||next.expected.checkpoint_schema>1)
    return fail("invalid solver configuration");
  if(!next.expected.step_ns||next.expected.step_ns>1000000000ULL||!next.expected.max_steps||next.expected.max_steps>1000000||
     !next.timeout_ms||next.timeout_ms>60000||next.input_proxy<0||next.input_proxy>1000||
     next.expected.provider.empty()||next.expected.provider.size()>64)return fail("solver bounds exceeded");
  next.expected.unit=static_cast<Unit>(unit);next.expected.model_sha256=model_pin;
  std::string identity=raw;
  auto pin=[&](std::string& name,std::string_view expected,std::size_t limit){
    if(name.empty()||name.size()>4096)return false;
    std::error_code ec;const auto absolute=fs::absolute(path.parent_path()/name,ec).lexically_normal();
    if(ec)return false;
    const auto resolved=fs::canonical(absolute,ec);
    if(ec)return false;
    std::string bytes;if(!db::read_bounded(resolved,limit,bytes))return false;
    const auto digest=christeye::living::plans::sha256_hex(bytes);
    if(!expected.empty()&&digest!=expected)return false;
    // Keep the launch path: virtual environments and some wrappers deliberately
    // use argv[0]/the executable location to select their runtime. Hash the
    // resolved file bytes without changing that execution contract.
    identity+='\n';identity+=digest;name=absolute.string();return true;
  };
  if(!pin(model_path,model_pin,1024*1024)||!pin(next.process.executable,{},64*1024*1024))return fail("solver model/executable unavailable or digest mismatch");
  unsigned count=0;if(!(input>>tag>>count)||tag!="arguments"||count>32)return fail("invalid solver argument count");
  for(unsigned i=0;i<count;++i){
    std::string argument;if(!(input>>tag))return fail("missing solver argument");
    if(tag=="model_argument")argument=model_path;
    else if(tag=="argument"){if(!(input>>std::quoted(argument))||argument.size()>4096)return fail("invalid literal argument");}
    else if(tag=="file_argument"){
      std::string hash;if(!(input>>std::quoted(argument)>>hash)||!db::valid_digest(hash)||!pin(argument,hash,1024*1024))return fail("invalid pinned file argument");
    }else return fail("unsupported solver argument kind");
    if(argument.find('\0')!=argument.npos)return fail("NUL in solver argument");
    next.process.arguments.push_back(std::move(argument));
  }
  if(!(input>>tag)||tag!="end"||input>>extra)return fail("invalid solver configuration ending");
  next.configuration_sha256=christeye::living::plans::sha256_hex(identity);output=std::move(next);error.clear();return true;
}
} // namespace anaiero::science::files
