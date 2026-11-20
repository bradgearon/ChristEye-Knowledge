#include "anaiero/infrastructure/native_ipc.hpp"
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <vector>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif
namespace fs=std::filesystem;
namespace ipc=anaiero::host;
using namespace std::chrono_literals;
#define CHECK(condition) do {if(!(condition)){std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)
namespace {
void delay() {std::mutex mutex;std::condition_variable changed;std::unique_lock lock(mutex);changed.wait_for(lock,5ms);}
#ifdef _WIN32
std::wstring wide(std::string_view text) {
  const auto length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
  CHECK(length>0);std::wstring value(static_cast<std::size_t>(length),L'\0');
  CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),value.data(),length)==length);
  return value;
}
std::wstring quote(std::wstring_view text) {
  std::wstring result=L"\"";std::size_t slashes=0;
  for(const auto c:text) {
    if(c==L'\\') {++slashes;continue;}
    result.append(c==L'"'?slashes*2+1:slashes,L'\\');result+=c;slashes=0;
  }
  result.append(slashes*2,L'\\');result+=L'"';return result;
}
#endif
class Process {
#ifdef _WIN32
  HANDLE handle_=nullptr;
#else
  pid_t pid_=-1;
#endif
public:
  Process(const std::string& executable,const std::vector<std::string>& arguments) {
#ifdef _WIN32
    const auto exe=wide(executable);auto command=quote(exe);
    for(const auto& argument:arguments) {command+=L' ';command+=quote(wide(argument));}
    STARTUPINFOW startup{};startup.cb=static_cast<DWORD>(sizeof(startup));PROCESS_INFORMATION process{};
    CHECK(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));
    CloseHandle(process.hThread);handle_=process.hProcess;
#else
    std::vector<std::string> owned{executable};owned.insert(owned.end(),arguments.begin(),arguments.end());
    std::vector<char*> argv;for(auto& item:owned)argv.push_back(item.data());argv.push_back(nullptr);
    CHECK(posix_spawn(&pid_,executable.c_str(),nullptr,nullptr,argv.data(),environ)==0);
#endif
  }
  ~Process(){kill();}
  int wait(std::uint32_t timeout_ms=30000) {
#ifdef _WIN32
    CHECK(handle_);const auto waited=WaitForSingleObject(handle_,timeout_ms);CHECK(waited==WAIT_OBJECT_0);
    DWORD code=0;CHECK(GetExitCodeProcess(handle_,&code));CloseHandle(handle_);handle_=nullptr;return static_cast<int>(code);
#else
    CHECK(pid_>0);const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
    int status=0;
    for(;;) {
      const auto done=waitpid(pid_,&status,WNOHANG);
      if(done==pid_) {pid_=-1;return WIFEXITED(status)?WEXITSTATUS(status):-1;}
      CHECK(done==0||(done<0&&errno==EINTR));CHECK(std::chrono::steady_clock::now()<deadline);delay();
    }
#endif
  }
  void kill() {
#ifdef _WIN32
    if(handle_) {TerminateProcess(handle_,71);WaitForSingleObject(handle_,30000);CloseHandle(handle_);handle_=nullptr;}
#else
    if(pid_>0) {::kill(pid_,SIGKILL);int status=0;while(waitpid(pid_,&status,0)<0&&errno==EINTR){}pid_=-1;}
#endif
  }
};
std::string path_string(const fs::path& path) {const auto bytes=path.u8string();return {bytes.begin(),bytes.end()};}
std::string endpoint() {
#ifdef _WIN32
  return "anaiero-modular-test-"+std::to_string(GetCurrentProcessId());
#else
  return (fs::temp_directory_path()/("anaiero-modular-test-"+std::to_string(getpid())+".sock")).string();
#endif
}
std::string query(std::string_view address,std::string_view command) {
  std::string result;CHECK(ipc::NativeIpcClient::request(address,command,result,3000));
  CHECK(!result.starts_with("ERR"));return result;
}
void ready(std::string_view address) {
  const auto deadline=std::chrono::steady_clock::now()+30s;std::string result;
  while(!ipc::NativeIpcClient::request(address,"PING",result,100)) {CHECK(std::chrono::steady_clock::now()<deadline);delay();}
  CHECK(result=="PONG\n");
  for(;;) {
    result=query(address,"STATUS");
    if(result.find("\"view_revision\":0")==result.npos)break;
    CHECK(std::chrono::steady_clock::now()<deadline);delay();
  }
}
std::uint64_t field(std::string_view json,std::string_view name) {
  const auto label="\""+std::string(name)+"\":";const auto at=json.find(label);CHECK(at!=json.npos);
  std::uint64_t number=0;const auto first=json.data()+at+label.size();
  const auto result=std::from_chars(first,json.data()+json.size(),number);CHECK(result.ec==std::errc{}&&result.ptr!=first);return number;
}
std::string through(std::string_view address,std::uint64_t count) {
  (void)query(address,"THROUGH "+std::to_string(count));const auto deadline=std::chrono::steady_clock::now()+30s;
  for(;;) {
    const auto state=query(address,"STATUS");
    if(field(state,"completed_cycles")==count&&state.find("\"cycle_pending\":false")!=state.npos)return state;
    CHECK(std::chrono::steady_clock::now()<deadline);delay();
  }
}
}
int main(int argc,char** argv) {
  CHECK(argc==5);
#ifndef _WIN32
  const int probe=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
  if(probe<0&&(errno==EPERM||errno==EACCES)) {
    std::cout<<"UNAVAILABLE: executor disallows native IPC; required Windows/Linux CI must run this test\n";
    return std::getenv("ANAIERO_REQUIRE_IPC")?1:77;
  }
  CHECK(probe>=0);close(probe);
#endif
  const auto repository=fs::absolute(argv[1]);const auto directory=fs::absolute(argv[4]);
  fs::remove_all(directory);fs::create_directories(directory);
  const auto store=path_string(directory/"store"),config=path_string(directory/"session.cevsession");
  {
    std::ofstream out(directory/"session.cevsession");
    out<<"ANAIEROSESSION/1\nname host-process\nengine environment environment source-environment 1 1 "
       <<std::quoted(path_string(repository/"examples/scientific/well-mixed-environment.cevmodel"))
       <<" 100000000 500\nend\n";CHECK(out.good());
  }
  Process initialize(argv[3],{"init",config,store});CHECK(initialize.wait()==0);
  const auto address=endpoint();
  {
    Process host(argv[2],{"start",address,store,"1","paused"});ready(address);
    const auto state=through(address,3);CHECK(field(state,"tick")==3);
    (void)query(address,"RUN");const auto deadline=std::chrono::steady_clock::now()+30s;
    std::uint64_t acknowledged=3;
    for(;;) {
      const auto current=query(address,"STATUS");acknowledged=field(current,"completed_cycles");
      if(acknowledged>=10)break;
      CHECK(std::chrono::steady_clock::now()<deadline);delay();
    }
    host.kill();
    // A process kill can occur after ROOT publication and before derived-index
    // publication. Explicit repair restores only that derived materialization.
    Process repair(argv[3],{"repair-index",store});CHECK(repair.wait()==0);
    Process restarted(argv[2],{"start",address,store,"1","paused"});ready(address);
    const auto deadline2=std::chrono::steady_clock::now()+30s;std::string recovered;
    for(;;) {recovered=query(address,"STATUS");if(recovered.find("\"cycle_pending\":false")!=recovered.npos)break;
      CHECK(std::chrono::steady_clock::now()<deadline2);delay();}
    const auto cycles=field(recovered,"completed_cycles");CHECK(cycles>=acknowledged&&field(recovered,"tick")==cycles);
    const auto one_more=through(address,cycles+1);CHECK(field(one_more,"tick")==cycles+1);
    (void)query(address,"RUN");(void)query(address,"STOP");CHECK(restarted.wait()==0);
  }
  Process compatibility(argv[2],{"check",store});CHECK(compatibility.wait()==0);
  Process audit(argv[3],{"audit",store});CHECK(audit.wait()==0);
  std::cout<<"native host subprocess: IPC, abrupt kill, explicit index repair, exact resumed cycles, drain, compatibility and audit passed\n";
}
