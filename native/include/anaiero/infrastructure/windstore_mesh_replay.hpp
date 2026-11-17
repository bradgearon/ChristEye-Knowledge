#pragma once
// Mesh-owned Registry adapter. The owner serializes all access; this adapter
// never opens or reads an active native session's worker-owned store.
#include "anaiero/contracts/mesh_replay.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include "christeye/living/durable_registry.hpp"
#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace anaiero::mesh {
class WindStoreMeshReplayStore final:public IMeshReplayStore {
  using Registry=christeye::living::durable::Registry;
  using Event=christeye::living::durable::Event;
  Registry& store_;
  std::string session_;
  std::filesystem::path root_;
  runtime::EpochObservationCodec codec_{};
  bool ready_=false;
  static ReplayCode translate(christeye::living::durable::Code code)noexcept {
    using Code=christeye::living::durable::Code;
    switch(code){
      case Code::ok:return ReplayCode::ok;
      case Code::capacity:return ReplayCode::capacity;
      case Code::invalid_argument:case Code::invalid_cursor:return ReplayCode::invalid;
      case Code::bad_digest:case Code::bad_head:case Code::corrupt_journal:case Code::missing_object:return ReplayCode::corrupt;
      default:return ReplayCode::persistence_failed;
    }
  }
  static std::string key(const ReplaySubscription& subscription) {
    return christeye::living::plans::sha256_hex("CE/MESH/SUBSCRIPTION/1\n"+subscription.principal+'\n'+subscription.id+'\n');
  }
  static std::string sealed(std::string body) {
    return body+"seal "+christeye::living::plans::sha256_hex(body)+'\n';
  }
  static std::string subscription_bytes(const ReplaySubscription& subscription) {
    const auto& scope=subscription.scope;
    return sealed("CEMESHSUB/1\npeer "+subscription.principal+"\nid "+subscription.id+"\nscope "+
      std::to_string(scope.source)+' '+std::to_string(scope.epoch)+' '+std::to_string(scope.clock)+' '+std::to_string(scope.colony)+'\n');
  }
  static bool parse_subscription(std::string_view bytes,ReplaySubscription& subscription) {
    std::istringstream input{std::string(bytes)};std::string magic,peer,id,scope,seal,hash,extra;
    ReplaySubscription next;
    if(!(input>>magic>>peer>>next.principal>>id>>next.id>>scope>>next.scope.source>>next.scope.epoch>>
      next.scope.clock>>next.scope.colony>>seal>>hash)||input>>extra||magic!="CEMESHSUB/1"||peer!="peer"||id!="id"||
      scope!="scope"||seal!="seal"||!valid_subscription(next)||subscription_bytes(next)!=bytes)return false;
    subscription=std::move(next);return true;
  }
  static std::string receive_bytes(const ReplaySubscription& subscription,std::uint64_t generation,const ReplayCursor& cursor) {
    const auto& scope=subscription.scope;
    return sealed("CEMESHRECEIVE/1\npeer "+subscription.principal+"\nid "+subscription.id+"\nscope "+
      std::to_string(scope.source)+' '+std::to_string(scope.epoch)+' '+std::to_string(scope.clock)+' '+std::to_string(scope.colony)+
      "\nreceipt "+std::to_string(generation)+' '+std::to_string(cursor.sequence)+' '+cursor.hash+'\n');
  }
  static bool parse_receive(std::string_view bytes,ReplaySubscription& subscription,std::uint64_t& generation,ReplayCursor& cursor) {
    std::istringstream input{std::string(bytes)};std::string magic,peer,id,scope,receipt,seal,hash,extra;
    ReplaySubscription next;ReplayCursor next_cursor;std::uint64_t next_generation=0;
    if(!(input>>magic>>peer>>next.principal>>id>>next.id>>scope>>next.scope.source>>next.scope.epoch>>
      next.scope.clock>>next.scope.colony>>receipt>>next_generation>>next_cursor.sequence>>next_cursor.hash>>seal>>hash)||
      input>>extra||magic!="CEMESHRECEIVE/1"||peer!="peer"||id!="id"||scope!="scope"||receipt!="receipt"||seal!="seal"||
      !valid_subscription(next)||!next_generation||!valid_replay_cursor(next_cursor)||
      receive_bytes(next,next_generation,next_cursor)!=bytes)return false;
    subscription=std::move(next);generation=next_generation;cursor=std::move(next_cursor);return true;
  }
  bool load_subscription(const ReplaySubscription& expected,bool& found)const {
    const auto path=root_/"subscriptions"/(key(expected)+".subscription");std::error_code ec;
    found=std::filesystem::exists(path,ec);if(ec)return false;if(!found)return true;
    std::string raw;ReplaySubscription loaded;
    return christeye::living::durable::read_bounded(path,1024,raw)&&parse_subscription(raw,loaded)&&loaded==expected;
  }
  bool room_for(const std::filesystem::path& directory,std::string_view extension)const {
    std::error_code ec;std::size_t count=0;
    for(std::filesystem::directory_iterator i(directory,ec),end;!ec&&i!=end;i.increment(ec)){
      if(i->is_symlink(ec)||ec)return false;
      if(i->path().extension()==extension&&++count>=kMaxReplaySubscriptions)return false;
    }
    return !ec;
  }
  bool decode_event(const Event& event,runtime::Observation& observation)const {
    using namespace christeye::living::durable;
    if(event.kind!="OBSERVATION_RECORDED"||event.entity!=session_)return false;
    std::string bytes;if(!store_.get(event.object_hash,bytes)||bytes.size()!=runtime::kEpochObservationFrame)return false;
    if(!codec_.decode({reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()},observation))return false;
    return observation.source_epoch&&event.source_tick==observation.sequence&&
      event.clock=="epoch-"+std::to_string(observation.source_epoch)&&event.producer=="source-"+std::to_string(observation.source)&&
      event.origin==(observation.origin==runtime::Origin::simulated?"SIMULATED":"IMPORTED_UNVERIFIED");
  }
public:
  WindStoreMeshReplayStore(Registry& store,std::string session_name,std::filesystem::path metadata_root):
    store_(store),session_(std::move(session_name)),root_(std::move(metadata_root)){}
  bool initialize() {
    namespace fs=std::filesystem;
    if(!store_.is_open()||!christeye::living::durable::valid_id(session_))return false;
    std::error_code ec;
    const auto exists=fs::exists(root_,ec);if(ec||(exists&&fs::is_symlink(root_,ec))||ec)return false;
    for(const auto* child:{"subscriptions","receivers"}){
      fs::create_directories(root_/child,ec);if(ec||fs::is_symlink(root_/child,ec)||ec)return false;
    }
    ready_=true;return true;
  }
  ReplayCode open_subscription(const ReplaySubscription& subscription,ReplayCursor& cursor)noexcept override {
    using namespace christeye::living::durable;
    if(!ready_||!valid_subscription(subscription))return ReplayCode::invalid;
    bool found=false;
    if(!load_subscription(subscription,found))return ReplayCode::conflict;
    const auto identity=key(subscription);
    if(!found){
      if(!room_for(root_/"subscriptions",".subscription"))return ReplayCode::capacity;
      if(!os::atomic_write(root_/"subscriptions"/(identity+".subscription"),subscription_bytes(subscription)))return ReplayCode::persistence_failed;
    }
    const auto saved=store_.read_cursor("mesh-"+identity);
    if(saved.code!=Code::ok)return translate(saved.code);
    // Creates a durable zero cursor as well. A disconnect before first ACK
    // must still retain the reader's unacknowledged history.
    const auto status=store_.acknowledge("mesh-"+identity,saved.sequence,saved.hash);
    if(status!=Code::ok)return translate(status);
    cursor={saved.sequence,saved.hash};return ReplayCode::ok;
  }
  ReplayCode read_page(const Scope& scope,const ReplayCursor& after,ReplayPage& output)noexcept override {
    using namespace christeye::living::durable;
    if(!ready_||!valid_scope(scope)||!valid_replay_cursor(after))return ReplayCode::invalid;
    std::vector<Event> events;bool caught_up=false;
    const auto status=store_.read_event_page({Code::ok,after.sequence,after.hash},kMaxReplayExamined,events,caught_up);
    if(status!=Code::ok)return translate(status);
    ReplayPage page;page.after=after;page.terminal=after;
    for(const auto& event:events){
      ++page.examined;page.terminal={event.sequence,event.hash};
      if(event.kind=="OBSERVATION_RECORDED"&&event.entity==session_){
        runtime::Observation observation;if(!decode_event(event,observation))return ReplayCode::corrupt;
        if(scope_matches(scope,observation)){
          page.records[page.count++]={{event.sequence,event.hash},observation};
          if(page.count==kMaxReplayRecords)break;
        }
      }
    }
    page.caught_up=caught_up&&page.examined==events.size();
    output=std::move(page);return ReplayCode::ok;
  }
  ReplayCode acknowledge(const ReplaySubscription& subscription,const ReplayCursor& cursor)noexcept override {
    if(!ready_||!valid_subscription(subscription)||!valid_replay_cursor(cursor))return ReplayCode::invalid;
    bool found=false;if(!load_subscription(subscription,found))return ReplayCode::conflict;
    if(!found)return ReplayCode::not_found;
    return translate(store_.acknowledge("mesh-"+key(subscription),cursor.sequence,cursor.hash));
  }
  ReplayCode check_append(const runtime::Observation& observation,bool& duplicate)const noexcept {
    using namespace christeye::living::durable;
    duplicate=false;
    if(!ready_||!runtime::valid_observation(observation)||!observation.source_epoch)return ReplayCode::invalid;
    std::array<std::byte,runtime::kEpochObservationFrame> frame{};
    if(!codec_.encode(observation,frame))return ReplayCode::invalid;
    const std::string_view bytes{reinterpret_cast<const char*>(frame.data()),frame.size()};
    const auto digest=christeye::living::plans::sha256_hex(bytes);
    const auto clock="epoch-"+std::to_string(observation.source_epoch),producer="source-"+std::to_string(observation.source);
    Event existing;bool found=false;
    if(!store_.find_event("OBSERVATION_RECORDED",session_,clock,producer,observation.sequence,existing,found))return ReplayCode::corrupt;
    if(found){
      runtime::Observation verified;
      if(existing.object_hash!=digest||!decode_event(existing,verified))return ReplayCode::conflict;
      duplicate=true;return ReplayCode::ok;
    }
    Event latest;bool has_latest=false;
    if(!store_.latest_event("OBSERVATION_RECORDED",session_,clock,latest,has_latest,producer))return ReplayCode::corrupt;
    if(has_latest){
      runtime::Observation previous;if(!decode_event(latest,previous))return ReplayCode::corrupt;
      // A source/epoch is a complete single-colony stream. This prevents
      // filtered subscriptions from creating silent per-source sequence gaps.
      if(previous.sequence==UINT64_MAX||observation.sequence!=previous.sequence+1||
        previous.clock_domain!=observation.clock_domain||previous.colony!=observation.colony||
        previous.origin!=observation.origin||observation.acquired_ns<previous.acquired_ns)return ReplayCode::conflict;
    }else if(observation.sequence!=1)return ReplayCode::conflict;
    return ReplayCode::ok;
  }
  ReplayCode append(const runtime::Observation& observation,ReplayCursor* local=nullptr)noexcept {
    using namespace christeye::living::durable;
    bool duplicate=false;const auto admissible=check_append(observation,duplicate);
    if(admissible!=ReplayCode::ok)return admissible;
    const auto clock="epoch-"+std::to_string(observation.source_epoch),producer="source-"+std::to_string(observation.source);
    if(duplicate){
      if(local){Event existing;bool found=false;
        if(!store_.find_event("OBSERVATION_RECORDED",session_,clock,producer,observation.sequence,existing,found)||!found)return ReplayCode::corrupt;
        *local={existing.sequence,existing.hash};
      }
      return ReplayCode::ok;
    }
    std::array<std::byte,runtime::kEpochObservationFrame> frame{};
    if(!codec_.encode(observation,frame))return ReplayCode::invalid;
    const std::string_view bytes{reinterpret_cast<const char*>(frame.data()),frame.size()};
    const auto object=store_.put(bytes);if(object.code!=Code::ok)return translate(object.code);
    const auto published=store_.publish("OBSERVATION_RECORDED",session_,object.hash,
      observation.origin==runtime::Origin::simulated?"SIMULATED":"IMPORTED_UNVERIFIED",observation.sequence,clock,producer);
    if(published.code!=Code::ok)return translate(published.code);
    if(local){Event committed;bool located=false;
      if(!store_.find_event("OBSERVATION_RECORDED",session_,clock,producer,observation.sequence,committed,located)||!located)return ReplayCode::corrupt;
      *local={committed.sequence,committed.hash};
    }
    return ReplayCode::ok;
  }
  ReplayCode load_receive(const AuthorizedPeer& peer,std::string_view id,const Scope& scope,ReplayCursor& cursor)noexcept {
    using namespace christeye::living::durable;
    const ReplaySubscription subscription{peer.principal,std::string(id),scope};
    if(!ready_||!peer.generation||!valid_subscription(subscription))return ReplayCode::invalid;
    const auto path=root_/"receivers"/(key(subscription)+".receipt");std::error_code ec;
    if(!std::filesystem::exists(path,ec)){if(ec)return ReplayCode::persistence_failed;
      cursor={};return ReplayCode::ok;
    }
    std::string raw;ReplaySubscription loaded;std::uint64_t generation=0;ReplayCursor next;
    if(!read_bounded(path,2048,raw)||!parse_receive(raw,loaded,generation,next))return ReplayCode::corrupt;
    if(loaded!=subscription)return ReplayCode::conflict;
    if(generation>peer.generation)return ReplayCode::denied;
    cursor=std::move(next);return ReplayCode::ok;
  }
  ReplayCode save_receive(const AuthorizedPeer& peer,std::string_view id,const Scope& scope,const ReplayCursor& cursor)noexcept {
    using namespace christeye::living::durable;
    if(!valid_replay_cursor(cursor))return ReplayCode::invalid;
    ReplayCursor old;const auto loaded=load_receive(peer,id,scope,old);if(loaded!=ReplayCode::ok)return loaded;
    if(cursor.sequence<old.sequence||(cursor.sequence==old.sequence&&cursor.hash!=old.hash))return ReplayCode::conflict;
    const ReplaySubscription subscription{peer.principal,std::string(id),scope};
    const auto path=root_/"receivers"/(key(subscription)+".receipt");std::error_code ec;
    if(!std::filesystem::exists(path,ec)&&(ec||!room_for(root_/"receivers",".receipt")))return ec?ReplayCode::persistence_failed:ReplayCode::capacity;
    return os::atomic_write(path,receive_bytes(subscription,peer.generation,cursor))?ReplayCode::ok:ReplayCode::persistence_failed;
  }
};
} // namespace anaiero::mesh
