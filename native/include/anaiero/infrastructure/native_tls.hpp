#pragma once
// Optional native Windows/Linux TLS1.3 transport. OpenSSL 3 owns cryptography;
// operator enrollment and request authorization belong to separate services.
// Lifecycle/I/O are single-owner. cancel() alone is thread-safe; join the owner
// before destroying a connection/listener. Contexts outlive their listeners.
// Configure a Context before starting transport owners; do not reopen it until
// all listeners and connections using it have stopped.
#include "anaiero/contracts/mesh_transport.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#if OPENSSL_VERSION_NUMBER < 0x30000000L
#error "Anaiero mesh requires OpenSSL 3 or newer"
#endif

namespace anaiero::mesh::tls {
using mesh::Endpoint;
enum class Role : std::uint8_t {server,client};
struct Configuration {
  std::string certificate_file,private_key_file,authority_file;
};
namespace detail {
inline constexpr std::string_view kProtocol="anaiero-mesh/1";
inline constexpr std::array<unsigned char,15> kAlpn{{14,'a','n','a','i','e','r','o','-','m','e','s','h','/','1'}};
inline bool valid_timeout(std::uint32_t ms) noexcept {return ms>0&&ms<=60000;}
inline bool digest(std::string_view value) noexcept {
  if(value.size()!=64)return false;
  for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline std::string openssl_error(std::string_view prefix) {
  std::array<char,256> text{};
  const auto error=ERR_peek_last_error();
  if(error)ERR_error_string_n(error,text.data(),text.size());
  return std::string(prefix)+(error?": "+std::string(text.data()):"");
}
inline bool bounded_file(const std::string& path) noexcept {
  if(path.empty()||path.size()>4096||path.find('\0')!=std::string::npos)return false;
  std::error_code error;
  if(!std::filesystem::is_regular_file(path,error)||error)return false;
  const auto bytes=std::filesystem::file_size(path,error);
  return !error&&bytes>0&&bytes<=1024*1024;
}
inline bool certificate_identity(X509* certificate,std::string& fingerprint,
                                 std::uint64_t& expiry) noexcept {
  fingerprint.clear();expiry=0;
  if(!certificate)return false;
  std::array<unsigned char,EVP_MAX_MD_SIZE> bytes{};unsigned int length=0;
  if(X509_digest(certificate,EVP_sha256(),bytes.data(),&length)!=1||length!=32)return false;
  std::tm value{};
  if(ASN1_TIME_to_tm(X509_get0_notAfter(certificate),&value)!=1)return false;
  const int year=value.tm_year+1900;
  if(year<1970||year>9999||value.tm_hour<0||value.tm_hour>23||
     value.tm_min<0||value.tm_min>59||value.tm_sec<0||value.tm_sec>59)return false;
  const std::chrono::year_month_day date{std::chrono::year{year},
    std::chrono::month{static_cast<unsigned>(value.tm_mon+1)},
    std::chrono::day{static_cast<unsigned>(value.tm_mday)}};
  if(!date.ok())return false;
  const auto time=std::chrono::sys_seconds{std::chrono::sys_days{date}}+
    std::chrono::hours{value.tm_hour}+std::chrono::minutes{value.tm_min}+
    std::chrono::seconds{value.tm_sec};
  const auto seconds=time.time_since_epoch().count();
  if(seconds<=0)return false;
  expiry=static_cast<std::uint64_t>(seconds);
  static constexpr char hex[]="0123456789abcdef";
  fingerprint.resize(64);
  for(std::size_t i=0;i<32;++i){fingerprint[i*2]=hex[bytes[i]>>4];fingerprint[i*2+1]=hex[bytes[i]&15];}
  return true;
}
class Deadline {
  std::chrono::steady_clock::time_point end_;
public:
  explicit Deadline(std::uint32_t ms) noexcept:
    end_(std::chrono::steady_clock::now()+std::chrono::milliseconds(ms)){}
  int remaining_ms()const noexcept {
    const auto now=std::chrono::steady_clock::now();
    if(now>=end_)return 0;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_-now).count())+1;
  }
};
#ifdef _WIN32
using Socket=SOCKET;
inline constexpr Socket kInvalidSocket=INVALID_SOCKET;
inline int socket_error() noexcept{return WSAGetLastError();}
inline bool would_block(int error) noexcept {
  return error==WSAEWOULDBLOCK||error==WSAEINPROGRESS||error==WSAEALREADY||error==WSAEINTR;
}
inline bool permission_error(int error) noexcept {return error==WSAEACCES;}
inline void close_socket(Socket value) noexcept {if(value!=kInvalidSocket)closesocket(value);}
inline bool socket_runtime() noexcept {
  struct Runtime {bool ok=false;Runtime(){WSADATA value{};ok=WSAStartup(MAKEWORD(2,2),&value)==0;}
    ~Runtime(){if(ok)WSACleanup();}};
  static Runtime runtime;return runtime.ok;
}
inline bool prepare_socket(Socket socket) noexcept {
  u_long mode=1;return ioctlsocket(socket,static_cast<long>(FIONBIO),&mode)==0&&
    SetHandleInformation(reinterpret_cast<HANDLE>(socket),HANDLE_FLAG_INHERIT,0)!=0;
}
class Wakeup {
  HANDLE event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  std::atomic<bool> cancelled_{false};
public:
  ~Wakeup(){if(event_)CloseHandle(event_);}
  Wakeup()=default;Wakeup(const Wakeup&)=delete;Wakeup&operator=(const Wakeup&)=delete;
  bool valid()const noexcept{return event_!=nullptr;}
  HANDLE handle()const noexcept{return event_;}
  bool cancelled()const noexcept{return cancelled_.load(std::memory_order_acquire);}
  void cancel()noexcept {cancelled_.store(true,std::memory_order_release);if(event_)SetEvent(event_);}
  void reset()noexcept {if(event_)ResetEvent(event_);cancelled_.store(false,std::memory_order_release);}
};
#else
using Socket=int;
inline constexpr Socket kInvalidSocket=-1;
inline int socket_error() noexcept{return errno;}
inline bool would_block(int error) noexcept {
  return error==EAGAIN||error==EWOULDBLOCK||error==EINPROGRESS||error==EALREADY||error==EINTR;
}
inline bool permission_error(int error) noexcept {return error==EACCES||error==EPERM;}
inline void close_socket(Socket value) noexcept {if(value!=kInvalidSocket)::close(value);}
inline bool socket_runtime() noexcept {return true;}
inline bool prepare_socket(Socket socket) noexcept {
  const int mode=fcntl(socket,F_GETFL);const int flags=fcntl(socket,F_GETFD);
  return mode>=0&&flags>=0&&fcntl(socket,F_SETFL,mode|O_NONBLOCK)==0&&
    fcntl(socket,F_SETFD,flags|FD_CLOEXEC)==0;
}
class Wakeup {
  std::array<int,2> descriptors_{{-1,-1}};
  std::atomic<bool> cancelled_{false};
public:
  Wakeup() noexcept {
    int values[2];if(pipe2(values,O_NONBLOCK|O_CLOEXEC)==0)descriptors_={values[0],values[1]};
  }
  ~Wakeup(){for(int fd:descriptors_)if(fd>=0)::close(fd);}
  Wakeup(const Wakeup&)=delete;Wakeup&operator=(const Wakeup&)=delete;
  bool valid()const noexcept{return descriptors_[0]>=0;}
  int handle()const noexcept{return descriptors_[0];}
  bool cancelled()const noexcept{return cancelled_.load(std::memory_order_acquire);}
  void cancel()noexcept {
    cancelled_.store(true,std::memory_order_release);
    const char value=1;if(descriptors_[1]>=0){const auto ignored=::write(descriptors_[1],&value,1);(void)ignored;}
  }
  void reset()noexcept {
    std::array<char,64> buffer{};
    if(descriptors_[0]>=0)while(::read(descriptors_[0],buffer.data(),buffer.size())>0){}
    cancelled_.store(false,std::memory_order_release);
  }
};
#endif
enum class Interest {read,write,accept,connect};
inline TransportCode wait_socket(Socket socket,Interest interest,const Deadline& deadline,
                                  const Wakeup& wakeup) noexcept {
  if(wakeup.cancelled())return TransportCode::cancelled;
  const int remaining=deadline.remaining_ms();
  if(!remaining)return TransportCode::timeout;
#ifdef _WIN32
  const WSAEVENT ready=WSACreateEvent();
  if(ready==WSA_INVALID_EVENT)return TransportCode::io_error;
  const long mask=FD_CLOSE|(interest==Interest::read?FD_READ:
    interest==Interest::write?FD_WRITE:interest==Interest::accept?FD_ACCEPT:FD_CONNECT|FD_WRITE);
  if(WSAEventSelect(socket,ready,mask)!=0){WSACloseEvent(ready);return TransportCode::io_error;}
  const HANDLE events[2]={wakeup.handle(),ready};
  const DWORD result=WaitForMultipleObjects(2,events,FALSE,static_cast<DWORD>(remaining));
  WSAEventSelect(socket,nullptr,0);WSACloseEvent(ready);
  if(wakeup.cancelled()||result==WAIT_OBJECT_0)return TransportCode::cancelled;
  if(result==WAIT_TIMEOUT)return TransportCode::timeout;
  return result==WAIT_OBJECT_0+1?TransportCode::ok:TransportCode::io_error;
#else
  const short wanted=(interest==Interest::write||interest==Interest::connect)?POLLOUT:POLLIN;
  pollfd descriptors[2]={{socket,wanted,0},{wakeup.handle(),POLLIN,0}};
  for(;;){
    const int time=deadline.remaining_ms();if(!time)return TransportCode::timeout;
    const int result=::poll(descriptors,2,time);
    if(wakeup.cancelled()||(descriptors[1].revents&POLLIN))return TransportCode::cancelled;
    if(result<0&&errno==EINTR)continue;
    if(result<0)return TransportCode::io_error;
    if(!result)return TransportCode::timeout;
    if(descriptors[0].revents&POLLNVAL)return TransportCode::io_error;
    if(descriptors[0].revents&(wanted|POLLERR|POLLHUP))return TransportCode::ok;
  }
#endif
}
struct Address {sockaddr_storage storage{};int length=0,family=0;};
inline bool address(const Endpoint& endpoint,Address& out) noexcept {
  if(endpoint.address.empty()||endpoint.address.size()>INET6_ADDRSTRLEN||
     endpoint.address.find('\0')!=std::string::npos)return false;
  sockaddr_in v4{};v4.sin_family=AF_INET;v4.sin_port=htons(endpoint.port);
  if(inet_pton(AF_INET,endpoint.address.c_str(),&v4.sin_addr)==1){
    std::memcpy(&out.storage,&v4,sizeof(v4));out.length=sizeof(v4);out.family=AF_INET;return true;
  }
  sockaddr_in6 v6{};v6.sin6_family=AF_INET6;v6.sin6_port=htons(endpoint.port);
  if(inet_pton(AF_INET6,endpoint.address.c_str(),&v6.sin6_addr)==1){
    std::memcpy(&out.storage,&v6,sizeof(v6));out.length=sizeof(v6);out.family=AF_INET6;return true;
  }
  return false;
}
inline Socket make_socket(int family) noexcept {
  if(!socket_runtime())return kInvalidSocket;
  const auto result=::socket(family,SOCK_STREAM,IPPROTO_TCP);
  if(result!=kInvalidSocket&&!prepare_socket(result)){close_socket(result);return kInvalidSocket;}
  return result;
}
// Custom socket BIO preserves native SOCKET width on Windows and suppresses
// SIGPIPE locally on Linux; the library never changes process-wide signals.
inline BIO_METHOD* socket_bio_method() noexcept {
  struct Method {
    BIO_METHOD* value=BIO_meth_new(BIO_TYPE_SOURCE_SINK|BIO_get_new_index(),"Anaiero native socket");
    Method()noexcept {
      if(!value)return;
      BIO_meth_set_create(value,[](BIO* bio)->int{BIO_set_init(bio,1);BIO_set_data(bio,nullptr);return 1;});
      BIO_meth_set_destroy(value,[](BIO* bio)->int{if(bio)BIO_set_data(bio,nullptr);return 1;});
      BIO_meth_set_read(value,[](BIO* bio,char* output,int bytes)->int{
        BIO_clear_retry_flags(bio);
        const auto* socket=static_cast<const Socket*>(BIO_get_data(bio));
        if(!socket||!output||bytes<=0)return 0;
#ifdef _WIN32
        const int result=::recv(*socket,output,bytes,0);
#else
        const int result=static_cast<int>(::recv(*socket,output,static_cast<std::size_t>(bytes),0));
#endif
        if(result<0&&would_block(socket_error()))BIO_set_retry_read(bio);
        return result;
      });
      BIO_meth_set_write(value,[](BIO* bio,const char* input,int bytes)->int{
        BIO_clear_retry_flags(bio);
        const auto* socket=static_cast<const Socket*>(BIO_get_data(bio));
        if(!socket||!input||bytes<=0)return 0;
#ifdef _WIN32
        const int result=::send(*socket,input,bytes,0);
#else
        const int result=static_cast<int>(::send(*socket,input,static_cast<std::size_t>(bytes),MSG_NOSIGNAL));
#endif
        if(result<0&&would_block(socket_error()))BIO_set_retry_write(bio);
        return result;
      });
      BIO_meth_set_ctrl(value,[](BIO*,int command,long,void*)->long{
        return command==BIO_CTRL_FLUSH||command==BIO_CTRL_DUP?1:0;
      });
    }
    ~Method(){BIO_meth_free(value);}
  };
  static Method method;return method.value;
}
} // namespace detail

class Context {
  SSL_CTX* value_=nullptr;
  Role role_=Role::client;
  std::string fingerprint_,error_;
  std::uint64_t expiry_=0;
  friend class Connection;friend class Listener;
  static int alpn(SSL*,const unsigned char** selected,unsigned char* selected_size,
                  const unsigned char* offered,unsigned int offered_size,void*) noexcept {
    unsigned int offset=0;
    while(offset<offered_size){
      const auto size=static_cast<unsigned int>(offered[offset++]);
      if(size==0||size>offered_size-offset)return SSL_TLSEXT_ERR_ALERT_FATAL;
      if(size==detail::kProtocol.size()&&std::memcmp(offered+offset,detail::kProtocol.data(),size)==0){
        *selected=offered+offset;*selected_size=static_cast<unsigned char>(size);return SSL_TLSEXT_ERR_OK;
      }
      offset+=size;
    }
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }
  TransportCode fail(TransportCode code,std::string_view message) noexcept {
    error_=detail::openssl_error(message);if(value_)SSL_CTX_free(value_);value_=nullptr;
    fingerprint_.clear();expiry_=0;return code;
  }
public:
  Context()=default;~Context(){if(value_)SSL_CTX_free(value_);}
  Context(const Context&)=delete;Context&operator=(const Context&)=delete;
  TransportCode open(const Configuration& config,Role role) noexcept {
    if(value_)SSL_CTX_free(value_);
    value_=nullptr;fingerprint_.clear();expiry_=0;error_.clear();role_=role;
    ERR_clear_error();
    if((role!=Role::server&&role!=Role::client)||!detail::bounded_file(config.certificate_file)||!detail::bounded_file(config.private_key_file)||
       !detail::bounded_file(config.authority_file))return fail(TransportCode::invalid,"invalid or oversized TLS credential file");
    value_=SSL_CTX_new(TLS_method());
    if(!value_)return fail(TransportCode::tls_error,"TLS context allocation");
    if(SSL_CTX_set_min_proto_version(value_,TLS1_3_VERSION)!=1||
       SSL_CTX_set_max_proto_version(value_,TLS1_3_VERSION)!=1||
       SSL_CTX_set_max_early_data(value_,0)!=1||SSL_CTX_set_num_tickets(value_,0)!=1)
      return fail(TransportCode::tls_error,"TLS1.3 configuration");
    SSL_CTX_set_options(value_,SSL_OP_NO_TICKET|SSL_OP_NO_COMPRESSION|SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_session_cache_mode(value_,SSL_SESS_CACHE_OFF);
    SSL_CTX_set_verify(value_,SSL_VERIFY_PEER|(role==Role::server?SSL_VERIFY_FAIL_IF_NO_PEER_CERT:0),nullptr);
    SSL_CTX_set_verify_depth(value_,4);
    SSL_CTX_set_max_cert_list(value_,65536);
    SSL_CTX_set_default_passwd_cb(value_,[](char*,int,int,void*)->int{return 0;});
    if(SSL_CTX_load_verify_locations(value_,config.authority_file.c_str(),nullptr)!=1||
       SSL_CTX_use_certificate_chain_file(value_,config.certificate_file.c_str())!=1||
       SSL_CTX_use_PrivateKey_file(value_,config.private_key_file.c_str(),SSL_FILETYPE_PEM)!=1||
       SSL_CTX_check_private_key(value_)!=1)
      return fail(TransportCode::tls_error,"TLS credential or trust configuration");
    X509* local=SSL_CTX_get0_certificate(value_);
    if(!detail::certificate_identity(local,fingerprint_,expiry_)||
       X509_cmp_current_time(X509_get0_notBefore(local))>=0||
       X509_cmp_current_time(X509_get0_notAfter(local))<=0)
      return fail(TransportCode::unverified_peer,"local certificate is outside its validity interval");
    if(role==Role::server)SSL_CTX_set_alpn_select_cb(value_,alpn,nullptr);
    else if(SSL_CTX_set_alpn_protos(value_,detail::kAlpn.data(),static_cast<unsigned>(detail::kAlpn.size()))!=0)
      return fail(TransportCode::tls_error,"ALPN configuration");
    return TransportCode::ok;
  }
  bool valid()const noexcept{return value_!=nullptr;}
  Role role()const noexcept{return role_;}
  std::string_view certificate_sha256()const noexcept{return fingerprint_;}
  std::uint64_t certificate_expiry_unix_seconds()const noexcept{return expiry_;}
  const std::string& last_error()const noexcept{return error_;}
};

class Connection {
  class Peer final:public IAuthenticatedPeer {
    bool verified_=false;std::string fingerprint_;std::uint64_t expiry_=0;
    friend class Connection;
  public:
    bool verified()const noexcept override{return verified_;}
    std::string_view certificate_sha256()const noexcept override{return fingerprint_;}
    std::uint64_t certificate_expiry_unix_seconds()const noexcept override{return expiry_;}
  } peer_;
  detail::Socket socket_=detail::kInvalidSocket;
  SSL* ssl_=nullptr;
  detail::Wakeup wakeup_;
  std::string error_;
  friend class Listener;
  TransportCode fail(TransportCode code,std::string_view message) noexcept {
    error_=detail::openssl_error(message);close();return code;
  }
  TransportCode retry(int result,const detail::Deadline& deadline) noexcept {
    const int error=SSL_get_error(ssl_,result);
    if(error==SSL_ERROR_WANT_READ)return detail::wait_socket(socket_,detail::Interest::read,deadline,wakeup_);
    if(error==SSL_ERROR_WANT_WRITE)return detail::wait_socket(socket_,detail::Interest::write,deadline,wakeup_);
    if(error==SSL_ERROR_ZERO_RETURN)return TransportCode::closed;
    return SSL_get_verify_result(ssl_)!=X509_V_OK?TransportCode::unverified_peer:TransportCode::tls_error;
  }
  TransportCode handshake(Context& context,const detail::Deadline& deadline,std::string_view expected) noexcept {
    if(!wakeup_.valid())return fail(TransportCode::io_error,"cancellation event allocation");
    ERR_clear_error();ssl_=SSL_new(context.value_);
    BIO_METHOD* method=detail::socket_bio_method();
    BIO* bio=method?BIO_new(method):nullptr;
    if(!ssl_||!bio){if(bio)BIO_free(bio);return fail(TransportCode::tls_error,"TLS connection allocation");}
    BIO_set_data(bio,&socket_);SSL_set_bio(ssl_,bio,bio);
    if(context.role_==Role::server)SSL_set_accept_state(ssl_);else SSL_set_connect_state(ssl_);
    for(;;){
      if(wakeup_.cancelled())return fail(TransportCode::cancelled,"TLS handshake cancelled");
      if(!deadline.remaining_ms())return fail(TransportCode::timeout,"TLS handshake deadline");
      ERR_clear_error();const int result=SSL_do_handshake(ssl_);
      if(result==1)break;
      const auto status=retry(result,deadline);
      if(status!=TransportCode::ok)return fail(status,"TLS handshake failed");
    }
    if(SSL_version(ssl_)!=TLS1_3_VERSION||SSL_session_reused(ssl_)||
       SSL_get_verify_result(ssl_)!=X509_V_OK)return fail(TransportCode::unverified_peer,"TLS peer verification required");
    const unsigned char* protocol=nullptr;unsigned int protocol_size=0;
    SSL_get0_alpn_selected(ssl_,&protocol,&protocol_size);
    if(protocol_size!=detail::kProtocol.size()||
       std::memcmp(protocol,detail::kProtocol.data(),protocol_size)!=0)
      return fail(TransportCode::protocol_error,"mesh ALPN required");
    X509* certificate=SSL_get1_peer_certificate(ssl_);
    const bool identity=detail::certificate_identity(certificate,peer_.fingerprint_,peer_.expiry_);
    X509_free(certificate);
    if(!identity||(!expected.empty()&&peer_.fingerprint_!=expected))
      return fail(TransportCode::unverified_peer,"peer certificate pin mismatch");
    if(wakeup_.cancelled())return fail(TransportCode::cancelled,"TLS handshake cancelled");
    if(!deadline.remaining_ms())return fail(TransportCode::timeout,"TLS handshake deadline");
    peer_.verified_=true;return TransportCode::ok;
  }
  TransportCode transfer(bool reading,std::byte* bytes,std::size_t count,const detail::Deadline& deadline) noexcept {
    std::size_t offset=0;
    while(offset<count){
      if(wakeup_.cancelled())return TransportCode::cancelled;
      if(!deadline.remaining_ms())return TransportCode::timeout;
      std::size_t moved=0;ERR_clear_error();
      const int result=reading?SSL_read_ex(ssl_,bytes+offset,count-offset,&moved):
        SSL_write_ex(ssl_,bytes+offset,count-offset,&moved);
      if(result==1){
        if(moved==0)return TransportCode::io_error;
        offset+=moved;
        if(wakeup_.cancelled())return TransportCode::cancelled;
        if(!deadline.remaining_ms())return TransportCode::timeout;
        continue;
      }
      const auto status=retry(result,deadline);if(status!=TransportCode::ok)return status;
    }
    return TransportCode::ok;
  }
public:
  Connection()=default;~Connection(){close();}
  Connection(const Connection&)=delete;Connection&operator=(const Connection&)=delete;
  void close() noexcept {
    peer_.verified_=false;peer_.fingerprint_.clear();peer_.expiry_=0;
    if(ssl_)SSL_free(ssl_);
    ssl_=nullptr;detail::close_socket(socket_);socket_=detail::kInvalidSocket;
  }
  void cancel()noexcept {wakeup_.cancel();}
  const IAuthenticatedPeer& peer()const noexcept{return peer_;}
  const std::string& last_error()const noexcept{return error_;}
  bool connected()const noexcept{return ssl_&&peer_.verified_;}
  TransportCode connect(Context& context,const Endpoint& endpoint,std::string_view expected_server_sha256,
                        std::uint32_t timeout_ms) noexcept {
    close();wakeup_.reset();error_.clear();
    if(!context.valid()||context.role_!=Role::client||!endpoint.port||
       !detail::valid_timeout(timeout_ms)||!detail::digest(expected_server_sha256))
      return fail(TransportCode::invalid,"invalid TLS client configuration");
    detail::Address address{};
    if(!detail::address(endpoint,address))return fail(TransportCode::invalid,"numeric IP address required");
    const detail::Deadline deadline(timeout_ms);
    socket_=detail::make_socket(address.family);
    if(socket_==detail::kInvalidSocket)return fail(detail::permission_error(detail::socket_error())?
      TransportCode::permission_denied:TransportCode::connection_failed,"socket allocation failed");
    const int result=::connect(socket_,reinterpret_cast<const sockaddr*>(&address.storage),address.length);
    if(result!=0){
      if(!detail::would_block(detail::socket_error()))return fail(TransportCode::connection_failed,"TCP connection failed");
      const auto status=detail::wait_socket(socket_,detail::Interest::connect,deadline,wakeup_);
      if(status!=TransportCode::ok)return fail(status,"TCP connection deadline or cancellation");
      int error=0;
#ifdef _WIN32
      int length=sizeof(error);
      if(getsockopt(socket_,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length)!=0||error)
#else
      socklen_t length=sizeof(error);
      if(getsockopt(socket_,SOL_SOCKET,SO_ERROR,&error,&length)!=0||error)
#endif
        return fail(TransportCode::connection_failed,"TCP connection completion failed");
    }
    return handshake(context,deadline,expected_server_sha256);
  }
  TransportCode read_frame(std::vector<std::byte>& output,std::uint32_t timeout_ms) noexcept {
    output.clear();
    if(!connected())return TransportCode::closed;
    if(!detail::valid_timeout(timeout_ms))return TransportCode::invalid;
    const detail::Deadline deadline(timeout_ms);
    std::array<std::byte,4> prefix{};
    auto status=transfer(true,prefix.data(),prefix.size(),deadline);
    if(status!=TransportCode::ok)return fail(status,"TLS frame header read failed");
    std::uint32_t length=0;
    for(auto byte:prefix)length=(length<<8)|std::to_integer<std::uint32_t>(byte);
    if(!length||length>kMaxTransportFrameBytes)return fail(TransportCode::frame_too_large,"invalid TLS frame size");
    try{output.resize(length);}catch(...){return fail(TransportCode::io_error,"TLS frame allocation failed");}
    status=transfer(true,output.data(),output.size(),deadline);
    if(status!=TransportCode::ok){output.clear();return fail(status,"TLS frame body read failed");}
    return TransportCode::ok;
  }
  TransportCode write_frame(std::span<const std::byte> input,std::uint32_t timeout_ms) noexcept {
    if(!connected())return TransportCode::closed;
    if(!detail::valid_timeout(timeout_ms))return TransportCode::invalid;
    if(input.empty()||input.size()>kMaxTransportFrameBytes)return TransportCode::frame_too_large;
    const detail::Deadline deadline(timeout_ms);
    const auto count=static_cast<std::uint32_t>(input.size());
    std::array<std::byte,4> prefix{};
    for(unsigned i=0;i<4;++i)prefix[i]=std::byte((count>>(24-8*i))&255);
    auto status=transfer(false,prefix.data(),prefix.size(),deadline);
    if(status==TransportCode::ok)status=transfer(false,const_cast<std::byte*>(input.data()),input.size(),deadline);
    return status==TransportCode::ok?status:fail(status,"TLS frame write failed");
  }
};

class Listener {
  Context* context_=nullptr;
  detail::Socket socket_=detail::kInvalidSocket;
  detail::Wakeup wakeup_;
  Endpoint endpoint_{};
  std::mutex active_mutex_;
  Connection* accepting_=nullptr;
  std::string error_;
  TransportCode fail(TransportCode code,std::string_view message) noexcept {error_=std::string(message);close();return code;}
public:
  Listener()=default;~Listener(){close();}
  Listener(const Listener&)=delete;Listener&operator=(const Listener&)=delete;
  void close()noexcept {detail::close_socket(socket_);socket_=detail::kInvalidSocket;context_=nullptr;}
  void cancel()noexcept {
    wakeup_.cancel();std::lock_guard lock(active_mutex_);if(accepting_)accepting_->cancel();
  }
  const Endpoint& local_endpoint()const noexcept{return endpoint_;}
  const std::string& last_error()const noexcept{return error_;}
  TransportCode open(Context& context,const Endpoint& endpoint,unsigned backlog=8) noexcept {
    close();wakeup_.reset();error_.clear();
    if(!context.valid()||context.role_!=Role::server||!wakeup_.valid()||backlog==0||backlog>64)
      return fail(TransportCode::invalid,"invalid TLS listener configuration");
    detail::Address address{};
    if(!detail::address(endpoint,address))return fail(TransportCode::invalid,"numeric IP address required");
    socket_=detail::make_socket(address.family);
    if(socket_==detail::kInvalidSocket)return fail(detail::permission_error(detail::socket_error())?
      TransportCode::permission_denied:TransportCode::bind_failed,"listener socket allocation failed");
#ifdef _WIN32
    const int exclusive=1;
    if(setsockopt(socket_,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive))!=0)
      return fail(TransportCode::bind_failed,"exclusive listener setup failed");
#else
    const int reuse=1;
    if(setsockopt(socket_,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))!=0)
      return fail(TransportCode::bind_failed,"listener setup failed");
#endif
    if(::bind(socket_,reinterpret_cast<const sockaddr*>(&address.storage),address.length)!=0)
      return fail(detail::permission_error(detail::socket_error())?TransportCode::permission_denied:
        TransportCode::bind_failed,"TCP bind failed");
    if(::listen(socket_,static_cast<int>(backlog))!=0)return fail(TransportCode::bind_failed,"TCP listen failed");
    sockaddr_storage actual{};
#ifdef _WIN32
    int actual_size=sizeof(actual);
#else
    socklen_t actual_size=sizeof(actual);
#endif
    if(getsockname(socket_,reinterpret_cast<sockaddr*>(&actual),&actual_size)!=0)
      return fail(TransportCode::bind_failed,"TCP endpoint inspection failed");
    endpoint_=endpoint;endpoint_.port=address.family==AF_INET?
      ntohs(reinterpret_cast<const sockaddr_in*>(&actual)->sin_port):
      ntohs(reinterpret_cast<const sockaddr_in6*>(&actual)->sin6_port);
    context_=&context;return TransportCode::ok;
  }
  TransportCode accept(Connection& output,std::uint32_t timeout_ms) noexcept {
    if(socket_==detail::kInvalidSocket||!context_||!detail::valid_timeout(timeout_ms))return TransportCode::invalid;
    output.close();output.wakeup_.reset();output.error_.clear();
    const detail::Deadline deadline(timeout_ms);
    detail::Socket accepted=detail::kInvalidSocket;
    for(;;){
      if(wakeup_.cancelled())return TransportCode::cancelled;
      if(!deadline.remaining_ms())return TransportCode::timeout;
      accepted=::accept(socket_,nullptr,nullptr);
      if(accepted!=detail::kInvalidSocket)break;
      if(!detail::would_block(detail::socket_error()))return TransportCode::io_error;
      const auto status=detail::wait_socket(socket_,detail::Interest::accept,deadline,wakeup_);
      if(status!=TransportCode::ok)return status;
    }
    if(!detail::prepare_socket(accepted)){detail::close_socket(accepted);return TransportCode::io_error;}
    output.socket_=accepted;
    {std::lock_guard lock(active_mutex_);accepting_=&output;if(wakeup_.cancelled())output.cancel();}
    const auto result=output.handshake(*context_,deadline,{});
    {std::lock_guard lock(active_mutex_);accepting_=nullptr;}
    if(result!=TransportCode::ok)error_=output.last_error();
    return result;
  }
};
} // namespace anaiero::mesh::tls
