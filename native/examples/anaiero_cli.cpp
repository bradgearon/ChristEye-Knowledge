// Anaiero: the public C++20 CLI dispatcher for existing native research tools.
// The ce_* executables are native backends, launched directly without a shell.
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace anaiero {
namespace fs=std::filesystem;
struct Module {std::string_view name, backend, purpose, example;};
constexpr std::array<Module,
#ifdef ANAIERO_HAS_MESH
18
#else
17
#endif
> modules{{
#ifdef ANAIERO_HAS_MESH
  {"mesh","ce_mesh","Authenticated evidence replication and scoped artifacts","anaiero mesh help"},
#endif
  {"session","ce_modular_session","Registered engines, durable shared plans and evidence lifecycle","anaiero session help"},
  {"vm","ce_vlm","Virtual Living Machines","anaiero vm inspect examples/vlm/crest-garden.avlm"},
  {"process","ce_vlm_process","Timed VLM2 processes","anaiero process inspect examples/vlm/process-garden.avlm"},
  {"plan","ce_vlm_plan","Crystal/RNA/learning plans","anaiero plan inspect examples/vlm/bread-first.cevplan examples/vlm/plan-garden.avlm ."},
  {"registry","ce_wind_registry","Legacy research registry","anaiero registry verify ./store"},
  {"store","ce_wind_durable","Durable WindStore and cursors","anaiero store verify ./durable-store"},
  {"colony","ce_colony_runner","Persistent multi-colony simulator","anaiero colony step examples/connected-gardens.cev ./culture-state 5"},
  {"lab","ce_colony_lab","Unified simulation laboratory","anaiero lab export examples/connected-gardens-lab.cev ./lab-state replay.ndjson"},
  {"fidelity","ce_colony_fidelity","Frozen model fidelity evaluation","anaiero fidelity evaluate examples/connected-gardens.cev ./culture-state examples/sim3/baseline.ceveval . fidelity.json"},
  {"select","ce_colony_select","Training-only model selection","anaiero select prepare examples/sim3/model-search.cevselect . ./selection-runs"},
  {"runtime","ce_colony_runtime","Latest ColonyState and weighted Animon status dispatch","anaiero runtime simulate examples/connected-gardens.cev ./state 5"},
  {"cells","ce_cell_behavior","Behavior atlas and synthetic cell development","anaiero cells execute examples/cells/crest-blood-neural.cevatlas ./digital-culture.cevcells examples/cells/reference-development.cevcellrun"},
  {"optimize","ce_optimization","Digital route analysis, trial scoring and feedback","anaiero optimize demo"},
  {"science","ce_science_lab","Coupled cell and scientific-model simulations","anaiero science compare examples/cells/crest-blood-neural.cevatlas"},
  {"calibrate","ce_scientific_calibrate","Training-only environmental model selection and held-out fidelity","anaiero calibrate examples/scientific/calibration/reference.cevcalibration ./qualification.json"},
  {"kernel","ce_simulation_kernel","Reusable SW5/SIM3 simulation engine orchestration","anaiero kernel demo examples/cells/crest-blood-neural.cevatlas examples/connected-gardens.cev ./kernel-session"},
  {"sim","ce_living_sim_demo","SIM1 synthetic demonstration","anaiero sim"}
}};
const Module* lookup(std::string_view name) noexcept {
  for(const auto& m:modules) if(m.name==name)return &m;
  return nullptr;
}
#ifdef _WIN32
std::wstring to_wide(std::string_view input) {
  if(input.empty())return {};
  const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),nullptr,0);
  if(n<=0)return {};
  std::wstring out(static_cast<std::size_t>(n),L'\0');
  if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),out.data(),n))return {};
  return out;
}
std::string to_utf8(std::wstring_view input) {
  if(input.empty())return {};
  const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,nullptr);
  if(n<=0)return {};
  std::string out(static_cast<std::size_t>(n),'\0');
  if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),out.data(),n,nullptr,nullptr))return {};
  return out;
}
std::wstring quote(std::wstring_view v) {
  std::wstring s=L"\"";std::size_t slashes=0;
  for(const auto ch:v){
    if(ch==L'\\'){++slashes;continue;}
    if(ch==L'"'){s.append(slashes*2+1,L'\\');s+=L'"';}
    else{s.append(slashes,L'\\');s+=ch;}
    slashes=0;
  }
  s.append(slashes*2,L'\\');return s+L'"';
}
fs::path self_folder(const char*) {
  std::wstring b(32768,L'\0');
  const auto n=GetModuleFileNameW(nullptr,b.data(),static_cast<DWORD>(b.size()));
  if(!n||n>=b.size())return {};
  b.resize(n);return fs::path(b).parent_path();
}
#else
fs::path self_folder(const char* argv0) {
#ifdef __linux__
  std::array<char,8192> b{};
  const auto n=::readlink("/proc/self/exe",b.data(),b.size()-1);
  if(n>0)return fs::path(std::string(b.data(),static_cast<std::size_t>(n))).parent_path();
#endif
  if(std::string_view(argv0).find('/')!=std::string_view::npos)
    return fs::absolute(argv0).parent_path();
  return {};
}
#endif
fs::path binary(const fs::path& folder,const Module& m) {
#ifdef _WIN32
  return folder/(std::string(m.backend)+".exe");
#else
  return folder/std::string(m.backend);
#endif
}
bool installed(const fs::path& folder,const Module& m) {
  std::error_code ec;return fs::is_regular_file(binary(folder,m),ec)&&!ec;
}
int dispatch(const fs::path& application,int argc,char**argv) {
#ifdef _WIN32
  const auto path=application.wstring();
  std::wstring command=quote(path);
  for(int i=2;i<argc;++i){
    const auto arg=to_wide(argv[i]);
    if(arg.empty()&&argv[i][0]!='\0')return 65;
    command+=L' ';command+=quote(arg);
  }
  std::vector<wchar_t> mutable_command(command.begin(),command.end());
  mutable_command.push_back(L'\0');
  STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
  if(!CreateProcessW(path.c_str(),mutable_command.data(),nullptr,nullptr,TRUE,0,nullptr,nullptr,&startup,&process)) {
    std::cerr<<"anaiero: backend launch failed\n";return 69;
  }
  CloseHandle(process.hThread);
  DWORD result=69;
  if(WaitForSingleObject(process.hProcess,INFINITE)!=WAIT_OBJECT_0||
     !GetExitCodeProcess(process.hProcess,&result))result=69;
  CloseHandle(process.hProcess);return static_cast<int>(result);
#else
  const std::string path=application.string();
  std::vector<char*> arguments;
  arguments.reserve(static_cast<std::size_t>(argc));
  arguments.push_back(const_cast<char*>(path.c_str()));
  for(int i=2;i<argc;++i)arguments.push_back(argv[i]);
  arguments.push_back(nullptr);
  const pid_t child=::fork();
  if(child<0)return 69;
  if(child==0){
    ::execv(path.c_str(),arguments.data());
    std::cerr<<"anaiero: exec failed: "<<std::strerror(errno)<<'\n';
    ::_exit(127);
  }
  int code=0;pid_t waited=0;
  do{waited=::waitpid(child,&code,0);}while(waited<0&&errno==EINTR);
  if(waited<0)return 69;
  if(WIFEXITED(code))return WEXITSTATUS(code);
  if(WIFSIGNALED(code))return 128+WTERMSIG(code);
  return 69;
#endif
}
int run(int argc,char**argv) {
  const auto folder=self_folder(argv[0]);
  const auto cmd=argc>1?std::string_view(argv[1]):std::string_view{};
  if(cmd.empty()||cmd=="help"||cmd=="--help"){
    if(argc>2){
      if(const auto* m=lookup(argv[2])){
        std::cout<<"Module: "<<m->name<<"\nPurpose: "<<m->purpose<<"\nExample: "<<m->example<<'\n';return 0;
      }
      return 64;
    }
    std::cout<<"Anaiero native C++20 toolkit\nUsage: anaiero <module> [args...]\n"
             <<"       anaiero help [module]\n       anaiero modules [--json]\n"
             <<"       anaiero doctor\n       anaiero version\n";
    for(const auto& m:modules)std::cout<<"  "<<m.name<<" — "<<m.purpose<<'\n';
    return 0;
  }
  if(cmd=="version"||cmd=="--version"){
    std::cout<<"anaiero 0.1.0\n";return 0;
  }
  if(cmd=="modules") {
    if(argc>3||(argc==3&&std::string_view(argv[2])!="--json"))return 64;
    if(argc==2){for(const auto& m:modules)std::cout<<m.name<<'\n';return 0;}
    std::cout<<"{\"schema\":\"anaiero.modules/1\",\"modules\":[";
    bool first=true;
    for(const auto& m:modules){
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"name\":\""<<m.name<<"\",\"backend\":\""<<m.backend<<"\",\"available\":"
               <<(installed(folder,m)?"true":"false")<<'}';
    }
    std::cout<<"]}\n";return 0;
  }
  if(cmd=="doctor"){
    if(argc!=2)return 64;
    int missing=0;
    for(const auto& m:modules){
      const bool ready=installed(folder,m);
      std::cout<<m.name<<": "<<(ready?"ready":"missing")<<'\n';
      if(!ready)++missing;
    }
    return missing?69:0;
  }
  const auto* module=lookup(cmd);
  if(!module){std::cerr<<"anaiero: unknown module "<<cmd<<'\n';return 64;}
  if(folder.empty()||!installed(folder,*module)){
    std::cerr<<"anaiero: module not installed; run 'anaiero doctor'\n";return 69;
  }
  return dispatch(binary(folder,*module),argc,argv);
}
} // namespace anaiero
#ifdef _WIN32
int wmain(int argc,wchar_t**argv){
  std::vector<std::string> strings;
  for(int i=0;i<argc;++i){
    strings.push_back(anaiero::to_utf8(argv[i]));
    if(argv[i][0]!=L'\0'&&strings.back().empty())return 65;
  }
  std::vector<char*> args;
  for(auto& s:strings)args.push_back(s.data());
  return anaiero::run(argc,args.data());
}
#else
int main(int argc,char**argv){return anaiero::run(argc,argv);}
#endif
