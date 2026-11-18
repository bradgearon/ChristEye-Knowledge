#include "anaiero/application/supervised_scientific_engine.hpp"
#include "anaiero/infrastructure/environment_model_file.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/services/environment_compartment_service.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace s=anaiero::science;
namespace app=anaiero::application;
namespace engine=anaiero::engine;
namespace fs=std::filesystem;
static double tolerance(const s::EnvironmentBalance& b){
  return 1e-8+16*std::numeric_limits<double>::epsilon()*(std::abs(b.before)+b.supply+b.exchange_out+b.consumption+std::abs(b.after));
}
// Independent time-domain integration of the differential equation. This
// deliberately uses RK4, not the service's exponential or phi-function path.
static long double numerical_step(long double previous,const s::EnvironmentCompartmentModel& model,
    std::uint64_t ns,std::int32_t input){
  const long double exchange=1000000000.0L/static_cast<long double>(model.exchange_tau_ns);
  const long double consumption=static_cast<long double>(model.consumption_ppm_per_second)/1000000.0L;
  const long double inlet=static_cast<long double>(model.inlet_max_micromolar)*input/1000.0L;
  constexpr unsigned steps=1000;
  const long double dt=static_cast<long double>(ns)/1000000000.0L/steps;
  auto derivative=[&](long double value){return exchange*(inlet-value)-consumption*value;};
  for(unsigned i=0;i<steps;++i){
    const auto a=derivative(previous),b=derivative(previous+dt*a/2),c=derivative(previous+dt*b/2),d=derivative(previous+dt*c);
    previous+=dt*(a+2*b+2*c+d)/6;
  }
  return previous;
}
static void numerical_and_flux(){
  s::EnvironmentCompartmentModel model{};s::EnvironmentCompartmentService value;
  CHECK(value.initialize(model,100000000));
  const std::array<std::int32_t,8> inputs{1000,0,350,700,0,1000,1000,40};
  long double independent=static_cast<long double>(model.initial_micromolar);
  for(std::uint64_t n=1;n<=80;++n){
    const auto input=inputs[static_cast<std::size_t>((n-1)%inputs.size())];
    independent=numerical_step(independent,model,100000000,input);
    CHECK(value.advance(n,input));
    CHECK(std::abs(value.state().concentration_micromolar-static_cast<double>(independent))<1e-8);
    const auto b=value.balance();
    CHECK(b.supply>=0&&b.exchange_out>=0&&b.consumption>=0);
    CHECK(std::abs(b.residual)<=tolerance(b));
    // This parameter set's independent stoichiometric ratio is k/a=1/4.
    CHECK(std::abs(b.consumption-b.exchange_out/4)<1e-10);
    CHECK(std::abs(b.supply-static_cast<double>(input)/10.0)<1e-10);
    CHECK(std::abs(static_cast<double>(value.concentration_micromolar())-value.state().concentration_micromolar)<=0.5);
  }
  model.initial_micromolar=1000;model.consumption_ppm_per_second=0;
  CHECK(value.initialize(model,1000000000));CHECK(value.advance(1,0));
  CHECK(std::abs(value.state().concentration_micromolar-1000/std::exp(1.0))<1e-10);
  CHECK(value.balance().supply==0&&value.balance().consumption==0);
  CHECK(std::abs(value.balance().exchange_out-1000*(1-1/std::exp(1.0)))<1e-10);
  model.initial_micromolar=0;
  CHECK(value.initialize(model,1000000000));CHECK(value.advance(1,1000));
  CHECK(std::abs(value.state().concentration_micromolar-1000*(1-1/std::exp(1.0)))<1e-10);
  model.initial_micromolar=500;model.consumption_ppm_per_second=1000000;
  CHECK(value.initialize(model,1000000000));CHECK(value.advance(1,1000));
  CHECK(value.concentration_micromolar()==500);
  CHECK(std::abs(value.balance().exchange_out-500)<1e-10&&std::abs(value.balance().consumption-500)<1e-10);
}
static void limits_and_restore(){
  s::EnvironmentCompartmentService value;CHECK(!value.advance(1,0));
  s::EnvironmentCompartmentModel model{};
  CHECK(!value.initialize(model,0)&&!value.initialize(model,1000000001));
  auto invalid=model;invalid.initial_micromolar=-1;CHECK(!value.initialize(invalid,1));
  invalid=model;invalid.inlet_max_micromolar=1000001;CHECK(!value.initialize(invalid,1));
  invalid=model;invalid.exchange_tau_ns=0;CHECK(!value.initialize(invalid,1));
  invalid=model;invalid.exchange_tau_ns=1000000000001ULL;CHECK(!value.initialize(invalid,1));
  invalid=model;invalid.consumption_ppm_per_second=1000000001;CHECK(!value.initialize(invalid,1));
  CHECK(value.initialize(model,100000000));CHECK(!value.advance(2,1)&&!value.advance(1,-1)&&!value.advance(1,1001));
  CHECK(value.state().sequence==0&&value.state().concentration_micromolar==100);
  for(std::uint64_t i=1;i<=23;++i)CHECK(value.advance(i,static_cast<std::int32_t>((i*39)%1001)));
  const auto state=value.state();s::EnvironmentCompartmentService resumed;
  CHECK(resumed.initialize(model,100000000)&&resumed.restore(state));
  CHECK(resumed.balance().before==resumed.balance().after&&resumed.balance().supply==0);
  for(std::uint64_t i=24;i<=300;++i){
    const auto input=static_cast<std::int32_t>((i*39)%1001);
    CHECK(value.advance(i,input)&&resumed.advance(i,input));
    CHECK(value.state().concentration_micromolar==resumed.state().concentration_micromolar);
    CHECK(value.balance().residual==resumed.balance().residual);
  }
  CHECK(!resumed.restore({0,101})&&!resumed.restore({1000001,100})&&!resumed.restore({3,-1}));
  CHECK(!resumed.restore({3,1001})&&!resumed.restore({3,std::numeric_limits<double>::quiet_NaN()}));
  CHECK(!resumed.restore({3,std::numeric_limits<double>::infinity()}));
  CHECK(resumed.state().sequence==300);
  CHECK(resumed.restore({1000000,100})&&!resumed.advance(1000001,0));
  // Very short and stiff steps exercise cancellation, underflow and integrated
  // flux magnitudes independently of ordinary example parameters.
  for(const auto tau:{1ULL,1000000000000ULL})for(const auto step:{1ULL,1000000000ULL}){
    model={1000000,1000000,tau,1000000000};CHECK(value.initialize(model,step));
    CHECK(value.advance(1,1000));const auto b=value.balance();
    CHECK(std::abs(b.residual)<=tolerance(b)&&b.after>=0&&b.after<=1000000);
  }
  model={0,1000000,1000000000000ULL,0};CHECK(value.initialize(model,1));CHECK(value.advance(1,1000));
  CHECK(value.balance().exchange_out>0&&value.balance().exchange_out<1e-15);
  CHECK(std::abs(value.balance().residual)<=tolerance(value.balance()));
  // Complementary exp/expm1 summation previously drifted out of range at
  // sample 703,688 despite this being an exact stationary concentration.
  model={100,100,1000000000000ULL,0};CHECK(value.initialize(model,107));
  for(std::uint64_t i=1;i<=1000000;++i)CHECK(value.advance(i,1000));
  CHECK(value.state().concentration_micromolar==100);
}
static s::ScientificConfiguration configuration(const fs::path& executable,const fs::path& path){
  s::EnvironmentCompartmentModel parsed{};std::string model;
  CHECK(s::files::read_environment_model(path,parsed,model));
  s::ScientificConfiguration result;
  result.process.executable=executable.string();result.process.arguments={path.string()};
  result.expected={model,christeye::living::plans::sha256_hex(s::kEnvironmentAnalyticBehavior),"native-environment-analytic",1,
    100000000,1000000,s::Unit::micromolar,1};
  result.timeout_ms=2000;result.input_proxy=1000;return result;
}
static void supervised_restart(const fs::path& executable,const fs::path& path){
  auto config=configuration(executable,path);
  app::SupervisedScientificEngine original(302,82,9,config);
  CHECK(original.initialize()==engine::Status::ok);
  CHECK(!original.describe().capabilities.evaluate_candidates&&!original.describe().capabilities.apply_changes);
  engine::Snapshot current{};
  for(std::uint64_t i=0;i<37;++i)CHECK(original.advance({i,{},i+1},current)==engine::Status::ok);
  s::Quantity quantity;CHECK(original.quantity(quantity));
  CHECK(quantity.unit==s::Unit::micromolar&&quantity.source==82&&quantity.epoch==9);
  CHECK(std::abs(static_cast<double>(quantity.value)-(800-700*std::exp(-1.25*3.7)))<=static_cast<double>(quantity.uncertainty));
  anaiero::infrastructure::MemoryCheckpoint checkpoint;CHECK(original.checkpoint(checkpoint)==engine::Status::ok);
  CHECK(std::string(current.state_sha256.data())==christeye::living::plans::sha256_hex(checkpoint.bytes()));
  app::SupervisedScientificEngine resumed(302,82,9,config);
  CHECK(resumed.initialize()==engine::Status::ok&&resumed.restore(checkpoint)==engine::Status::ok);
  engine::Snapshot recovered;CHECK(resumed.latest(recovered)==engine::Status::ok&&recovered.state_sha256==current.state_sha256);
  for(std::uint64_t i=37;i<137;++i){
    CHECK(original.advance({i,{},i+1},current)==engine::Status::ok);
    CHECK(resumed.advance({i,{},i+1},recovered)==engine::Status::ok);
    CHECK(current.state_sha256==recovered.state_sha256&&current.energy==recovered.energy);
  }
  anaiero::infrastructure::MemoryCheckpoint first,second;
  CHECK(original.checkpoint(first)==engine::Status::ok&&resumed.checkpoint(second)==engine::Status::ok&&first.bytes()==second.bytes());
  auto other=config;other.expected.unit=s::Unit::microvolt;
  app::SupervisedScientificEngine wrong_unit(302,82,9,other);CHECK(wrong_unit.initialize()!=engine::Status::ok);
  other=config;other.expected.behavior_sha256[0]=other.expected.behavior_sha256[0]=='a'?'b':'a';
  app::SupervisedScientificEngine wrong_behavior(302,82,9,other);CHECK(wrong_behavior.initialize()!=engine::Status::ok);
  app::SupervisedScientificEngine wrong_epoch(302,82,10,config);
  CHECK(wrong_epoch.initialize()==engine::Status::ok&&wrong_epoch.restore(checkpoint)!=engine::Status::ok);
  std::string damaged(checkpoint.bytes());damaged[30]=damaged[30]=='a'?'b':'a';
  anaiero::infrastructure::MemoryCheckpoint broken;CHECK(broken.write(damaged));
  app::SupervisedScientificEngine wrong_bytes(302,82,9,config);
  CHECK(wrong_bytes.initialize()==engine::Status::ok&&wrong_bytes.restore(broken)!=engine::Status::ok);
}
static void files(const fs::path& path,const fs::path& root){
  fs::create_directories(root);s::EnvironmentCompartmentModel parsed{};std::string hash;
  CHECK(s::files::read_environment_model(path,parsed,hash));
  const auto canonical=s::files::serialize_environment_model(parsed);
  CHECK(hash==christeye::living::plans::sha256_hex(canonical));
  const auto copy=root/"model copy.cevmodel";
  {std::ofstream out(copy,std::ios::binary);out<<canonical;}
  std::string again;CHECK(s::files::read_environment_model(copy,parsed,again)&&again==hash);
  {std::ofstream out(copy,std::ios::binary);out<<canonical<<"unexpected\n";}
  CHECK(!s::files::read_environment_model(copy,parsed,again));
  {std::ofstream out(copy,std::ios::binary);out<<std::string(4097,'x');}
  CHECK(!s::files::read_environment_model(copy,parsed,again));
}
int main(int argc,char**argv){
  CHECK(argc==4);const auto executable=fs::absolute(argv[1]),model=fs::absolute(argv[2]),root=fs::absolute(argv[3]);
  fs::remove_all(root);numerical_and_flux();limits_and_restore();supervised_restart(executable,model);files(model,root);
  std::cout<<"environment numerical reference, flux closure, bounded input and exact process restart PASS\n";
}
