// Session records are recoverable intent, not merely printable diagnostics.
#include "anaiero/infrastructure/session_files.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#define CHECK(e) do{if(!(e)){std::cerr<<"FAILED " #e <<" at "<<__LINE__<<"\n";std::abort();}}while(false)
namespace ns=anaiero::session;
namespace sf=anaiero::session::files;
namespace eng=anaiero::engine;
namespace cells=anaiero::cells;
namespace opt=anaiero::optimization;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
static void write(const fs::path& path,std::string_view bytes){
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  CHECK(bool(out));out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
  out.close();CHECK(bool(out));
}
static std::string read(const fs::path& path){
  std::string bytes;CHECK(db::read_bounded(path,1024*1024,bytes));return bytes;
}
static eng::Snapshot snapshot(std::uint32_t id,std::uint64_t epoch,std::uint64_t tick,
                              std::uint64_t revision,std::string_view sha){
  eng::Snapshot value{};value.id=id;value.source_epoch=epoch;value.tick=tick;
  value.revision=revision;value.participants=4;value.connections=1;
  value.energy=526;value.learning_proxy=40;
  CHECK(sha.empty()||sha.size()==64);
  std::copy(sha.begin(),sha.end(),value.state_sha256.begin());return value;
}
static ns::Record sample(const fs::path& repo){
  cells::Atlas atlas{};cells::World before{};
  CHECK(cells::files::load_checked_atlas(repo/"examples/cells/crest-blood-neural.cevatlas",atlas)==cells::Status::ok);
  CHECK(cells::files::read_world(repo/"examples/cells/sample-saved-culture.cevcells",atlas,before)==cells::Status::ok);
  CHECK(before.intent_count==1&&before.intents[0].from==3&&before.intents[0].to==4);
  const auto original=cells::files::serialize_world(before,atlas);
  const auto source_sha=christeye::living::plans::sha256_hex(original);
  auto after=before;after.intents[0].guidance_weight=900;++after.generation;
  const auto target=cells::files::serialize_world(after,atlas);
  const auto target_sha=christeye::living::plans::sha256_hex(target);
  ns::Record r{};r.revision=7;r.changes=2;r.target_cycles=20;
  r.configuration_sha256=std::string(64,'a');
  auto& p=r.coordinator;p.initialized=true;p.engine_count=2;p.completed_cycles=18;
  p.default_inputs={500,10,30,true};
  p.engines[0]={101,61,3,eng::Kind::sw5,1,atlas.model_digest,
    snapshot(101,3,before.tick,before.generation,source_sha),{500,10,30,true},eng::Status::ok};
  p.engines[1]={202,62,7,eng::Kind::sim3,2,std::string(64,'b'),
    snapshot(202,7,9,9,std::string(64,'c')),{},eng::Status::ok};
  r.evidence[0].next_sequence=96;r.evidence[0].published=p.engines[0].last;
  r.evidence[1].next_sequence=51;r.evidence[1].published=p.engines[1].last;
  opt::Candidate route{1,101,3,4,opt::RouteKind::digital_signal,2,1,900};
  r.evidence[0].route={true,true,700,2,route};
  r.publication={true,0,2,snapshot(101,3,after.tick,after.generation,target_sha)};
  r.change={true,0,source_sha,target};
  auto& q=r.proposal;q.engine_id=101;q.horizon=4;q.option={route,900,1};
  auto& selection=q.selection;selection.candidate={1,1,2,1,1};
  selection.approved=true;selection.score=120;selection.commit_id=2;
  selection.forecast.source_revision=before.generation;
  selection.forecast.source_state_sha256=source_sha;selection.forecast.valid=true;
  // SW5 numerical forecasts intentionally do not claim checkpoint hashes.
  selection.forecast.baseline=snapshot(101,3,before.tick+4,before.generation,{});
  selection.forecast.trial=selection.forecast.baseline;
  selection.forecast.trial.connections=2;selection.forecast.trial.learning_proxy=80;
  CHECK(sf::valid_record(r));return r;
}
static std::string replaced(std::string value,std::string_view before,std::string_view after){
  const auto found=value.find(before);CHECK(found!=std::string::npos);
  value.replace(found,before.size(),after);return value;
}
static void canonical_and_invalid(const ns::Record& expected){
  const auto wire=sf::encode_record(expected);CHECK(!wire.empty());
  ns::Record actual{};CHECK(sf::decode_record(wire,actual));
  CHECK(sf::encode_record(actual)==wire);
  CHECK(actual.publication.active&&actual.publication.metric_index==2);
  CHECK(actual.change.active&&actual.change.target_checkpoint==expected.change.target_checkpoint);
  CHECK(actual.proposal.selection.approved&&actual.proposal.selection.commit_id==2);
  CHECK(actual.proposal.selection.forecast.source_state_sha256==expected.change.source_sha256);
  CHECK(actual.evidence[0].route.prior_existed&&actual.evidence[0].route.prior_guidance==700);
  auto reject=[&](std::string_view bad){
    auto output=expected;output.revision=777;const auto untouched=sf::encode_record(output);
    CHECK(!sf::decode_record(bad,output));CHECK(sf::encode_record(output)==untouched);
  };
  reject({});reject(wire.substr(0,wire.size()/2));reject(wire.substr(0,wire.size()-1));
  reject(wire.substr(0,wire.size()-5));reject(wire+"extra\n");reject(wire+"\n");
  reject(replaced(wire,"CEVSESSIONSTATE/1","CEVSESSIONSTATE/2"));
  reject(replaced(wire,"\n7 2 20 ","\n+7 2 20 "));
  reject(replaced(wire,"\n7 2 20 ","\n18446744073709551616 2 20 "));
  for(const auto* progress:{"1 18 0 0 1 0 0\n","1 18 9 0 1 0 0\n",
      "1 18 2 3 1 0 0\n","1 18 2 0 2 0 0\n","2 18 2 0 1 0 0\n"})
    reject(replaced(wire,"1 18 2 0 1 0 0\n",progress));
  reject(std::string(1024*1024+1,'x'));
  auto invalid=[&](const ns::Record& value){CHECK(!sf::valid_record(value));CHECK(sf::encode_record(value).empty());};
  auto changed=expected;changed.coordinator.engine_count=9;invalid(changed);
  changed=expected;changed.coordinator.engines[1].id=changed.coordinator.engines[0].id;invalid(changed);
  changed=expected;changed.coordinator.engines[1].source=changed.coordinator.engines[0].source;invalid(changed);
  changed=expected;changed.coordinator.engines[0].last.id=999;invalid(changed);
  changed=expected;changed.coordinator.engines[0].last.source_epoch=99;invalid(changed);
  changed=expected;changed.coordinator.engines[0].last.state_sha256.fill('a');invalid(changed);
  changed=expected;changed.coordinator.default_inputs.digital_signal=1001;invalid(changed);
  changed=expected;changed.coordinator.engines[0].cycle_inputs.stress_proxy=-1;invalid(changed);
  changed=expected;changed.target_cycles=17;invalid(changed);
  changed=expected;changed.publication.engine_index=2;invalid(changed);
  changed=expected;changed.publication.metric_index=6;invalid(changed);
  changed=expected;changed.publication.snapshot.source_epoch=4;invalid(changed);
  changed=expected;changed.evidence[0].next_sequence=UINT64_MAX;invalid(changed);
  changed=expected;changed.change.target_checkpoint.back()='x';invalid(changed);
  changed=expected;changed.change.source_sha256=std::string(64,'f');invalid(changed);
  changed=expected;changed.evidence[0].route.selected.guidance_weight=899;invalid(changed);
  changed=expected;changed.proposal.selection.candidate.cost=1001;invalid(changed);
  changed=expected;changed.proposal.selection.forecast.trial.tick++;invalid(changed);
  changed=expected;changed.proposal.selection.forecast.source_state_sha256="bad";invalid(changed);
  changed=expected;changed.proposal.engine_id=999;invalid(changed);
  changed=expected;changed.proposal.selection.approved=false;invalid(changed);
  changed=expected;changed.coordinator.cycle_active=true;invalid(changed);
  changed=expected;changed.publication.engine_index=1;invalid(changed);
  // A saved coordinator publication is also a legal restart point. Route
  // mutations have completed before another controller cycle can be active.
  auto publishing=expected;publishing.change={};
  publishing.coordinator.cycle_active=true;publishing.coordinator.publication_pending=true;
  publishing.coordinator.engines[0].last=publishing.publication.snapshot;
  publishing.coordinator.engines[0].last_status=eng::Status::publish_failed;
  publishing.coordinator.pending_snapshot=publishing.publication.snapshot;
  CHECK(sf::decode_record(sf::encode_record(publishing),actual));
  CHECK(actual.coordinator.publication_pending&&actual.coordinator.next_slot==0);
  publishing.coordinator.pending_snapshot.id=202;invalid(publishing);
  // Initial records have no public snapshot, pending mutation or proposal.
  auto initial=expected;initial.revision=0;initial.changes=0;
  initial.publication={};initial.change={};initial.proposal={};
  for(std::size_t i=0;i<initial.coordinator.engine_count;++i)initial.evidence[i]={};
  CHECK(sf::decode_record(sf::encode_record(initial),actual));
}
static std::string configuration(unsigned period=100,std::string_view first_id="101",
                                 std::string_view first_source="61",std::string_view epoch="3"){
  std::ostringstream out;
  out<<"CEVSESSION/1\nname record-tests\nperiod_ms "<<period<<"\nengine "<<first_id
     <<" sw5 "<<first_source<<' '<<epoch<<" 1 \"model atlas.cevatlas\" \"initial state.cevcells\" 500 10 30 1\n"
     <<"engine 202 sim3 62 7 2 \"sim3.model\" \"-\" 0 0 0 0\nend\n";
  return out.str();
}
static void config_pins(const fs::path& repo,const fs::path& root){
  fs::create_directories(root);
  const auto model=read(repo/"examples/cells/crest-blood-neural.cevatlas");
  const auto state=read(repo/"examples/cells/sample-saved-culture.cevcells");
  write(root/"model atlas.cevatlas",model);write(root/"initial state.cevcells",state);
  write(root/"sim3.model",read(repo/"examples/sim3/synthetic-model.cevmodel"));
  const auto path=root/"test.cevsession";write(path,configuration());
  ns::Configuration first{};std::string error;
  CHECK(sf::read_configuration(path,first,error)&&error.empty());
  CHECK(first.count==2&&first.engines[0].id==101&&first.engines[1].kind==eng::Kind::sim3);
  CHECK(fs::path(first.engines[0].model_path).is_absolute()&&db::valid_digest(first.digest));
  ns::Configuration second{};CHECK(sf::read_configuration(path,second,error)&&first.digest==second.digest);
  std::string crlf;for(char c:configuration()){if(c=='\n')crlf+='\r';crlf+=c;}
  write(path,crlf);CHECK(sf::read_configuration(path,second,error)&&second.digest==first.digest);
  write(path,configuration(200));CHECK(sf::read_configuration(path,second,error)&&second.digest!=first.digest);
  write(path,configuration(100,"101","61","4"));
  CHECK(sf::read_configuration(path,second,error)&&second.digest!=first.digest);
  write(path,configuration());write(root/"model atlas.cevatlas",model+"\n");
  CHECK(sf::read_configuration(path,second,error)&&second.digest!=first.digest);
  write(root/"model atlas.cevatlas",model);write(root/"initial state.cevcells",state+"\n");
  CHECK(sf::read_configuration(path,second,error)&&second.digest!=first.digest);
  write(root/"initial state.cevcells",state);
  auto reject=[&](const std::string& bytes){
    write(path,bytes);auto output=first;
    CHECK(!sf::read_configuration(path,output,error)&&!error.empty());
    CHECK(output.digest==first.digest&&output.engines[0].id==101);
  };
  reject(configuration(9));reject(configuration(60001));
  reject(configuration(100,"-1"));reject(configuration(100,"+101"));
  reject(configuration(100,"4294967296"));reject(configuration(100,"101","-1"));
  reject(configuration(100,"101","61","-1"));
  reject(replaced(configuration(),"engine 202 sim3 62","engine 101 sim3 62"));
  reject(replaced(configuration(),"engine 202 sim3 62","engine 202 sim3 61"));
  reject(replaced(configuration(),"sw5 61 3 1","sw5 61 3 65"));
  reject(replaced(configuration(),"\"-\" 0 0 0 0","\"-\" 1 0 0 0"));
  reject(replaced(configuration(),"model atlas.cevatlas","missing.cevatlas"));
  reject(replaced(configuration(),"sw5 61","scientific 61"));
  reject(configuration()+"trailing\n");reject("CEVSESSION/1\nname empty\nperiod_ms 100\nend\n");
  std::ostringstream maximum;maximum<<"CEVSESSION/1\nname maximal\nperiod_ms 100\n";
  for(unsigned i=1;i<=8;++i)
    maximum<<"engine "<<i<<" sw5 "<<100+i<<" 1 1 \"model atlas.cevatlas\" \"initial state.cevcells\" 0 0 0 0\n";
  write(path,maximum.str()+"end\n");
  CHECK(sf::read_configuration(path,second,error)&&second.count==8);
  reject(maximum.str()+"engine 9 sw5 109 1 1 \"model atlas.cevatlas\" \"initial state.cevcells\" 0 0 0 0\nend\n");
  write(path,configuration());CHECK(sf::read_configuration(path,second,error)&&second.digest==first.digest);
}
static void record_store(const ns::Record& source,const fs::path& root){
  const auto path=root/"records";
  {
    db::Registry database;CHECK(database.open(path)==db::Code::ok);
    sf::WindStoreRecordStore first(database,"session-test"),stale(database,"session-test");
    ns::Record ignored{};bool exists=true;
    CHECK(first.load(ignored,exists)&&!exists);CHECK(stale.load(ignored,exists)&&!exists);
    auto a=source;a.revision=0;auto b=a;
    CHECK(first.save(a)&&a.revision==1&&database.event_count()==1);
    CHECK(!stale.save(b)&&b.revision==0&&database.event_count()==1);
    CHECK(stale.load(b,exists)&&exists&&b.revision==1);
    CHECK(sf::encode_record(a)==sf::encode_record(b));
    CHECK(stale.save(b)&&b.revision==2&&database.event_count()==2);
    CHECK(!first.save(a)&&a.revision==1&&database.event_count()==2);
    const auto before=database.event_count();
    ns::Record again{};CHECK(stale.load(again,exists)&&exists&&again.revision==2);
    CHECK(stale.load(again,exists)&&exists&&database.event_count()==before);
    auto invalid=b;invalid.coordinator.engine_count=9;
    CHECK(!stale.save(invalid)&&invalid.revision==2&&database.event_count()==before);
    invalid=b;invalid.configuration_sha256=std::string(64,'f');CHECK(!stale.save(invalid));
    invalid=b;invalid.coordinator.engines[1].source=99;CHECK(!stale.save(invalid));
    invalid=b;invalid.evidence[0].next_sequence--;CHECK(!stale.save(invalid));
    CHECK(database.event_count()==before&&database.audit_disk());
  }
  {
    db::Registry database;CHECK(database.open(path)==db::Code::ok);
    sf::WindStoreRecordStore store(database,"session-test");ns::Record loaded{};bool exists=false;
    CHECK(store.load(loaded,exists)&&exists&&loaded.revision==2);
    auto expected=source;expected.revision=2;
    CHECK(sf::encode_record(loaded)==sf::encode_record(expected));
    CHECK(database.event_count()==2&&database.audit_disk());
    database.close();CHECK(!store.save(loaded)&&loaded.revision==2);
    CHECK(!store.load(loaded,exists)&&!exists);
  }
  // A CAS-valid event can still contain an invalid or discontinuous manifest.
  for(unsigned mode=0;mode<4;++mode){
    db::Registry database;CHECK(database.open(root/("bad-"+std::to_string(mode)))==db::Code::ok);
    sf::WindStoreRecordStore store(database,"session-test");auto first=source;first.revision=0;
    CHECK(store.save(first));
    auto next=first;next.revision=mode==0?3:2;
    std::uint64_t event_revision=mode==0?3:2;
    if(mode==2)next.revision=3; // event/payload revision disagreement
    if(mode==3){next.revision=1;event_revision=1;next.changes=3;}
    auto wire=sf::encode_record(next);CHECK(!wire.empty());
    if(mode==1)wire.resize(wire.size()-8);
    const auto stored=database.put(wire);CHECK(stored.code==db::Code::ok);
    CHECK(database.publish("CULTURE_CHECKPOINT","session-test",stored.hash,
      "SIMULATED",event_revision,"session","anaiero-session").code==db::Code::ok);
    auto output=source;output.revision=777;const auto unchanged=sf::encode_record(output);bool exists=true;
    CHECK(!store.load(output,exists)&&!exists&&sf::encode_record(output)==unchanged);
    CHECK(database.audit_disk());
  }
}
static void checkpoint_store(const fs::path& root){
  const auto path=root/"catalog";const std::string model(64,'d');
  const std::string bytes="model-pinned immutable checkpoint\n";
  const auto sha=christeye::living::plans::sha256_hex(bytes);
  eng::Descriptor descriptor{301,71,9,eng::Kind::sim3,model};
  auto state=snapshot(301,9,0,0,sha);
  {
    db::Registry database;CHECK(database.open(path)==db::Code::ok);
    eng::WindStoreCheckpointCatalog catalog(database);sf::CatalogCheckpointStore store(catalog);
    CHECK(store.save(descriptor,state,bytes)&&database.event_count()==1);
    CHECK(store.save(descriptor,state,bytes)&&database.event_count()==1);
    auto invalid=state;invalid.id=302;CHECK(!store.save(descriptor,invalid,bytes));
    invalid=state;invalid.source_epoch=10;CHECK(!store.save(descriptor,invalid,bytes));
    invalid=state;invalid.state_sha256.fill('a');CHECK(!store.save(descriptor,invalid,bytes));
    auto other=descriptor;other.source=0;CHECK(!store.save(other,state,bytes));
    auto conflict=snapshot(301,9,0,0,christeye::living::plans::sha256_hex("different"));
    CHECK(!store.save(descriptor,conflict,"different")&&database.event_count()==1);
    auto newer=state;newer.revision=4;newer.tick=4;
    CHECK(store.save(descriptor,newer,bytes)&&database.event_count()==2);
    auto older=state;older.revision=2;older.tick=2;
    CHECK(!store.save(descriptor,older,bytes)&&database.event_count()==2);
    CHECK(database.audit_disk());
  }
  {
    db::Registry database;CHECK(database.open(path)==db::Code::ok);
    eng::WindStoreCheckpointCatalog catalog(database);sf::CatalogCheckpointStore store(catalog);
    auto newer=state;newer.revision=4;newer.tick=4;
    CHECK(store.save(descriptor,newer,bytes)&&database.event_count()==2);
    eng::CheckpointRecord found{};std::string recovered;
    CHECK(catalog.latest(301,9,found,recovered)&&found.revision==5&&recovered==bytes);
    CHECK(found.model_sha256==model&&found.source==71&&database.audit_disk());
  }
}
int main(int argc,char** argv){
  CHECK(argc==3);const auto repo=fs::path(argv[1]);const auto root=fs::path(argv[2]);
  fs::remove_all(root);fs::create_directories(root);
  const auto record=sample(repo);
  canonical_and_invalid(record);config_pins(repo,root/"configuration");
  record_store(record,root);checkpoint_store(root);
  std::cout<<"session canonical records, semantic bounds, pinned configuration, atomic reopen and idempotent checkpoints PASS\n";
}
