#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
// Anaiero science live: native C++20 multirate controller over a bounded
// optional external solver stdio channel. No device or biological actuation.
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/stdio_live_solver.hpp"
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/services/epoch_scientific_frame_service.hpp"
#include "anaiero/services/live_solver_feature_services.hpp"
#include "anaiero/services/coupled_simulation_service.hpp"
#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
namespace cell=anaiero::cells;
namespace sci=anaiero::science;
namespace rt=anaiero::runtime;
namespace durable=christeye::living::durable;
static bool seed_live(const cell::Atlas& atlas,cell::ISpatialPort& spatial,
                      cell::World& world) {
  cell::CellBehaviorService builder(atlas,spatial);
  cell::Cell a{};a.id=3;a.kind=cell::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cell::Cell b=a;b.id=4;b.position={23,17,2};
  cell::Cell c=a;c.id=5;c.kind=cell::Kind::astrocyte;
  c.position={21,18,2};
  if(builder.add(a)!=cell::Status::ok||builder.add(b)!=cell::Status::ok||
     builder.add(c)!=cell::Status::ok||
     builder.encourage(3,4,700)!=cell::Status::ok)return false;
  world=builder.world();return true;
}
int run_science_live(int argc,char**argv) {
  if(argc!=7) {
    std::cerr<<"usage: anaiero science live <atlas.cevatlas> <windstore-dir> "
      <<"<epoch> <model-sha256> <synthetic|neuron|braincell>\n";
    return 2;
  }
  std::uint32_t epoch=0;
  const std::string_view number{argv[4]};
  const auto [end,error]=std::from_chars(number.data(),number.data()+number.size(),epoch);
  if(error!=std::errc{}||end!=number.data()+number.size()||epoch==0||
     epoch>4096||!sci::valid_sha256(argv[5]))return 2;
  const std::string_view mode{argv[6]};
  sci::Provider provider=sci::Provider::native_reference;
  if(mode=="neuron")provider=sci::Provider::neuron;
  else if(mode=="braincell")provider=sci::Provider::braincell;
  else if(mode!="synthetic")return 2;
  cell::Atlas atlas{};
  if(cell::files::load_checked_atlas(argv[2],atlas)!=cell::Status::ok)return 1;
  durable::Registry store;
  if(store.open(argv[3])!=durable::Code::ok)return 1;
  rt::WeightedQueue<> runtime_queue;rt::RuntimeService runtime(runtime_queue);
  rt::EpochRuntimeProjectionPort port(runtime);
  constexpr std::uint32_t source=64;
  if(!port.enroll({source,3,rt::Origin::simulated,true,false,false,0}))return 1;
  rt::WindStoreEpochObservationJournal journal(store,"science-live");
  rt::EpochObservationCodec epoch_codec;
  rt::EpochObservationDeliveryService delivery(epoch_codec,journal,port);
  if(!delivery.recover())return 1;
  const auto state=port.latest();
  for(std::size_t i=0;i<state.sources;++i)
    if(state.cursors[i].id==source&&state.cursors[i].epoch==epoch){
      std::cerr<<"source epoch already used: start a new external session/epoch\n";
      return 1;
    }
  // Persist the full numerical-model identity separately from fast features.
  // A session epoch is reserved even if the external model fails before its
  // first macro-step. This is evidence provenance, not peer authentication.
  for(const auto& event:store.events())
    if(event.kind=="SCIENTIFIC_MODEL_SESSION"&&event.entity=="science-live"&&
       event.source_tick==epoch){
      std::cerr<<"source epoch already has a model session record\n";
      return 1;
    }
  const std::string manifest=
    "CEVLIVEMODEL/1\nmodel_sha256 "+std::string(argv[5])+
    "\natlas_sha256 "+atlas.model_digest+
    "\nepoch "+std::to_string(epoch)+
    "\ndeclared_provider "+std::string(mode)+
    "\nelectrical_transform DIMENSIONLESS_PROXY_V1\n"+
    "environment_provider SYNTHETIC\nfast_step_ns 25000\n"+
    "environment_step_ns 100000\nend\n";
  const auto object=store.put(manifest);
  if(object.code!=durable::Code::ok)return 1;
  const auto registered=store.publish("SCIENTIFIC_MODEL_SESSION","science-live",
      object.hash,"SIMULATED",epoch,"simulation","live-model");
  if(registered.code!=durable::Code::ok)return 1;
  cell::BoundedPositionPort spatial;
  cell::World world{};
  if(!seed_live(atlas,spatial,world))return 1;
  sci::StdioLiveSolverExchange connection(std::cin,std::cout);
  sci::LiveElectricalFeatureService electrical(connection,argv[5],epoch,source,provider);
  sci::LiveEnvironmentFeatureService environment(connection,argv[5],epoch,65);
  sci::CoupledSimulationService engine(atlas,spatial,electrical);
  engine.set_environment(&environment);
  sci::EpochScientificFramePublisher publisher(delivery,source,epoch);
  engine.set_publisher(&publisher);
  sci::State initial{};initial.epoch=epoch;initial.source=source;
  if(engine.initialize(world,initial)!=sci::Code::ok)return 1;
  sci::Plan plan{};plan.condition=sci::Condition::astrocyte_coupled;
  sci::Result result{};
  if(engine.run(plan,result)!=sci::Code::ok){
    // A partial fast-step may already have advanced a remote solver. Abort
    // the session and require a new source epoch; do not silently retry.
    std::cerr<<"live exchange failed; external solver session must restart\n";
    return 1;
  }
  const auto projected=port.latest();
  if(publisher.published_count()!=112||projected.count<4)return 1;
  std::cout<<"RESULT {\"schema\":\"anaiero.science.live/1\","
           <<"\"domain\":\"DIGITAL_MODEL_OUTPUT_UNVERIFIED\","
           <<"\"epoch\":"<<epoch<<",\"macro_steps\":"<<result.steps
           <<",\"fast_steps\":"<<engine.state().fast_steps
           <<",\"observations\":"<<publisher.published_count()
           <<",\"projector_revision\":"<<projected.revision
           <<",\"retention_proxy\":"<<result.end_retention
           <<",\"windstore_events\":"<<store.event_count()
           <<",\"source_epochs\":"<<delivery.source_epochs()
           <<"}\n";
  std::cout.flush();
  return std::cout?0:1;
}
