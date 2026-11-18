// Real TLS sessions and adversarial clients. The CA and private keys are
// generated inside the test directory; no deployable credentials are shipped.
#include "mesh_test_credentials.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace mesh=anaiero::mesh;
namespace tls=mesh::tls;
using Code=mesh::TransportCode;
using namespace std::chrono_literals;
static constexpr std::uint32_t kDeadline=5000;

static bool tls_required() {
#ifdef _WIN32
  // An empty value still requires TLS; only absence permits a skip.
  ::SetLastError(ERROR_SUCCESS);
  return ::GetEnvironmentVariableA("ANAIERO_REQUIRE_MESH_TLS",nullptr,0)!=0||
    ::GetLastError()!=ERROR_ENVVAR_NOT_FOUND;
#else
  return std::getenv("ANAIERO_REQUIRE_MESH_TLS")!=nullptr;
#endif
}
static tls::detail::Socket tcp(const mesh::Endpoint& endpoint) {
  tls::detail::Address address{};CHECK(tls::detail::address(endpoint,address));
  const auto socket=tls::detail::make_socket(address.family);CHECK(socket!=tls::detail::kInvalidSocket);
  if(::connect(socket,reinterpret_cast<const sockaddr*>(&address.storage),address.length)!=0){
    CHECK(tls::detail::would_block(tls::detail::socket_error()));
    tls::detail::Wakeup wake;tls::detail::Deadline deadline(kDeadline);
    CHECK(tls::detail::wait_socket(socket,tls::detail::Interest::connect,deadline,wake)==Code::ok);
  }
  return socket;
}
// Deliberately bypasses only Anaiero framing to send malformed application
// records, absent identities and wrong ALPN through a real OpenSSL client.
class RawClient {
  SSL_CTX* context_=nullptr;SSL* ssl_=nullptr;
  tls::detail::Socket socket_=tls::detail::kInvalidSocket;
  tls::detail::Wakeup wake_;
  bool retry(int result,const tls::detail::Deadline& deadline) {
    const int error=SSL_get_error(ssl_,result);
    if(error!=SSL_ERROR_WANT_READ&&error!=SSL_ERROR_WANT_WRITE)return false;
    return tls::detail::wait_socket(socket_,error==SSL_ERROR_WANT_READ?
      tls::detail::Interest::read:tls::detail::Interest::write,deadline,wake_)==Code::ok;
  }
public:
  ~RawClient(){if(ssl_)SSL_free(ssl_);if(context_)SSL_CTX_free(context_);tls::detail::close_socket(socket_);}
  bool connect(const mesh::Endpoint& endpoint,const tls::Configuration& config,
               bool identity=true,bool protocol=true,bool tls12=false) {
    context_=SSL_CTX_new(TLS_client_method());CHECK(context_);
    CHECK(SSL_CTX_set_min_proto_version(context_,tls12?TLS1_2_VERSION:TLS1_3_VERSION)==1);
    CHECK(SSL_CTX_set_max_proto_version(context_,tls12?TLS1_2_VERSION:TLS1_3_VERSION)==1);
    SSL_CTX_set_verify(context_,SSL_VERIFY_PEER,nullptr);
    CHECK(SSL_CTX_load_verify_locations(context_,config.authority_file.c_str(),nullptr)==1);
    if(identity){
      CHECK(SSL_CTX_use_certificate_chain_file(context_,config.certificate_file.c_str())==1);
      CHECK(SSL_CTX_use_PrivateKey_file(context_,config.private_key_file.c_str(),SSL_FILETYPE_PEM)==1);
    }
    static constexpr unsigned char wrong[]={5,'w','r','o','n','g'};
    CHECK(SSL_CTX_set_alpn_protos(context_,protocol?tls::detail::kAlpn.data():wrong,
      protocol?static_cast<unsigned>(tls::detail::kAlpn.size()):static_cast<unsigned>(sizeof(wrong)))==0);
    socket_=tcp(endpoint);ssl_=SSL_new(context_);CHECK(ssl_);
    BIO* bio=BIO_new(tls::detail::socket_bio_method());CHECK(bio);
    BIO_set_data(bio,&socket_);SSL_set_bio(ssl_,bio,bio);SSL_set_connect_state(ssl_);
    const tls::detail::Deadline deadline(kDeadline);
    for(;;){ERR_clear_error();const int result=SSL_do_handshake(ssl_);
      if(result==1)return true;
      if(!retry(result,deadline))return false;}
  }
  bool write(std::span<const std::byte> bytes) {
    const tls::detail::Deadline deadline(kDeadline);std::size_t offset=0;
    while(offset<bytes.size()){
      std::size_t written=0;ERR_clear_error();
      const int result=SSL_write_ex(ssl_,bytes.data()+offset,bytes.size()-offset,&written);
      if(result==1){offset+=written;continue;}
      if(!retry(result,deadline))return false;
    }
    return true;
  }
};
static void exchange(tls::Context& server,tls::Context& client,const mesh::test::Credentials& credentials,
                     const std::string& address) {
  tls::Listener listener;CHECK(listener.open(server,{address,0})==Code::ok);
  std::thread owner([&]{
    tls::Connection peer;CHECK(listener.accept(peer,kDeadline)==Code::ok);
    CHECK(peer.peer().verified());CHECK(peer.peer().certificate_sha256()==credentials.client_pin);
    CHECK(peer.peer().certificate_expiry_unix_seconds()>1700000000);
    std::vector<std::byte> frame;
    CHECK(peer.read_frame(frame,kDeadline)==Code::ok&&frame.size()==mesh::kMaxTransportFrameBytes);
    CHECK(peer.write_frame(frame,kDeadline)==Code::ok);
    CHECK(peer.read_frame(frame,kDeadline)==Code::ok&&frame.size()==3);
    CHECK(peer.write_frame(frame,kDeadline)==Code::ok);
  });
  tls::Connection peer;peer.cancel(); // a fresh connect resets an old cancelled lifetime
  CHECK(peer.connect(client,listener.local_endpoint(),credentials.server_pin,kDeadline)==Code::ok);
  CHECK(peer.peer().verified()&&peer.peer().certificate_sha256()==credentials.server_pin);
  std::vector<std::byte> oversized(mesh::kMaxTransportFrameBytes+1);
  CHECK(peer.write_frame(oversized,kDeadline)==Code::frame_too_large);
  CHECK(peer.write_frame({},kDeadline)==Code::frame_too_large);
  std::vector<std::byte> frame(mesh::kMaxTransportFrameBytes);
  for(std::size_t i=0;i<frame.size();++i)frame[i]=std::byte(i&255);
  CHECK(peer.write_frame(frame,kDeadline)==Code::ok);
  std::vector<std::byte> output;CHECK(peer.read_frame(output,kDeadline)==Code::ok&&output==frame);
  frame={std::byte{1},std::byte{0},std::byte{255}};
  CHECK(peer.write_frame(frame,kDeadline)==Code::ok);
  CHECK(peer.read_frame(output,kDeadline)==Code::ok&&output==frame);
  peer.close();CHECK(!peer.peer().verified()&&peer.peer().certificate_sha256().empty());
  owner.join();
}
static void reject_identities(tls::Context& server,tls::Context& client,
                              const mesh::test::Credentials& credentials) {
  {
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code accepted=Code::invalid;
    std::thread owner([&]{tls::Connection peer;accepted=listener.accept(peer,kDeadline);});
    tls::Connection peer;
    CHECK(peer.connect(client,listener.local_endpoint(),std::string(64,'0'),kDeadline)==Code::unverified_peer);
    CHECK(!peer.peer().verified());owner.join();
    CHECK(accepted==Code::ok||accepted==Code::tls_error||accepted==Code::closed);
  }
  for(unsigned fault=0;fault<4;++fault){
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code accepted=Code::invalid;bool verified=true;
    std::thread owner([&]{tls::Connection peer;accepted=listener.accept(peer,kDeadline);verified=peer.peer().verified();});
    RawClient raw;
    const bool established=raw.connect(listener.local_endpoint(),fault==0?credentials.stranger:credentials.client,
                                       fault!=1,fault!=2,fault==3);
    (void)established;owner.join();
    CHECK(accepted!=Code::ok&&accepted!=Code::timeout&&!verified);
  }
  {
    auto config=credentials.client;
    config.authority_file=(std::filesystem::path(config.authority_file).parent_path()/"untrusted-ca.pem").string();
    tls::Context untrusted;CHECK(untrusted.open(config,tls::Role::client)==Code::ok);
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    std::thread owner([&]{tls::Connection peer;CHECK(listener.accept(peer,kDeadline)!=Code::ok);});
    tls::Connection peer;
    CHECK(peer.connect(untrusted,listener.local_endpoint(),credentials.server_pin,kDeadline)==Code::unverified_peer);
    owner.join();
  }
}
static void malformed_frames(tls::Context& server,const mesh::test::Credentials& credentials) {
  for(unsigned fault=0;fault<4;++fault){
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code received=Code::invalid;
    std::thread owner([&]{
      tls::Connection peer;CHECK(listener.accept(peer,kDeadline)==Code::ok);
      std::vector<std::byte> frame{std::byte{1}};received=peer.read_frame(frame,150);
      CHECK(frame.empty()&&!peer.peer().verified());
    });
    RawClient raw;CHECK(raw.connect(listener.local_endpoint(),credentials.client));
    std::vector<std::byte> bytes;
    if(fault==0)bytes={std::byte{0},std::byte{1},std::byte{0},std::byte{1}}; // 65537
    if(fault==1)bytes=std::vector<std::byte>(4); // empty frame is forbidden
    if(fault==2)bytes={std::byte{0},std::byte{0}}; // incomplete length
    if(fault==3)bytes={std::byte{0},std::byte{0},std::byte{0},std::byte{100},std::byte{1}};
    CHECK(raw.write(bytes));owner.join();
    CHECK(received==(fault<2?Code::frame_too_large:Code::timeout));
  }
}
static void deadlines_and_cancel(tls::Context& server,tls::Context& client,
                                 const mesh::test::Credentials& credentials) {
  { // TCP accepts in the kernel, but the server never starts its TLS handshake.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    tls::Connection peer;
    CHECK(peer.connect(client,listener.local_endpoint(),credentials.server_pin,150)==Code::timeout);
    CHECK(!peer.peer().verified());
  }
  { // A peer opens TCP but never starts TLS.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code result=Code::invalid;
    std::thread owner([&]{tls::Connection peer;result=listener.accept(peer,150);});
    const auto socket=tcp(listener.local_endpoint());owner.join();tls::detail::close_socket(socket);
    CHECK(result==Code::timeout);
  }
  { // An idle listener is woken by its cancellation event.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code result=Code::invalid;
    std::thread owner([&]{tls::Connection peer;result=listener.accept(peer,kDeadline);});
    std::this_thread::sleep_for(30ms);const auto started=std::chrono::steady_clock::now();
    listener.cancel();owner.join();
    CHECK(result==Code::cancelled&&std::chrono::steady_clock::now()-started<2s);
  }
  { // Listener cancellation also reaches a peer stalled inside its TLS handshake.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code result=Code::invalid;
    std::thread owner([&]{tls::Connection peer;result=listener.accept(peer,kDeadline);});
    const auto socket=tcp(listener.local_endpoint());std::this_thread::sleep_for(30ms);
    listener.cancel();owner.join();tls::detail::close_socket(socket);CHECK(result==Code::cancelled);
  }
  { // The active connection's event wakes SSL's read wait.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    tls::Connection incoming;std::promise<void> ready;Code result=Code::invalid;
    std::thread owner([&]{CHECK(listener.accept(incoming,kDeadline)==Code::ok);ready.set_value();
      std::vector<std::byte> frame;result=incoming.read_frame(frame,kDeadline);});
    tls::Connection peer;CHECK(peer.connect(client,listener.local_endpoint(),credentials.server_pin,kDeadline)==Code::ok);
    ready.get_future().wait();const auto started=std::chrono::steady_clock::now();incoming.cancel();owner.join();
    CHECK(result==Code::cancelled&&std::chrono::steady_clock::now()-started<2s);
  }
  { // A connected peer never drains application data. Cancellation interrupts backpressure.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    tls::Connection outgoing;std::promise<void> ready;Code result=Code::invalid;
    std::atomic<unsigned> writes=0;
    std::thread owner([&]{CHECK(listener.accept(outgoing,kDeadline)==Code::ok);ready.set_value();
      const std::vector<std::byte> frame(mesh::kMaxTransportFrameBytes,std::byte{7});
      result=Code::ok;
      for(unsigned i=0;i<512&&result==Code::ok;++i){result=outgoing.write_frame(frame,kDeadline);if(result==Code::ok)++writes;}
    });
    tls::Connection peer;CHECK(peer.connect(client,listener.local_endpoint(),credentials.server_pin,kDeadline)==Code::ok);
    ready.get_future().wait();std::this_thread::sleep_for(150ms);
    CHECK(writes.load()<512);outgoing.cancel();owner.join();CHECK(result==Code::cancelled);
  }
  { // The same blocked TLS write also obeys its absolute deadline without cancellation.
    tls::Listener listener;CHECK(listener.open(server,{"127.0.0.1",0})==Code::ok);
    Code result=Code::invalid;
    std::thread owner([&]{
      tls::Connection outgoing;CHECK(listener.accept(outgoing,kDeadline)==Code::ok);
      const std::vector<std::byte> frame(mesh::kMaxTransportFrameBytes,std::byte{8});result=Code::ok;
      for(unsigned i=0;i<512&&result==Code::ok;++i)result=outgoing.write_frame(frame,150);
      CHECK(!outgoing.peer().verified());
    });
    tls::Connection peer;CHECK(peer.connect(client,listener.local_endpoint(),credentials.server_pin,kDeadline)==Code::ok);
    owner.join();CHECK(result==Code::timeout);
  }
}
int main(int argc,char** argv) {
  CHECK(argc==2);const std::filesystem::path directory=argv[1];
  mesh::test::Credentials credentials;CHECK(mesh::test::create_credentials(directory,credentials));
  tls::Context server,client;
  CHECK(server.open(credentials.server,tls::Role::server)==Code::ok);
  CHECK(client.open(credentials.client,tls::Role::client)==Code::ok);
  CHECK(server.certificate_sha256()==credentials.server_pin);
  CHECK(server.certificate_expiry_unix_seconds()>1700000000);
  tls::Context missing;CHECK(missing.open({},tls::Role::client)==Code::invalid);
  auto wrong_key=credentials.client;wrong_key.private_key_file=credentials.server.private_key_file;
  CHECK(missing.open(wrong_key,tls::Role::client)==Code::tls_error);
  tls::Connection invalid;
  CHECK(invalid.connect(client,{"localhost",443},credentials.server_pin,10)==Code::invalid);
  CHECK(invalid.connect(client,{"127.0.0.1",443},"",10)==Code::invalid);
  CHECK(invalid.connect(client,{"127.0.0.1",443},credentials.server_pin,0)==Code::invalid);
  tls::Listener probe;const auto status=probe.open(server,{"127.0.0.1",0});
  if(status==Code::permission_denied){
    if(tls_required()){std::cerr<<"Required TCP/TLS unavailable\n";return 1;}
    std::cout<<"SKIP: environment denies native TCP sockets\n";return 77;
  }
  CHECK(status==Code::ok);probe.close();
  exchange(server,client,credentials,"127.0.0.1");
  exchange(server,client,credentials,"::1");
  reject_identities(server,client,credentials);
  malformed_frames(server,credentials);
  deadlines_and_cancel(server,client,credentials);
  std::cout<<"TLS1.3 mTLS, pinning, IPv4/IPv6, malformed frames, deadlines and cancellation passed\n";
  return 0;
}
