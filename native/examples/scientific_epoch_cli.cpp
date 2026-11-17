// SW6.2 diagnostic: two sources, repeated source after epoch restart, replay.
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
namespace cell=anaiero::cells;
namespace rt=anaiero::runtime;
namespace dur=christeye::living::durable;
int run_science_epoch_demo(int argc,char**argv) {
  if(argc!=4){
    std::cerr<<"usage: anaiero science epoch-demo <atlas.cevatlas> <store-dir>\n";
    return 2;
  }
  cell::Atlas atlas{};
  if(cell::files::load_checked_atlas(argv[2],atlas)!=cell::Status::ok)return 1;
  std::uint64_t committed=0;
  {
    dur::Registry store;
    if(store.open(argv[3])!=dur::Code::ok)return 1;
    rt::RuntimeService runtime;
    rt::EpochRuntimeProjectionPort projector(runtime);
    if(!projector.enroll({71,3,rt::Origin::simulated,true,false,false,0})||
       !projector.enroll({72,3,rt::Origin::simulated,true,false,false,0}))
      return 1;
    rt::WindStoreEpochObservationJournal journal(store,"science-recovery");
    rt::EpochObservationCodec codec;
    rt::EpochObservationDeliveryService service(codec,journal,projector);
    if(!service.recover())return 1;
    if(store.event_count()==0){
      auto send=[&](std::uint32_t src,std::uint64_t epoch,
        std::uint64_t sequence,rt::Metric metric,std::int32_t value){
        rt::Observation o{};
        o.source=src;o.source_epoch=epoch;o.colony=1;o.animon=3;
        o.clock_domain=3;o.origin=rt::Origin::simulated;
        o.sequence=sequence;o.metric=metric;o.lane=rt::Lane::evidence;
        o.acquired_ns=epoch*10000000ULL+sequence*1000000ULL;
        o.value=value;o.evidence_tag=epoch;
        return service.submit(o);
      };
      if(!send(71,1,1,rt::Metric::energy,100)||
         !send(72,1,1,rt::Metric::activity,40)||
         !send(71,1,2,rt::Metric::digital_learning,7)||
         !send(71,2,1,rt::Metric::energy,130))return 1;
    }
    committed=store.event_count();
  }
  // Open again as a new process would. Recovery comes only from durable
  // journal packets, never from the previous in-memory projector.
  dur::Registry reopened;
  if(reopened.open(argv[3])!=dur::Code::ok)return 1;
  rt::RuntimeService restored;
  rt::EpochRuntimeProjectionPort projector(restored);
  if(!projector.enroll({71,3,rt::Origin::simulated,true,false,false,0})||
     !projector.enroll({72,3,rt::Origin::simulated,true,false,false,0}))return 1;
  rt::WindStoreEpochObservationJournal journal(reopened,"science-recovery");
  rt::EpochObservationCodec codec;
    rt::EpochObservationDeliveryService service(codec,journal,projector);
  if(!service.recover())return 1;
  const auto latest=projector.latest();
  std::size_t stale=0,current=0;
  for(std::size_t i=0;i<latest.count;++i){
    if(latest.values[i].quality==rt::Quality::stale)++stale;
    if(latest.values[i].quality==rt::Quality::current)++current;
  }
  std::cout<<"{\"schema\":\"anaiero.science.epochs/1\","
           <<"\"journal_events\":"<<committed
           <<",\"revision\":"<<latest.revision
           <<",\"source_epochs\":"<<service.source_epochs()
           <<",\"metrics\":"<<latest.count
           <<",\"stale\":"<<stale<<",\"current\":"<<current<<"}\n";
  return reopened.audit_disk()&&committed==4&&latest.revision==4&&
    stale==2&&current==2?0:1;
}
