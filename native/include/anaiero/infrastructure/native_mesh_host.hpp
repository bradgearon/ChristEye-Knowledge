#pragma once
// Bounded native TLS clients feed a single application owner. The transport
// workers never read a Registry, mutate a Projector or execute gateway policy.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "anaiero/infrastructure/native_tls.hpp"
#include "anaiero/contracts/mesh_host.hpp"
#include "anaiero/contracts/host_runtime.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace anaiero::mesh {
class NativeMeshHost {
  enum class Kind {open,request,close,admin};
  struct Job {
    Kind kind=Kind::request;const IAuthenticatedPeer* peer=nullptr;
    std::uint64_t session=0;std::string input,output;bool accepted=false,done=false;
    std::mutex mutex;std::condition_variable completed;
  };
  struct Client {
    tls::Connection connection;std::thread worker;
    bool occupied=false,finished=false;
  };
  class Administration final:public host::ICommandService {
    NativeMeshHost& owner_;
  public:
    explicit Administration(NativeMeshHost& owner):owner_(owner){}
    host::Reply handle(std::string_view command)noexcept override {return owner_.administer(command);}
  } administration_{*this};
  IMeshGateway& gateway_;tls::Context& context_;Endpoint endpoint_;
  std::uint32_t deadline_ms_;
  tls::Listener listener_;
  std::array<Client,kMeshHostClientCapacity> clients_{};
  std::thread owner_,acceptor_;
  std::atomic<bool> started_{false},stopping_{false};
  std::mutex lifecycle_mutex_,clients_mutex_,queue_mutex_;
  std::condition_variable clients_changed_,queue_ready_,queue_space_;
  std::array<std::shared_ptr<Job>,kMeshHostQueueCapacity> queue_{};
  std::size_t queue_head_=0,queue_count_=0;
  bool owner_stopping_=false;
  static constexpr std::string_view kReady="CEVMESH/1 READY\n";
  static constexpr std::string_view kBusy="CEVMESH/1 ERROR busy\n";
  static constexpr std::string_view kStopped="CEVMESH/1 ERROR stopped\n";
  static constexpr std::string_view kFailure="CEVMESH/1 ERROR internal\n";
  static std::span<const std::byte> bytes(std::string_view text) noexcept {
    return std::as_bytes(std::span(text.data(),text.size()));
  }
  bool submit(const std::shared_ptr<Job>& job) {
    {
      std::unique_lock lock(queue_mutex_);
      if(job->kind==Kind::close){
        // A close releases an already admitted handle. It must reach the owner
        // even while shutdown cancels ordinary admission.
        queue_space_.wait(lock,[&]{return owner_stopping_||queue_count_<queue_.size();});
      }
      if(owner_stopping_||(!started_.load(std::memory_order_acquire))||
         (job->kind!=Kind::close&&stopping_.load(std::memory_order_acquire))||queue_count_==queue_.size())return false;
      queue_[(queue_head_+queue_count_)%queue_.size()]=job;++queue_count_;
    }
    queue_ready_.notify_one();
    std::unique_lock lock(job->mutex);job->completed.wait(lock,[&]{return job->done;});
    return true;
  }
  void owner_loop() noexcept {
    for(;;){
      std::shared_ptr<Job> job;
      {
        std::unique_lock lock(queue_mutex_);
        queue_ready_.wait(lock,[&]{return owner_stopping_||queue_count_!=0;});
        if(!queue_count_&&owner_stopping_)break;
        job=std::move(queue_[queue_head_]);queue_head_=(queue_head_+1)%queue_.size();--queue_count_;
      }
      queue_space_.notify_all();
      try{
        switch(job->kind){
          case Kind::open:
            job->accepted=job->peer&&gateway_.open_peer(*job->peer,job->session);
            if(job->accepted&&job->session==0)job->accepted=false;
            break;
          case Kind::request:job->output=gateway_.request(job->session,job->input);break;
          case Kind::close:gateway_.close_peer(job->session);break;
          case Kind::admin:job->output=gateway_.administer(job->input);break;
        }
      }catch(...){job->accepted=false;job->output=std::string(kFailure);}
      {
        std::lock_guard lock(job->mutex);job->done=true;
      }
      job->completed.notify_one();
    }
  }
  void release_peer(std::uint64_t session) noexcept {
    if(!session)return;
    try{auto job=std::make_shared<Job>();job->kind=Kind::close;job->session=session;(void)submit(job);}catch(...){}
  }
  void client_loop(std::size_t index) noexcept {
    auto& client=clients_[index];std::uint64_t session=0;
    try{
      auto open=std::make_shared<Job>();open->kind=Kind::open;open->peer=&client.connection.peer();
      if(submit(open)&&open->accepted){
        session=open->session;
        if(client.connection.write_frame(bytes(kReady),deadline_ms_)==TransportCode::ok){
          std::vector<std::byte> frame;
          while(!stopping_.load(std::memory_order_acquire)&&
                client.connection.read_frame(frame,deadline_ms_)==TransportCode::ok){
            auto request=std::make_shared<Job>();request->kind=Kind::request;request->session=session;
            request->input.assign(reinterpret_cast<const char*>(frame.data()),frame.size());
            const bool admitted=submit(request);
            const std::string_view response=admitted?std::string_view(request->output):
              stopping_.load(std::memory_order_acquire)?kStopped:kBusy;
            if(response.empty()||response.size()>kMaxTransportFrameBytes){
              (void)client.connection.write_frame(bytes(kFailure),deadline_ms_);break;
            }
            if(client.connection.write_frame(bytes(response),deadline_ms_)!=TransportCode::ok||!admitted||
               response=="CEVMESH/1 ERROR denied\n")break;
          }
        }
      }
    }catch(...){}
    release_peer(session);client.connection.close();
    {std::lock_guard lock(clients_mutex_);client.finished=true;}
    clients_changed_.notify_one();
  }
  bool available_client() const noexcept {
    for(const auto& client:clients_)if(!client.occupied||client.finished)return true;
    return false;
  }
  void accept_loop() noexcept {
    while(!stopping_.load(std::memory_order_acquire)){
      std::size_t index=clients_.size();
      {
        std::unique_lock lock(clients_mutex_);
        clients_changed_.wait(lock,[&]{return stopping_.load(std::memory_order_acquire)||available_client();});
        if(stopping_.load(std::memory_order_acquire))break;
        for(std::size_t i=0;i<clients_.size();++i)if(!clients_[i].occupied||clients_[i].finished){index=i;break;}
        if(index==clients_.size())continue;
        clients_[index].occupied=true;clients_[index].finished=false;
      }
      auto& client=clients_[index];
      if(client.worker.joinable())client.worker.join();
      const auto result=listener_.accept(client.connection,deadline_ms_);
      if(result==TransportCode::ok&&!stopping_.load(std::memory_order_acquire)){
        try{client.worker=std::thread([this,index]{client_loop(index);});continue;}catch(...){}
      }
      client.connection.close();
      {std::lock_guard lock(clients_mutex_);client.occupied=false;}
      if(result==TransportCode::cancelled||stopping_.load(std::memory_order_acquire))break;
      if(result==TransportCode::io_error||result==TransportCode::invalid){request_stop();break;}
      // TLS/authentication faults affect their connection only. The next
      // accept waits on the socket event; there is no polling of store state.
    }
  }
  host::Reply administer(std::string_view command) noexcept {
    host::Reply reply{};std::string output;
    if(command.empty()||command.size()>host::kMaxRequestBytes)output="ERROR invalid command\n";
    else {
      try{
        auto job=std::make_shared<Job>();job->kind=Kind::admin;job->input=command;
        output=submit(job)?std::move(job->output):std::string(stopping_.load()?kStopped:kBusy);
        // The local IPC host sends this reply before invoking external native
        // lifecycle shutdown. Do not cancel its socket from the owner thread.
        reply.stop=command=="STOP"&&job->done;
      }catch(...){output=std::string(kFailure);}
    }
    if(output.empty())output=std::string(kFailure);
    if(output.size()>reply.bytes.size())output="ERROR response too large\n";
    std::memcpy(reply.bytes.data(),output.data(),output.size());reply.length=output.size();return reply;
  }
public:
  NativeMeshHost(IMeshGateway& gateway,tls::Context& context,Endpoint endpoint,
                 std::uint32_t deadline_ms=5000):
    gateway_(gateway),context_(context),endpoint_(std::move(endpoint)),deadline_ms_(deadline_ms){}
  ~NativeMeshHost(){stop();}
  NativeMeshHost(const NativeMeshHost&)=delete;NativeMeshHost&operator=(const NativeMeshHost&)=delete;
  TransportCode start() noexcept {
    std::lock_guard lifecycle(lifecycle_mutex_);
    if(started_.load(std::memory_order_acquire))return TransportCode::invalid;
    if(!tls::detail::valid_timeout(deadline_ms_))return TransportCode::invalid;
    const auto result=listener_.open(context_,endpoint_);if(result!=TransportCode::ok)return result;
    endpoint_=listener_.local_endpoint();stopping_.store(false,std::memory_order_release);
    {std::lock_guard lock(queue_mutex_);owner_stopping_=false;queue_head_=0;queue_count_=0;}
    for(auto& client:clients_){client.occupied=false;client.finished=false;}
    try{
      owner_=std::thread([this]{owner_loop();});started_.store(true,std::memory_order_release);
      acceptor_=std::thread([this]{accept_loop();});
    }
    catch(...){
      request_stop();
      {std::lock_guard lock(queue_mutex_);owner_stopping_=true;}
      queue_ready_.notify_all();if(owner_.joinable())owner_.join();listener_.close();
      started_.store(false,std::memory_order_release);return TransportCode::io_error;
    }
    return TransportCode::ok;
  }
  void request_stop() noexcept {
    stopping_.store(true,std::memory_order_release);listener_.cancel();
    for(auto& client:clients_)client.connection.cancel();
    clients_changed_.notify_all();queue_space_.notify_all();
  }
  void stop() noexcept {
    std::lock_guard lifecycle(lifecycle_mutex_);
    if(!started_.load(std::memory_order_acquire))return;
    request_stop();if(acceptor_.joinable())acceptor_.join();
    for(auto& client:clients_)if(client.worker.joinable())client.worker.join();
    {std::lock_guard lock(queue_mutex_);owner_stopping_=true;}
    queue_ready_.notify_all();queue_space_.notify_all();if(owner_.joinable())owner_.join();
    listener_.close();for(auto& client:clients_)client.connection.close();
    started_.store(false,std::memory_order_release);
  }
  const Endpoint& endpoint()const noexcept{return endpoint_;}
  bool running()const noexcept{return started_.load(std::memory_order_acquire)&&!stopping_.load(std::memory_order_acquire);}
  host::ICommandService& administration()noexcept{return administration_;}
};
} // namespace anaiero::mesh
