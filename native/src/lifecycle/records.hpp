#pragma once
#include "anaiero/sdk/lifecycle.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>

namespace anaiero::sdk::lifecycle::detail {
inline Result bad(std::string message) { return {Code::corrupt,std::move(message)}; }
inline std::string number(std::uint64_t value) { return std::to_string(value); }
inline bool integer(std::string_view value,std::uint64_t& out) {
  if(value.empty())return false;
  const auto result=std::from_chars(value.data(),value.data()+value.size(),out);
  return result.ec==std::errc{}&&result.ptr==value.data()+value.size();
}
struct Reader {
  std::vector<std::string> fields;
  std::size_t at=0;
  bool valid=true;
  explicit Reader(std::string_view bytes,std::string_view schema) {
    valid=unpack(bytes,fields,8192).ok()&&!fields.empty()&&fields[0]==schema;at=1;
  }
  std::string text() { if(!valid||at>=fields.size()){valid=false;return{};}return fields[at++]; }
  std::uint64_t value() {std::uint64_t out=0;const auto field=text();valid=valid&&integer(field,out);return out;}
  template<class E> E enumeration(std::uint64_t maximum) {
    const auto out=value();valid=valid&&out<=maximum;return static_cast<E>(out);
  }
  bool done() const {return valid&&at==fields.size();}
};
inline bool valid_ref(const SourceRef& ref) {return valid_token(ref.uid,256)&&ref.epoch>0;}
inline bool valid_cursor(const Cursor& cursor) {
  return cursor.sequence==0?cursor.payload_sha256.empty():valid_digest(cursor.payload_sha256);
}
inline std::string source_key(const SourceRef& ref) {return sha256(ref.uid)+":"+number(ref.epoch);}
inline Bytes encode(const SourceRef& ref) {return pack({ref.uid,number(ref.epoch)});}
inline bool decode(std::string_view bytes,SourceRef& out) {
  std::vector<std::string> f;
  return unpack(bytes,f,2).ok()&&f.size()==2&&(out.uid=f[0],integer(f[1],out.epoch))&&valid_ref(out);
}
inline Bytes encode(const Cursor& cursor) {return pack({number(cursor.sequence),cursor.payload_sha256});}
inline bool decode(std::string_view bytes,Cursor& out) {
  std::vector<std::string> f;
  return unpack(bytes,f,2).ok()&&f.size()==2&&integer(f[0],out.sequence)&&
         (out.payload_sha256=f[1],valid_cursor(out));
}
inline Bytes encode(const Source& value) {
  return pack({"source/3",encode(value.id),encode_identity(value.identity),value.owner,
    number(static_cast<std::uint64_t>(value.phase)),number(value.engine_revision),
    number(value.last_sequence),number(value.last_tick),value.last_payload_sha256,value.state_sha256,value.state_object_sha256});
}
inline bool decode(std::string_view bytes,Source& value) {
  Reader r(bytes,"source/3");
  r.valid=decode(r.text(),value.id)&&r.valid;
  r.valid=decode_identity(r.text(),value.identity).ok()&&r.valid;
  value.owner=r.text();value.phase=r.enumeration<SourcePhase>(4);
  value.engine_revision=r.value();value.last_sequence=r.value();value.last_tick=r.value();
  value.last_payload_sha256=r.text();value.state_sha256=r.text();
  value.state_object_sha256=r.text();
  return r.done()&&valid_token(value.owner,256)&&value.identity.source_uid==value.id.uid&&
    value.identity.epoch==value.id.epoch&&valid_cursor({value.last_sequence,value.last_payload_sha256})&&
    ((value.state_sha256.empty()&&value.state_object_sha256.empty())||
      (valid_digest(value.state_sha256)&&valid_digest(value.state_object_sha256)));
}
inline Bytes encode(const Metric& value) {
  return pack({"metric/2",encode(value.source),encode_observation(value.observation),number(value.sequence)});
}
inline bool decode(std::string_view bytes,Metric& value) {
  Reader r(bytes,"metric/2");r.valid=decode(r.text(),value.source)&&r.valid;
  r.valid=decode_observation(r.text(),value.observation).ok()&&r.valid;value.sequence=r.value();
  return r.done()&&value.sequence>0;
}
struct Active {
  Snapshot view;
  std::vector<std::string> peers,subscriptions,pins;
  std::uint64_t protected_revision=0,subscription_revision=0,last_unix_s=0;
  bool quarantined=false;
  std::string restore_id;
  Digest recovery_policy_sha256;
};
inline Bytes strings(const std::vector<std::string>& values) {return pack(values);}
inline bool strings(std::string_view bytes,std::vector<std::string>& values,std::size_t bound) {
  if(!unpack(bytes,values,bound).ok()||values.size()>bound)return false;
  std::set<std::string> seen;
  for(const auto& value:values)if(!valid_token(value,256)||!seen.insert(value).second)return false;
  return true;
}
inline Bytes encode(const Active& value) {
  std::vector<std::string> sources,metrics;
  for(const auto& source:value.view.sources)sources.push_back(encode(source));
  for(const auto& metric:value.view.metrics)metrics.push_back(encode(metric));
  return pack({"active/2",number(value.view.revision),number(value.view.lifetime_observations),
    pack(sources),pack(metrics),strings(value.peers),strings(value.subscriptions),strings(value.pins),
    number(value.protected_revision),number(value.subscription_revision),number(value.last_unix_s),
    value.quarantined?"1":"0",value.restore_id,value.recovery_policy_sha256});
}
inline bool decode(std::string_view bytes,Active& value,const Options& options) {
  Reader r(bytes,"active/2");value.view.revision=r.value();value.view.lifetime_observations=r.value();
  std::vector<std::string> sources,metrics;
  r.valid=unpack(r.text(),sources,options.max_active_sources).ok()&&r.valid;
  r.valid=unpack(r.text(),metrics,options.max_metrics).ok()&&r.valid;
  std::set<std::string> ids;
  for(const auto& bytes_:sources){Source source;if(!decode(bytes_,source)||source.phase==SourcePhase::retired||
      !ids.insert(source_key(source.id)).second)return false;
    value.view.sources.push_back(std::move(source));}
  ids.clear();
  for(const auto& bytes_:metrics){Metric metric;if(!decode(bytes_,metric)||
      !ids.insert(source_key(metric.source)+":"+metric.observation.metric).second)return false;
    value.view.metrics.push_back(std::move(metric));}
  r.valid=strings(r.text(),value.peers,options.max_active_peers)&&r.valid;
  r.valid=strings(r.text(),value.subscriptions,options.max_active_subscriptions)&&r.valid;
  r.valid=strings(r.text(),value.pins,options.max_pins)&&r.valid;
  value.protected_revision=r.value();value.subscription_revision=r.value();value.last_unix_s=r.value();
  const auto quarantined=r.value();value.quarantined=quarantined==1;value.restore_id=r.text();
  value.recovery_policy_sha256=r.text();
  value.view.active_peers=value.peers.size();value.view.active_subscriptions=value.subscriptions.size();
  value.view.pins=value.pins.size();
  value.view.quarantined=value.quarantined;value.view.incarnation=value.restore_id;
  value.view.recovery_policy_sha256=value.recovery_policy_sha256;
  return r.done()&&quarantined<=1&&sources.size()<=options.max_active_sources&&metrics.size()<=options.max_metrics&&
    (value.restore_id.empty()||valid_digest(value.restore_id))&&
    (value.recovery_policy_sha256.empty()||valid_digest(value.recovery_policy_sha256))&&
    (!value.quarantined||(!value.restore_id.empty()&&value.recovery_policy_sha256.empty()));
}
struct Fence {std::string owner,clock;std::uint64_t issued=0,closed=0;};
inline Bytes encode(const Fence& v) {return pack({"fence/1",v.owner,v.clock,number(v.issued),number(v.closed)});}
inline bool decode(std::string_view bytes,Fence& v) {
  Reader r(bytes,"fence/1");v.owner=r.text();v.clock=r.text();v.issued=r.value();v.closed=r.value();
  return r.done()&&valid_token(v.owner,256)&&valid_token(v.clock)&&v.closed<=v.issued;
}
inline Bytes encode(const Peer& v) {return pack({"peer/1",v.principal,v.certificate_sha256,number(v.generation),
  number(v.expires_unix_s),number(static_cast<std::uint64_t>(v.phase))});}
inline bool decode(std::string_view bytes,Peer& v) {
  Reader r(bytes,"peer/1");v.principal=r.text();v.certificate_sha256=r.text();v.generation=r.value();
  v.expires_unix_s=r.value();v.phase=r.enumeration<PeerPhase>(2);
  return r.done()&&valid_token(v.principal,256)&&valid_digest(v.certificate_sha256)&&v.generation>0;
}
inline Bytes encode(const Subscription& v) {return pack({"subscription/1",v.id,v.principal,encode(v.scope),
  number(v.generation),number(v.lease_until_unix_s),number(static_cast<std::uint64_t>(v.phase)),
  encode(v.acknowledged),encode(v.offered),v.has_offer?"1":"0"});}
inline bool decode(std::string_view bytes,Subscription& v) {
  Reader r(bytes,"subscription/1");v.id=r.text();v.principal=r.text();r.valid=decode(r.text(),v.scope)&&r.valid;
  v.generation=r.value();v.lease_until_unix_s=r.value();v.phase=r.enumeration<SubscriptionPhase>(3);
  r.valid=decode(r.text(),v.acknowledged)&&r.valid;r.valid=decode(r.text(),v.offered)&&r.valid;
  const auto offered=r.value();v.has_offer=offered==1;
  return r.done()&&offered<=1&&valid_token(v.id,256)&&valid_token(v.principal,256)&&v.generation>0;
}
inline Bytes encode(const Pin& v) {return pack({"pin/1",v.id,v.owner,number(static_cast<std::uint64_t>(v.kind)),
  number(v.generation),number(v.expires_unix_s),pack(v.objects)});}
inline bool decode(std::string_view bytes,Pin& v) {
  Reader r(bytes,"pin/1");v.id=r.text();v.owner=r.text();v.kind=r.enumeration<PinKind>(6);
  v.generation=r.value();v.expires_unix_s=r.value();r.valid=unpack(r.text(),v.objects,1024).ok()&&r.valid;
  return r.done()&&valid_token(v.id,256)&&valid_token(v.owner,256)&&v.generation>0&&
    std::all_of(v.objects.begin(),v.objects.end(),[](const auto& x){return valid_digest(x);});
}
inline Bytes encode(const MaintenanceRequest& v) {return pack({"maintenance-request/1",v.id,encode(v.source),
  number(v.expire_through),number(v.now_unix_s),number(static_cast<std::uint64_t>(v.disposition)),
  v.archive_directory,pack(v.objects)});}
inline bool decode(std::string_view bytes,MaintenanceRequest& v) {
  Reader r(bytes,"maintenance-request/1");v.id=r.text();r.valid=decode(r.text(),v.source)&&r.valid;
  v.expire_through=r.value();v.now_unix_s=r.value();v.disposition=r.enumeration<HistoryAvailability>(2);
  v.archive_directory=r.text();r.valid=unpack(r.text(),v.objects,1024).ok()&&r.valid;
  return r.done()&&valid_token(v.id,256)&&v.disposition!=HistoryAvailability::retained&&
    std::all_of(v.objects.begin(),v.objects.end(),[](const auto& x){return valid_digest(x);});
}
inline Bytes encode(const MaintenancePlan& v) {return pack({"maintenance-plan/1",encode(v.request),
  number(static_cast<std::uint64_t>(v.phase)),number(v.protected_revision),number(v.subscription_revision),
  number(v.source_sequence),number(v.expected_bytes),number(v.new_floor),v.protected_root_sha256,v.source_tip_sha256});}
inline bool decode(std::string_view bytes,MaintenancePlan& v) {
  Reader r(bytes,"maintenance-plan/1");r.valid=decode(r.text(),v.request)&&r.valid;
  v.phase=r.enumeration<MaintenancePhase>(2);v.protected_revision=r.value();v.subscription_revision=r.value();
  v.source_sequence=r.value();v.expected_bytes=r.value();v.new_floor=r.value();
  v.protected_root_sha256=r.text();v.source_tip_sha256=r.text();
  return r.done()&&valid_digest(v.protected_root_sha256)&&
    valid_cursor({v.source_sequence,v.source_tip_sha256})&&v.new_floor>0;
}
struct Floor {std::uint64_t next=1;HistoryAvailability availability=HistoryAvailability::retained;std::string archive;Cursor tip;};
inline Bytes encode(const Floor& v) {return pack({"floor/1",number(v.next),number(static_cast<std::uint64_t>(v.availability)),v.archive,encode(v.tip)});}
inline bool decode(std::string_view bytes,Floor& v) {
  Reader r(bytes,"floor/1");v.next=r.value();v.availability=r.enumeration<HistoryAvailability>(2);
  v.archive=r.text();r.valid=decode(r.text(),v.tip)&&r.valid;return r.done()&&v.next>0;
}
inline bool add(std::uint64_t& target,std::uint64_t increment=1) {
  if(target>std::numeric_limits<std::uint64_t>::max()-increment)return false;
  target+=increment;return true;
}
inline void erase(std::vector<std::string>& values,std::string_view value) {
  values.erase(std::remove(values.begin(),values.end(),value),values.end());
}
inline bool contains(const std::vector<std::string>& values,std::string_view value) {
  return std::find(values.begin(),values.end(),value)!=values.end();
}
} // namespace anaiero::sdk::lifecycle::detail
