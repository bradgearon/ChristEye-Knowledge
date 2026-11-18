#pragma once
// One serial owner persists operation intent before allowing the session effect.
// The session records operation_id in the same manifest as its effect intent.
#include "anaiero/contracts/operation_store.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <algorithm>
#include <atomic>
namespace anaiero::session {
class OperationService final:public IDriver {
  IDriver& inner_;operations::IStore& store_;
  operations::Ledger ledger_{};
  operations::Ledger committed_{};
  bool ready_=false,cancel_requested_=false;
  std::atomic<bool> cancel_signal_{false},operation_active_{false};
  std::string error_;
  contracts::IServiceExtension* extension_=nullptr;
  IViewEncoder* encoder_=nullptr;
  static bool same_command(const Command& a,const Command& b)noexcept {
    return a.operation==b.operation&&a.engine==b.engine&&a.from==b.from&&a.to==b.to&&
      a.count==b.count&&a.reference==b.reference;
  }
  operations::Item* active()noexcept {
    for(std::size_t i=0;i<ledger_.count;++i)
      if(ledger_.items[i].receipt.id==ledger_.active_id)return &ledger_.items[i];
    return nullptr;
  }
  bool save(std::uint64_t completed_operation=0)noexcept {
    if(!store_.save(ledger_)){error_="operation_persistence_failed";return false;}
    committed_=ledger_;
    if(extension_)extension_->on_service_event({contracts::ServiceId::operation,
      contracts::ServicePhase::committed,ledger_.revision,0,0,
      completed_operation?completed_operation:ledger_.active_id,0,0});
    return true;
  }
  bool finish(operations::State state,operations::Result result,std::string detail={})noexcept {
    auto* item=active();if(!item)return false;
    const auto completed_operation=item->receipt.id;
    if(detail.size()>256)detail.resize(256);
    item->receipt.state=state;item->receipt.result=result;item->receipt.detail=std::move(detail);
    item->receipt.session_revision=inner_.revision();
    item->receipt.evidence_revision=inner_.view().projected.revision;
    ledger_.active_id=0;cancel_requested_=false;
    operation_active_.store(false,std::memory_order_release);return save(completed_operation);
  }
  // request_cancel only wakes the blocked infrastructure. The serial owner
  // persists that intent before classifying its resulting solver failure.
  bool capture_cancel()noexcept {
    if(!cancel_signal_.exchange(false,std::memory_order_acq_rel))return true;
    if(auto* pending=active();pending&&!cancel_requested_){
      cancel_requested_=true;pending->receipt.result=operations::Result::cancelled;
      if(!save())return false;
    }
    inner_.cancel_operations();return true;
  }
  bool engine_failed()noexcept {
    return finish(cancel_requested_?operations::State::cancelled:operations::State::failed,
      cancel_requested_?operations::Result::cancelled:operations::Result::engine_failure,inner_.view().error);
  }
  bool completed()noexcept {
    return finish(cancel_requested_?operations::State::cancelled:operations::State::completed,
      cancel_requested_?operations::Result::cancelled:operations::Result::ok);
  }
  bool launch(const ICancellation* cancel)noexcept {
    if(!capture_cancel())return false;
    auto* item=active();if(!item)return false;
    if(inner_.operation_id()==item->receipt.id){
      if(inner_.failed())return engine_failed();
      if(inner_.busy())return true;
      return completed();
    }
    // This intent already passed admission and reached storage. A fresh engine
    // may clear its transient error while retaining the interrupted cycle.
    if(inner_.busy()&&item->command.operation!=Operation::retry)return false;
    if(cancel_requested_||(cancel&&cancel->cancelled()))
      return finish(operations::State::cancelled,operations::Result::cancelled);
    item->receipt.state=operations::State::running;if(!save())return false;
    const bool accepted=inner_.execute(item->command,cancel);
    if(!capture_cancel())return false;
    if(!accepted){
      const bool stopped=cancel_requested_||(cancel&&cancel->cancelled());
      return finish(stopped?operations::State::cancelled:operations::State::failed,
        stopped?operations::Result::cancelled:operations::Result::invalid,inner_.view().error);
    }
    if(inner_.operation_id()!=item->receipt.id){error_="operation_stamp_missing";return false;}
    if(inner_.failed())return engine_failed();
    if(!inner_.busy())return completed();
    return true;
  }
public:
  OperationService(IDriver& inner,operations::IStore& store):inner_(inner),store_(store){}
  bool start()noexcept {
    if(ready_)return false;
    bool exists=false;if(!store_.load(ledger_,exists))return false;
    if(exists)committed_=ledger_;
    else if(!save())return false;
    operation_active_.store(ledger_.active_id!=0,std::memory_order_release);
    if(auto* pending=active();pending&&pending->receipt.result==operations::Result::cancelled){
      cancel_requested_=true;inner_.cancel_operations();
    }
    // A terminal cancellation can outlive an unfinished session cycle. The
    // fresh session must drain only that boundary, not its original target.
    if(!ledger_.active_id&&inner_.busy()){
      for(std::size_t i=0;i<ledger_.count;++i){
        const auto& receipt=ledger_.items[i].receipt;
        if(receipt.id==inner_.operation_id()&&receipt.state==operations::State::cancelled){
          inner_.cancel_operations();break;
        }
      }
    }
    ready_=true;return true;
  }
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  void set_view_encoder(IViewEncoder* encoder)noexcept{encoder_=encoder;}
  bool execute(const Command& input,const ICancellation* cancel)noexcept override {
    if(!ready_||!error_.empty())return false;
    if(!capture_cancel())return false;
    auto command=input;
    if(!command.operation_id)command.operation_id=ledger_.next_id;
    if(!command.operation_id||command.operation_id==UINT64_MAX||command.operation_id<=ledger_.expired_through)return false;
    for(std::size_t i=0;i<ledger_.count;++i){
      const auto& item=ledger_.items[i];
      if(item.receipt.id==command.operation_id)return same_command(item.command,command);
    }
    const bool retry_failed=command.operation==Operation::retry&&inner_.failed();
    if(command.operation==Operation::retry&&!retry_failed)return false;
    if(ledger_.active_id||(inner_.busy()&&!retry_failed)||(inner_.failed()&&!retry_failed))return false;
    // Reject gaps before touching retention or its expiry watermark.
    if(command.operation_id<ledger_.next_id)return false;
    if(ledger_.count==ledger_.items.size()){
      if(!operations::terminal(ledger_.items[0].receipt.state))return false;
      ledger_.expired_through=std::max(ledger_.expired_through,ledger_.items[0].receipt.id);
      for(std::size_t i=1;i<ledger_.count;++i)ledger_.items[i-1]=std::move(ledger_.items[i]);
      --ledger_.count;
    }
    // Strict monotonic admission gives expired IDs an unambiguous no-replay rule.
    ledger_.next_id=command.operation_id+1;ledger_.active_id=command.operation_id;
    auto& item=ledger_.items[ledger_.count++];item={};
    item.receipt.id=command.operation_id;item.command=std::move(command);
    if(!save())return false;
    operation_active_.store(true,std::memory_order_release);
    return launch(cancel);
  }
  bool pump()noexcept override {
    if(!ready_||!error_.empty())return false;
    if(!capture_cancel())return false;
    if(ledger_.active_id){
      if(inner_.operation_id()!=ledger_.active_id)return launch(nullptr);
      if(inner_.failed())return engine_failed();
      if(inner_.busy()){
        const bool progressed=inner_.pump();
        if(!capture_cancel())return false;
        if(inner_.failed())return engine_failed();
        if(!inner_.busy())return completed();
        return progressed;
      }
      return completed();
    }
    return inner_.busy()?inner_.pump():false;
  }
  void cancel_operations()noexcept override {
    cancel_signal_.store(true,std::memory_order_release);
    (void)capture_cancel();
  }
  bool busy()const noexcept override{return ledger_.active_id||inner_.busy();}
  void request_cancel()noexcept override{
    if(!operation_active_.load(std::memory_order_acquire))return;
    cancel_signal_.store(true,std::memory_order_release);inner_.request_cancel();
  }
  bool quiescent()const noexcept override{return inner_.quiescent();}
  bool failed()const noexcept override{return !error_.empty()||(!ledger_.active_id&&inner_.failed());}
  std::string state_json()const override{return encoder_?encoder_->encode(view()):inner_.state_json();}
  std::string trace_json()const override{return inner_.trace_json();}
  std::uint64_t revision()const noexcept override{return inner_.revision();}
  std::uint64_t operation_id()const noexcept override{return inner_.operation_id();}
  View view()const override{
    auto result=inner_.view();result.busy=busy();
    if(ledger_.active_id)result.operation_id=ledger_.active_id;
    if(!error_.empty())result.error=error_;
    return result;
  }
  ExperimentView experiment_view()const override{return inner_.experiment_view();}
  operations::View operation_view()const override {
    operations::View result{};result.next_id=committed_.next_id;result.expired_through=committed_.expired_through;
    result.active_id=committed_.active_id;result.count=committed_.count;
    for(std::size_t i=0;i<committed_.count;++i)result.entries[i]=committed_.items[i].receipt;
    return result;
  }
};
} // namespace anaiero::session
