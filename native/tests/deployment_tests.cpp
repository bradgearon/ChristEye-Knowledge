#include "deployment.hpp"
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
d::VerifiedPackage package(std::string name){
  d::VerifiedPackage value;value.host="same compatible executable";value.manifest="exact inventory "+name;
  value.identity={"/retained/"+name,s::sha256(value.manifest),s::sha256(value.host),s::sha256("compiled engine identity")};return value;
}
void write(const fs::path& path,std::string_view bytes){std::ofstream file(path,std::ios::binary);file<<bytes;CHECK(file.good());}
void advance(ev::ICommitStore& store,std::string name){
  ev::CommitBatch batch;batch.operation_id=name;batch.request_digest=s::sha256(name);batch.records.push_back({"accepted.suffix",name,"accepted",{},false});
  ev::CommitReceipt receipt;require(store.commit(batch,receipt));
}
void canonical_selection(const fs::path& directory){
  std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(directory.string(),{},store));
  std::unique_ptr<ev::ICommitStore> concurrent;CHECK(ev::open_native_store(directory.string(),{},concurrent).code==s::Code::busy);
  const auto a=package("release-a"),b=package("release-b");ev::CommitReceipt receipt;
  d::Plan first;d::Service service(*store);require(service.review(a.identity,"first",first));
  d::Plan decoded;require(d::decode_plan(d::encode_plan(first),decoded));CHECK(decoded.plan_sha256==first.plan_sha256);
  auto broken=d::encode_plan(first);broken.back()^=1;CHECK(d::decode_plan(broken,decoded).code==s::Code::corrupt);
  require(service.activate(first,a,receipt));CHECK(!receipt.duplicate);const auto first_receipt=receipt;
  d::Selection selected;require(service.selection(selected));CHECK(selected.current==a.identity&&selected.previous==d::Package{});
  s::Bytes retained;require(store->get_blob(a.identity.host_sha256,retained));CHECK(retained==a.host);
  d::Plan stale;require(service.review(b.identity,"stale",stale));advance(*store,"accepted-after-review");
  CHECK(service.activate(stale,b,receipt).code==s::Code::conflict);
  require(service.activate(first,a,receipt));CHECK(receipt.duplicate&&receipt.root_digest==first_receipt.root_digest);
  d::Plan upgrade;require(service.review(b.identity,"upgrade",upgrade));
  auto altered=b;altered.host+='!';CHECK(service.activate(upgrade,altered,receipt).code==s::Code::conflict);
  require(service.activate(upgrade,b,receipt));require(service.selection(selected));CHECK(selected.current==b.identity&&selected.previous==a.identity);
  advance(*store,"new-accepted-suffix");const auto before=store->generation();
  d::Plan rollback;require(service.review(selected.previous,"rollback",rollback));require(service.activate(rollback,a,receipt));
  CHECK(store->generation()==before+1);ev::StoredRecord suffix;require(store->latest("accepted.suffix","new-accepted-suffix",suffix));CHECK(suffix.record.payload=="accepted");
  require(service.selection(selected));CHECK(selected.current==a.identity&&selected.previous==b.identity);
  // Retrying an old upgrade after rollback reports its original durable receipt
  // and cannot silently select the old target again.
  require(service.activate(upgrade,b,receipt));CHECK(receipt.duplicate&&store->generation()==before+1);
  require(service.selection(selected));CHECK(selected.current==a.identity);
  auto incompatible=b.identity;incompatible.module_sha256=s::sha256("different engine");
  CHECK(service.review(incompatible,"incompatible",decoded).code==s::Code::unsupported);
  CHECK(service.review(b.identity,"upgrade",decoded).code==s::Code::conflict);
  ev::CommitBatch expiry;expiry.operation_id="expire-rollback-artifact";expiry.request_digest=s::sha256(expiry.operation_id);expiry.expire_blobs={b.identity.manifest_sha256};
  CHECK(store->commit(expiry,receipt).code==s::Code::conflict);
  ev::AuditReport audit;require(store->audit(audit));
  write(directory/"blobs"/a.identity.host_sha256,"corrupted retained package bytes");
  CHECK(service.review(b.identity,"corrupt-rollback-witness",decoded).code==s::Code::corrupt);
  // The store deliberately latches corruption; restoring the test fixture's
  // original bytes does not clear that in-process error. Reopen to revalidate.
  write(directory/"blobs"/a.identity.host_sha256,a.host);
  store.reset();require(ev::open_native_store(directory.string(),{},store));d::Service recovered(*store);require(recovered.selection(selected));CHECK(selected.current==a.identity);
  require(recovered.reconcile(upgrade,receipt));CHECK(receipt.duplicate);
  require(store->audit(audit));
}
void cuts(const fs::path& directory){
  const auto a=package("a"),b=package("b");
  for(const auto point:{ev::FaultPoint::after_objects,ev::FaultPoint::after_transaction,ev::FaultPoint::after_root,ev::FaultPoint::before_index,ev::FaultPoint::after_index}){
    const auto root=directory/std::to_string(static_cast<unsigned>(point));std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(root.string(),{},store));
    d::Plan initial,next;ev::CommitReceipt receipt;
    {d::Service service(*store);require(service.review(a.identity,"initial",initial));require(service.activate(initial,a,receipt));require(service.review(b.identity,"next",next));}
    ev::set_native_store_fault(*store,point);d::Service interrupted(*store);const auto result=interrupted.activate(next,b,receipt);CHECK(!result.ok());
    store.reset();require(ev::open_native_store(root.string(),{},store));if(!store->info().index_ready)require(store->repair_index());
    d::Service recovered(*store);require(recovered.activate(next,b,receipt));d::Selection selection;require(recovered.selection(selection));
    CHECK(selection.current==b.identity&&selection.previous==a.identity&&selection.revision==2);
    require(recovered.activate(next,b,receipt));CHECK(receipt.duplicate);
    ev::AuditReport audit;require(store->audit(audit));
  }
}
void inventory(const fs::path& root){
  fs::create_directories(root/"bin");
#ifdef _WIN32
  const std::string relative="bin/ce_modular_host.exe";
  const std::string system="Windows";
#else
  const std::string relative="bin/ce_modular_host";
  const std::string system="Linux";
#endif
  const std::string host="native fixture bytes",module=s::sha256("engine");
  write(root/relative,host);
  const auto manifest="{\"schema\":\"anaiero.native-package/1\",\"profile\":\"fixture\",\"configuration\":\"Release\",\"host_system\":\""+system+
    "\",\"module_sha256\":\""+module+"\",\"files\":[{\"path\":\""+relative+"\",\"sha256\":\""+s::sha256(host)+"\",\"bytes\":"+std::to_string(host.size())+"}]}";
  write(root/"package-manifest.json",manifest);d::VerifiedPackage result;require(d::verify_native_package(root.string(),module,result,false));
  CHECK(result.files==1&&result.host==host&&result.identity.manifest_sha256==s::sha256(manifest));
  CHECK(d::verify_native_package(root.string(),s::sha256("wrong"),result,false).code==s::Code::unsupported);
  write(root/relative,host+"!");CHECK(!d::verify_native_package(root.string(),module,result,false));write(root/relative,host);
  auto changed=host;changed.back()='X';write(root/relative,changed);CHECK(!d::verify_native_package(root.string(),module,result,false));write(root/relative,host);
  write(root/"outside","unlisted");CHECK(!d::verify_native_package(root.string(),module,result,false));fs::remove(root/"outside");
  write(root/"package-manifest.json",manifest.substr(0,manifest.size()-1)+",\"schema\":\"anaiero.native-package/1\"}");CHECK(!d::verify_native_package(root.string(),module,result,false));
  write(root/"package-manifest.json",manifest+"trailing");CHECK(!d::verify_native_package(root.string(),module,result,false));
  write(root/"package-manifest.json",manifest);require(d::verify_native_package(root.string(),module,result,false));
  const auto link=root.parent_path()/"package-symlink";std::error_code error;fs::create_directory_symlink(root,link,error);
  if(!error)CHECK(!d::verify_native_package(link.string(),module,result,false));
}
int main(int argc,char** argv){
  CHECK(argc==2);const fs::path root=argv[1];fs::remove_all(root);fs::create_directories(root);
  canonical_selection(root/"store");cuts(root/"cuts");inventory(root/"native package");
  std::cout<<"canonical package review/activation, suffix-preserving rollback, stale/tampered plans, retained bytes and five publication cuts passed\n";
}
