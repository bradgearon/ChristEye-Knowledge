#pragma once
// Native local IPC (Linux Unix-domain socket / Windows named pipe).
// The server authenticates same-user local clients before dispatching commands.
// It intentionally contains no biological/physical actuation capability.
#include "anaiero/contracts/host_runtime.hpp"
#include <array>
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
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
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
#ifdef _WIN32
namespace native_io {
inline std::string pipe_name(std::string_view endpoint) {
  return std::string("\\\\.\\pipe\\")+std::string(endpoint);
}
inline bool same_user(HANDLE pipe) noexcept {
  if(!ImpersonateNamedPipeClient(pipe))return false;
  HANDLE caller=nullptr;
  const bool client_token=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&caller)!=0;
  RevertToSelf();
  if(!client_token)return false;
  HANDLE server=nullptr;
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&server)) {
    CloseHandle(caller);return false;
  }
  std::array<std::byte,2048> a{},b{};
  DWORD na=0,nb=0;
  const bool ok=GetTokenInformation(caller,TokenUser,a.data(),
      static_cast<DWORD>(a.size()),&na)&&
    GetTokenInformation(server,TokenUser,b.data(),
      static_cast<DWORD>(b.size()),&nb);
  const bool same=ok&&EqualSid(reinterpret_cast<TOKEN_USER*>(a.data())->User.Sid,
      reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid)!=0;
  CloseHandle(caller);CloseHandle(server);return same;
}
inline bool overlapped(HANDLE pipe,bool reading,void* buffer,DWORD max,DWORD& transferred) noexcept {
  OVERLAPPED ov{};
  ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  if(!ov.hEvent)return false;
  BOOL success=reading?ReadFile(pipe,buffer,max,&transferred,&ov):
                           WriteFile(pipe,buffer,max,&transferred,&ov);
  if(!success&&GetLastError()==ERROR_IO_PENDING) {
    const auto wait=WaitForSingleObject(ov.hEvent,2000);
    if(wait!=WAIT_OBJECT_0) {
      CancelIoEx(pipe,&ov);CloseHandle(ov.hEvent);return false;
    }
    success=GetOverlappedResult(pipe,&ov,&transferred,FALSE);
  }
  CloseHandle(ov.hEvent);
  return success!=0;
}
} // namespace native_io
class NativeIpcServer final:public ILocalServer {
  std::string endpoint_;
public:
  explicit NativeIpcServer(std::string endpoint):endpoint_(std::move(endpoint)){}
  Code serve(ICommandService& service,std::size_t max_requests=0) noexcept override {
    if(!valid_endpoint(endpoint_,true))return Code::invalid;
    const auto name=native_io::pipe_name(endpoint_);
    std::size_t served=0;
    while(!max_requests||served<max_requests) {
      HANDLE pipe=CreateNamedPipeA(name.c_str(),
        PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,
        1,256,256,2000,nullptr);
      if(pipe==INVALID_HANDLE_VALUE)return Code::bind_failure;
      OVERLAPPED connect{};
      connect.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
      if(!connect.hEvent){CloseHandle(pipe);return Code::read_failure;}
      BOOL connected=ConnectNamedPipe(pipe,&connect);
      if(!connected) {
        const DWORD error=GetLastError();
        if(error==ERROR_PIPE_CONNECTED)connected=TRUE;
        else if(error==ERROR_IO_PENDING){
          DWORD transferred=0;
          if(WaitForSingleObject(connect.hEvent,INFINITE)==WAIT_OBJECT_0)
            connected=GetOverlappedResult(pipe,&connect,&transferred,FALSE);
        }
      }
      CloseHandle(connect.hEvent);
      if(!connected){CloseHandle(pipe);return Code::interrupted;}
      char buffer[128]{};
      DWORD bytes=0;
      bool good=native_io::overlapped(pipe,true,buffer,127,bytes);
      if(!good||!bytes||bytes>127){
        DisconnectNamedPipe(pipe);CloseHandle(pipe);return Code::read_failure;
      }
      const std::string_view line(buffer,static_cast<std::size_t>(bytes));
      const bool valid=!line.empty()&&line.back()=='\n'&&
        line.find('\n')==line.size()-1&&native_io::same_user(pipe);
      if(!valid) {
        DisconnectNamedPipe(pipe);CloseHandle(pipe);
        return Code::permission_denied;
      }
      const auto result=service.handle(line.substr(0,line.size()-1));
      DWORD written=0;
      good=result.length>0&&native_io::overlapped(pipe,false,
        const_cast<char*>(result.bytes.data()),static_cast<DWORD>(result.length),written)&&
        written==result.length;
      FlushFileBuffers(pipe);
      DisconnectNamedPipe(pipe);CloseHandle(pipe);
      if(!good)return Code::write_failure;
      ++served;
      if(result.stop)return Code::ok;
    }
    return Code::ok;
  }
};
class NativeIpcClient {
public:
  static bool request(std::string_view endpoint,std::string_view command,
                      std::string& response) {
    if(!valid_endpoint(endpoint,true)||command.empty()||command.size()>120)return false;
    auto name=native_io::pipe_name(endpoint);
    if(!WaitNamedPipeA(name.c_str(),3000))return false;
    HANDLE pipe=CreateFileA(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,
                            nullptr,OPEN_EXISTING,0,nullptr);
    if(pipe==INVALID_HANDLE_VALUE)return false;
    const std::string request=std::string(command)+"\n";
    DWORD n=0;
    bool ok=WriteFile(pipe,request.data(),static_cast<DWORD>(request.size()),&n,nullptr)&&
            n==request.size();
    char reply[256]{};
    DWORD got=0;
    if(ok)ok=ReadFile(pipe,reply,255,&got,nullptr)!=0;
    CloseHandle(pipe);
    if(ok&&got)response.assign(reply,reply+got);
    return ok&&got>0&&response.back()=='\n';
  }
};
#else
class NativeIpcServer final:public ILocalServer {
  std::string endpoint_;
public:
  explicit NativeIpcServer(std::string endpoint):endpoint_(std::move(endpoint)){}
  Code serve(ICommandService& service,std::size_t max_requests=0) noexcept override {
    if(!valid_endpoint(endpoint_,false)||endpoint_.size()>=sizeof(sockaddr_un::sun_path))
      return Code::invalid;
    const int listener=socket(AF_UNIX,SOCK_STREAM,0);
    if(listener<0)return Code::bind_failure;
    struct sockaddr_un addr{};addr.sun_family=AF_UNIX;
    std::memcpy(addr.sun_path,endpoint_.c_str(),endpoint_.size()+1);
    struct stat old{};
    if(lstat(endpoint_.c_str(),&old)==0) {
      if(!S_ISSOCK(old.st_mode)||old.st_uid!=geteuid()) {
        close(listener);return Code::permission_denied;
      }
      if(unlink(endpoint_.c_str())!=0){close(listener);return Code::bind_failure;}
    }
    if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0||
       chmod(endpoint_.c_str(),0600)!=0||listen(listener,8)!=0) {
      close(listener);unlink(endpoint_.c_str());return Code::bind_failure;
    }
    std::size_t served=0;Code result=Code::ok;
    while(!max_requests||served<max_requests){
      struct pollfd pfd{listener,POLLIN,0};
      if(poll(&pfd,1,-1)<=0){result=Code::interrupted;break;}
      const int client=accept(listener,nullptr,nullptr);
      if(client<0){result=Code::read_failure;break;}
#ifdef SO_PEERCRED
      struct {pid_t pid;uid_t uid;gid_t gid;} peer{};
      socklen_t size=sizeof(peer);
      const bool allowed=getsockopt(client,SOL_SOCKET,SO_PEERCRED,
                                   &peer,&size)==0&&size==sizeof(peer)&&
                                   peer.uid==geteuid();
#else
      const bool allowed=false;
#endif
      if(!allowed){close(client);result=Code::permission_denied;break;}
      struct timeval timeout{2,0};
      setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
      char input[128]{};
      std::size_t used=0;
      bool valid=false;
      while(used<sizeof(input)-1){
        char c=0;
        const auto got=recv(client,&c,1,0);
        if(got!=1)break;
        if(c=='\n'){valid=true;break;}
        if(c=='\r'||c=='\0'){valid=false;break;}
        input[used++]=c;
      }
      if(!valid){close(client);result=Code::read_failure;break;}
      const auto reply=service.handle(std::string_view(input,used));
      std::size_t sent=0;
      while(sent<reply.length){
        const auto n=send(client,reply.bytes.data()+sent,reply.length-sent,MSG_NOSIGNAL);
        if(n<=0)break;
        sent+=static_cast<std::size_t>(n);
      }
      close(client);
      if(sent!=reply.length){result=Code::write_failure;break;}
      ++served;
      if(reply.stop)break;
    }
    close(listener);unlink(endpoint_.c_str());
    return result;
  }
};
class NativeIpcClient {
public:
  static bool request(std::string_view endpoint,std::string_view command,
                      std::string& response) {
    if(!valid_endpoint(endpoint,false)||command.empty()||command.size()>120)return false;
    const int fd=socket(AF_UNIX,SOCK_STREAM,0);
    if(fd<0)return false;
    struct sockaddr_un addr{};addr.sun_family=AF_UNIX;
    std::memcpy(addr.sun_path,endpoint.data(),endpoint.size());
    bool good=connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0;
    const std::string packet=std::string(command)+"\n";
    if(good)good=send(fd,packet.data(),packet.size(),MSG_NOSIGNAL)==
                 static_cast<ssize_t>(packet.size());
    if(good){
      char buf[256]{};
      const auto n=recv(fd,buf,sizeof(buf),0);
      if(n>0)response.assign(buf,buf+n);
      good=n>0&&response.back()=='\n';
    }
    close(fd);return good;
  }
};
#endif
} // namespace anaiero::host
