#pragma once
// Native process lifecycle around the local server. Business and engine work
// remain in injected services. No service installation is performed here.
#include "anaiero/infrastructure/native_ipc.hpp"
#include <cstdlib>
#include <csignal>
#include <mutex>
#include <string>

namespace anaiero::host {
struct NativeLifecycleOptions {
  bool windows_service=false;
  std::string service_name="Anaiero";
  // Called once on the lifecycle owner after serve() finishes; may join/drain
  // the session worker. Never called from an OS signal/control handler.
  void (*on_stop)(void*) noexcept=nullptr;
  void* stop_context=nullptr;
  void (*on_ready)(void*) noexcept=nullptr;
  void* ready_context=nullptr;
};
class NativeHostLifecycle {
  struct Context {
    NativeIpcServer& server;
    ICommandService& commands;
    const NativeLifecycleOptions& options;
    Code result=Code::interrupted;
#ifdef _WIN32
    SERVICE_STATUS_HANDLE status_handle=nullptr;
    std::wstring service_name{};
#endif
  };
  inline static std::mutex context_mutex_{};
  inline static Context* active_=nullptr;
#ifndef _WIN32
  inline static volatile std::sig_atomic_t signal_fd_=-1;
  static void signal_handler(int) noexcept {
    const int previous_errno=errno;
    const int fd=signal_fd_;
    if(fd>=0){const char stop=1;const auto ignored=write(fd,&stop,1);(void)ignored;}
    errno=previous_errno;
  }
  static void systemd_notify(std::string_view message) noexcept {
    const char* socket_name=std::getenv("NOTIFY_SOCKET");
    if(!socket_name||!socket_name[0])return;
    const std::string_view name(socket_name);
    if(name.size()>=sizeof(sockaddr_un::sun_path))return;
    native_io::Descriptor descriptor(socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));
    if(descriptor.value<0)return;
    sockaddr_un target{};target.sun_family=AF_UNIX;
    std::memcpy(target.sun_path,name.data(),name.size());
    socklen_t length=static_cast<socklen_t>(offsetof(sockaddr_un,sun_path)+name.size()+1);
    if(name.front()=='@'){
      target.sun_path[0]='\0';
      length=static_cast<socklen_t>(offsetof(sockaddr_un,sun_path)+name.size());
    }else if(name.front()!='/')return;
    const auto ignored=sendto(descriptor.value,message.data(),message.size(),MSG_NOSIGNAL,
      reinterpret_cast<const sockaddr*>(&target),length);
    (void)ignored;
  }
#else
  static void report(Context& context,DWORD state,DWORD exit_code=NO_ERROR) noexcept {
    if(!context.status_handle)return;
    SERVICE_STATUS status{};
    status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState=state;
    status.dwControlsAccepted=state==SERVICE_RUNNING?
      SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN:0;
    status.dwWin32ExitCode=exit_code;
    status.dwCheckPoint=(state==SERVICE_START_PENDING||state==SERVICE_STOP_PENDING)?1:0;
    status.dwWaitHint=status.dwCheckPoint?10000:0;
    if(exit_code==ERROR_SERVICE_SPECIFIC_ERROR)
      status.dwServiceSpecificExitCode=static_cast<DWORD>(context.result);
    SetServiceStatus(context.status_handle,&status);
  }
  static DWORD WINAPI service_control(DWORD control,DWORD,void*,void*) noexcept {
    std::lock_guard<std::mutex> lock(context_mutex_);
    if(!active_)return ERROR_SERVICE_NOT_ACTIVE;
    if(control==SERVICE_CONTROL_STOP||control==SERVICE_CONTROL_SHUTDOWN){
      report(*active_,SERVICE_STOP_PENDING);
      active_->server.request_stop();
      return NO_ERROR;
    }
    if(control==SERVICE_CONTROL_INTERROGATE)return NO_ERROR;
    return ERROR_CALL_NOT_IMPLEMENTED;
  }
  static BOOL WINAPI console_control(DWORD control) noexcept {
    if(control!=CTRL_C_EVENT&&control!=CTRL_BREAK_EVENT&&control!=CTRL_CLOSE_EVENT&&
       control!=CTRL_SHUTDOWN_EVENT&&control!=CTRL_LOGOFF_EVENT)return FALSE;
    std::lock_guard<std::mutex> lock(context_mutex_);
    if(!active_)return FALSE;
    active_->server.request_stop();return TRUE;
  }
  static void WINAPI service_main(DWORD,LPWSTR*) noexcept {
    Context* context=nullptr;
    {std::lock_guard<std::mutex> lock(context_mutex_);context=active_;}
    if(!context)return;
    context->status_handle=RegisterServiceCtrlHandlerExW(
      context->service_name.c_str(),service_control,nullptr);
    if(!context->status_handle){context->result=Code::bind_failure;return;}
    report(*context,SERVICE_START_PENDING);
    context->result=context->server.serve(context->commands);
    report(*context,SERVICE_STOP_PENDING);
    if(context->options.on_stop)context->options.on_stop(context->options.stop_context);
    report(*context,SERVICE_STOPPED,
      context->result==Code::ok||context->result==Code::interrupted?
        NO_ERROR:ERROR_SERVICE_SPECIFIC_ERROR);
  }
#endif
  static void ready(void* opaque) noexcept {
    auto& context=*static_cast<Context*>(opaque);
#ifdef _WIN32
    if(context.options.windows_service){
      std::lock_guard<std::mutex> lock(context_mutex_);
      if(!context.server.stopping())report(context,SERVICE_RUNNING);
    }
#else
    systemd_notify("READY=1\nSTATUS=Anaiero native controller is accepting local commands");
#endif
    if(context.options.on_ready)context.options.on_ready(context.options.ready_context);
  }
public:
  static Code run(NativeIpcServer& server,ICommandService& commands,
                  const NativeLifecycleOptions& options={}) noexcept {
    Context context{server,commands,options};
    {
      std::lock_guard<std::mutex> lock(context_mutex_);
      if(active_)return Code::invalid; // One process signal/SCM owner.
      active_=&context;
    }
    server.set_ready_callback(ready,&context);
#ifdef _WIN32
    if(options.windows_service){
      bool valid=!options.service_name.empty()&&options.service_name.size()<=128;
      for(unsigned char c:options.service_name)
        if(c<32||c>126||c=='/'||c=='\\')valid=false;
      if(valid){
        context.service_name.assign(options.service_name.begin(),options.service_name.end());
        SERVICE_TABLE_ENTRYW table[2]={{context.service_name.data(),service_main},{nullptr,nullptr}};
        if(!StartServiceCtrlDispatcherW(table))context.result=Code::bind_failure;
      }else context.result=Code::invalid;
      // service_main owns shutdown when registration succeeded.
      if(!context.status_handle&&options.on_stop)options.on_stop(options.stop_context);
    }else{
      if(SetConsoleCtrlHandler(console_control,TRUE)){
        context.result=server.serve(commands);
        SetConsoleCtrlHandler(console_control,FALSE);
      }else context.result=Code::bind_failure;
      if(options.on_stop)options.on_stop(options.stop_context);
    }
#else
    if(options.windows_service)context.result=Code::invalid;
    else{
      struct sigaction action{},old_int{},old_term{};
      action.sa_handler=signal_handler;
      sigemptyset(&action.sa_mask);
      signal_fd_=server.signal_stop_fd();
      const bool int_set=sigaction(SIGINT,&action,&old_int)==0;
      const bool term_set=int_set&&sigaction(SIGTERM,&action,&old_term)==0;
      if(term_set)context.result=server.serve(commands);
      else context.result=Code::bind_failure;
      signal_fd_=-1;
      if(term_set)sigaction(SIGTERM,&old_term,nullptr);
      if(int_set)sigaction(SIGINT,&old_int,nullptr);
    }
    systemd_notify("STOPPING=1\nSTATUS=Anaiero is draining accepted native work");
    if(options.on_stop)options.on_stop(options.stop_context);
#endif
    {
      std::lock_guard<std::mutex> lock(context_mutex_);
      active_=nullptr;
    }
    server.set_ready_callback(nullptr,nullptr);
    return context.result;
  }
};
} // namespace anaiero::host
