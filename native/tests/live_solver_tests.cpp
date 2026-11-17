// Native live solver port conformance and multirate coordination.
#include "anaiero/infrastructure/stdio_live_solver.hpp"
#include "anaiero/services/live_solver_feature_services.hpp"
#include "anaiero/services/coupled_simulation_service.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#ifdef assert
#undef assert
#endif
#define assert(x) do{if(!(x)){std::cerr<<"FAILED " #x <<" at "<<__LINE__<<"\n";std::abort();}}while(false)
namespace sci=anaiero::science;
namespace cells=anaiero::cells;
struct FakeExchange final:sci::ILiveSolverExchange {
  std::uint64_t fast=0,environment=0;
  bool advance(const sci::LiveRequest& req,sci::LiveResponse& out)noexcept override {
    if(req.epoch!=1||req.sequence==0||req.model_sha256.size()!=64)return false;
    if(req.channel==sci::Channel::electrical_activity){
      if(req.sequence!=++fast||req.step_ns!=sci::kLiveFastStepNs)return false;
      out={req.channel,req.epoch,req.sequence,req.sequence*req.step_ns,0,
           static_cast<std::int32_t>(100+req.input_proxy/4)};
      return true;
    }
    if(req.channel!=sci::Channel::environment_proxy||
       req.sequence!=++environment||req.step_ns!=sci::kLiveSlowStepNs)return false;
    out={req.channel,req.epoch,req.sequence,req.sequence*req.step_ns,0,75};
    return true;
  }
};
static cells::Atlas make_atlas() {
  cells::Atlas a{};
  for(unsigned i=0;i<cells::kKinds;++i) {
    a.by_kind[i].kind=static_cast<cells::Kind>(i+1);
    a.by_kind[i].maintenance=1;
    a.by_kind[i].support_response=2;
    a.by_kind[i].maturation=2;
    a.by_kind[i].process_extension=4;
    a.by_kind[i].activity_response=5;
    a.by_kind[i].plasticity=2;
    a.by_kind[i].contact_radius=6;
  }
  return a;
}
int main(){
  const std::string hash(64,'a');
  {
    std::istringstream input("SAMPLE E 1 1 25000 505 0 "+hash+" OK\n");
    std::ostringstream output;
    sci::StdioLiveSolverExchange io(input,output);
    sci::LiveResponse r{};
    assert(io.advance({sci::Channel::electrical_activity,1,1,25000,320,hash},r));
    assert(r.normalized_feature==505);
    assert(output.str()=="STEP E 1 1 25000 320 "+hash+"\n");
  }
  {
    std::istringstream input("SAMPLE E 1 2 25000 505 0 "+hash+" OK\n");
    std::ostringstream output;
    sci::StdioLiveSolverExchange io(input,output);
    sci::LiveResponse r{};
    assert(!io.advance({sci::Channel::electrical_activity,1,1,25000,320,hash},r));
    assert(io.failed());
    assert(!io.advance({sci::Channel::electrical_activity,1,1,25000,320,hash},r));
  }
  {
    std::istringstream input("SAMPLE M 1 1 100000 75 0 "+hash+" OK\n");
    std::ostringstream output;
    sci::StdioLiveSolverExchange io(input,output);
    sci::LiveResponse r{};
    assert(io.advance({sci::Channel::environment_proxy,1,1,100000,0,hash},r));
    assert(r.normalized_feature==75);
  }
  {
    FakeExchange fake;
    sci::LiveElectricalFeatureService electrical(fake,hash,1,64);
    sci::LiveEnvironmentFeatureService environment(fake,hash,1,65);
    cells::BoundedPositionPort spatial;
    auto atlas=make_atlas();
    cells::CellBehaviorService seed(atlas,spatial);
    cells::Cell x{};x.id=3;x.kind=cells::Kind::central_neuron;
    x.energy=100;x.position={20,17,2};
    auto y=x;y.id=4;y.position={23,17,2};
    auto z=x;z.id=5;z.kind=cells::Kind::astrocyte;z.position={21,18,2};
    assert(seed.add(x)==cells::Status::ok);
    assert(seed.add(y)==cells::Status::ok);
    assert(seed.add(z)==cells::Status::ok);
    assert(seed.encourage(3,4,700)==cells::Status::ok);
    sci::CoupledSimulationService coupled(atlas,spatial,electrical);
    coupled.set_environment(&environment);
    sci::State start{};start.epoch=1;start.source=64;
    assert(coupled.initialize(seed.world(),start)==sci::Code::ok);
    sci::Result result{};sci::Plan plan{};
    plan.condition=sci::Condition::astrocyte_coupled;
    assert(coupled.run(plan,result)==sci::Code::ok);
    assert(result.steps==28&&fake.fast==112&&fake.environment==28);
    assert(coupled.world().tick==14&&result.viable==3);
  }
  std::cout<<"SW7.1 live electrical/environment stepping and multirate PASS\n";
}
