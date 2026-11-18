#include "anaiero/infrastructure/windstore_mesh_replay.hpp"
#include "anaiero/infrastructure/windstore_mesh_artifact.hpp"
#include "anaiero/services/mesh_replay_service.hpp"
#include "anaiero/services/mesh_artifact_service.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " line "<<__LINE__<<'\n';std::abort();}}while(false)
namespace mesh=anaiero::mesh;
namespace runtime=anaiero::runtime;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
using mesh::ReplayCode;
using mesh::ArtifactCode;
const mesh::Scope scope_a{71,1,3,1},scope_b{72,1,4,2};
const mesh::AuthorizedPeer alice{"alice",1},bob{"bob",1};
static runtime::Observation observation(mesh::Scope scope,std::uint64_t sequence) {
  runtime::Observation result{};result.source=scope.source;result.source_epoch=scope.epoch;
  result.colony=scope.colony;result.clock_domain=scope.clock;result.sequence=sequence;
  result.metric=runtime::Metric::energy;result.lane=runtime::Lane::health;result.origin=runtime::Origin::simulated;
  result.value=static_cast<std::int32_t>(sequence);result.acquired_ns=sequence*1000;
  return result;
}
struct FailAck final:mesh::IMeshReplayStore {
  mesh::IMeshReplayStore& store;bool fail=false,after=false,bad_terminal=false,no_progress=false;
  explicit FailAck(mesh::IMeshReplayStore& value):store(value){}
  ReplayCode open_subscription(const mesh::ReplaySubscription& sub,mesh::ReplayCursor& cursor)noexcept override{return store.open_subscription(sub,cursor);}
  ReplayCode read_page(const mesh::Scope& scope,const mesh::ReplayCursor& cursor,mesh::ReplayPage& page)noexcept override{
    const auto status=store.read_page(scope,cursor,page);if(status!=ReplayCode::ok)return status;
    if(bad_terminal){bad_terminal=false;page.terminal.hash=std::string(64,'a');}
    if(no_progress){no_progress=false;page={};page.after=cursor;page.terminal=cursor;}
    return status;
  }
  ReplayCode acknowledge(const mesh::ReplaySubscription& sub,const mesh::ReplayCursor& cursor)noexcept override{
    if(fail){fail=false;if(after)CHECK(store.acknowledge(sub,cursor)==ReplayCode::ok);return ReplayCode::persistence_failed;}
    return store.acknowledge(sub,cursor);
  }
};
struct FailArtifactCommit final:mesh::IMeshArtifactStore {
  mesh::IMeshArtifactStore& store;bool fail=true;
  explicit FailArtifactCommit(mesh::IMeshArtifactStore& value):store(value){}
  ArtifactCode describe(std::string_view digest,std::uint64_t& bytes)noexcept override{return store.describe(digest,bytes);}
  ArtifactCode read(std::string_view digest,std::uint64_t offset,std::span<std::byte> output,std::size_t& count)noexcept override{return store.read(digest,offset,output,count);}
  ArtifactCode completed(const mesh::ArtifactReceipt& receipt,bool& found)noexcept override{return store.completed(receipt,found);}
  ArtifactCode commit(const mesh::ArtifactReceipt& receipt,std::string_view bytes)noexcept override{
    if(fail){fail=false;return ArtifactCode::persistence_failed;}return store.commit(receipt,bytes);
  }
};
static std::size_t drain(mesh::MeshReplayService& service,const mesh::AuthorizedPeer& peer,std::string_view id) {
  std::size_t records=0;
  for(unsigned attempts=0;attempts<1000;++attempts){
    mesh::ReplayPage page;CHECK(service.offer(peer,id,page)==ReplayCode::ok);
    CHECK(page.count<=mesh::kMaxReplayRecords&&page.examined<=mesh::kMaxReplayExamined);
    records+=page.count;CHECK(service.acknowledge(peer,id,page.terminal)==ReplayCode::ok);
    if(page.caught_up)return records;
  }
  CHECK(false);return 0;
}
static void replay_cases(const fs::path& root) {
  mesh::ReplayCursor acknowledged;
  {
    db::Registry store;CHECK(store.open(root/"evidence",db::StorageOptions::sustained(8))==db::Code::ok);
    mesh::WindStoreMeshReplayStore adapter(store,"mesh",root/"metadata");CHECK(adapter.initialize());
    FailAck faults(adapter);mesh::MeshReplayService service(faults);mesh::ReplayCursor cursor;
    CHECK(service.open(alice,"display",scope_a,cursor)==ReplayCode::ok&&cursor.sequence==0);
    CHECK(service.open(bob,"display",scope_b,cursor)==ReplayCode::ok);
    CHECK(service.open(alice,"display",scope_b,cursor)==ReplayCode::conflict);
    CHECK(service.open({"../alice",1},"display",scope_a,cursor)==ReplayCode::invalid);
    for(std::uint64_t i=1;i<=40;++i){CHECK(adapter.append(observation(scope_a,i))==ReplayCode::ok);CHECK(adapter.append(observation(scope_b,i))==ReplayCode::ok);}
    CHECK(service.open(alice,"bad-adapter",scope_a,cursor)==ReplayCode::ok);
    mesh::ReplayPage invalid;faults.bad_terminal=true;CHECK(service.offer(alice,"bad-adapter",invalid)==ReplayCode::corrupt);
    faults.no_progress=true;CHECK(service.offer(alice,"bad-adapter",invalid)==ReplayCode::corrupt);
    bool duplicate=false;CHECK(adapter.check_append(observation(scope_a,1),duplicate)==ReplayCode::ok&&duplicate);
    CHECK(adapter.append(observation(scope_a,1))==ReplayCode::ok&&store.event_count()==80);
    auto changed=observation(scope_a,1);++changed.value;CHECK(adapter.append(changed)==ReplayCode::conflict);
    CHECK(adapter.append(observation(scope_a,42))==ReplayCode::conflict);
    changed=observation(scope_a,41);++changed.clock_domain;CHECK(adapter.append(changed)==ReplayCode::conflict);
    changed=observation(scope_a,41);++changed.colony;CHECK(adapter.append(changed)==ReplayCode::conflict);
    mesh::ReplayPage first;CHECK(service.offer(alice,"display",first)==ReplayCode::ok);
    CHECK(first.count==16&&first.examined==31&&first.terminal.sequence==31&&!first.caught_up);
    for(std::size_t i=0;i<first.count;++i)CHECK(mesh::scope_matches(scope_a,first.records[i].observation));
    CHECK(service.acknowledge(bob,"display",first.terminal)==ReplayCode::conflict);
    CHECK(service.acknowledge(alice,"display",first.records[0].cursor)==ReplayCode::conflict);
    auto bad=first.terminal;bad.hash=std::string(64,'a');CHECK(service.acknowledge(alice,"display",bad)==ReplayCode::conflict);
    CHECK(adapter.append(observation(scope_a,41))==ReplayCode::ok);
    mesh::ReplayPage repeated;CHECK(service.offer(alice,"display",repeated)==ReplayCode::ok&&repeated.terminal==first.terminal);
    faults.fail=true;CHECK(service.acknowledge(alice,"display",first.terminal)==ReplayCode::persistence_failed);
    service.disconnect(alice);CHECK(service.open(alice,"display",scope_a,cursor)==ReplayCode::ok&&cursor.sequence==0);
    CHECK(service.offer(alice,"display",repeated)==ReplayCode::ok&&repeated.terminal==first.terminal);
    faults.fail=true;faults.after=true;
    CHECK(service.acknowledge(alice,"display",repeated.terminal)==ReplayCode::persistence_failed);
    CHECK(service.open(alice,"display",scope_a,cursor)==ReplayCode::ok&&cursor==first.terminal);
    acknowledged=cursor;
    CHECK(service.offer({"alice",2},"display",repeated)==ReplayCode::denied);
    CHECK(service.open({"alice",2},"display",scope_a,cursor)==ReplayCode::ok&&cursor==acknowledged);
    CHECK(adapter.save_receive(alice,"from-origin",scope_a,first.terminal)==ReplayCode::ok);
    CHECK(adapter.save_receive(alice,"from-origin",scope_a,bad)==ReplayCode::conflict);
    CHECK(adapter.save_receive(alice,"from-origin",scope_a,{})==ReplayCode::conflict);
    CHECK(adapter.load_receive(bob,"from-origin",scope_a,cursor)==ReplayCode::ok&&cursor.sequence==0);
    CHECK(adapter.load_receive(alice,"from-origin",scope_b,cursor)==ReplayCode::conflict);
    CHECK(adapter.save_receive({"alice",2},"from-origin",scope_a,first.terminal)==ReplayCode::ok);
    CHECK(adapter.load_receive(alice,"from-origin",scope_a,cursor)==ReplayCode::denied);
  }
  {
    db::Registry store;CHECK(store.open(root/"evidence")==db::Code::ok);
    mesh::WindStoreMeshReplayStore adapter(store,"mesh",root/"metadata");CHECK(adapter.initialize());
    mesh::MeshReplayService service(adapter);mesh::ReplayCursor cursor;
    CHECK(service.open({"alice",2},"display",scope_a,cursor)==ReplayCode::ok&&cursor==acknowledged);
    CHECK(drain(service,{"alice",2},"display")==25);
    CHECK(service.open(bob,"display",scope_b,cursor)==ReplayCode::ok&&cursor.sequence==0);
    CHECK(drain(service,bob,"display")==40);
    mesh::ReplayPage empty;CHECK(service.offer(bob,"display",empty)==ReplayCode::ok&&empty.count==0&&empty.caught_up);
    CHECK(adapter.append(observation(scope_b,41))==ReplayCode::ok);
    CHECK(service.offer(bob,"display",empty)==ReplayCode::ok&&empty.count==1);
    CHECK(adapter.load_receive({"alice",2},"from-origin",scope_a,cursor)==ReplayCode::ok&&cursor==acknowledged);
  }
  // Zero-cursor registration is durable in legacy mode as well.
  db::Registry legacy;CHECK(legacy.open(root/"legacy")==db::Code::ok);
  CHECK(legacy.acknowledge("new-reader",0,db::kZeroDigest)==db::Code::ok);
  CHECK(fs::exists(root/"legacy"/"cursors"/"new-reader.cursor"));
}
static void archive_cases(const fs::path& root) {
  db::Registry store;CHECK(store.open(root/"evidence",db::StorageOptions::sustained(32))==db::Code::ok);
  mesh::WindStoreMeshReplayStore adapter(store,"mesh",root/"metadata");CHECK(adapter.initialize());
  mesh::MeshReplayService service(adapter);mesh::ReplayCursor cursor;
  CHECK(service.open(alice,"fast",scope_a,cursor)==ReplayCode::ok);
  CHECK(service.open(bob,"slow",scope_a,cursor)==ReplayCode::ok);
  for(std::uint64_t i=1;i<=4203;++i)CHECK(adapter.append(observation(scope_a,i))==ReplayCode::ok);
  CHECK(store.event_count()==4203&&store.events().size()<=32);
  CHECK(drain(service,alice,"fast")==4203);
  CHECK(store.save_replay_checkpoint("mesh-retention-test"));
  CHECK(store.archive_ready()==db::Code::ok&&store.capacity().archived_segments==0);
  CHECK(drain(service,bob,"slow")==4203);
  CHECK(store.archive_ready()==db::Code::ok&&store.capacity().archived_segments>100);
  // A new subscription can replay the retained cold archive.
  CHECK(service.open({"carol",1},"cold",scope_a,cursor)==ReplayCode::ok&&cursor.sequence==0);
  mesh::ReplayPage cold;CHECK(service.offer({"carol",1},"cold",cold)==ReplayCode::ok&&cold.records[0].observation.sequence==1);
  CHECK(adapter.append(observation(scope_a,2))==ReplayCode::ok&&store.event_count()==4203);
  std::vector<db::Event> events;bool caught=false;
  CHECK(store.read_event_page({db::Code::ok,1,std::string(64,'a')},64,events,caught)==db::Code::invalid_cursor);
  CHECK(store.read_event_page({},65,events,caught)==db::Code::invalid_cursor);
}
static void artifact_cases(const fs::path& root) {
  db::Registry source,destination;
  CHECK(source.open(root/"source",db::StorageOptions::sustained(8))==db::Code::ok);
  CHECK(destination.open(root/"destination",db::StorageOptions::sustained(8))==db::Code::ok);
  std::string content(2*mesh::kMeshArtifactChunkBytes+37,'x');content[20]='\0';content.back()='z';
  const auto object=source.put(content);CHECK(object.code==db::Code::ok);
  mesh::WindStoreMeshArtifactStore source_adapter(source),destination_adapter(destination);
  mesh::MeshArtifactService server(source_adapter);mesh::ArtifactReceipt receipt;
  CHECK(server.describe(alice,object.hash,receipt)==ArtifactCode::ok&&receipt.bytes==content.size());
  mesh::ArtifactChunk chunk;CHECK(server.read(alice,object.hash,0,chunk)==ArtifactCode::ok&&chunk.count==mesh::kMeshArtifactChunkBytes&&!chunk.final);
  CHECK(server.read(alice,object.hash,content.size()+1,chunk)==ArtifactCode::invalid);
  CHECK(server.describe({"alice",0},object.hash,receipt)==ArtifactCode::invalid);
  {
    mesh::MeshArtifactReceiver partial(destination_adapter);bool complete=false;
    CHECK(partial.begin(alice,receipt,complete)==ArtifactCode::ok&&!complete);
    CHECK(partial.append(bob,receipt.sha256,0,{chunk.bytes.data(),chunk.count})==ArtifactCode::denied);
    CHECK(partial.append(alice,receipt.sha256,1,{chunk.bytes.data(),chunk.count})==ArtifactCode::conflict);
    CHECK(partial.append(alice,receipt.sha256,0,{chunk.bytes.data(),chunk.count})==ArtifactCode::ok);
    CHECK(partial.append(alice,receipt.sha256,0,{chunk.bytes.data(),chunk.count})==ArtifactCode::ok);
    CHECK(partial.finish(alice,receipt)==ArtifactCode::conflict);
    std::string missing;CHECK(!destination.get(receipt.sha256,missing));
  }
  FailArtifactCommit failures(destination_adapter);mesh::MeshArtifactReceiver receiver(failures);bool complete=false;
  CHECK(receiver.begin(alice,receipt,complete)==ArtifactCode::ok&&!complete&&receiver.received()==0);
  for(std::uint64_t offset=0;offset<receipt.bytes;){
    CHECK(server.read(alice,receipt.sha256,offset,chunk)==ArtifactCode::ok);
    CHECK(receiver.append(alice,receipt.sha256,offset,{chunk.bytes.data(),chunk.count})==ArtifactCode::ok);offset+=chunk.count;
  }
  CHECK(receiver.finish(alice,receipt)==ArtifactCode::persistence_failed);
  CHECK(receiver.finish(alice,receipt)==ArtifactCode::ok);
  CHECK(receiver.finish(alice,receipt)==ArtifactCode::ok&&destination.event_count()==1);
  std::string received;CHECK(destination.get(receipt.sha256,received)&&received==content);
  CHECK(receiver.begin(alice,receipt,complete)==ArtifactCode::ok&&complete);
  auto wrong=receipt;wrong.sha256=std::string(64,'a');
  CHECK(receiver.begin(alice,wrong,complete)==ArtifactCode::ok&&!complete);
  for(std::uint64_t offset=0;offset<receipt.bytes;){CHECK(server.read(alice,receipt.sha256,offset,chunk)==ArtifactCode::ok);
    CHECK(receiver.append(alice,wrong.sha256,offset,{chunk.bytes.data(),chunk.count})==ArtifactCode::ok);offset+=chunk.count;}
  CHECK(receiver.finish(alice,wrong)==ArtifactCode::corrupt&&!destination.get(wrong.sha256,received));
  wrong.bytes=mesh::kMaxMeshArtifactBytes+1;CHECK(receiver.begin(alice,wrong,complete)==ArtifactCode::invalid);
  destination.close();CHECK(destination.open(root/"destination")==db::Code::ok);
  CHECK(destination_adapter.completed(receipt,complete)==ArtifactCode::ok&&complete);
}
int main(int argc,char** argv) {
  CHECK(argc==2);const fs::path root=argv[1];fs::remove_all(root);
  replay_cases(root/"replay");archive_cases(root/"archive");artifact_cases(root/"artifact");
  std::cout<<"Mesh bounded replay, durable ACK/reconnect, archived history and verified artifact transfer PASS\n";
}
