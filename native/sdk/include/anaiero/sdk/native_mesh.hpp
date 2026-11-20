#pragma once
#include "anaiero/sdk/host.hpp"
#include "anaiero/sdk/mesh.hpp"

namespace anaiero::sdk::mesh {
struct NativeCredentials {std::string certificate_file,private_key_file,authority_file;};
struct AcknowledgementTiming {
  std::uint64_t elapsed_ns=0,subscription_generation=0,to_sequence=0;
};
// Borrowed bounded observation port, called on authenticated client workers.
// Calls can overlap; implementations must be thread safe, nonblocking and must
// not reenter transport or canonical storage. Elapsed time spans offered-page
// write through durable acceptance of its exact ACK, including receiver work.
class IAcknowledgementObserver {
public:
  virtual ~IAcknowledgementObserver()=default;
  virtual void observe(const AcknowledgementTiming&) noexcept=0;
};
struct NativeServerOptions {
  NativeCredentials credentials;
  std::string address="127.0.0.1";
  std::uint16_t port=0;
  std::size_t maximum_clients=8,maximum_handshakes=4;
  std::uint32_t handshake_timeout_ms=5000,io_timeout_ms=5000;
  Options service;
  IAcknowledgementObserver* acknowledgement_observer=nullptr;
};
struct NativeServerStatus {
  std::uint64_t accepted_connections=0,rejected_connections=0,delivered_pages=0,
                acknowledged_pages=0,delivered_bytes=0,charged_read_bytes=0,
                owner_wakeups=0,availability_generation=0;
  std::size_t handshakes=0,clients=0,queued_work=0;
  bool running=false;
  std::string address;
  std::uint16_t port=0;
};
// Storage tasks execute through the host's bounded lane, after in-flight engine
// commits. Transport workers never touch canonical state directly. notify_commit
// is a coalescible signal, safe on the host's signal-only completion callback.
class NativeServer {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  NativeServer(host::IStoreTaskExecutor&,NativeServerOptions);
  ~NativeServer();
  Result start();
  void notify_commit(std::uint64_t generation) noexcept;
  NativeServerStatus status() const;
  void request_stop() noexcept;
  void stop() noexcept;
};
Result read_native_server_options(std::string_view path,NativeServerOptions&);
// Local IPC/operator adapter only. The remote TLS protocol has no admin verb.
Result administer_native(host::IStoreTaskExecutor&,std::string_view command,Bytes& response);
Result submit_native_administration(host::IStoreTaskExecutor&,std::string command,
                                    std::function<void(Result,Bytes)> completion);

struct NativeSubscriberOptions {
  NativeCredentials credentials;
  std::string address="127.0.0.1",local_principal,remote_principal;
  std::uint16_t port=0;
  Digest server_certificate_sha256;
  lifecycle::Subscription subscription;
  std::uint32_t connect_timeout_ms=5000,io_timeout_ms=5000;
  std::uint32_t minimum_retry_ms=100,maximum_retry_ms=5000;
  // Zero runs until stopped. A finite target supports reproducible qualification.
  std::uint64_t through_sequence=0;
  Options service;
};
struct NativeSubscriberStatus {
  std::uint64_t connections=0,reconnects=0,committed_pages=0,committed_bytes=0;
  lifecycle::Cursor cursor;
  bool running=false,blocked=false,completed=false;
  Result result;
};
class NativeSubscriber {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  NativeSubscriber(host::IStoreTaskExecutor&,NativeSubscriberOptions);
  ~NativeSubscriber();
  Result start();
  NativeSubscriberStatus status() const;
  Result wait(std::uint32_t timeout_ms) const;
  void request_stop() noexcept;
  void stop() noexcept;
};
// Finite artifact/bootstrap transfers use independent authenticated connections
// and the same bounded scheduler. Every chunk is reauthorized before handoff.
Result fetch_native_artifact(host::IStoreTaskExecutor&,const NativeSubscriberOptions&,
                            std::string_view digest,ReceiveReceipt&);
Result install_native_bootstrap(host::IStoreTaskExecutor&,const NativeSubscriberOptions&,
                               std::uint64_t next_generation,ReceiveReceipt&);
}
