#pragma once
#include "anaiero/infrastructure/native_tls.hpp"
#include "anaiero/application/native_mesh_application.hpp"
#include <array>
#include <span>
#include <sstream>
#include <string>
#include <vector>
namespace anaiero::application {
struct MeshTransferStats {std::uint64_t records=0,duplicates=0,pages=0,bytes=0;};
// Local import proves possession of the configured private key and matches an
// operator enrollment. Its receipts are explicitly labeled local_import.
class LocalMeshIdentity final:public mesh::IAuthenticatedPeer {
  const mesh::tls::Context& context_;
public:
  explicit LocalMeshIdentity(const mesh::tls::Context& context)noexcept:context_(context){}
  bool verified()const noexcept override{return context_.valid();}
  std::string_view certificate_sha256()const noexcept override{return context_.certificate_sha256();}
  std::uint64_t certificate_expiry_unix_seconds()const noexcept override{return context_.certificate_expiry_unix_seconds();}
};
class NativeMeshClient final {
  NativeMeshApplication& application_;
  mesh::tls::Context context_;
  mesh::tls::Connection connection_;
  mesh::VerifiedMeshSession remote_;
  std::string error_;
  std::uint32_t timeout_=5000;
  void disconnect()noexcept {
    const auto peer=remote_.authorized_peer();
    if(peer.generation){application_.replay().disconnect(peer);application_.artifact_receiver().cancel(peer);}
    remote_={};connection_.close();
  }
  bool fail(std::string message){error_=std::move(message);disconnect();return false;}
  bool exchange(const mesh::wire::Request& request,std::string& response){
    if(!application_.authorization().active(remote_))return fail("peer authorization expired or changed");
    const auto bytes=mesh::wire::encode(request);if(bytes.empty())return fail("invalid mesh request");
    auto code=connection_.write_frame(std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()),timeout_);
    std::vector<std::byte> received;
    if(code==mesh::TransportCode::ok)code=connection_.read_frame(received,timeout_);
    if(code!=mesh::TransportCode::ok)return fail(std::string("mesh exchange: ")+mesh::transport_code_name(code));
    response.assign(reinterpret_cast<const char*>(received.data()),received.size());
    if(response.starts_with("CEVMESH/1 ERROR "))return fail(response);
    return true;
  }
  bool published(std::string_view response,const runtime::Observation& observation,bool& duplicate){
    std::istringstream in{std::string(response)};std::string magic,type,hash,receipt,payload,extra;std::uint64_t sequence=0;unsigned repeated=0;
    if(!(in>>magic>>type>>sequence>>hash>>receipt>>payload>>repeated)||magic!="CEVMESH/1"||type!="PERSISTED"||
      !sequence||!mesh::mesh_digest(hash)||!mesh::mesh_digest(receipt)||!mesh::mesh_digest(payload)||repeated>1||(in>>extra))return false;
    std::array<std::byte,runtime::kEpochObservationFrame> bytes{};runtime::EpochObservationCodec codec;
    if(!codec.encode(observation,bytes)||payload!=christeye::living::plans::sha256_hex(
      std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size())))return false;
    const auto canonical="CEVMESH/1 PERSISTED "+std::to_string(sequence)+' '+hash+' '+receipt+' '+payload+' '+std::to_string(repeated)+'\n';
    duplicate=repeated!=0;return response==canonical;
  }
public:
  explicit NativeMeshClient(NativeMeshApplication& application)noexcept:application_(application){}
  ~NativeMeshClient(){disconnect();}
  const std::string& error()const noexcept{return error_;}
  const mesh::VerifiedMeshSession& remote()const noexcept{return remote_;}
  bool connect(){
    disconnect();error_.clear();
    const auto& config=application_.configuration();timeout_=config.deadline_ms;
    if(!config.connect_port)return fail("mesh configuration has no remote endpoint");
    auto code=context_.open({config.certificate_path.string(),config.private_key_path.string(),config.ca_path.string()},mesh::tls::Role::client);
    if(code==mesh::TransportCode::ok)code=connection_.connect(context_,{config.connect_address,config.connect_port},config.server_pin,timeout_);
    if(code!=mesh::TransportCode::ok)return fail(std::string("mesh connect: ")+mesh::transport_code_name(code));
    if(application_.authorization().authenticate(connection_.peer(),remote_)!=mesh::EnrollmentCode::ok)return fail("remote certificate is not actively enrolled");
    std::vector<std::byte> ready;code=connection_.read_frame(ready,timeout_);
    if(code!=mesh::TransportCode::ok)return fail("remote did not authorize this peer");
    const std::string_view reply(reinterpret_cast<const char*>(ready.data()),ready.size());
    return reply=="CEVMESH/1 READY\n"||fail("invalid mesh greeting");
  }
  bool synchronize(std::string_view id,const mesh::Scope& scope,MeshTransferStats& stats){
    using namespace mesh;
    if(!mesh_token(id)||!valid_scope(scope))return fail("invalid subscription");
    VerifiedMeshSession publication;
    if(!application_.authorization().bind_publish(remote_,scope,publication))return fail("remote may not publish the requested source");
    ReplayCursor durable;
    if(application_.replay_store().load_receive(remote_.authorized_peer(),id,scope,durable)!=ReplayCode::ok)return fail("invalid local receive cursor");
    const auto minimum_tip=durable;
    wire::Request request;request.command=wire::Command::subscribe;request.id=id;request.scope=scope;request.cursor=durable;
    std::string response;ReplayCursor server;
    if(!exchange(request,response)||!wire::parse_cursor_reply(response,"SUBSCRIBED",server))return fail("subscription was not established");
    if(server.sequence>durable.sequence||(server.sequence==durable.sequence&&server.hash!=durable.hash))
      return fail("sender cursor is ahead of local durability or has diverged; use a new explicit subscription after restoring evidence");
    for(std::size_t budget=0;budget<4096;++budget){
      request.command=wire::Command::next;
      ReplayPage page;
      if(!exchange(request,response)||!wire::parse_page(response,page)||page.after!=server)return fail("invalid replay page");
      if(page.caught_up&&page.terminal.sequence<minimum_tip.sequence)return fail("remote history rolled back behind the durable receive tip");
      for(std::size_t i=0;i<page.count;++i){const auto& record=page.records[i];
        if(!scope_matches(scope,record.observation)||(record.cursor.sequence==durable.sequence&&record.cursor.hash!=durable.hash))return fail("replay scope or tip conflict");
        const auto received=application_.receive(remote_,record.observation);
        if(received.code!=ReplayCode::ok)return fail("observation was not durably received: "+std::to_string(static_cast<unsigned>(received.code)));
        ++stats.records;if(received.duplicate)++stats.duplicates;
        if(record.cursor.sequence>durable.sequence){
          if(application_.replay_store().save_receive(remote_.authorized_peer(),id,scope,record.cursor)!=ReplayCode::ok)return fail("receive cursor persistence failed");
          durable=record.cursor;
        }
      }
      if(page.terminal.sequence==durable.sequence&&page.terminal.hash!=durable.hash)return fail("replay terminal conflict");
      if(page.terminal.sequence>durable.sequence){
        if(application_.replay_store().save_receive(remote_.authorized_peer(),id,scope,page.terminal)!=ReplayCode::ok)return fail("receive terminal persistence failed");
        durable=page.terminal;
      }
      request.command=wire::Command::ack;request.cursor=page.terminal;
      ReplayCursor acknowledged;
      if(!exchange(request,response)||!wire::parse_cursor_reply(response,"ACKNOWLEDGED",acknowledged)||acknowledged!=page.terminal)return fail("replay acknowledgement failed");
      server=page.terminal;++stats.pages;
      if(page.caught_up)return true;
    }
    return fail("reconciliation page budget exhausted; committed progress is retained for the next invocation");
  }
  bool push(std::string_view id,const mesh::Scope& scope,MeshTransferStats& stats){
    using namespace mesh;
    if(!application_.authorization().may_subscribe(remote_,scope))return fail("remote is not authorized to receive this source");
    ReplayCursor cursor;
    auto& replay=application_.replay();const auto peer=remote_.authorized_peer();
    if(replay.open(peer,id,scope,cursor)!=ReplayCode::ok)return fail("outbound subscription unavailable");
    for(std::size_t budget=0;budget<4096;++budget){
      ReplayPage page;if(replay.offer(peer,id,page)!=ReplayCode::ok)return fail("outbound replay failed");
      for(std::size_t i=0;i<page.count;++i){wire::Request request;request.command=wire::Command::publish;request.observation=page.records[i].observation;
        std::string response;bool duplicate=false;
        if(!exchange(request,response)||!published(response,request.observation,duplicate))return fail("publication receipt invalid or unavailable");
        ++stats.records;if(duplicate)++stats.duplicates;
      }
      if(replay.acknowledge(peer,id,page.terminal)!=ReplayCode::ok)return fail("outbound acknowledgement persistence failed");
      ++stats.pages;if(page.caught_up){replay.disconnect(peer);return true;}
    }
    replay.disconnect(peer);return fail("publication page budget exhausted; progress retained");
  }
  bool fetch_artifact(std::string_view digest,MeshTransferStats& stats){
    using namespace mesh;
    wire::Request request;request.command=wire::Command::artifact;request.digest=digest;
    std::string response;ArtifactReceipt receipt;
    if(!exchange(request,response)||!wire::parse_artifact(response,receipt)||receipt.sha256!=digest)return fail("artifact identity or size rejected");
    auto& receiver=application_.artifact_receiver();const auto peer=remote_.authorized_peer();bool complete=false;
    if(receiver.begin(peer,receipt,complete)!=ArtifactCode::ok)return fail("artifact staging unavailable");
    if(complete){stats.bytes=receipt.bytes;return true;}
    std::uint64_t offset=0;
    while(offset<receipt.bytes){
      request.command=wire::Command::chunk;request.offset=offset;ArtifactChunk chunk;
      if(!exchange(request,response)||!wire::parse_chunk(response,chunk)||chunk.artifact!=receipt||chunk.offset!=offset||!chunk.count||
        receiver.append(peer,digest,offset,std::span<const std::byte>(chunk.bytes.data(),chunk.count))!=ArtifactCode::ok){
        receiver.cancel(peer);return fail("artifact chunk rejected");
      }
      offset+=chunk.count;
    }
    if(receiver.finish(peer,receipt)!=ArtifactCode::ok)return fail("artifact digest or durable completion failed");
    stats.bytes=receipt.bytes;return true;
  }
};
} // namespace anaiero::application
