#pragma once
#include "anaiero/contracts/mesh_artifact.hpp"
#include "christeye/living/durable_registry.hpp"
#include <algorithm>
#include <string>
#include <string_view>

namespace anaiero::mesh {
class WindStoreMeshArtifactStore final:public IMeshArtifactStore {
  christeye::living::durable::Registry& store_;
public:
  explicit WindStoreMeshArtifactStore(christeye::living::durable::Registry& store)noexcept:store_(store){}
  ArtifactCode describe(std::string_view digest,std::uint64_t& size)noexcept override {
    if(!mesh_digest(digest))return ArtifactCode::invalid;
    std::string bytes;if(!store_.get(digest,bytes))return ArtifactCode::not_found;
    if(bytes.size()>kMaxMeshArtifactBytes)return ArtifactCode::capacity;
    size=bytes.size();return ArtifactCode::ok;
  }
  ArtifactCode read(std::string_view digest,std::uint64_t offset,std::span<std::byte> output,std::size_t& count)noexcept override {
    count=0;
    if(!mesh_digest(digest)||output.empty()||output.size()>kMeshArtifactChunkBytes)return ArtifactCode::invalid;
    // Registry verifies the complete immutable object on each read. This keeps
    // transient memory bounded by 4 MiB and detects local corruption between
    // chunks; no lifetime artifact cache or unverified file handle is retained.
    std::string bytes;if(!store_.get(digest,bytes))return ArtifactCode::not_found;
    if(bytes.size()>kMaxMeshArtifactBytes||offset>bytes.size())return ArtifactCode::invalid;
    count=std::min(output.size(),bytes.size()-static_cast<std::size_t>(offset));
    for(std::size_t i=0;i<count;++i)output[i]=static_cast<std::byte>(static_cast<unsigned char>(bytes[static_cast<std::size_t>(offset)+i]));
    return ArtifactCode::ok;
  }
  ArtifactCode completed(const ArtifactReceipt& artifact,bool& found)noexcept override {
    using namespace christeye::living::durable;
    found=false;if(!mesh_digest(artifact.sha256)||artifact.bytes>kMaxMeshArtifactBytes)return ArtifactCode::invalid;
    Event event;bool exists=false;
    if(!store_.latest_event("CRYSTAL_EVIDENCE",artifact.sha256,"mesh-artifact",event,exists,"mesh-receiver"))return ArtifactCode::corrupt;
    if(!exists)return ArtifactCode::ok;
    std::string bytes;
    if(event.object_hash!=artifact.sha256||event.source_tick!=artifact.bytes||event.origin!="IMPORTED_UNVERIFIED"||
      !store_.get(artifact.sha256,bytes)||bytes.size()!=artifact.bytes)return ArtifactCode::corrupt;
    found=true;return ArtifactCode::ok;
  }
  ArtifactCode commit(const ArtifactReceipt& artifact,std::string_view bytes)noexcept override {
    using namespace christeye::living::durable;
    if(!mesh_digest(artifact.sha256)||artifact.bytes>kMaxMeshArtifactBytes||artifact.bytes!=bytes.size()||
      christeye::living::plans::sha256_hex(bytes)!=artifact.sha256)return ArtifactCode::corrupt;
    bool found=false;const auto prior=completed(artifact,found);if(prior!=ArtifactCode::ok||found)return prior;
    const auto object=store_.put(bytes);
    if(object.code!=Code::ok)return object.code==Code::capacity?ArtifactCode::capacity:ArtifactCode::persistence_failed;
    const auto published=store_.publish("CRYSTAL_EVIDENCE",artifact.sha256,object.hash,"IMPORTED_UNVERIFIED",artifact.bytes,"mesh-artifact","mesh-receiver");
    return published.code==Code::ok?ArtifactCode::ok:ArtifactCode::persistence_failed;
  }
};
} // namespace anaiero::mesh
