#pragma once
// Bounded local Windows/Linux control transport. Client faults close only that
// connection. One absolute request deadline includes the complete response.
#include "anaiero/contracts/host_runtime.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace anaiero::host {
inline bool valid_endpoint(std::string_view name,bool windows) noexcept {
  if(name.empty()||name.size()>(windows?64u:100u))return false;
  for(char c:name){
    if(c=='\0'||c=='\n'||c=='\r')return false;
    if(windows&&!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
        (c>='0'&&c<='9')||c=='_'||c=='-'))return false;
  }
  return true;
}
namespace native_io {
inline constexpr char kReceipt='\x06';
inline bool valid_timeout(std::uint32_t ms) noexcept {return ms>0&&ms<=600000;}
inline bool valid_command(std::string_view command) noexcept {
  if(command.empty()||command.size()>kMaxRequestBytes)return false;
  for(unsigned char c:command)if(c<32)return false;
  return true;
}
class Deadline {
  std::chrono::steady_clock::time_point end_;
public:
  explicit Deadline(std::uint32_t ms) noexcept:
    end_(std::chrono::steady_clock::now()+std::chrono::milliseconds(ms)){}
  std::uint32_t remaining_ms() const noexcept {
    const auto now=std::chrono::steady_clock::now();
    if(now>=end_)return 0;
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(end_-now).count();
    return static_cast<std::uint32_t>(ms)+1u;
  }
};
struct ServingGuard {
  std::atomic<bool>& serving;
  std::atomic<bool>& ready;
  ~ServingGuard(){ready.store(false);serving.store(false);}
};
#ifdef _WIN32
inline std::string pipe_name(std::string_view endpoint) {
  return std::string("\\\\.\\pipe\\")+std::string(endpoint);
}
struct Handle {
  HANDLE value=INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE h) noexcept:value(h){}
  ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
  Handle(const Handle&)=delete;
  Handle&operator=(const Handle&)=delete;
};
inline bool same_user(HANDLE pipe) noexcept {
  if(!ImpersonateNamedPipeClient(pipe))return false;
  HANDLE caller=nullptr;
  const bool client_token=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&caller)!=0;
  RevertToSelf();
  if(!client_token)return false;
  Handle client(caller);
  HANDLE process=nullptr;
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&process))return false;
  Handle server(process);
  alignas(TOKEN_USER) std::array<std::byte,2048> a{},b{};
  DWORD na=0,nb=0;
  const bool ok=GetTokenInformation(client.value,TokenUser,a.data(),
      static_cast<DWORD>(a.size()),&na)&&
    GetTokenInformation(server.value,TokenUser,b.data(),
      static_cast<DWORD>(b.size()),&nb);
  return ok&&EqualSid(reinterpret_cast<TOKEN_USER*>(a.data())->User.Sid,
      reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid)!=0;
}
inline void cancel_and_complete(HANDLE pipe,OVERLAPPED& operation) noexcept {
  // Cancellation is a request. Reap it before releasing OVERLAPPED or buffers.
  CancelIoEx(pipe,&operation);
  DWORD ignored=0;
  GetOverlappedResult(pipe,&operation,&ignored,TRUE);
}
inline bool finish_pending(HANDLE pipe,OVERLAPPED& operation,HANDLE stop,
                           const Deadline* deadline,DWORD& transferred) noexcept {
  const DWORD remaining=deadline?deadline->remaining_ms():INFINITE;
  if(remaining==0){cancel_and_complete(pipe,operation);return false;}
  if(stop){
    HANDLE events[2]={stop,operation.hEvent};
    const DWORD outcome=WaitForMultipleObjects(2,events,FALSE,remaining);
    if(outcome==WAIT_OBJECT_0+1)
      return GetOverlappedResult(pipe,&operation,&transferred,FALSE)!=0;
  }else{
    const DWORD outcome=WaitForSingleObject(operation.hEvent,remaining);
    if(outcome==WAIT_OBJECT_0)
      return GetOverlappedResult(pipe,&operation,&transferred,FALSE)!=0;
  }
  cancel_and_complete(pipe,operation);return false;
}
inline bool transfer(HANDLE pipe,bool reading,void* buffer,DWORD maximum,
                     DWORD& transferred,HANDLE stop,const Deadline& deadline) noexcept {
  transferred=0;
  if(deadline.remaining_ms()==0||
     (stop&&WaitForSingleObject(stop,0)==WAIT_OBJECT_0))return false;
  Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
  if(!event.value)return false;
  OVERLAPPED operation{};operation.hEvent=event.value;
  const BOOL completed=reading?ReadFile(pipe,buffer,maximum,&transferred,&operation):
    WriteFile(pipe,buffer,maximum,&transferred,&operation);
  if(completed)return transferred>0;
  if(GetLastError()!=ERROR_IO_PENDING)return false;
  return finish_pending(pipe,operation,stop,&deadline,transferred)&&transferred>0;
}
enum class ConnectionResult {connected,client_gone,failed};
inline ConnectionResult connect_pipe(HANDLE pipe,HANDLE stop) noexcept {
  Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
  if(!event.value)return ConnectionResult::failed;
  OVERLAPPED operation{};operation.hEvent=event.value;
  if(ConnectNamedPipe(pipe,&operation))return ConnectionResult::connected;
  auto error=GetLastError();
  if(error==ERROR_PIPE_CONNECTED)return ConnectionResult::connected;
  if(error==ERROR_IO_PENDING){
    DWORD ignored=0;
    if(finish_pending(pipe,operation,stop,nullptr,ignored))return ConnectionResult::connected;
    error=GetLastError();
  }
  if(error==ERROR_NO_DATA||error==ERROR_PIPE_NOT_CONNECTED||error==ERROR_BROKEN_PIPE)
    return ConnectionResult::client_gone;
  return ConnectionResult::failed;
}
inline bool write_all(HANDLE pipe,std::string_view bytes,HANDLE stop,
                      const Deadline& deadline) noexcept {
  std::size_t sent=0;
  while(sent<bytes.size()){
    DWORD count=0;
    if(!transfer(pipe,false,const_cast<char*>(bytes.data()+sent),
                 static_cast<DWORD>(bytes.size()-sent),count,stop,deadline))return false;
    sent+=count;
  }
  return true;
}
inline bool read_line(HANDLE pipe,char* bytes,std::size_t maximum,
                      std::size_t& length,HANDLE stop,const Deadline& deadline) noexcept {
  length=0;
  while(length<maximum){
    DWORD count=0;
    if(!transfer(pipe,true,bytes+length,static_cast<DWORD>(maximum-length),
                 count,stop,deadline))return false;
    const auto old=length;length+=count;
    for(std::size_t i=old;i<length;++i){
      if(bytes[i]=='\0'||bytes[i]=='\r')return false;
      if(bytes[i]=='\n')return i+1==length;
    }
  }
  return false;
}
#else
struct Descriptor {
  int value=-1;
  explicit Descriptor(int fd) noexcept:value(fd){}
  ~Descriptor(){if(value>=0)close(value);}
  Descriptor(const Descriptor&)=delete;
  Descriptor&operator=(const Descriptor&)=delete;
};
inline bool wait_ready(int fd,short events,int stop_fd,
                       std::atomic<bool>* stopping,const Deadline* deadline) noexcept {
  while(true){
    if(stopping&&stopping->load())return false;
    const auto remaining=deadline?deadline->remaining_ms():0;
    if(deadline&&!remaining)return false;
    struct pollfd fds[2]={{fd,events,0},{stop_fd,POLLIN,0}};
    const nfds_t count=stop_fd>=0?2u:1u;
    const int result=poll(fds,count,deadline?static_cast<int>(remaining):-1);
    if(result<0&&errno==EINTR)continue;
    if(result<=0)return false;
    if(stop_fd>=0&&fds[1].revents){
      if(stopping)stopping->store(true);
      return false;
    }
    if(fds[0].revents&POLLNVAL)return false;
    if(fds[0].revents&(events|POLLERR|POLLHUP))return true;
  }
}
inline bool write_all(int fd,std::string_view bytes,int stop_fd,
                      std::atomic<bool>* stopping,const Deadline& deadline) noexcept {
  std::size_t sent=0;
  while(sent<bytes.size()){
    if(!wait_ready(fd,POLLOUT,stop_fd,stopping,&deadline))return false;
    const auto n=send(fd,bytes.data()+sent,bytes.size()-sent,MSG_NOSIGNAL);
    if(n<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
    if(n<=0)return false;
    sent+=static_cast<std::size_t>(n);
  }
  return true;
}
inline bool read_line(int fd,char* bytes,std::size_t maximum,std::size_t& length,
                      int stop_fd,std::atomic<bool>* stopping,
                      const Deadline& deadline) noexcept {
  length=0;
  while(length<maximum){
    if(!wait_ready(fd,POLLIN,stop_fd,stopping,&deadline))return false;
    const auto n=recv(fd,bytes+length,maximum-length,0);
    if(n<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
    if(n<=0)return false;
    const auto old=length;length+=static_cast<std::size_t>(n);
    for(std::size_t i=old;i<length;++i){
      if(bytes[i]=='\0'||bytes[i]=='\r')return false;
      if(bytes[i]=='\n')return i+1==length;
    }
  }
  return false;
}
inline bool same_user(int fd) noexcept {
#ifdef SO_PEERCRED
  struct {pid_t pid;uid_t uid;gid_t gid;} peer{};
  socklen_t size=sizeof(peer);
  return getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&size)==0&&
    size==sizeof(peer)&&peer.uid==geteuid();
#else
  (void)fd;return false;
#endif
}
inline Code remove_stale_endpoint(const std::string& endpoint,
                                  const sockaddr_un& address) noexcept {
  struct stat before{};
  if(lstat(endpoint.c_str(),&before)!=0)
    return errno==ENOENT?Code::ok:Code::bind_failure;
  if(!S_ISSOCK(before.st_mode)||before.st_uid!=geteuid())return Code::permission_denied;
  Descriptor probe(socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));
  if(probe.value<0)return Code::bind_failure;
  if(connect(probe.value,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==0)
    return Code::bind_failure; // A live owner must never be unlinked.
  const auto error=errno;
  if(error==ENOENT)return Code::ok;
  if(error!=ECONNREFUSED)return Code::bind_failure;
  struct stat after{};
  if(lstat(endpoint.c_str(),&after)!=0)return errno==ENOENT?Code::ok:Code::bind_failure;
  if(before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||
     !S_ISSOCK(after.st_mode)||after.st_uid!=geteuid())return Code::bind_failure;
  return unlink(endpoint.c_str())==0?Code::ok:Code::bind_failure;
}
struct EndpointCleanup {
  const std::string& path;
  dev_t device{};ino_t inode{};
  bool owned=false;
  explicit EndpointCleanup(const std::string& value) noexcept:path(value){}
  void capture() noexcept {
    struct stat info{};
    if(lstat(path.c_str(),&info)==0&&S_ISSOCK(info.st_mode)){
      device=info.st_dev;inode=info.st_ino;owned=true;
    }
  }
  ~EndpointCleanup(){
    struct stat info{};
    if(owned&&lstat(path.c_str(),&info)==0&&info.st_dev==device&&info.st_ino==inode)
      unlink(path.c_str());
  }
};
#endif
} // namespace native_io

class NativeIpcServer final:public ILocalServer {
  std::string endpoint_;
  std::uint32_t timeout_ms_;
  std::atomic<bool> stopping_{false},serving_{false},ready_{false};
  void (*ready_callback_)(void*) noexcept=nullptr;
  void* ready_context_=nullptr;
#ifdef _WIN32
  HANDLE stop_event_=nullptr;
#else
  int stop_pipe_[2]={-1,-1};
#endif
  void announce_ready() noexcept {
    ready_.store(true);
    if(ready_callback_)ready_callback_(ready_context_);
  }
public:
  explicit NativeIpcServer(std::string endpoint,std::uint32_t request_timeout_ms=2000):
    endpoint_(std::move(endpoint)),timeout_ms_(request_timeout_ms){
#ifdef _WIN32
    stop_event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
#else
    if(pipe2(stop_pipe_,O_CLOEXEC|O_NONBLOCK)!=0)stop_pipe_[0]=stop_pipe_[1]=-1;
#endif
  }
  NativeIpcServer(const NativeIpcServer&)=delete;
  NativeIpcServer&operator=(const NativeIpcServer&)=delete;
  // The owner joins serve() before destruction.
  ~NativeIpcServer() override {
    request_stop();
#ifdef _WIN32
    if(stop_event_)CloseHandle(stop_event_);
#else
    for(int fd:stop_pipe_)if(fd>=0)close(fd);
#endif
  }
  void request_stop() noexcept override {
    stopping_.store(true);
#ifdef _WIN32
    if(stop_event_)SetEvent(stop_event_);
#else
    if(stop_pipe_[1]>=0){
      const char byte=1;const auto ignored=write(stop_pipe_[1],&byte,1);(void)ignored;
    }
#endif
  }
#ifndef _WIN32
  // POSIX handlers only write to this self-pipe; the owner sets ordinary state.
  int signal_stop_fd() const noexcept{return stop_pipe_[1];}
#endif
  bool ready()const noexcept{return ready_.load();}
  bool stopping()const noexcept{return stopping_.load();}
  // Configure before serve(), never concurrently with the server owner.
  void set_ready_callback(void (*fn)(void*) noexcept,void* context) noexcept {
    ready_callback_=fn;ready_context_=context;
  }
  Code serve(ICommandService& service,std::size_t max_requests=0) noexcept override {
    if(serving_.exchange(true))return Code::invalid;
    native_io::ServingGuard guard{serving_,ready_};
    if(!native_io::valid_timeout(timeout_ms_))return Code::invalid;
    if(stopping_.load())return Code::interrupted;
    std::size_t served=0;
#ifdef _WIN32
    if(!valid_endpoint(endpoint_,true))return Code::invalid;
    if(!stop_event_)return Code::bind_failure;
    const auto name=native_io::pipe_name(endpoint_);
    // Keep one first-instance handle until shutdown; no namespace ownership gap.
    native_io::Handle pipe(CreateNamedPipeA(name.c_str(),
      PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,
      1,static_cast<DWORD>(kMaxReplyBytes),static_cast<DWORD>(kMaxRequestBytes+1),
      timeout_ms_,nullptr));
    if(pipe.value==INVALID_HANDLE_VALUE)return Code::bind_failure;
    announce_ready();
    while(!stopping_.load()&&(!max_requests||served<max_requests)){
      const auto connection=native_io::connect_pipe(pipe.value,stop_event_);
      if(connection!=native_io::ConnectionResult::connected){
        if(stopping_.load())return Code::interrupted;
        if(connection==native_io::ConnectionResult::client_gone){
          DisconnectNamedPipe(pipe.value);continue;
        }
        return Code::read_failure;
      }
      native_io::Deadline deadline(timeout_ms_);
      std::array<char,kMaxRequestBytes+1> input{};
      std::size_t used=0;
      const bool framed=native_io::read_line(pipe.value,input.data(),input.size(),
                                             used,stop_event_,deadline);
      if(!framed||!native_io::same_user(pipe.value)||
         !native_io::valid_command(std::string_view(input.data(),used?used-1:0))){
        DisconnectNamedPipe(pipe.value);continue;
      }
      const auto reply=service.handle(std::string_view(input.data(),used-1));
      const bool valid=reply.length>0&&reply.length<=reply.bytes.size()&&
                        reply.bytes[reply.length-1]=='\n';
      if(valid&&native_io::write_all(pipe.value,
          std::string_view(reply.bytes.data(),reply.length),stop_event_,deadline)){
        // DisconnectNamedPipe drops unread buffered bytes. An optional receipt
        // confirms that the client read the response, without unbounded flush.
        char receipt=0;DWORD received=0;
        native_io::transfer(pipe.value,true,&receipt,1,received,stop_event_,deadline);
      }
      DisconnectNamedPipe(pipe.value);
      ++served;
      if(reply.stop)return Code::ok;
    }
#else
    if(!valid_endpoint(endpoint_,false)||endpoint_.size()>=sizeof(sockaddr_un::sun_path))
      return Code::invalid;
    if(stop_pipe_[0]<0)return Code::bind_failure;
    // Hold ownership before probing/binding so another starting server cannot
    // mistake the short bind-before-listen interval for an abandoned socket.
    // Keep the lock inode after shutdown: unlinking it would permit two locks
    // on different inodes bearing the same pathname during a restart race.
    const auto lock_name=endpoint_+".lock";
    native_io::Descriptor ownership(open(lock_name.c_str(),
      O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600));
    if(ownership.value<0)return Code::bind_failure;
    struct stat owner{};
    if(fstat(ownership.value,&owner)!=0||!S_ISREG(owner.st_mode)||
       owner.st_uid!=geteuid()||owner.st_nlink!=1||(owner.st_mode&0077)!=0)
      return Code::permission_denied;
    if(flock(ownership.value,LOCK_EX|LOCK_NB)!=0)return Code::bind_failure;
    native_io::Descriptor listener(socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));
    if(listener.value<0)return Code::bind_failure;
    struct sockaddr_un address{};address.sun_family=AF_UNIX;
    std::memcpy(address.sun_path,endpoint_.c_str(),endpoint_.size()+1);
    if(const auto code=native_io::remove_stale_endpoint(endpoint_,address);code!=Code::ok)
      return code;
    if(bind(listener.value,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0)
      return Code::bind_failure;
    native_io::EndpointCleanup endpoint_cleanup(endpoint_);endpoint_cleanup.capture();
    if(chmod(endpoint_.c_str(),0600)!=0||listen(listener.value,8)!=0)return Code::bind_failure;
    announce_ready();
    while(!stopping_.load()&&(!max_requests||served<max_requests)){
      if(!native_io::wait_ready(listener.value,POLLIN,stop_pipe_[0],&stopping_,nullptr))
        return stopping_.load()?Code::interrupted:Code::read_failure;
      native_io::Descriptor client(accept4(listener.value,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK));
      if(client.value<0){
        if(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK)continue;
        return Code::read_failure;
      }
      native_io::Deadline deadline(timeout_ms_);
      if(!native_io::same_user(client.value))continue;
      std::array<char,kMaxRequestBytes+1> input{};
      std::size_t used=0;
      if(!native_io::read_line(client.value,input.data(),input.size(),used,
                               stop_pipe_[0],&stopping_,deadline)||
         !native_io::valid_command(std::string_view(input.data(),used?used-1:0)))continue;
      const auto reply=service.handle(std::string_view(input.data(),used-1));
      const bool valid=reply.length>0&&reply.length<=reply.bytes.size()&&
                        reply.bytes[reply.length-1]=='\n';
      if(valid)native_io::write_all(client.value,
        std::string_view(reply.bytes.data(),reply.length),stop_pipe_[0],&stopping_,deadline);
      ++served;
      if(reply.stop)return Code::ok;
    }
#endif
    return stopping_.load()?Code::interrupted:Code::ok;
  }
};

class NativeIpcClient {
public:
  static bool request(std::string_view endpoint,std::string_view command,
                       std::string& response,std::uint32_t timeout_ms=3000) {
    response.clear();
    if(!native_io::valid_command(command)||!native_io::valid_timeout(timeout_ms))return false;
    native_io::Deadline deadline(timeout_ms);
    const std::string packet=std::string(command)+"\n";
    std::array<char,kMaxReplyBytes> reply{};
    std::size_t received=0;
#ifdef _WIN32
    if(!valid_endpoint(endpoint,true))return false;
    const auto name=native_io::pipe_name(endpoint);
    HANDLE opened=INVALID_HANDLE_VALUE;
    while(deadline.remaining_ms()){
      opened=CreateFileA(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
                         OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
      if(opened!=INVALID_HANDLE_VALUE)break;
      if(GetLastError()!=ERROR_PIPE_BUSY||
         !WaitNamedPipeA(name.c_str(),deadline.remaining_ms()))return false;
    }
    native_io::Handle pipe(opened);
    if(pipe.value==INVALID_HANDLE_VALUE)return false;
    if(!native_io::write_all(pipe.value,packet,nullptr,deadline)||
       !native_io::read_line(pipe.value,reply.data(),reply.size(),received,nullptr,deadline))return false;
    // A complete reply is definitive even if its delivery receipt fails.
    native_io::write_all(pipe.value,std::string_view(&native_io::kReceipt,1),nullptr,deadline);
#else
    if(!valid_endpoint(endpoint,false)||endpoint.size()>=sizeof(sockaddr_un::sun_path))return false;
    native_io::Descriptor fd(socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));
    if(fd.value<0)return false;
    struct sockaddr_un address{};address.sun_family=AF_UNIX;
    std::memcpy(address.sun_path,endpoint.data(),endpoint.size());
    if(connect(fd.value,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0){
      if(errno!=EINPROGRESS||!native_io::wait_ready(fd.value,POLLOUT,-1,nullptr,&deadline))return false;
      int error=0;socklen_t size=sizeof(error);
      if(getsockopt(fd.value,SOL_SOCKET,SO_ERROR,&error,&size)!=0||error)return false;
    }
    if(!native_io::write_all(fd.value,packet,-1,nullptr,deadline)||
       !native_io::read_line(fd.value,reply.data(),reply.size(),received,-1,nullptr,deadline))return false;
#endif
    response.assign(reply.data(),received);return true;
  }
};
} // namespace anaiero::host
