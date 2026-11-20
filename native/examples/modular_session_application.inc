class LifecyclePolicy final:public s::ICommitPolicy {
  life::Service& lifecycle_;
public:
  explicit LifecyclePolicy(life::Service& lifecycle):lifecycle_(lifecycle){}
  s::Result scheduling_admission(const s::EngineIdentity& identity,bool& eligible)const override{
    life::Source source;
    if(auto r=lifecycle_.source({identity.source_uid,identity.epoch},source);!r)return r;
    if(source.identity!=identity)return {s::Code::conflict,"scheduled source identity changed"};
    eligible=source.phase==life::SourcePhase::active;
    return {};
  }
  s::Result enrich(ev::CommitBatch& batch,const s::PreparedTransition& transition)override{
    return lifecycle_.stage_commit(batch,transition);
  }
  s::Result restore_admission(const s::EngineIdentity& identity)const override{
    life::Snapshot snapshot;
    if(auto r=lifecycle_.snapshot(snapshot);!r)return r;
    if(snapshot.quarantined)return {s::Code::unauthorized,"restored store is quarantined"};
    life::Source source;
    if(auto r=lifecycle_.source({identity.source_uid,identity.epoch},source);!r)return r;
    if(source.identity!=identity)return {s::Code::conflict,"source identity differs from engine"};
    if(source.phase==life::SourcePhase::retired)return {s::Code::expired,"source is retired"};
    return {};
  }
  s::Result recovery_sources(std::vector<s::EngineIdentity>& output)const override{
    life::Snapshot snapshot;
    if(auto r=lifecycle_.snapshot(snapshot);!r)return r;
    if(snapshot.quarantined)return {s::Code::unauthorized,"restored store is quarantined"};
    output.clear();
    for(const auto& source:snapshot.sources)
      if(source.phase!=life::SourcePhase::retired&&!source.state_sha256.empty())output.push_back(source.identity);
    return {};
  }
};
struct Application {
  s::EngineModuleCatalog catalog;
  std::unique_ptr<ev::ICommitStore> store;
  std::unique_ptr<life::IArchiveStore> archive;
  std::unique_ptr<life::Service> lifecycle;
  s::ProjectionService projection;
  std::unique_ptr<LifecyclePolicy> policy;
  std::unique_ptr<s::SessionService> runtime;
  std::unique_ptr<s::SessionRunner> runner;
  s::Result open(std::string_view directory,bool create,std::string_view archive_path={},
                  std::uint64_t maximum_bytes=0){
    if(auto r=e::register_builtin_engines(catalog);!r)return r;
    const fs::path directory_path(directory);
    if(!create&&!canonical_root_exists(directory_path))return {s::Code::not_found,"canonical store ROOT does not exist"};
    if(create&&!canonical_root_exists(directory_path)){
      std::error_code error;
      if(fs::exists(directory_path,error)&&(!fs::is_directory(directory_path,error)||!fs::is_empty(directory_path,error)))
        return {s::Code::conflict,"initialization requires an empty directory or an existing canonical store"};
      if(error)return {s::Code::io_error,"cannot inspect initialization directory"};
    }
    ev::StoreOptions options;
    if(maximum_bytes)options.maximum_bytes=maximum_bytes;
    if(auto r=ev::open_native_store(directory,options,store);!r)return r;
    std::uint64_t persisted=0;bool policy_exists=false;
    const auto store_id=store->info().store_id;
    for(unsigned attempt=0;attempt<4;++attempt){
      if(auto r=stored_budget(*store,persisted,policy_exists);!r)return r;
      if(!policy_exists||options.maximum_bytes==persisted)break;
      if(maximum_bytes)return {s::Code::conflict,"store already has a different byte budget; use the capacity command"};
      if(attempt==3)return {s::Code::busy,"store byte policy changed repeatedly while reopening"};
      store.reset();options.maximum_bytes=persisted;
      if(!canonical_root_exists(directory_path))return {s::Code::conflict,"store root disappeared while reopening its byte policy"};
      if(auto r=ev::open_native_store(directory,options,store);!r)return r;
      if(store->info().store_id!=store_id)return {s::Code::conflict,"store identity changed while reopening its byte policy"};
    }
    if(auto r=s::host::NativeSession::bind_catalog(*store,catalog);!r)return r;
    if(auto r=catalog.freeze();!r)return r;
    if(!policy_exists&&create){
      ev::CommitBatch batch;batch.operation_id="session-storage-policy";
      const auto payload=s::pack({"store-budget/1",std::to_string(options.maximum_bytes)});
      batch.request_digest=s::sha256(payload);batch.guards.push_back({"operator.policy","capacity",0,{}});
      batch.records.push_back({"operator.policy","capacity",payload,{},false});
      ev::CommitReceipt receipt;
      if(auto r=store->commit(batch,receipt);!r)return r;
    }
    if(!create){
      ev::StoredRecord definition;
      const auto found=store->latest("session-definition","main",definition);
      if(found.code==s::Code::not_found)return {s::Code::not_found,"canonical store has no modular session; use audit or explicit init"};
      if(!found)return found;
      if(definition.record.tombstone)return {s::Code::expired,"modular session definition is retired"};
    }
    if(!archive_path.empty())if(auto r=life::open_native_archive(archive_path,{},archive);!r)return r;
    lifecycle=std::make_unique<life::Service>(*store,life::Options{},archive.get());
    if(auto r=lifecycle->open();!r)return r;
    policy=std::make_unique<LifecyclePolicy>(*lifecycle);
    runtime=std::make_unique<s::SessionService>(catalog,*store,projection,policy.get());
    life::Snapshot snapshot;
    if(auto r=lifecycle->snapshot(snapshot);!r)return r;
    if(!snapshot.quarantined)if(auto r=runtime->recover();!r)return r;
    runner=std::make_unique<s::SessionRunner>(*runtime,*store);
    return {};
  }
  s::Result ensure_engines(){
    for(const auto& scheduled:runner->definition().engines){
      const auto& id=scheduled.identity;
      life::Source source;
      auto result=lifecycle->source({id.source_uid,id.epoch},source);
      if(result.code==s::Code::not_found){
        if(auto r=lifecycle->register_engine(id,"local-operator","enroll-"+s::sha256(s::encode_identity(id)));!r)return r;
        if(auto r=lifecycle->source({id.source_uid,id.epoch},source);!r)return r;
      }else if(!result)return result;
      if(source.identity!=id)return {s::Code::conflict,"configured source has another identity"};
      if(source.phase==life::SourcePhase::registered)
        if(auto r=lifecycle->activate(source.id,"activate-"+s::sha256(s::encode_identity(id)));!r)return r;
      if(source.phase==life::SourcePhase::retired||source.phase==life::SourcePhase::sealed||source.phase==life::SourcePhase::draining)continue;
      if(!runtime->find(id.instance_id))if(auto r=runtime->add_engine(id,scheduled.configuration);!r)return r;
    }
    return {};
  }
  s::Result retain_executable(const fs::path& path){
    std::string bytes;
    if(!read_file(path,bytes))return invalid("cannot retain the current executable within its 8 MiB artifact limit");
    const auto digest=s::sha256(bytes);
    const auto module=catalog.describe().front().module_sha256;
    ev::StoredRecord existing;
    const auto found=store->latest("sdk.module-installation",module,existing);
    if(found){
      if(existing.record.payload!=digest)return {s::Code::conflict,"installed executable differs from the retained module package"};
      return {};
    }
    if(found.code!=s::Code::not_found)return found;
    ev::CommitBatch batch;
    batch.operation_id="module-package-"+module;
    batch.request_digest=s::sha256(module+digest);
    batch.guards.push_back({"sdk.module-installation",module,0,{}});
    batch.blobs.push_back({digest,std::move(bytes),{}});
    batch.records.push_back({"sdk.module-installation",module,digest,{digest},false});
    ev::CommitReceipt receipt;
    auto r=store->commit(batch,receipt);
    if(!r.committed())return r;
    life::Pin pin;
    pin.id="module-"+module;
    pin.owner="local-operator";
    pin.kind=life::PinKind::module;
    pin.generation=1;
    pin.objects={digest};
    return lifecycle->put_pin(pin,0,now(),"pin-"+module);
  }
  s::Result run(std::uint64_t through){
    if(auto r=runner->open();!r)return r;
    if(auto r=ensure_engines();!r)return r;
    if(through<runner->progress().completed_cycles)return {s::Code::conflict,"requested cycle is before committed progress"};
    if(through-runner->progress().completed_cycles>10000)return {s::Code::capacity,"one invocation admits at most 10000 cycles"};
    std::mutex mutex;
    std::condition_variable changed;
    std::uint64_t completions=0;
    ev::CommitWorker writer(*store,8);
    writer.set_wakeup([&]{std::lock_guard lock(mutex);++completions;changed.notify_one();});
    runtime->set_commit_queue(&writer);
    auto finish=[&](s::Result result){writer.stop();
    runtime->set_commit_queue(nullptr);
    return result;
    };
    while(runner->progress().completed_cycles<through){
      if(auto r=runner->begin_cycle();!r)return finish(r);
      for(;;){
        std::string instance;
        s::StepRequest request;
        bool pending=false;
        if(auto r=runner->next_step(instance,request,pending);!r)return finish(r);
        if(!pending)break;
        std::uint64_t seen=0;
        {std::lock_guard lock(mutex);
        seen=completions;
        }
        if(auto r=runtime->begin_step(instance,request);!r)return finish(r);
        while(runtime->busy()){
          {std::unique_lock lock(mutex);
          changed.wait(lock,[&]{return completions!=seen;});
          seen=completions;
          }
          if(auto r=runtime->pump();!r&&r.code!=s::Code::busy)return finish(r);
        }
      }
      if(auto r=runner->finish_cycle();!r)return finish(r);
    }
    return finish({});
  }
  s::Result status(){
    const auto info=store->info();
    life::CapacityStatus capacity;
    if(auto r=lifecycle->capacity(capacity);!r)return r;
    life::Snapshot snapshot;
    const auto life_result=lifecycle->snapshot(snapshot);
    if(!life_result)return life_result;
    const auto opened=runner->open();
    if(!opened&&opened.code!=s::Code::not_found)return opened;
    const auto progress=opened?runner->progress():s::SessionProgress{};
    std::cout<<"{\"schema\":\"anaiero.session-status/1\",\"evidence_class\":\"SIMULATED_UNCALIBRATED\",\"store_id\":"<<json(info.store_id)
      <<",\"generation\":"<<info.generation<<",\"index_generation\":"<<info.index_generation
      <<",\"index_backend\":"<<json(info.index_backend)<<",\"index_layout\":"<<json(index_layout_label(info.index_backend))<<",\"accounted_bytes\":"<<info.physical_bytes
      <<",\"quota_basis\":\"accounted_serialized_bytes\",\"maintenance_reserve_bytes\":"<<info.maintenance_reserve_bytes
      <<",\"ordinary_limit_bytes\":"<<info.ordinary_limit_bytes
      <<",\"ordinary_remaining_bytes\":"<<capacity.ordinary_remaining_bytes
      <<",\"maintenance_remaining_bytes\":"<<capacity.maintenance_remaining_bytes
      <<",\"writable\":"<<(capacity.writable?"true":"false")<<",\"index_ready\":"<<(capacity.index_ready?"true":"false")
      <<",\"retained_identity_policy\":\"preserve-authority-source-tombstone-and-exact-retry-identities\""
      <<",\"canonical_base_generation\":"<<info.canonical_base_generation<<",\"canonical_base_sha256\":"<<json(info.canonical_base_digest)
      <<",\"operation_epoch\":"<<info.operation_epoch<<",\"pending_reclamation_bytes\":"<<info.pending_reclamation_bytes
      <<",\"maximum_bytes\":"<<info.maximum_bytes<<",\"completed_cycles\":"<<progress.completed_cycles
      <<",\"cycle_pending\":"<<(progress.cycle_pending?"true":"false")
      <<",\"quarantined\":"<<(snapshot.quarantined?"true":"false")<<",\"incarnation\":"<<json(snapshot.incarnation)
      <<",\"lifetime_observations\":"<<snapshot.lifetime_observations<<",\"active_sources\":"<<snapshot.sources.size()
      <<",\"active_peers\":"<<snapshot.active_peers<<",\"active_subscriptions\":"<<snapshot.active_subscriptions
      <<",\"pins\":"<<snapshot.pins<<",\"pending_bootstraps\":"<<snapshot.pending_bootstraps
      <<",\"engines\":[";
    bool first=true;
    for(const auto& engine:runtime->view().engines){
      life::Source source;
      if(auto r=lifecycle->source({engine.identity.source_uid,engine.identity.epoch},source);!r)return r;
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"id\":"<<json(engine.identity.instance_id)<<",\"type\":"<<json(engine.identity.type_id)
        <<",\"source\":"<<json(engine.identity.source_uid)<<",\"epoch\":"<<engine.identity.epoch
        <<",\"source_phase\":"<<json(life::phase_name(source.phase))
        <<",\"phase\":"<<json(s::phase_name(engine.state.phase))<<",\"tick\":"<<engine.state.tick
        <<",\"revision\":"<<engine.state.revision<<",\"state_sha256\":"<<json(s::state_digest(engine.state))<<'}';
    }
    std::cout<<"],\"metrics\":[";
    first=true;
    for(const auto& metric:projection.snapshot()){
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"engine\":"<<json(metric.identity.instance_id)<<",\"metric\":"<<json(metric.observation.metric)
        <<",\"quantity\":"<<json(metric.observation.quantity)<<",\"unit\":"<<json(metric.observation.unit)
        <<",\"value\":"<<metric.observation.value<<",\"tick\":"<<metric.observation.tick<<'}';
    }
    std::cout<<"],\"lifecycle_status\":"<<json(s::code_name(life_result.code))<<"}\n";
    return {};
  }
};
s::Result detach_identity(Application& app,const s::EngineIdentity& identity){
  const auto* current=app.runtime->find(identity.instance_id);
  if(current&&current->identity()!=identity)return {};
  ev::StoredRecord stored;
  const auto found=app.store->latest("sdk.engine",identity.instance_id,stored);
  if(found.code==s::Code::not_found)return {};
  if(!found)return found;
  std::vector<std::string> fields;s::EngineIdentity recorded;
  if(!s::unpack(stored.record.payload,fields,4)||fields.size()!=4||!s::decode_identity(fields[1],recorded))
    return {s::Code::corrupt,"retired engine cleanup identity is invalid"};
  if(recorded!=identity)return {};
  return app.runtime->remove_retired(identity.instance_id);
}
