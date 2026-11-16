#include "anaiero/infrastructure/sim3_ingress.hpp"
#include "anaiero/infrastructure/windstore_sink.hpp"
#include "anaiero/infrastructure/movement_router.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#define CHECK(x) do {if(!(x)){std::cerr<<"FAILED " #x << " at line " << __LINE__ << '\n';std::exit(1);}}while(false)
namespace rt=anaiero::runtime;
namespace sim=christeye::living::sim3;
namespace db=christeye::living::durable;
static rt::Observation value(std::uint32_t source,std::uint64_t seq,rt::Metric metric,
                             rt::Lane lane,std::uint64_t time=100){
  rt::Observation o{};
  o.colony=1;o.animon=7;o.source=source;o.clock_domain=1;
  o.sequence=seq;o.metric=metric;o.lane=lane;o.origin=rt::Origin::simulated;
  o.acquired_ns=time;o.value=12;o.urgency=100;o.importance=200;
  return o;
}
struct Listener final:rt::IUpdateObserver{
  rt::RuntimeService* runtime=nullptr;
  std::uint64_t calls=0,revision=0;
  void on_revision(std::uint64_t r,const rt::Observation&) noexcept override {
    ++calls;revision=r;
    CHECK(runtime->latest(100,1000).revision==r); // callback is after projector commit
  }
};
struct FailingJournal final:rt::IEventCommit{
  bool commit(const rt::Observation&) noexcept override {return false;}
};
static void test_wire(){
  const auto o=value(1,3,rt::Metric::digital_desire,rt::Lane::desire);
  std::array<std::byte,rt::kObservationFrame> buffer{};
  std::size_t count=0;
  CHECK(rt::encode_observation(o,buffer,count));
  CHECK(count==buffer.size());
  rt::Observation decoded{};
  CHECK(rt::decode_observation(buffer,decoded)==rt::Status::ok);
  CHECK(decoded.value==o.value&&decoded.metric==o.metric&&decoded.sequence==3);
  buffer[108]=std::byte{1}; // tamper with payload evidence tag (not a signature)
  CHECK(rt::decode_observation(buffer,decoded)==rt::Status::ok);
  buffer[56]=std::byte{0}; // invalid colony id
  CHECK(rt::decode_observation(buffer,decoded)==rt::Status::malformed_wire);
}
static void test_projection(){
  rt::RuntimeService r;
  CHECK(r.enroll({1,1,rt::Origin::simulated,true,true})==rt::Status::ok);
  CHECK(r.enroll({2,1,rt::Origin::external_unverified_readonly,true,false})==rt::Status::ok);
  Listener observer;observer.runtime=&r;r.set_observer(&observer);
  auto o=value(1,1,rt::Metric::energy,rt::Lane::telemetry);
  CHECK(r.post(o)==rt::Status::ok);
  CHECK(r.drain()==1);
  CHECK(observer.calls==1&&observer.revision==1);
  CHECK(r.latest(150,100).values[0].quality==rt::Quality::current);
  CHECK(r.latest(301,100).values[0].quality==rt::Quality::stale);
  CHECK(r.post(o)==rt::Status::ok);r.drain();
  CHECK(r.latest(100,100).revision==1);
  o.sequence=3;o.acquired_ns=300;
  CHECK(r.post(o)==rt::Status::ok);r.drain();
  CHECK(r.latest(300,100).sequence_gaps==1);
  CHECK(r.latest(300,100).values[0].quality==rt::Quality::degraded);
  o.sequence=2;o.acquired_ns=200;
  CHECK(r.post(o)==rt::Status::ok);r.drain();
  CHECK(r.latest(300,100).revision==2);
  o.source=2;o.origin=rt::Origin::external_unverified_readonly;o.metric=rt::Metric::digital_desire;
  CHECK(r.post(o)==rt::Status::unauthorized);
  o.metric=rt::Metric::energy;o.clock_domain=2;
  CHECK(r.post(o)==rt::Status::unauthorized);
  o.source=99;o.clock_domain=1;
  CHECK(r.post(o)==rt::Status::unauthorized);
}
static void test_movement_contract(){
  rt::RuntimeService runtime;
  CHECK(runtime.enroll({1,1,rt::Origin::simulated,false,false})==rt::Status::ok);
  rt::ObservationMovementRouter router(runtime);
  const rt::Movement transfer{1,2,1,1,rt::MovementKind::digital_data,17,1,20,11};
  CHECK(router.submit(transfer)==rt::Status::ok);
  CHECK(runtime.drain()==1);
  const auto state=runtime.latest(20,100);
  CHECK(state.count==1&&state.values[0].colony==2&&state.values[0].animon==1);
  CHECK(state.values[0].metric==rt::Metric::connection&&state.values[0].value==17);
  auto illegal=transfer;illegal.kind=static_cast<rt::MovementKind>(255);
  CHECK(router.submit(illegal)==rt::Status::invalid);
}
static void test_source_isolation(){
  rt::RuntimeService runtime;
  CHECK(runtime.enroll({1,1,rt::Origin::simulated,true,true})==rt::Status::ok);
  CHECK(runtime.enroll({2,1,rt::Origin::external_unverified_readonly,true,false})==rt::Status::ok);
  auto synthetic=value(1,1,rt::Metric::energy,rt::Lane::health);
  auto observed=value(2,1,rt::Metric::energy,rt::Lane::health);
  observed.origin=rt::Origin::external_unverified_readonly;
  observed.value=77;
  CHECK(runtime.post(synthetic)==rt::Status::ok);
  CHECK(runtime.post(observed)==rt::Status::ok);
  CHECK(runtime.drain()==2);
  const auto state=runtime.latest(100,1000);
  CHECK(state.count==2 && state.values[0].source!=state.values[1].source);
  CHECK(state.values[0].origin!=state.values[1].origin);
  auto desire=observed;
  desire.sequence=2;desire.metric=rt::Metric::digital_desire;
  CHECK(runtime.post(desire)==rt::Status::unauthorized);
}
static void test_priority_and_fairness(){
  rt::WeightedQueue<64> q;
  auto health=value(1,1,rt::Metric::energy,rt::Lane::health);
  auto desire=value(2,1,rt::Metric::digital_desire,rt::Lane::desire);
  desire.urgency=1000;
  auto fault=value(3,1,rt::Metric::acquisition_health,rt::Lane::fault);
  CHECK(q.push(health)==rt::Status::ok);
  CHECK(q.push(desire)==rt::Status::ok);
  CHECK(q.push(fault)==rt::Status::ok);
  rt::Observation out;
  CHECK(q.take(out)&&out.source==3); // fault notification immediately
  CHECK(q.take(out));CHECK(q.take(out));CHECK(!q.take(out));
  rt::WeightedQueue<10> score;
  auto low=value(4,1,rt::Metric::digital_desire,rt::Lane::desire);low.urgency=10;
  auto high=value(5,1,rt::Metric::digital_desire,rt::Lane::desire);high.urgency=900;
  CHECK(score.push(low)==rt::Status::ok);CHECK(score.push(high)==rt::Status::ok);
  CHECK(score.take(out)&&out.source==5);
  rt::WeightedQueue<64> mixed;
  for(unsigned i=1;i<=20;++i){auto e=value(1,i,rt::Metric::energy,rt::Lane::health);
    CHECK(mixed.push(e)==rt::Status::ok);
  }
  for(unsigned i=1;i<=12;++i){auto e=value(2,i,rt::Metric::digital_desire,rt::Lane::desire);
    CHECK(mixed.push(e)==rt::Status::ok);
  }
  std::uint64_t a=0,b=0;unsigned desire_seen=0;
  while(mixed.take(out)){
    if(out.source==1)CHECK(out.sequence==++a);
    else{CHECK(out.sequence==++b);++desire_seen;}
  }
  CHECK(a==20&&b==12&&desire_seen==12);
  rt::WeightedQueue<2> bounded;
  CHECK(bounded.push(health)==rt::Status::ok);
  CHECK(bounded.push(desire)==rt::Status::ok);
  CHECK(bounded.push(fault)==rt::Status::queue_full);
}
static void test_persistence_and_concurrency(){
  rt::RuntimeService r;CHECK(r.enroll({1,1,rt::Origin::simulated,true,true})==rt::Status::ok);
  FailingJournal bad;r.set_journal(&bad);
  CHECK(r.post(value(1,1,rt::Metric::energy,rt::Lane::health))==rt::Status::ok);
  CHECK(r.drain()==1);CHECK(r.latest(100,100).revision==0);
  CHECK(r.pending()==1&&r.commit_failures()==1);
  r.set_journal(nullptr);
  CHECK(r.drain()==1&&r.pending()==0);
  CHECK(r.latest(100,100).revision==1);
  std::atomic<bool> running{true}, started{false};std::atomic<unsigned> reads{0};
  std::thread reader([&](){started=true;while(running.load()){
    auto snapshot=r.latest(1000,10000);
    CHECK(snapshot.count<=rt::kMaxMetrics&&snapshot.revision==snapshot.committed_updates);
    ++reads;
  }});
  while(!started.load())std::this_thread::yield();
  for(unsigned i=2;i<=100;++i){
    const auto o=value(1,i,rt::Metric::energy,rt::Lane::health,100+i);
    CHECK(r.post(o)==rt::Status::ok);
    if(i%20==0)r.drain(20);
  }
  while(r.pending())r.drain();
  running=false;reader.join();
  CHECK(r.latest(1000,10000).revision==100);
  CHECK(reads>0);
}
static void test_sim3_input(const std::filesystem::path& root){
  sim::Runner run;
  const auto temp=std::filesystem::temp_directory_path()/"anaiero-runtime-state-test";
  std::error_code ec;std::filesystem::remove_all(temp,ec);
  CHECK(run.open(root/"examples/connected-gardens.cev",temp)==sim::Result::ok);
  rt::RuntimeService service;rt::Sim3Ingress input(service);
  CHECK(input.enroll(run.configuration())==rt::Status::ok);
  CHECK(input.import(run.configuration(),run.state())==rt::Status::ok);
  while(service.pending())service.drain(64);
  auto first=service.latest(0,10000000);
  CHECK(first.revision>0 && first.count>0 && first.sources==2);
  CHECK(run.step(false)==sim::Result::ok);
  CHECK(input.import(run.configuration(),run.state())==rt::Status::ok);
  while(service.pending())service.drain(64);
  auto next=service.latest(1000000,10000000);
  CHECK(next.revision>first.revision);
  CHECK(next.sequence_gaps==0);
  // Commit a synthetic observation through actual durable WindStore integration.
  db::Registry database;
  CHECK(database.open(temp/"projector-ledger")==db::Code::ok);
  rt::WindStoreSink sink(database);
  rt::RuntimeService durable_service;
  CHECK(durable_service.enroll({11,1,rt::Origin::simulated,true,true})==rt::Status::ok);
  durable_service.set_journal(&sink);
  CHECK(durable_service.post(value(11,1,rt::Metric::energy,rt::Lane::health))==rt::Status::ok);
  durable_service.drain();
  CHECK(durable_service.latest(100,100).revision==1);
  CHECK(database.event_count()==1 && database.audit_disk());
  std::filesystem::remove_all(temp,ec); // after store releases lock
}
int main(int argc,char** argv){
  CHECK(argc==2);
  test_wire();test_projection();test_priority_and_fairness();test_movement_contract();test_source_isolation();
  test_persistence_and_concurrency();test_sim3_input(argv[1]);
  std::cout<<"Anaiero ColonyState projector / priority scheduler / bus / persistence tests passed\n";
}
