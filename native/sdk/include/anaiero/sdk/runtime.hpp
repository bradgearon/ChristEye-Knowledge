#pragma once
#include "anaiero/sdk/catalog.hpp"
#include "anaiero/sdk/projection.hpp"
#include "anaiero/sdk/observer.hpp"
#include <memory>
namespace anaiero::sdk::evidence {struct CommitBatch;struct CommitReceipt;class ICommitStore;class ICommitQueue;}
namespace anaiero::sdk {
struct ICommitPolicy {
  virtual ~ICommitPolicy()=default;
  virtual Result enrich(evidence::CommitBatch&,const PreparedTransition&)=0;
  // Expired means explicitly retired: recovery skips active materialization.
  virtual Result restore_admission(const EngineIdentity&) const { return {}; }
  virtual Result scheduling_admission(const EngineIdentity&,bool& eligible) const {eligible=true;return {};}
  // A lifecycle provider returns only active/draining/sealed materializations.
  // This bounds recovery by active sources instead of lifetime engine count.
  virtual Result recovery_sources(std::vector<EngineIdentity>&) const {
    return {Code::unsupported,"no active source catalog"};
  }
};
struct EngineView { EngineIdentity identity;EngineState state;std::uint64_t committed_generation=0; };
struct SessionView { bool busy=false,degraded=false;std::string error;std::vector<EngineView> engines; };
class SessionService {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  SessionService(EngineModuleCatalog&,evidence::ICommitStore&,ProjectionService&,
                 ICommitPolicy* policy=nullptr,std::size_t capacity=32);
  ~SessionService();
  void set_commit_queue(evidence::ICommitQueue*) noexcept;
  void set_observer_sink(IObserverSink*) noexcept;
  Result add_engine(const EngineIdentity&,const Envelope& configuration,
                    const EngineState* initial=nullptr);
  Result recover();
  Result begin_step(std::string_view instance_id,const StepRequest&,bool change=false);
  // Complete at most one pending transition; no polling loop is hidden here.
  Result pump();
  Result step(std::string_view instance_id,const StepRequest&,bool change=false);
  Result control(std::string_view instance_id,std::string operation_id,Phase phase);
  // Review and activate a versioned checkpoint transformation on the session owner.
  // The engine must be paused; an accepted transition preserves its tick/identity.
  Result review_migration(std::string_view instance_id,const MigrationRequest&,MigrationPlan&);
  Result apply_migration(std::string_view instance_id,const MigrationPlan&);
  Result remove_retired(std::string_view instance_id);
  Result scheduling_admission(std::string_view instance_id,bool& eligible) const;
  bool busy() const noexcept;
  SessionView view() const;
  IEngine* find(std::string_view instance_id) noexcept;
};
}
