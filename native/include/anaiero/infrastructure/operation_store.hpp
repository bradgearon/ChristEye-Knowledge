#pragma once
#include "anaiero/contracts/operation_store.hpp"
#include "christeye/living/durable_registry.hpp"
#include <iomanip>
#include <sstream>
namespace anaiero::operations {
namespace db=christeye::living::durable;
inline bool valid(const Ledger& value)noexcept {
  if(!value.next_id||value.count>value.items.size()||value.expired_through>=value.next_id)return false;
  std::uint64_t prior=value.expired_through;std::size_t active=0;
  for(std::size_t i=0;i<value.count;++i){
    const auto& item=value.items[i];const auto& e=item.receipt;const auto& c=item.command;
    if(e.id<=prior||e.id>=value.next_id||c.operation_id!=e.id||e.detail.size()>256||c.reference.size()>4096||
       static_cast<unsigned>(e.state)<1||static_cast<unsigned>(e.state)>5||
       static_cast<unsigned>(e.result)>static_cast<unsigned>(Result::capacity)||
       static_cast<unsigned>(c.operation)>static_cast<unsigned>(session::Operation::resume_experiment))return false;
    if(!terminal(e.state)){if(e.id!=value.active_id)return false;++active;}
    else if(e.id==value.active_id)return false;
    prior=e.id;
  }
  return active==(value.active_id?1u:0u);
}
inline std::string encode(const Ledger& value){
  if(!valid(value))return {};
  std::ostringstream out;out<<"CEVOPERATIONS/1\n"<<value.revision<<' '<<value.next_id<<' '<<value.expired_through<<' '
    <<value.active_id<<' '<<value.count<<'\n';
  for(std::size_t i=0;i<value.count;++i){
    const auto& item=value.items[i];const auto& e=item.receipt;const auto& c=item.command;
    out<<e.id<<' '<<static_cast<unsigned>(e.state)<<' '<<static_cast<unsigned>(e.result)<<' '
      <<e.session_revision<<' '<<e.evidence_revision<<' '<<std::quoted(e.detail)<<'\n'
      <<static_cast<unsigned>(c.operation)<<' '<<c.engine<<' '<<c.from<<' '<<c.to<<' '<<c.count<<' '
      <<c.operation_id<<' '<<std::quoted(c.reference)<<'\n';
  }
  out<<"end\n";return out.str();
}
inline bool decode(std::string_view bytes,Ledger& result){
  if(bytes.size()>512*1024)return false;
  Ledger next{};std::istringstream in{std::string(bytes)};std::string tag;
  if(!(in>>tag)||tag!="CEVOPERATIONS/1"||!(in>>next.revision>>next.next_id>>next.expired_through>>next.active_id>>next.count)||
     !next.revision||next.count>next.items.size())return false;
  for(std::size_t i=0;i<next.count;++i){
    auto& item=next.items[i];auto& e=item.receipt;auto& c=item.command;unsigned state=0,status=0,operation=0;
    if(!(in>>e.id>>state>>status>>e.session_revision>>e.evidence_revision>>std::quoted(e.detail)>>
         operation>>c.engine>>c.from>>c.to>>c.count>>c.operation_id>>std::quoted(c.reference))||state>255||status>255||operation>255)return false;
    e.state=static_cast<State>(state);e.result=static_cast<Result>(status);c.operation=static_cast<session::Operation>(operation);
  }
  if(!(in>>tag)||tag!="end"||in>>tag||!valid(next)||encode(next)!=bytes)return false;
  result=std::move(next);return true;
}
class WindStoreOperationStore final:public IStore {
  db::Registry& database_;std::string name_;std::uint64_t revision_=0;
  bool latest(Ledger& value,bool& found)noexcept {
    db::Event event{};
    if(!database_.latest_event("CULTURE_CHECKPOINT",name_,"operations",event,found,"anaiero-operations"))return false;
    if(!found)return true;
    std::string bytes;
    return event.origin=="SIMULATED"&&database_.get(event.object_hash,bytes)&&decode(bytes,value)&&value.revision==event.source_tick;
  }
public:
  WindStoreOperationStore(db::Registry& database,std::string name):database_(database),name_(std::move(name)){}
  bool load(Ledger& value,bool& exists)noexcept override {
    Ledger result{};exists=false;if(!latest(result,exists))return false;
    if(exists){value=std::move(result);revision_=value.revision;}else revision_=0;
    return true;
  }
  bool save(Ledger& value)noexcept override {
    if(!valid(value)||value.revision!=revision_||revision_==UINT64_MAX)return false;
    Ledger prior{};bool found=false;
    if(!latest(prior,found)||(found?prior.revision:0)!=revision_||
      (found&&(value.next_id<prior.next_id||value.expired_through<prior.expired_through)))return false;
    auto next=value;++next.revision;const auto bytes=encode(next);if(bytes.empty())return false;
    const auto stored=database_.put(bytes);
    if(stored.code!=db::Code::ok||database_.publish("CULTURE_CHECKPOINT",name_,stored.hash,"SIMULATED",
      next.revision,"operations","anaiero-operations").code!=db::Code::ok)return false;
    revision_=next.revision;value.revision=next.revision;return true;
  }
};
} // namespace anaiero::operations
