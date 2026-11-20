#pragma once
// Single-owner process/pipe adapter. cancel() is the only concurrently callable
// method; registration/lifetime is controlled by the owning application.
#include "anaiero/contracts/child_process.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
namespace anaiero::process {
class NativeChildProcess final:public IProcessExchange {
  using Clock=std::chrono::steady_clock;
  std::atomic<std::uint64_t> cancellation_generation_{0};
  std::uint64_t exchange_generation_=0;
  Code code_=Code::ok;
  bool running_=false;
  std::string buffered_,diagnostics_;
  void diagnostic(const char* bytes,std::size_t count){
    if(count>=kMaxDiagnosticBytes){diagnostics_.assign(bytes+count-kMaxDiagnosticBytes,kMaxDiagnosticBytes);return;}
    if(diagnostics_.size()+count>kMaxDiagnosticBytes)
      diagnostics_.erase(0,diagnostics_.size()+count-kMaxDiagnosticBytes);
    diagnostics_.append(bytes,count);
  }
  static int remaining(Clock::time_point deadline)noexcept{
    const auto now=Clock::now();if(now>=deadline)return 0;
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now).count()+1;
    return static_cast<int>(std::min<std::int64_t>(ms,INT32_MAX));
  }
  static bool valid(const Specification& spec){
    if(spec.executable.empty()||spec.executable.size()>4096||spec.executable.find('\0')!=spec.executable.npos||spec.arguments.size()>32)return false;
    std::size_t total=spec.executable.size();
    for(const auto& argument:spec.arguments){
      if(argument.size()>4096||argument.find('\0')!=argument.npos)return false;
      total+=argument.size()+1;
    }
    return total<=16384;
  }
#ifdef _WIN32
  HANDLE process_=nullptr,job_=nullptr,input_=INVALID_HANDLE_VALUE,output_=INVALID_HANDLE_VALUE,error_=INVALID_HANDLE_VALUE;
  HANDLE cancel_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  OVERLAPPED error_read_{};
  std::array<char,1024> error_buffer_{};
  bool error_pending_=false,error_eof_=false;
  static void close(HANDLE& h)noexcept{if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);h=INVALID_HANDLE_VALUE;}
  static std::wstring wide(std::string_view value){
    if(value.empty())return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(!n)return {};
    std::wstring out(static_cast<std::size_t>(n),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),n))return {};
    return out;
  }
  static std::wstring quote(std::wstring_view value){
    std::wstring out=L"\"";std::size_t slashes=0;
    for(wchar_t c:value){
      if(c==L'\\'){++slashes;continue;}
      if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;}
      else{out.append(slashes,L'\\');out+=c;}
      slashes=0;
    }
    out.append(slashes*2,L'\\');out+=L'\"';return out;
  }
  static bool pipe_pair(bool reads,HANDLE& parent,HANDLE& child){
    static std::atomic<unsigned long> serial{0};
    const auto name=L"\\\\.\\pipe\\anaiero-child-"+std::to_wstring(GetCurrentProcessId())+L"-"+
      std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(serial.fetch_add(1));
    parent=CreateNamedPipeW(name.c_str(),(reads?PIPE_ACCESS_INBOUND:PIPE_ACCESS_OUTBOUND)|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,8192,8192,0,nullptr);
    if(parent==INVALID_HANDLE_VALUE)return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    child=CreateFileW(name.c_str(),reads?GENERIC_WRITE:GENERIC_READ,0,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(child==INVALID_HANDLE_VALUE){close(parent);return false;}
    OVERLAPPED connected{};connected.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!connected.hEvent){close(child);close(parent);return false;}
    const BOOL ok=ConnectNamedPipe(parent,&connected);const DWORD failure=ok?ERROR_SUCCESS:GetLastError();
    bool success=ok||failure==ERROR_PIPE_CONNECTED;
    if(failure==ERROR_IO_PENDING){
      DWORD transferred=0;success=WaitForSingleObject(connected.hEvent,1000)==WAIT_OBJECT_0&&GetOverlappedResult(parent,&connected,&transferred,FALSE);
      if(!success){CancelIoEx(parent,&connected);GetOverlappedResult(parent,&connected,&transferred,TRUE);}
    }
    CloseHandle(connected.hEvent);
    if(!success){close(child);close(parent);}return success;
  }
  void arm_error()noexcept{
    if(error_eof_||error_==INVALID_HANDLE_VALUE)return;
    ResetEvent(error_read_.hEvent);DWORD amount=0;
    const BOOL ok=ReadFile(error_,error_buffer_.data(),static_cast<DWORD>(error_buffer_.size()),&amount,&error_read_);
    if(ok||GetLastError()==ERROR_IO_PENDING)error_pending_=true;
    else{error_eof_=true;error_pending_=false;ResetEvent(error_read_.hEvent);}
  }
  void consume_error()noexcept{
    if(!error_pending_)return;
    DWORD amount=0;
    if(GetOverlappedResult(error_,&error_read_,&amount,FALSE)&&amount){diagnostic(error_buffer_.data(),amount);error_pending_=false;arm_error();}
    else{error_pending_=false;error_eof_=true;ResetEvent(error_read_.hEvent);}
  }
  Code transfer(HANDLE pipe,char* data,std::size_t size,bool write,Clock::time_point deadline,std::size_t& transferred)noexcept{
    transferred=0;
    if(cancellation_generation_.load(std::memory_order_acquire)!=exchange_generation_)return Code::cancelled;
    OVERLAPPED operation{};operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!operation.hEvent)return Code::io_failed;
    DWORD amount=0;const BOOL immediate=write?
      WriteFile(pipe,data,static_cast<DWORD>(size),&amount,&operation):
      ReadFile(pipe,data,static_cast<DWORD>(size),&amount,&operation);
    Code result=Code::ok;
    if(!immediate&&GetLastError()!=ERROR_IO_PENDING)result=Code::io_failed;
    else if(!immediate){
      while(true){
        if(cancellation_generation_.load(std::memory_order_acquire)!=exchange_generation_){result=Code::cancelled;break;}
        const auto wait=remaining(deadline);if(!wait){result=Code::deadline;break;}
        const HANDLE handles[]{cancel_,operation.hEvent,error_read_.hEvent,process_};
        const auto ready=WaitForMultipleObjects(4,handles,FALSE,static_cast<DWORD>(wait));
        if(ready==WAIT_OBJECT_0){
          if(cancellation_generation_.load(std::memory_order_acquire)!=exchange_generation_){result=Code::cancelled;break;}
          ResetEvent(cancel_);continue; // late wake from an earlier idle cancel
        }
        if(ready==WAIT_OBJECT_0+1){if(!GetOverlappedResult(pipe,&operation,&amount,FALSE))result=Code::io_failed;break;}
        if(ready==WAIT_OBJECT_0+2){consume_error();continue;}
        result=ready==WAIT_TIMEOUT?Code::deadline:ready==WAIT_OBJECT_0+3?Code::exited:Code::io_failed;break;
      }
      if(result!=Code::ok){CancelIoEx(pipe,&operation);GetOverlappedResult(pipe,&operation,&amount,TRUE);}
    }
    CloseHandle(operation.hEvent);transferred=amount;
    return result==Code::ok&&amount==0?Code::exited:result;
  }
#else
  pid_t process_=-1;
  int input_=-1,output_=-1,error_=-1;
  int cancel_[2]{-1,-1};
  static void close(int& fd)noexcept{if(fd>=0)::close(fd);fd=-1;}
  static bool nonblocking(int fd)noexcept{const auto flags=fcntl(fd,F_GETFL);return flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)>=0;}
  void drain_cancel()noexcept{
    std::array<char,256> bytes{};
    for(unsigned i=0;i<256;++i)if(::read(cancel_[0],bytes.data(),bytes.size())<=0)break;
  }
  void consume_error()noexcept{
    if(error_<0)return;
    std::array<char,1024> bytes{};
    for(unsigned i=0;i<8;++i){
      const auto n=::read(error_,bytes.data(),bytes.size());
      if(n>0)diagnostic(bytes.data(),static_cast<std::size_t>(n));
      else{if(n==0)close(error_);return;}
    }
  }
  Code ready(int fd,short events,Clock::time_point deadline)noexcept{
    while(true){
      if(cancellation_generation_.load(std::memory_order_acquire)!=exchange_generation_)return Code::cancelled;
      const int wait=remaining(deadline);if(!wait)return Code::deadline;
      pollfd descriptors[3]{{fd,events,0},{cancel_[0],POLLIN,0},{error_,POLLIN,0}};
      const auto result=::poll(descriptors,3,wait);
      if(result<0){if(errno==EINTR)continue;return Code::io_failed;}
      if(result==0)return Code::deadline;
      if(descriptors[1].revents){
        drain_cancel();
        if(cancellation_generation_.load(std::memory_order_acquire)!=exchange_generation_)return Code::cancelled;
      }
      if(descriptors[2].revents)consume_error();
      if(descriptors[0].revents&events)return Code::ok;
      if(descriptors[0].revents&(POLLHUP|POLLERR|POLLNVAL))return Code::exited;
    }
  }
  Code transfer(int fd,char* data,std::size_t size,bool write,Clock::time_point deadline,std::size_t& transferred)noexcept{
    transferred=0;
    while(true){
      const auto status=ready(fd,write?POLLOUT:POLLIN,deadline);if(status!=Code::ok)return status;
      // Block SIGPIPE only on this owner thread. Consume only a signal generated
      // by this write; preserve a previously pending SIGPIPE for the caller.
      sigset_t blocked{},previous{},pending{};bool was_pending=false;
      if(write){sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);pthread_sigmask(SIG_BLOCK,&blocked,&previous);sigpending(&pending);was_pending=sigismember(&pending,SIGPIPE)==1;}
      const auto n=write ? ::write(fd,data,size) : ::read(fd,data,size);const int failure=errno;
      if(write){
        if(n<0&&failure==EPIPE&&!was_pending){timespec zero{};while(sigtimedwait(&blocked,nullptr,&zero)<0&&errno==EINTR){}}
        pthread_sigmask(SIG_SETMASK,&previous,nullptr);
      }
      if(n>0){transferred=static_cast<std::size_t>(n);return Code::ok;}
      if(n==0)return Code::exited;
      if(failure!=EINTR&&failure!=EAGAIN&&failure!=EWOULDBLOCK)return Code::io_failed;
    }
  }
#endif
  Code failure(Code code)noexcept{code_=code;stop();return code_;}
public:
  NativeChildProcess(){
#ifndef _WIN32
    if(::pipe2(cancel_,O_CLOEXEC|O_NONBLOCK)<0){cancel_[0]=-1;cancel_[1]=-1;}
#endif
  }
  ~NativeChildProcess()override{
    stop();
#ifdef _WIN32
    close(cancel_);
#else
    close(cancel_[0]);close(cancel_[1]);
#endif
  }
  NativeChildProcess(const NativeChildProcess&)=delete;
  NativeChildProcess& operator=(const NativeChildProcess&)=delete;
  Code start(const Specification& spec)override{
    stop();buffered_.clear();diagnostics_.clear();code_=Code::ok;
    if(!valid(spec))return code_=Code::invalid;
#ifdef _WIN32
    if(!cancel_)return code_=Code::start_failed;
    ResetEvent(cancel_);
    HANDLE child_in=INVALID_HANDLE_VALUE,child_out=INVALID_HANDLE_VALUE,child_error=INVALID_HANDLE_VALUE;
    auto cleanup=[&]{close(child_in);close(child_out);close(child_error);};
    if(!pipe_pair(false,input_,child_in)||!pipe_pair(true,output_,child_out)||!pipe_pair(true,error_,child_error)){
      cleanup();return failure(Code::start_failed);
    }
    const auto executable=wide(spec.executable);if(executable.empty()){cleanup();return failure(Code::invalid);}
    auto command=quote(executable);
    for(const auto& arg:spec.arguments){const auto encoded=wide(arg);if(encoded.empty()&&!arg.empty()){cleanup();return failure(Code::invalid);}command+=L' ';command+=quote(encoded);}
    std::vector<wchar_t> writable(command.begin(),command.end());writable.push_back(L'\0');
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput=child_in;startup.StartupInfo.hStdOutput=child_out;startup.StartupInfo.hStdError=child_error;
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);std::vector<unsigned char> attributes(size);
    startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&size)){cleanup();return failure(Code::start_failed);}
    HANDLE inherited[]{child_in,child_out,child_error};
    if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)){
      DeleteProcThreadAttributeList(startup.lpAttributeList);cleanup();return failure(Code::start_failed);
    }
    job_=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
    limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job_||!SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limit,sizeof(limit))){DeleteProcThreadAttributeList(startup.lpAttributeList);cleanup();return failure(Code::start_failed);}
    PROCESS_INFORMATION info{};
    const BOOL created=CreateProcessW(executable.c_str(),writable.data(),nullptr,nullptr,TRUE,
      EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup.StartupInfo,&info);
    DeleteProcThreadAttributeList(startup.lpAttributeList);cleanup();
    if(!created)return failure(Code::start_failed);
    process_=info.hProcess;
    if(!AssignProcessToJobObject(job_,process_)){TerminateProcess(process_,1);CloseHandle(info.hThread);return failure(Code::start_failed);}
    error_read_={};error_read_.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);error_eof_=false;error_pending_=false;
    if(!error_read_.hEvent){TerminateProcess(process_,1);CloseHandle(info.hThread);return failure(Code::start_failed);}
    arm_error();
    const auto resumed=ResumeThread(info.hThread);CloseHandle(info.hThread);
    if(resumed==static_cast<DWORD>(-1))return failure(Code::start_failed);
#else
    if(cancel_[0]<0)return code_=Code::start_failed;
    drain_cancel();
    int in[2]{-1,-1},out[2]{-1,-1},err[2]{-1,-1};
    auto cleanup=[&]{for(auto* f:{in,out,err}){close(f[0]);close(f[1]);}};
    if(::pipe2(in,O_CLOEXEC)<0||::pipe2(out,O_CLOEXEC)<0||::pipe2(err,O_CLOEXEC)<0){cleanup();return code_=Code::start_failed;}
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    if(posix_spawn_file_actions_init(&actions)!=0){cleanup();return code_=Code::start_failed;}
    if(posix_spawnattr_init(&attributes)!=0){posix_spawn_file_actions_destroy(&actions);cleanup();return code_=Code::start_failed;}
    int result=0;
    result|=posix_spawn_file_actions_adddup2(&actions,in[0],STDIN_FILENO);
    result|=posix_spawn_file_actions_adddup2(&actions,out[1],STDOUT_FILENO);
    result|=posix_spawn_file_actions_adddup2(&actions,err[1],STDERR_FILENO);
    for(auto* f:{in,out,err})for(int i=0;i<2;++i)result|=posix_spawn_file_actions_addclose(&actions,f[i]);
    result|=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    result|=posix_spawnattr_setpgroup(&attributes,0);
    std::vector<char*> arguments;arguments.reserve(spec.arguments.size()+2);
    arguments.push_back(const_cast<char*>(spec.executable.c_str()));for(const auto& a:spec.arguments)arguments.push_back(const_cast<char*>(a.c_str()));arguments.push_back(nullptr);
    if(result==0)result=posix_spawnp(&process_,spec.executable.c_str(),&actions,&attributes,arguments.data(),environ);
    posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);
    close(in[0]);close(out[1]);close(err[1]);
    if(result!=0){cleanup();process_=-1;return code_=Code::start_failed;}
    input_=in[1];output_=out[0];error_=err[0];
    if(!nonblocking(input_)||!nonblocking(output_)||!nonblocking(error_))return failure(Code::start_failed);
#endif
    running_=true;return Code::ok;
  }
  Code exchange(std::string_view request,std::string& response,std::uint32_t timeout_ms)noexcept override{
    response.clear();
    if(!running_)return code_==Code::ok?Code::exited:code_;
    // Cancellation belongs to an exchange in progress. An idle cancel cannot
    // poison the next operation or another engine in a mixed session.
    exchange_generation_=cancellation_generation_.load(std::memory_order_acquire);
#ifdef _WIN32
    ResetEvent(cancel_);
#else
    drain_cancel();
#endif
    if(request.empty()||request.size()>=kMaxFrameBytes||!timeout_ms||timeout_ms>60000)return failure(Code::invalid);
    for(char ch:request)if(ch<32||ch>126)return failure(Code::invalid);
    if(cancellation_generation_.load(std::memory_order_acquire)!=exchange_generation_)return failure(Code::cancelled);
    const auto deadline=Clock::now()+std::chrono::milliseconds(timeout_ms);
    std::string wire(request);wire+='\n';std::size_t sent=0;
    while(sent<wire.size()){
      std::size_t n=0;const auto result=transfer(input_,wire.data()+sent,wire.size()-sent,true,deadline,n);
      if(result!=Code::ok)return failure(result);
      sent+=n;
    }
    while(true){
      const auto newline=buffered_.find('\n');
      if(newline!=buffered_.npos){
        if(newline>=kMaxFrameBytes)return failure(Code::frame_limit);
        response=buffered_.substr(0,newline);buffered_.erase(0,newline+1);
        // CRT text streams on Windows emit CRLF; the payload remains ASCII.
        if(!response.empty()&&response.back()=='\r')response.pop_back();
        for(char ch:response)if(ch<32||ch>126)return failure(Code::io_failed);
        return Code::ok;
      }
      if(buffered_.size()>=kMaxFrameBytes)return failure(Code::frame_limit);
      std::array<char,1024> bytes{};std::size_t n=0;
      const auto result=transfer(output_,bytes.data(),std::min(bytes.size(),kMaxFrameBytes-buffered_.size()),false,deadline,n);
      if(result!=Code::ok)return failure(result);
      buffered_.append(bytes.data(),n);
    }
  }
  void cancel()noexcept override{
    cancellation_generation_.fetch_add(1,std::memory_order_acq_rel);
#ifdef _WIN32
    if(cancel_)SetEvent(cancel_);
#else
    if(cancel_[1]>=0){const char one=1;const auto ignored=::write(cancel_[1],&one,1);(void)ignored;}
#endif
  }
  void stop()noexcept override{
#ifdef _WIN32
    if(job_&&job_!=INVALID_HANDLE_VALUE)TerminateJobObject(job_,1);
    else if(process_&&process_!=INVALID_HANDLE_VALUE)TerminateProcess(process_,1);
    if(error_pending_&&error_!=INVALID_HANDLE_VALUE){DWORD n=0;CancelIoEx(error_,&error_read_);GetOverlappedResult(error_,&error_read_,&n,TRUE);error_pending_=false;}
    if(process_&&process_!=INVALID_HANDLE_VALUE)WaitForSingleObject(process_,5000);
    close(error_read_.hEvent);close(input_);close(output_);close(error_);close(process_);close(job_);
#else
    if(process_>0){::kill(-process_,SIGKILL);int status=0;while(::waitpid(process_,&status,0)<0&&errno==EINTR){}process_=-1;}
    close(input_);close(output_);close(error_);
#endif
    running_=false;
  }
  bool running()const noexcept override{return running_;}
  Code status()const noexcept override{return code_;}
  // Owner-thread collection for supervisors that use a separate IPC channel.
  // Never wait for stderr or consume an incomplete Windows overlapped read.
  void collect_diagnostics()noexcept{
#ifdef _WIN32
    for(unsigned i=0;i<8&&error_pending_;++i){
      if(WaitForSingleObject(error_read_.hEvent,0)!=WAIT_OBJECT_0)break;
      consume_error();
    }
#else
    consume_error();
#endif
  }
  std::string diagnostics()const override{return diagnostics_;}
};
} // namespace anaiero::process
