// Offline export validates the sealed native configuration and committed
// manifest without constructing an engine or invoking a solver process.
#include "anaiero/infrastructure/native_session_mesh_export.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace mesh=anaiero::mesh;
namespace session=anaiero::session;
namespace engine=anaiero::engine;
namespace runtime=anaiero::runtime;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
static void write(const fs::path& path,std::string_view bytes){std::ofstream out(path,std::ios::binary);CHECK(out.good());out<<bytes;CHECK(out.good());}
struct Fixture {
  fs::path root,configuration,state;
  session::Configuration config;
  session::Record record;
  explicit Fixture(bool sustained=true,bool legacy_record=false){
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    root=fs::temp_directory_path()/("anaiero-mesh-export-"+std::to_string(stamp));CHECK(fs::create_directories(root));
    configuration=root/"session.cevsession";state=root/"state";
    write(root/"model.cevatlas","local fixture model identity\n");write(root/"initial.cevcells","local fixture initial identity\n");
    write(configuration,std::string(sustained?"CEVSESSION/2\n":"CEVSESSION/1\n")+
      "name export-fixture\nperiod_ms 10\n"+(sustained?"storage segmented 8\n":"")+
      "engine 101 sw5 61 1 1 \"model.cevatlas\" \"initial.cevcells\" 0 0 0 0\nend\n");
    std::string error;CHECK(session::files::read_configuration(configuration,config,error));
    record.schema=legacy_record?1u:2u;record.revision=1;record.configuration_sha256=config.digest;
    auto& coordinator=record.coordinator;coordinator.initialized=true;coordinator.engine_count=1;
    engine::Snapshot snapshot;snapshot.id=101;snapshot.source_epoch=1;snapshot.revision=1;snapshot.participants=2;
    const std::string hash(64,'b');std::copy(hash.begin(),hash.end(),snapshot.state_sha256.begin());
    coordinator.engines[0]={101,61,1,engine::Kind::sw5,1,std::string(64,'a'),snapshot,{},engine::Status::ok};
    record.contracts[0]={"fixture/1",1,1,1,101,0,{},true,true,true,true};
    record.evidence[0].next_sequence=6;record.evidence[0].published=snapshot;
    CHECK(session::files::valid_record(record));
  }
  ~Fixture(){std::error_code ec;fs::remove_all(root,ec);}
  runtime::Observation observation(std::uint64_t sequence=1)const {
    runtime::Observation o;o.source=61;o.source_epoch=1;o.colony=101;o.clock_domain=config.sustained_storage?101u:1u;
    o.sequence=sequence;o.metric=runtime::Metric::activity;o.value=2;return o;
  }
  void persist(const std::vector<runtime::Observation>& observations){
    db::Registry metadata,evidence;CHECK(metadata.open(state/"session",db::StorageOptions::sustained(8))==db::Code::ok);
    CHECK(evidence.open(state/"evidence",db::StorageOptions::sustained(8))==db::Code::ok);
    const auto identity=metadata.put("CEVSESSIONIDENTITY/1\n"+config.name+'\n'+config.digest+'\n');CHECK(identity.code==db::Code::ok);
    CHECK(metadata.publish("CULTURE_CHECKPOINT","native-session-root",identity.hash,"SIMULATED",1,"identity","anaiero-session-host").code==db::Code::ok);
    const auto bytes=session::files::encode_record(record);CHECK(!bytes.empty());const auto manifest=metadata.put(bytes);CHECK(manifest.code==db::Code::ok);
    CHECK(metadata.publish("CULTURE_CHECKPOINT","session-"+config.name,manifest.hash,"SIMULATED",record.revision,"session","anaiero-session").code==db::Code::ok);
    runtime::EpochObservationCodec codec;
    for(const auto& observation:observations){std::array<std::byte,runtime::kEpochObservationFrame> wire{};CHECK(codec.encode(observation,wire));
      const auto object=evidence.put(std::string_view(reinterpret_cast<const char*>(wire.data()),wire.size()));CHECK(object.code==db::Code::ok);
      CHECK(evidence.publish("OBSERVATION_RECORDED",config.name,object.hash,"SIMULATED",observation.sequence,
        "epoch-"+std::to_string(observation.source_epoch),"source-"+std::to_string(observation.source)).code==db::Code::ok);}
  }
  bool export_to(std::vector<runtime::Observation>& received,std::uint64_t& count,std::string& error){
    return mesh::export_native_session(configuration,state,[&](const runtime::Observation& o){received.push_back(o);return true;},error,count);
  }
};
static void valid_versions(){
  for(const auto variant:{0,1,2}){
    Fixture fixture(variant==2,variant==0);fixture.persist({fixture.observation(),fixture.observation(2)});
    std::vector<runtime::Observation> output;std::uint64_t count=0;std::string error;
    CHECK(fixture.export_to(output,count,error));CHECK(count==2&&output.size()==2);
    CHECK(output[0].clock_domain==(variant==2?101u:1u));
  }
}
static void identity_and_scope(){
  for(unsigned kind=0;kind<8;++kind){
    Fixture fixture;auto o=fixture.observation();
    switch(kind){
      case 0:fixture.record.configuration_sha256=std::string(64,'f');break;
      case 1:++fixture.record.coordinator.engines[0].source;break;
      case 2:fixture.record.coordinator.engine_count=2;fixture.record.coordinator.engines[1]=fixture.record.coordinator.engines[0];
        ++fixture.record.coordinator.engines[1].id;++fixture.record.coordinator.engines[1].source;++fixture.record.coordinator.engines[1].last.id;
        fixture.record.contracts[1]=fixture.record.contracts[0];fixture.record.evidence[1]=fixture.record.evidence[0];++fixture.record.evidence[1].published.id;break;
      case 3:++o.source;break;
      case 4:++o.source_epoch;break;
      case 5:++o.colony;break;
      case 6:++o.clock_domain;break;
      default:o.sequence=6;break;
    }
    CHECK(session::files::valid_record(fixture.record));fixture.persist({o});
    std::vector<runtime::Observation> output;std::uint64_t count=0;std::string error;
    CHECK(!fixture.export_to(output,count,error));CHECK(count==0&&output.empty()&&!error.empty());
  }
}
static void committed_pending_prefix(){
  {Fixture fixture;fixture.record.evidence[0].next_sequence=1;fixture.record.evidence[0].published={};
    fixture.record.publication={true,0,0,fixture.record.coordinator.engines[0].last};
    fixture.persist({fixture.observation()});std::vector<runtime::Observation> output;std::uint64_t count=0;std::string error;
    CHECK(fixture.export_to(output,count,error)&&count==1);}
  {Fixture fixture;fixture.record.evidence[0].next_sequence=1;fixture.record.evidence[0].published={};
    fixture.record.publication={true,0,0,fixture.record.coordinator.engines[0].last};
    fixture.persist({fixture.observation(),fixture.observation(2)});std::vector<runtime::Observation> output;std::uint64_t count=0;std::string error;
    CHECK(!fixture.export_to(output,count,error));CHECK(count==1&&output.size()==1);}
  {Fixture fixture;fixture.record.publication={true,0,0,fixture.record.coordinator.engines[0].last};
    std::vector<runtime::Observation> observations;for(std::uint64_t i=1;i<=6;++i)observations.push_back(fixture.observation(i));
    fixture.persist(observations);std::vector<runtime::Observation> output;std::uint64_t count=0;std::string error;
    CHECK(fixture.export_to(output,count,error)&&count==6);}
}
static void requires_offline_owner(){
  Fixture fixture;fixture.persist({fixture.observation()});db::Registry owned;
  CHECK(owned.open(fixture.state/"session")==db::Code::ok);
  std::vector<runtime::Observation> output;std::uint64_t count=0;std::string error;
  CHECK(!fixture.export_to(output,count,error));CHECK(count==0&&output.empty());
}
int main(){valid_versions();identity_and_scope();committed_pending_prefix();requires_offline_owner();
  std::cout<<"mesh native export passed\n";}
