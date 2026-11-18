#pragma once
// Owner-only enrollment persistence. A canonical policy becomes authoritative
// only when its WindStore event is durably published. SHA is not a signature.
#include "anaiero/contracts/mesh_identity.hpp"
#include "anaiero/infrastructure/mesh_configuration.hpp"
#include "christeye/living/durable_registry.hpp"
#include <string>
#include <utility>

namespace anaiero::mesh {
class WindStoreEnrollmentStore final:public IEnrollmentStore {
  christeye::living::durable::Registry& database_;
  std::string entity_;
  bool latest(EnrollmentPolicy& output,bool& found)noexcept {
    namespace db=christeye::living::durable;
    if(!database_.is_open()||!db::valid_id(entity_))return false;
    db::Event event;
    if(!database_.latest_event("CULTURE_CHECKPOINT",entity_,"mesh-enrollment",event,found,"anaiero-mesh"))return false;
    if(!found){output={};return true;}
    std::string bytes,error;EnrollmentPolicy value;
    if(event.origin!="IMPORTED_UNVERIFIED"||!database_.get(event.object_hash,bytes)||
       !files::parse_policy(bytes,value,error)||value.revision!=event.source_tick)return false;
    output=std::move(value);return true;
  }
public:
  WindStoreEnrollmentStore(christeye::living::durable::Registry& database,std::string name):
    database_(database),entity_("mesh-policy-"+std::move(name)){}
  bool load(EnrollmentPolicy& output,bool& found)noexcept override {
    found=false;return latest(output,found);
  }
  EnrollmentStoreCode compare_exchange(std::uint64_t expected_revision,const EnrollmentPolicy& proposed)noexcept override {
    namespace db=christeye::living::durable;
    if(expected_revision==UINT64_MAX||proposed.revision!=expected_revision+1||!valid_enrollment_policy(proposed))return EnrollmentStoreCode::failure;
    EnrollmentPolicy current;bool found=false;
    if(!latest(current,found))return EnrollmentStoreCode::failure;
    if((found?current.revision:0)!=expected_revision)return EnrollmentStoreCode::conflict;
    const auto bytes=files::serialize_policy(proposed);if(bytes.empty())return EnrollmentStoreCode::failure;
    const auto object=database_.put(bytes);if(object.code!=db::Code::ok)return EnrollmentStoreCode::failure;
    const auto receipt=database_.publish("CULTURE_CHECKPOINT",entity_,object.hash,"IMPORTED_UNVERIFIED",
      proposed.revision,"mesh-enrollment","anaiero-mesh");
    return receipt.code==db::Code::ok?EnrollmentStoreCode::ok:EnrollmentStoreCode::failure;
  }
};
} // namespace anaiero::mesh
