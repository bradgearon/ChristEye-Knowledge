#include "deployment.hpp"
#include "package_retention.hpp"
#include "anaiero/sdk/session_runner.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>

namespace d=anaiero::deployment;
namespace s=anaiero::sdk;
namespace ev=s::evidence;
namespace fs=std::filesystem;
#define CHECK(value) do{if(!(value)){std::cerr<<"FAILED "<<__LINE__<<": " #value "\n";std::exit(1);}}while(false)
void require(const s::Result& result,std::source_location location=std::source_location::current()){
  if(!result){std::cerr<<"FAILED "<<location.line()<<": "<<s::code_name(result.code)<<": "<<result.message<<'\n';std::exit(1);}
}
void replace_digest(std::string& manifest,std::string_view file,std::string_view replacement){
  const auto at=manifest.find(std::string(file)+" ");CHECK(at!=manifest.npos);manifest.replace(at+file.size()+1,64,replacement);
}
d::VerifiedPackage package(std::string name,d::Witness witness){
  d::VerifiedPackage value;value.host="actual fixture host "+name;value.manifest="inventory "+name;
  value.identity={"/release/"+name,s::sha256(value.manifest),s::sha256(value.host),witness.build_id};
  value.witness=std::move(witness);return value;
}
d::Witness host_change(const d::Witness& original,std::string marker){
  auto next=original;replace_digest(next.build_manifest,"src/host/release_notice.txt",s::sha256(marker));
  next.build_id=s::sha256(next.build_manifest);require(d::validate_witness(next));return next;
}
s::EngineIdentity identity(const s::Digest& module,std::string source="source"){
  s::EngineIdentity id;id.instance_id=source;id.type_id="fixture.engine/1";id.module_sha256=module;
  id.model_sha256=s::sha256("model");id.behavior_id="fixture.behavior/1";id.source_uid=source;id.epoch=1;id.clock_domain="logical";return id;
}
void session(ev::ICommitStore& store,const s::Digest& module){
  s::SessionDefinition definition;definition.name="fixture";
  definition.engines.push_back({identity(module),{"fixture.config/1","configuration"},{"fixture.input/1","input"},1});
  ev::CommitBatch batch;batch.operation_id="session-init";batch.request_digest=s::sha256("session-init");
  batch.records.push_back({"session-definition","main",s::encode_session_definition(definition),{},false});
  batch.records.push_back({"session-progress","main",s::pack({"session-progress/2","0","0",s::pack({})}),{},false});
  ev::CommitReceipt receipt;require(store.commit(batch,receipt));
}
void suffix(ev::ICommitStore& store,std::string operation){
  ev::CommitBatch batch;batch.operation_id=operation;batch.request_digest=s::sha256(operation);
  batch.records.push_back({"accepted",operation,"retained suffix",{},false});ev::CommitReceipt receipt;require(store.commit(batch,receipt));
}
void select(d::Service& service,const d::VerifiedPackage& package,std::string operation,d::Plan* retained=nullptr){
  d::Plan plan;require(service.review(package,operation,plan));ev::CommitReceipt receipt;require(service.activate(plan,package,receipt));
  if(retained)*retained=std::move(plan);
}
void binding(const fs::path& root){
  auto original=d::compiled_witness();require(d::validate_witness(original));
  const auto changed=host_change(original,"independent host source update");
  CHECK(original.build_id!=changed.build_id&&original.compatibility_id==changed.compatibility_id);
  const auto a=package("a",original),b=package("b",changed);
  std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(root.string(),{},store));session(*store,a.identity.module_sha256);
  d::Service service(*store);select(service,a,"a");
  d::Plan upgrade;require(service.review(b,"b",upgrade));CHECK(!upgrade.compatibility_binding.empty());
  d::Plan decoded;require(d::decode_plan(d::encode_plan(upgrade),decoded));CHECK(decoded.compatibility_binding==upgrade.compatibility_binding);
  ev::CommitReceipt receipt;require(service.activate(upgrade,b,receipt));
  std::vector<s::Digest> aliases;require(d::authorize_binding(*store,b.witness,aliases));
  CHECK(aliases==std::vector<s::Digest>{a.identity.module_sha256});
  CHECK(d::authorize_binding(*store,a.witness,aliases).code==s::Code::unsupported);
  suffix(*store,"work-after-upgrade");const auto generation=store->generation();
  d::Plan rollback;require(service.review(a,"rollback",rollback));require(service.activate(rollback,a,receipt));
  CHECK(store->generation()==generation+1);require(d::authorize_binding(*store,a.witness,aliases));CHECK(aliases.empty());
  ev::StoredRecord accepted;require(store->latest("accepted","work-after-upgrade",accepted));
  require(service.activate(upgrade,b,receipt));CHECK(receipt.duplicate&&store->generation()==generation+1);
  d::Selection selection;require(service.selection(selection));CHECK(selection.current==a.identity);
  auto engine_change=changed;
  replace_digest(engine_change.build_manifest,"src/engines/plan_module.cpp",s::sha256("changed engine"));
  replace_digest(engine_change.compatibility_manifest,"src/engines/plan_module.cpp",s::sha256("changed engine"));
  engine_change.build_id=s::sha256(engine_change.build_manifest);engine_change.compatibility_id.clear();require(d::validate_witness(engine_change));
  CHECK(service.review(package("engine-change",engine_change),"engine-change",decoded).code==s::Code::unsupported);
  for(const auto file:{"src/storage/native_store.cpp","src/session/session_runner.cpp","src/lifecycle/records.hpp"}){
    auto codec_change=changed;replace_digest(codec_change.build_manifest,file,s::sha256("changed codec"));
    replace_digest(codec_change.compatibility_manifest,file,s::sha256("changed codec"));
    codec_change.build_id=s::sha256(codec_change.build_manifest);codec_change.compatibility_id.clear();require(d::validate_witness(codec_change));
    CHECK(service.review(package("codec-change",codec_change),"codec-change",decoded).code==s::Code::unsupported);
  }
  auto compiler_change=changed;
  compiler_change.build_manifest.insert(compiler_change.build_manifest.find('\n')+1,"different-");
  compiler_change.compatibility_manifest.insert(compiler_change.compatibility_manifest.find('\n')+1,"different-");
  compiler_change.build_id=s::sha256(compiler_change.build_manifest);compiler_change.compatibility_id.clear();require(d::validate_witness(compiler_change));
  CHECK(service.review(package("compiler-change",compiler_change),"compiler-change",decoded).code==s::Code::unsupported);
  auto corrupt=changed;corrupt.build_manifest+='!';CHECK(!d::validate_witness(corrupt));
  const auto retired=identity(s::sha256("unwitnessed retired build"),"retired");
  ev::CommitBatch retire;retire.operation_id="retired";retire.request_digest=s::sha256("retired");
  retire.records.push_back({"lifecycle.source",s::sha256(retired.source_uid)+":1",
    s::pack({"source/3",s::pack({retired.source_uid,"1"}),s::encode_identity(retired),"operator","4","0","0","0","","",""}),{},false});
  require(store->commit(retire,receipt));CHECK(service.review(b,"retired-reject",decoded).code==s::Code::unsupported);
  ev::StoredRecord saved_binding;require(store->latest("operator.deployment-binding","active",saved_binding));
  ev::CommitBatch malformed;malformed.operation_id="unrooted-binding";malformed.request_digest=s::sha256("unrooted binding fixture");
  auto broken=saved_binding.record;broken.references.clear();malformed.records.push_back(broken);require(store->commit(malformed,receipt));
  CHECK(d::authorize_binding(*store,a.witness,aliases).code==s::Code::corrupt);
  ev::CommitBatch restore;restore.operation_id="restore-binding-fixture";restore.request_digest=s::sha256("restore original binding fixture");
  restore.records.push_back(saved_binding.record);require(store->commit(restore,receipt));
  CHECK(fs::remove(root/"blobs"/a.witness.build_sha256));
  CHECK(d::authorize_binding(*store,a.witness,aliases).code==s::Code::corrupt);
}
void binding_cuts(const fs::path& root){
  auto witness=d::compiled_witness();require(d::validate_witness(witness));
  const auto a=package("cut-a",witness),b=package("cut-b",host_change(witness,"host B"));
  for(const auto point:{ev::FaultPoint::after_objects,ev::FaultPoint::after_transaction,ev::FaultPoint::after_root,ev::FaultPoint::before_index,ev::FaultPoint::after_index}){
    const auto path=root/std::to_string(static_cast<unsigned>(point));std::unique_ptr<ev::ICommitStore> store;
    require(ev::open_native_store(path.string(),{},store));session(*store,a.identity.module_sha256);
    d::Plan plan;{d::Service service(*store);select(service,a,"a");require(service.review(b,"b",plan));}
    ev::set_native_store_fault(*store,point);ev::CommitReceipt receipt;d::Service interrupted(*store);CHECK(!interrupted.activate(plan,b,receipt).ok());
    store.reset();require(ev::open_native_store(path.string(),{},store));if(!store->info().index_ready)require(store->repair_index());
    d::Service recovered(*store);require(recovered.activate(plan,b,receipt));std::vector<s::Digest> aliases;require(d::authorize_binding(*store,b.witness,aliases));
    CHECK(aliases.size()==1);d::Selection selected;require(recovered.selection(selected));CHECK(selected.revision==2&&selected.current==b.identity);
    require(recovered.activate(plan,b,receipt));CHECK(receipt.duplicate);ev::AuditReport audit;require(store->audit(audit));
  }
}
void pending_and_precontract(const fs::path& root){
  auto witness=d::compiled_witness();require(d::validate_witness(witness));
  auto a=package("legacy",witness);const auto b=package("candidate",host_change(witness,"candidate"));a.witness={};
  std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(root.string(),{},store));session(*store,a.identity.module_sha256);
  d::Service service(*store);select(service,a,"legacy");d::Plan plan;CHECK(service.review(b,"bridge",plan).code==s::Code::unsupported);
  ev::CommitBatch pending;pending.operation_id="pending";pending.request_digest=s::sha256("pending");
  pending.records.push_back({"session-progress","main",s::pack({"session-progress/2","0","1",s::pack({})}),{},false});
  ev::CommitReceipt receipt;require(store->commit(pending,receipt));std::vector<s::Digest> modules;CHECK(d::session_modules(*store,modules,true).code==s::Code::busy);
}
void retention(const fs::path& root){
  d::Witness witness;witness.build_id=s::sha256("same build");
  const auto a=package("retain-a",witness),b=package("retain-b",witness),c=package("retain-c",witness),e=package("retain-d",witness);
  std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(root.string(),{},store));d::Service service(*store);
  d::Plan first;select(service,a,"a",&first);select(service,b,"b");select(service,c,"c");
  ev::CommitReceipt receipt;require(d::package_policy(*store,3,65536,"limits",receipt));
  d::Plan next;CHECK(service.review(e,"d",next).code==s::Code::capacity);
  d::PackageRetentionPlan plan;
  CHECK(d::package_retention_review(*store,{b.identity.manifest_sha256},"protected",plan).code==s::Code::conflict);
  require(d::package_pin(*store,a.identity.manifest_sha256,true,"pin",receipt));
  CHECK(d::package_retention_review(*store,{a.identity.manifest_sha256},"pinned",plan).code==s::Code::conflict);
  require(d::package_pin(*store,a.identity.manifest_sha256,false,"unpin",receipt));
  require(d::package_retention_review(*store,{a.identity.manifest_sha256},"stale",plan));suffix(*store,"advanced");
  CHECK(d::package_retention_activate(*store,plan,receipt).code==s::Code::conflict);
  require(d::package_retention_review(*store,{a.identity.manifest_sha256},"expire",plan));CHECK(plan.objects.size()==2);
  d::PackageRetentionPlan decoded;require(d::decode_package_retention(d::encode_package_retention(plan),decoded));CHECK(decoded.plan_sha256==plan.plan_sha256);
  require(d::package_retention_activate(*store,plan,receipt));const auto expiry_generation=receipt.generation;
  CHECK(fs::remove(root/"blobs"/plan.objects.front()));
  std::uint64_t remaining=99;require(d::package_retention_reclaim(*store,"expire",remaining));CHECK(remaining==0);
  require(d::package_retention_reclaim(*store,"expire",remaining));
  require(d::package_retention_activate(*store,plan,receipt));CHECK(receipt.duplicate&&receipt.generation==expiry_generation);
  require(service.activate(first,a,receipt));CHECK(receipt.duplicate);
  d::Selection selected;require(service.selection(selected));CHECK(selected.current==c.identity);
  CHECK(service.review(a,"resurrect",next).code==s::Code::history_expired);
  require(service.review(e,"d",next));require(service.activate(next,e,receipt));
  d::PackageUsage usage;require(d::package_usage(*store,usage));CHECK(usage.policy.packages.size()==3&&usage.retained_bytes<=65536);
  ev::AuditReport audit;require(store->audit(audit));
}
int main(int argc,char** argv){
  CHECK(argc==2);const fs::path root=argv[1];fs::remove_all(root);fs::create_directories(root);
  binding(root/"binding");binding_cuts(root/"cuts");pending_and_precontract(root/"legacy");retention(root/"retention");
  std::cout<<"exact-source compatibility binding, rejected engine/compiler changes, five migration cuts, suffix rollback and bounded package retention passed\n";
}
