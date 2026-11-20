// CE-SW-43: native, canonical, state-changing checkpoint migration.
#include "anaiero/sdk/catalog.hpp"
#include "anaiero/sdk/runtime.hpp"
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/migration_receipts.hpp"
#include "anaiero/sdk/migration_evidence_service.hpp"
#include "anaiero/sdk/session_evidence_service.hpp"
#include "anaiero/sdk/session_record_codec.hpp"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <memory>

namespace s=anaiero::sdk;
namespace ev=anaiero::sdk::evidence;
namespace fs=std::filesystem;
#define CHECK(expression) do {if(!(expression)) { \
  std::cerr<<"CE-SW-43 failure at line "<<__LINE__<<": " #expression "\n";return 1; \
}}while(false)
namespace {
const s::Digest& migrator_sha() {
  static const s::Digest digest=s::sha256("ce-sw43-counter-checkpoint-v1-to-v2/1");
  return digest;
}
bool integer(std::string_view raw,std::uint64_t& output) {
  if(raw.empty())return false;
  const auto result=std::from_chars(raw.data(),raw.data()+raw.size(),output);
  return result.ec==std::errc{}&&result.ptr==raw.data()+raw.size()&&std::to_string(output)==raw;
}
bool state_value(const s::EngineState& state,std::uint64_t& output) {
  std::vector<std::string> fields;
  if(!s::unpack(state.checkpoint.payload,fields,3)||fields.size()<2||
     fields.size()>3||!integer(fields[0],output)||fields[1]!="seed")return false;
  if(state.checkpoint.schema=="counter.checkpoint/1")return fields.size()==2;
  if(state.checkpoint.schema=="counter.checkpoint/2")
    return fields.size()==3&&fields[2]==s::sha256(s::pack({fields[0],fields[1]}));
  return false;
}
s::Envelope checkpoint(std::uint64_t value,bool next_version) {
  const auto count=std::to_string(value);
  if(!next_version)return {"counter.checkpoint/1",s::pack({count,"seed"})};
  return {"counter.checkpoint/2",s::pack({count,"seed",s::sha256(s::pack({count,"seed"}))})};
}
class Counter final:public s::IEngine,public s::IMigrationCapability {
  s::EngineIdentity identity_;s::EngineState state_;
public:
  Counter(s::EngineIdentity identity,s::Envelope config):identity_(std::move(identity)) {
    state_.configuration=std::move(config);state_.checkpoint=checkpoint(0,false);
  }
  const s::EngineIdentity& identity()const noexcept override{return identity_;}
  const s::EngineState& state()const noexcept override{return state_;}
  s::Result prepare_step(const s::StepRequest& request,s::PreparedTransition& out)override {
    std::uint64_t value=0,step=0;
    if(request.input.schema!="counter.add/1"||!integer(request.input.payload,step)||
       !state_value(state_,value)||step>1000||value>1000000-step)
      return {s::Code::invalid,"invalid counter step"};
    if(request.expected_revision!=state_.revision)return {s::Code::conflict,"counter revision"};
    if(state_.phase==s::Phase::paused)return {s::Code::conflict,"counter paused"};
    out={};out.identity=identity_;out.operation_id=request.operation_id;
    out.request_sha256=s::request_digest(identity_,request);
    out.expected_state_sha256=s::state_digest(state_);
    out.expected_revision=state_.revision;out.target=state_;
    ++out.target.revision;++out.target.tick;out.target.phase=s::Phase::running;
    out.target.checkpoint=checkpoint(value+step,state_.checkpoint.schema=="counter.checkpoint/2");
    return {};
  }
  s::Result install(const s::EngineState& next)override {
    if(next.revision!=state_.revision+1||next.tick<state_.tick)return {s::Code::conflict,"install revision"};
    return restore(next);
  }
  s::Result restore(const s::EngineState& next)override {
    std::uint64_t value=0;
    if(!s::valid_state(next)||next.configuration!=state_.configuration||!state_value(next,value))
      return {s::Code::invalid,"unrecognized checkpoint"};
    state_=next;return {};
  }
  s::IMigrationCapability* migrations()noexcept override{return this;}
  std::string_view implementation_sha256()const noexcept override{return migrator_sha();}
  s::Result prepare_migration(const s::MigrationRequest& request,s::PreparedTransition& out)override {
    std::uint64_t value=0;
    if(request.source_schema!="counter.checkpoint/1"||request.target_schema!="counter.checkpoint/2"||
       request.implementation_sha256!=migrator_sha()||request.parameters.schema!="anaiero.migration-parameters/1"||
       !request.parameters.payload.empty())return {s::Code::unsupported,"unknown conversion"};
    if(request.expected_revision!=state_.revision||state_.phase!=s::Phase::paused||
       !state_value(state_,value))return {s::Code::conflict,"stale migration input"};
    out={};out.identity=identity_;out.operation_id=request.operation_id;
    out.request_sha256=s::migration_request_digest(identity_,request);
    out.expected_state_sha256=s::state_digest(state_);
    out.expected_revision=state_.revision;out.target=state_;
    ++out.target.revision;out.target.checkpoint=checkpoint(value,true);
    return {};
  }
};
class Factory final:public s::IEngineFactory {
public:
  s::Result create(const s::EngineIdentity& id,const s::Envelope& config,
                   const s::EngineState* saved,std::unique_ptr<s::IEngine>& output)override {
    if(config.schema!="counter.configuration/1"||config.payload!="fixture")
      return {s::Code::invalid,"test configuration"};
    auto engine=std::make_unique<Counter>(id,config);
    if(saved){auto result=engine->restore(*saved);if(!result)return result;}
    output=std::move(engine);return {};
  }
};
s::EngineDescriptor descriptor() {
  s::EngineDescriptor d;d.type_id="fixture.migrating-counter";d.display_name="Checkpoint migration fixture";
  d.module_sha256=s::sha256("fixture-module/1");d.behavior_id="fixture-counter/1";
  d.configuration_schemas={"counter.configuration/1"};d.input_schemas={"counter.add/1"};
  d.checkpoint_schemas={"counter.checkpoint/1","counter.checkpoint/2"};
  return d;
}
s::EngineIdentity identity(std::string instance) {
  return {instance,"fixture.migrating-counter",s::sha256("fixture-module/1"),
    s::sha256("fixture-model/1"),"fixture-counter/1","source-"+instance,"logical",1,0};
}
s::MigrationRequest request(std::string op,std::uint64_t revision) {
  return {std::move(op),revision,"counter.checkpoint/1","counter.checkpoint/2",
    migrator_sha(),{"anaiero.migration-parameters/1",{}}};
}
}
int main(int argc,char** argv) {
  if(argc!=2){std::cerr<<"usage: anaiero_engine_migration_tests STATE_DIR\n";return 2;}
  const fs::path path=argv[1];fs::remove_all(path);
  std::unique_ptr<ev::ICommitStore> store;
  CHECK(ev::open_native_store(path.string(),{},store));
  ev::MigrationEvidenceService migration_evidence(*store);
  ev::SessionEvidenceService session_evidence(*store);
  s::EngineModuleCatalog catalog;CHECK(catalog.add(descriptor(),std::make_shared<Factory>()));
  CHECK(catalog.freeze());
  s::ProjectionService projection;s::SessionService runtime(catalog,*store,projection);
  const s::Envelope configuration{"counter.configuration/1","fixture"};
  CHECK(runtime.add_engine(identity("learner"),configuration));
  s::PreparedTransition registration;
  CHECK(session_evidence.registration(identity("learner"),runtime.find("learner")->state(),registration));
  CHECK(registration.operation_id=="register-"+s::sha256(s::encode_identity(identity("learner"))));
  CHECK(registration.request_sha256==s::sha256(s::pack({"register/1",
    s::encode_identity(identity("learner")),s::encode_state(runtime.find("learner")->state())})));
  CHECK(session_evidence.phase_request(identity("learner"),"pause-learner",s::Phase::paused)==
    s::sha256(s::pack({"phase-control/1",s::encode_identity(identity("learner")),
      "pause-learner","paused"})));
  CHECK(session_evidence.detach_operation(identity("learner"))==
    "detach-"+s::sha256(s::encode_identity(identity("learner"))));
  CHECK(runtime.step("learner",{"train-3",0,{"counter.add/1","3"}}));
  auto* engine=runtime.find("learner");CHECK(engine);
  std::uint64_t value=0;CHECK(state_value(engine->state(),value)&&value==3);
  s::MigrationPlan plan;
  CHECK(runtime.review_migration("learner",request("upgrade-learner",1),plan).code==s::Code::conflict);
  CHECK(runtime.control("learner","pause-learner",s::Phase::paused));
  engine=runtime.find("learner");CHECK(engine);
  const auto before=engine->state();
  auto expected=request("upgrade-learner",before.revision);
  CHECK(runtime.review_migration("learner",expected,plan));
  CHECK(plan.identity==identity("learner"));
  CHECK(s::valid_digest(plan.source_checkpoint_sha256)&&s::valid_digest(plan.target_checkpoint_sha256));
  CHECK(plan.source_checkpoint_sha256!=plan.target_checkpoint_sha256);
  s::EngineState stored_before;
  CHECK(session_evidence.read_checkpoint(plan.source_checkpoint_sha256,plan.identity,stored_before));
  CHECK(stored_before==before);
  s::StepRequest migration_step;
  CHECK(migration_evidence.request_step(plan.identity,plan.request,migration_step));
  CHECK(migration_step.input.schema=="anaiero.engine-migration/1");
  CHECK(s::request_digest(plan.identity,migration_step)==s::migration_request_digest(plan.identity,plan.request));
  CHECK(migration_evidence.verify_plan(plan));
  s::MigrationPlan decoded;
  const auto encoded=s::encode_migration_plan(plan);
  CHECK(s::decode_migration_plan(encoded,decoded));
  CHECK(s::encode_migration_plan(decoded)==encoded);
  CHECK(s::decode_migration_plan(encoded+"x",decoded).code==s::Code::corrupt);
  auto tampered=plan;tampered.target_checkpoint_sha256=s::sha256("wrong target");
  CHECK(runtime.apply_migration("learner",tampered).code==s::Code::conflict);
  CHECK(engine->state()==before);
  auto foreign=expected;foreign.implementation_sha256=s::sha256("unrecognized migrator");
  s::MigrationPlan rejected;
  CHECK(runtime.review_migration("learner",foreign,rejected).code==s::Code::unsupported);
  CHECK(runtime.apply_migration("learner",plan));
  engine=runtime.find("learner");CHECK(engine);
  CHECK(engine->state().revision==before.revision+1);
  CHECK(engine->state().tick==before.tick&&engine->state().phase==s::Phase::paused);
  CHECK(engine->state().checkpoint.schema=="counter.checkpoint/2");
  CHECK(state_value(engine->state(),value)&&value==3);
  bool replayed=false;
  CHECK(migration_evidence.accepted_retry(plan,replayed)&&replayed);
  CHECK(runtime.apply_migration("learner",plan)); // exact durable operation retry
  auto reused=plan;reused.request.parameters.payload="changed";
  CHECK(migration_evidence.match_plans(plan,reused).code==s::Code::conflict);
  CHECK(runtime.apply_migration("learner",reused).code==s::Code::conflict);
  ev::StoredRecord audit;CHECK(store->latest("sdk.engine-migration","upgrade-learner",audit));
  ev::StoredMigrationReceipt typed_audit;
  CHECK(ev::read_migration_receipt(*store,"upgrade-learner",typed_audit));
  CHECK(typed_audit.generation==audit.generation);
  CHECK(s::encode_migration_plan(typed_audit.receipt.plan)==encoded);
  CHECK(s::encode_migration_receipt(typed_audit.receipt)==audit.record.payload);
  s::MigrationReceipt receipt_decoded;
  CHECK(s::decode_migration_receipt(audit.record.payload,receipt_decoded));
  CHECK(s::decode_migration_receipt(audit.record.payload+"x",receipt_decoded).code==s::Code::corrupt);
  CHECK(s::encode_migration_receipt({plan,""}).empty());
  s::SessionEngineRecord source_record{plan.identity,plan.source_checkpoint_sha256,plan.target_checkpoint_sha256};
  const auto source_wire=s::encode_session_engine_record(source_record);
  CHECK(source_wire==s::pack({"session-engine/1",s::encode_identity(plan.identity),
    plan.source_checkpoint_sha256,plan.target_checkpoint_sha256}));
  s::SessionEngineRecord engine_decoded;
  CHECK(s::decode_session_engine_record(source_wire,engine_decoded));
  CHECK(engine_decoded.identity==plan.identity);
  CHECK(engine_decoded.checkpoint_sha256==plan.source_checkpoint_sha256);
  CHECK(s::decode_session_engine_record(source_wire+"x",engine_decoded).code==s::Code::corrupt);
  s::SessionMetricRecord metric_record{plan.identity,
    {"counter.observation/1","count","dimensionless_count","count",3,0},2};
  const auto metric_wire=s::encode_session_metric_record(metric_record);
  CHECK(metric_wire==s::pack({"session-metric/1",s::encode_identity(plan.identity),
    s::encode_observation(metric_record.observation),"2"}));
  s::SessionMetricRecord metric_decoded;
  CHECK(s::decode_session_metric_record(metric_wire,metric_decoded));
  CHECK(metric_decoded.revision==2&&metric_decoded.observation.value==3);
  CHECK(s::decode_session_metric_record(metric_wire+"x",metric_decoded).code==s::Code::corrupt);
  CHECK(audit.record.references.size()==2);
  CHECK(audit.record.references[0]==plan.source_checkpoint_sha256);
  CHECK(audit.record.references[1]==plan.target_checkpoint_sha256);
  s::Bytes old_bytes,new_bytes;
  CHECK(store->get_blob(plan.source_checkpoint_sha256,old_bytes));
  CHECK(store->get_blob(plan.target_checkpoint_sha256,new_bytes));
  CHECK(s::sha256(old_bytes)==plan.source_checkpoint_sha256&&s::sha256(new_bytes)==plan.target_checkpoint_sha256);
  CHECK(runtime.control("learner","resume-learner",s::Phase::running));
  CHECK(runtime.step("learner",{"train-2",engine->state().revision,{"counter.add/1","2"}}));
  CHECK(state_value(engine->state(),value)&&value==5);
  const auto final_state=engine->state();
  s::ProjectionService restored_projection;s::SessionService restored(catalog,*store,restored_projection);
  CHECK(restored.recover());
  CHECK(restored.find("learner")&&restored.find("learner")->state()==final_state);
  // A second engine demonstrates an interrupted pre-ROOT migration and exact retry.
  CHECK(runtime.add_engine(identity("fault"),configuration));
  CHECK(runtime.control("fault","pause-fault",s::Phase::paused));
  const auto fault_revision=runtime.find("fault")->state().revision;
  s::MigrationPlan interrupted;
  CHECK(runtime.review_migration("fault",request("migrate-fault",fault_revision),interrupted));
  ev::set_native_store_fault(*store,ev::FaultPoint::after_transaction);
  CHECK(!runtime.apply_migration("fault",interrupted).committed());
  ev::set_native_store_fault(*store,ev::FaultPoint::none);
  CHECK(runtime.recover());
  CHECK(runtime.apply_migration("fault",interrupted));
  CHECK(runtime.find("fault")->state().checkpoint.schema=="counter.checkpoint/2");
  ev::AuditReport report;CHECK(store->audit(report));
  std::cout<<"CE-SW-43 PASS: reviewed state migration, canonical audit, exact replay, recovery and v2 learning\n";
  return 0;
}
