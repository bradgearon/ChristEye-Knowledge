#include "anaiero/sdk/session_runner.hpp"
#include <array>
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace anaiero::sdk {
namespace {
namespace ev=evidence;
bool number(std::string_view text,std::uint64_t& value){
  const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&r.ec==std::errc{}&&r.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
Result invalid(std::string message){return {Code::invalid,std::move(message)};
}
Bytes encode_schedule(const EngineSchedule& value){
  const std::array<std::string,4> f{encode_identity(value.identity),encode_envelope(value.configuration),
    encode_envelope(value.input),std::to_string(value.cadence)};
    return pack(f);
}
struct Intent {std::string instance;
StepRequest request;
};
Bytes intent_bytes(const std::vector<Intent>& intents){
  std::vector<std::string> fields;
  for(const auto& item:intents){
    const std::array<std::string,4> one{item.instance,item.request.operation_id,
      std::to_string(item.request.expected_revision),encode_envelope(item.request.input)};
      fields.push_back(pack(one));
  }
  return pack(fields);
}
Result parse_intents(std::string_view bytes,std::vector<Intent>& output){
  std::vector<std::string> fields;
  if(auto r=unpack(bytes,fields,32);!r)return r;
  std::set<std::string> names;
  std::vector<Intent> values;
  for(const auto& record:fields){
    std::vector<std::string> f;
    if(auto r=unpack(record,f,4);!r)return r;
    Intent one;
    if(f.size()!=4||!valid_token(f[0])||!names.insert(f[0]).second||!valid_token(f[1])||
      !number(f[2],one.request.expected_revision)||!decode_envelope(f[3],one.request.input))return invalid("invalid cycle intent");
    one.instance=f[0];
    one.request.operation_id=f[1];
    values.push_back(std::move(one));
  }
  output=std::move(values);
  return {};
}
}
Bytes encode_session_definition(const SessionDefinition& value){
  std::vector<std::string> f{"anaiero-session/2",value.name};
  for(const auto& item:value.engines)f.push_back(encode_schedule(item));
  return pack(f);
}
Result decode_session_definition(std::string_view bytes,SessionDefinition& value){
  std::vector<std::string> f;
  if(auto r=unpack(bytes,f,34);!r)return r;
  if(f.size()<3||f[0]!="anaiero-session/2"||!valid_token(f[1]))return invalid("invalid session definition");
  SessionDefinition result;
  result.name=f[1];
  std::set<std::string> ids,sources;
  for(std::size_t i=2;i<f.size();++i){
    std::vector<std::string> row;
    if(auto r=unpack(f[i],row,4);!r)return r;
    EngineSchedule schedule;
    std::uint64_t cadence=0;
    if(row.size()!=4||!decode_identity(row[0],schedule.identity)||!decode_envelope(row[1],schedule.configuration)||
       !decode_envelope(row[2],schedule.input)||!number(row[3],cadence)||cadence==0||cadence>1000||
       !ids.insert(schedule.identity.instance_id).second||!sources.insert(schedule.identity.source_uid).second)
      return invalid("invalid or duplicate scheduled engine");
    schedule.cadence=static_cast<std::uint32_t>(cadence);
    result.engines.push_back(std::move(schedule));
  }
  value=std::move(result);
  return {};
}
struct SessionRunner::Impl {
  SessionService& runtime;
  ev::ICommitStore& store;
  SessionDefinition definition;
  SessionProgress progress;
  std::vector<Intent> intents;
  ev::StoredRecord record;
  Impl(SessionService& r,ev::ICommitStore& s):runtime(r),store(s){}
  Bytes progress_bytes()const{
    const std::array<std::string,4> f{"session-progress/2",std::to_string(progress.completed_cycles),
      progress.cycle_pending?"1":"0",intent_bytes(intents)};
      return pack(f);
  }
  Result save(std::string operation){
    ev::CommitBatch batch;
    batch.operation_id=std::move(operation);
    batch.expected_generation=store.generation();
    const auto payload=progress_bytes();
    batch.request_digest=sha256(payload);
    batch.guards.push_back({"session-progress","main",record.revision,record.digest});
    batch.records.push_back({"session-progress","main",payload,{},false});
    ev::CommitReceipt receipt;
    const auto result=store.commit(batch,receipt);
    if(result.code==Code::committed_degraded)return result;
    if(result.ok())return store.latest("session-progress","main",record);
    return result;
  }
};
SessionRunner::SessionRunner(SessionService& runtime,ev::ICommitStore& store):impl_(std::make_unique<Impl>(runtime,store)){}
SessionRunner::~SessionRunner()=default;
Result SessionRunner::initialize(const SessionDefinition& definition){
  SessionDefinition check;
  if(auto r=decode_session_definition(encode_session_definition(definition),check);!r)return r;
  ev::StoredRecord existing;
  const auto found=impl_->store.latest("session-definition","main",existing);
  if(found){
    if(existing.record.payload!=encode_session_definition(definition))return {Code::conflict,"session definition already exists with different bytes"};
    return open();
  }
  if(found.code!=Code::not_found)return found;
  ev::CommitBatch batch;
  batch.operation_id="session-initialize";
  batch.expected_generation=impl_->store.generation();
  const auto encoded=encode_session_definition(check);
  batch.request_digest=sha256(encoded);
  batch.guards.push_back({"session-definition","main",0,{}});
  batch.guards.push_back({"session-progress","main",0,{}});
  batch.records.push_back({"session-definition","main",encoded,{},false});
  impl_->definition=std::move(check);
  impl_->progress={};
  impl_->intents.clear();
  batch.records.push_back({"session-progress","main",impl_->progress_bytes(),{},false});
  ev::CommitReceipt receipt;
  const auto committed=impl_->store.commit(batch,receipt);
  return committed.ok()?open():committed;
}
Result SessionRunner::reconfigure(const SessionDefinition& definition,std::string operation_id){
  auto& p=*impl_;
  if(!valid_token(operation_id))return invalid("invalid reconfiguration operation ID");
  SessionDefinition checked;
  if(auto r=decode_session_definition(encode_session_definition(definition),checked);!r)return r;
  if(p.runtime.busy())return {Code::busy,"engine commit pending"};
  const auto encoded=encode_session_definition(checked);
  ev::CommitReceipt previous_receipt;
  const auto previous_result=p.store.lookup_operation(operation_id,previous_receipt);
  if(previous_result){
    if(previous_receipt.request_digest!=sha256(encoded))return {Code::conflict,"reconfiguration operation ID has different bytes"};
    return open();
  }
  if(previous_result.code!=Code::not_found)return previous_result;
  if(auto r=open();!r)return r;
  if(p.progress.cycle_pending)return {Code::busy,"finish the pending cycle before reconfiguration"};
  for(const auto& live:p.runtime.view().engines){
    const auto found=std::find_if(checked.engines.begin(),checked.engines.end(),[&](const auto& item){return item.identity==live.identity;});
    if(found==checked.engines.end())return {Code::conflict,"retire existing sources before removing their schedule"};
    const auto previous=std::find_if(p.definition.engines.begin(),p.definition.engines.end(),[&](const auto& item){return item.identity==live.identity;});
    if(previous!=p.definition.engines.end()&&previous->configuration!=found->configuration)
      return {Code::conflict,"engine configuration changes require the registered change capability"};
  }
  ev::StoredRecord current;
  if(auto r=p.store.latest("session-definition","main",current);!r)return r;
  ev::CommitBatch batch;batch.operation_id=std::move(operation_id);
  batch.request_digest=sha256(encoded);
  batch.guards.push_back({"session-definition","main",current.revision,current.digest});
  batch.guards.push_back({"session-progress","main",p.record.revision,p.record.digest});
  batch.records.push_back({"session-definition","main",encoded,{},false});
  ev::CommitReceipt receipt;const auto result=p.store.commit(batch,receipt);
  return result.ok()?open():result;
}
Result SessionRunner::open(){
  auto& p=*impl_;
  ev::StoredRecord definition;
  if(auto r=p.store.latest("session-definition","main",definition);!r)return r;
  if(auto r=decode_session_definition(definition.record.payload,p.definition);!r)return r;
  if(auto r=p.store.latest("session-progress","main",p.record);!r)return r;
  std::vector<std::string> f;
  if(auto r=unpack(p.record.record.payload,f,4);!r)return r;
  if(f.size()!=4||f[0]!="session-progress/2"||!number(f[1],p.progress.completed_cycles)||
     (f[2]!="0"&&f[2]!="1"))return {Code::corrupt,"invalid persisted session progress"};
  p.progress.cycle_pending=f[2]=="1";
  if(auto r=parse_intents(f[3],p.intents);!r)return r;
  if(!p.progress.cycle_pending&&!p.intents.empty())return {Code::corrupt,"cycle intents outside active cycle"};
  for(const auto& intent:p.intents){
    bool found=false;
    for(const auto& engine:p.definition.engines)
      if(engine.identity.instance_id==intent.instance&&engine.input==intent.request.input)found=true;
    if(!found)return {Code::corrupt,"cycle intent differs from pinned session definition"};
  }
  return {};
}
const SessionDefinition& SessionRunner::definition()const noexcept{return impl_->definition;
}
SessionProgress SessionRunner::progress()const noexcept{return impl_->progress;
}
Result SessionRunner::begin_cycle(){
  auto& p=*impl_;
  if(p.runtime.busy())return {Code::busy,"engine commit pending"};
  if(p.progress.cycle_pending)return {};
  if(p.progress.completed_cycles==std::numeric_limits<std::uint64_t>::max())return {Code::capacity,"cycle counter exhausted"};
  p.intents.clear();
  const auto cycle=p.progress.completed_cycles+1;
  for(const auto& scheduled:p.definition.engines){
    if(cycle%scheduled.cadence)continue;
    bool eligible=false;
    if(auto r=p.runtime.scheduling_admission(scheduled.identity.instance_id,eligible);!r)return r;
    if(!eligible)continue;
    auto* engine=p.runtime.find(scheduled.identity.instance_id);
    if(!engine)return {Code::corrupt,"eligible scheduled engine is missing"};
    // Explicitly retired sources are historical, not schedulable.
    if(engine->identity()!=scheduled.identity)return {Code::conflict,"scheduled engine identity changed"};
    const auto phase=engine->state().phase;
    if(phase!=Phase::ready&&phase!=Phase::running)continue;
    p.intents.push_back({scheduled.identity.instance_id,{"cycle-"+sha256(pack({std::to_string(cycle),encode_identity(scheduled.identity)})),
      engine->state().revision,scheduled.input}});
  }
  p.progress.cycle_pending=true;
  const auto result=p.save("cycle-plan-"+std::to_string(cycle));
  if(!result)(void)open();
  return result;
}
Result SessionRunner::next_step(std::string& instance,StepRequest& request,bool& has_step){
  auto& p=*impl_;
  has_step=false;
  if(p.runtime.busy())return {Code::busy,"engine commit pending"};
  if(!p.progress.cycle_pending)return {Code::conflict,"no active cycle"};
  for(const auto& intent:p.intents){
    ev::CommitReceipt receipt;
    const auto found=p.store.lookup_operation(intent.request.operation_id,receipt);
    if(found){
      const EngineSchedule* scheduled=nullptr;
      for(const auto& item:p.definition.engines)if(item.identity.instance_id==intent.instance)scheduled=&item;
      if(!scheduled||receipt.request_digest!=request_digest(scheduled->identity,intent.request))
        return {Code::conflict,"cycle operation identity was reused with different input"};
      continue;
    }
    if(found.code!=Code::not_found)return found;
    instance=intent.instance;
    request=intent.request;
    has_step=true;
    return {};
  }
  return {};
}
Result SessionRunner::finish_cycle(){
  auto& p=*impl_;
  if(p.runtime.busy())return {Code::busy,"engine commit pending"};
  bool pending=false;
  std::string id;
  StepRequest request;
  if(auto r=next_step(id,request,pending);!r)return r;
  if(pending)return {Code::busy,"cycle still has uncommitted engine transitions"};
  ++p.progress.completed_cycles;
  p.progress.cycle_pending=false;
  p.intents.clear();
  const auto result=p.save("cycle-complete-"+std::to_string(p.progress.completed_cycles));
  if(!result)(void)open();
  return result;
}
}
