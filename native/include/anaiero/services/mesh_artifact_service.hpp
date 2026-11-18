#pragma once
#include "anaiero/contracts/mesh_artifact.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <string>
#include <string_view>

namespace anaiero::mesh {
// The gateway checks may_read_artifact for every describe/read request. This
// service receives the resulting principal and exact authorized root digest.
class MeshArtifactService final {
  IMeshArtifactStore& store_;
  contracts::IServiceExtension* extension_=nullptr;
public:
  explicit MeshArtifactService(IMeshArtifactStore& store)noexcept:store_(store){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  ArtifactCode describe(const AuthorizedPeer& peer,std::string_view digest,ArtifactReceipt& receipt)noexcept {
    if(!peer.generation||!mesh_token(peer.principal)||!mesh_digest(digest))return ArtifactCode::invalid;
    std::uint64_t bytes=0;const auto status=store_.describe(digest,bytes);if(status!=ArtifactCode::ok)return status;
    if(bytes>kMaxMeshArtifactBytes)return ArtifactCode::capacity;
    receipt={std::string(digest),bytes};return ArtifactCode::ok;
  }
  ArtifactCode read(const AuthorizedPeer& peer,std::string_view digest,std::uint64_t offset,ArtifactChunk& chunk)noexcept {
    ArtifactReceipt receipt;const auto status=describe(peer,digest,receipt);if(status!=ArtifactCode::ok)return status;
    if(offset>receipt.bytes)return ArtifactCode::invalid;
    ArtifactChunk next;next.artifact=receipt;next.offset=offset;
    const auto read=store_.read(digest,offset,next.bytes,next.count);if(read!=ArtifactCode::ok)return read;
    const auto expected=std::min<std::uint64_t>(kMeshArtifactChunkBytes,receipt.bytes-offset);
    if(next.count!=expected)return ArtifactCode::corrupt;
    next.final=offset+next.count==receipt.bytes;chunk=std::move(next);
    if(extension_)extension_->on_service_event({contracts::ServiceId::mesh_artifact,contracts::ServicePhase::committed,offset+chunk.count,0,0});
    return ArtifactCode::ok;
  }
};

// One bounded in-memory staging transfer. Process loss discards incomplete
// staging and restarts at byte zero; complete transfers retain durable receipts.
class MeshArtifactReceiver final {
  IMeshArtifactStore& store_;
  AuthorizedPeer peer_{};
  ArtifactReceipt artifact_{};
  std::string staging_;
  bool active_=false;
  contracts::IServiceExtension* extension_=nullptr;
  bool owns(const AuthorizedPeer& peer,std::string_view digest)const noexcept {
    return active_&&peer==peer_&&digest==artifact_.sha256;
  }
  void clear()noexcept{active_=false;peer_={};artifact_={};staging_.clear();}
public:
  explicit MeshArtifactReceiver(IMeshArtifactStore& store)noexcept:store_(store){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  ArtifactCode begin(const AuthorizedPeer& peer,const ArtifactReceipt& artifact,bool& complete)noexcept {
    complete=false;
    if(!peer.generation||!mesh_token(peer.principal)||!mesh_digest(artifact.sha256)||artifact.bytes>kMaxMeshArtifactBytes)return ArtifactCode::invalid;
    if(active_){
      if(peer==peer_&&artifact==artifact_)return ArtifactCode::ok;
      return ArtifactCode::capacity;
    }
    const auto status=store_.completed(artifact,complete);if(status!=ArtifactCode::ok||complete)return status;
    peer_=peer;artifact_=artifact;staging_.clear();staging_.reserve(static_cast<std::size_t>(artifact.bytes));active_=true;
    return ArtifactCode::ok;
  }
  ArtifactCode append(const AuthorizedPeer& peer,std::string_view digest,std::uint64_t offset,std::span<const std::byte> bytes)noexcept {
    if(!owns(peer,digest))return ArtifactCode::denied;
    if(bytes.empty()||bytes.size()>kMeshArtifactChunkBytes||offset>artifact_.bytes||bytes.size()>artifact_.bytes-offset)return ArtifactCode::invalid;
    const std::string_view chunk{reinterpret_cast<const char*>(bytes.data()),bytes.size()};
    if(offset<staging_.size()){
      return bytes.size()<=staging_.size()-static_cast<std::size_t>(offset)&&
        std::string_view(staging_).substr(static_cast<std::size_t>(offset),bytes.size())==chunk?ArtifactCode::ok:ArtifactCode::conflict;
    }
    if(offset!=staging_.size())return ArtifactCode::conflict;
    staging_.append(chunk);return ArtifactCode::ok;
  }
  ArtifactCode finish(const AuthorizedPeer& peer,const ArtifactReceipt& artifact)noexcept {
    if(!owns(peer,artifact.sha256)){
      if(!peer.generation||!mesh_token(peer.principal)||!mesh_digest(artifact.sha256)||artifact.bytes>kMaxMeshArtifactBytes)return ArtifactCode::invalid;
      bool complete=false;const auto status=store_.completed(artifact,complete);
      return status!=ArtifactCode::ok?status:(complete?ArtifactCode::ok:ArtifactCode::not_found);
    }
    if(artifact!=artifact_||staging_.size()!=artifact.bytes)return ArtifactCode::conflict;
    if(christeye::living::plans::sha256_hex(staging_)!=artifact.sha256){clear();return ArtifactCode::corrupt;}
    const auto status=store_.commit(artifact,staging_);if(status!=ArtifactCode::ok)return status;
    if(extension_)extension_->on_service_event({contracts::ServiceId::mesh_artifact,contracts::ServicePhase::committed,artifact.bytes,0,0});
    clear();return ArtifactCode::ok;
  }
  void cancel(const AuthorizedPeer& peer)noexcept{if(active_&&peer_==peer)clear();}
  std::uint64_t received()const noexcept{return staging_.size();}
};
} // namespace anaiero::mesh
