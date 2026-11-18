#pragma once
// Canonical bounded materialization checkpoint. The Registry binds these bytes
// to a verified global journal cursor; this codec preserves each source cursor.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include "christeye/living/plan_hash.hpp"
#include <sstream>
#include <string>
#include <string_view>
namespace anaiero::runtime {
class ObservationCheckpointCodec final {
  static std::string body(const ObservationReplayCheckpoint& saved,std::string_view session) {
    std::ostringstream out;const auto& s=saved.state;
    out<<"CEVOBCP/1\nsession "<<session<<"\nstate "<<s.revision<<' '<<s.committed_updates<<' '<<s.sequence_gaps<<' '<<s.count<<' '<<s.sources<<'\n';
    for(std::size_t i=0;i<s.count;++i){const auto& m=s.values[i];
      out<<"metric "<<m.colony<<' '<<m.animon<<' '<<m.source<<' '<<static_cast<unsigned>(m.metric)<<' '<<m.value<<' '
        <<static_cast<unsigned>(m.origin)<<' '<<static_cast<unsigned>(m.quality)<<' '<<m.sequence<<' '
        <<m.acquired_ns<<' '<<m.evidence_tag<<' '<<m.source_epoch<<'\n';
    }
    for(std::size_t i=0;i<s.sources;++i){const auto& c=s.cursors[i];
      out<<"source "<<c.id<<' '<<c.clock_domain<<' '<<c.last_sequence<<' '<<c.last_acquired_ns<<' '
        <<c.has_sequence<<' '<<c.gap<<' '<<c.epoch<<'\n';
    }
    out<<"last "<<saved.count<<'\n';
    for(std::size_t i=0;i<saved.count;++i){const auto& o=saved.last[i];
      out<<"observation "<<o.colony<<' '<<o.animon<<' '<<o.source<<' '<<o.clock_domain<<' '
        <<static_cast<unsigned>(o.metric)<<' '<<static_cast<unsigned>(o.lane)<<' '<<static_cast<unsigned>(o.origin)<<' '
        <<o.value<<' '<<o.urgency<<' '<<o.importance<<' '<<o.sequence<<' '<<o.acquired_ns<<' '<<o.evidence_tag<<' '<<o.source_epoch<<'\n';
    }
    out<<"end\n";return out.str();
  }
public:
  static constexpr std::size_t max_bytes=128*1024;
  static bool encode(const ObservationReplayCheckpoint& saved,std::string& wire,std::string_view session="default") {
    if(saved.state.count>kMaxMetrics||saved.state.sources>kMaxSources||saved.count!=saved.state.sources)return false;
    const auto canonical=body(saved,session);wire=canonical+"seal "+christeye::living::plans::sha256_hex(canonical)+'\n';
    return wire.size()<=max_bytes;
  }
  static bool decode(std::string_view wire,ObservationReplayCheckpoint& saved,std::string_view expected_session="default") {
    if(wire.size()>max_bytes)return false;
    const auto seal=wire.rfind("seal ");
    if(seal==std::string_view::npos||wire.size()!=seal+70||wire.back()!='\n'||
       christeye::living::plans::sha256_hex(wire.substr(0,seal))!=wire.substr(seal+5,64))return false;
    ObservationReplayCheckpoint next;auto& s=next.state;
    std::istringstream in{std::string(wire.substr(0,seal))};std::string magic,tag,extra,session;
    if(!(in>>magic>>tag>>session)||magic!="CEVOBCP/1"||tag!="session"||session!=expected_session||
       !(in>>tag)||tag!="state"||
       !(in>>s.revision>>s.committed_updates>>s.sequence_gaps>>s.count>>s.sources)||s.count>kMaxMetrics||s.sources>kMaxSources)return false;
    for(std::size_t i=0;i<s.count;++i){auto& m=s.values[i];unsigned metric=0,origin=0,quality=0;
      if(!(in>>tag>>m.colony>>m.animon>>m.source>>metric>>m.value>>origin>>quality>>m.sequence>>m.acquired_ns>>m.evidence_tag>>m.source_epoch)||
         tag!="metric"||metric>255||origin>255||quality>3)return false;
      m.metric=static_cast<Metric>(metric);m.origin=static_cast<Origin>(origin);m.quality=static_cast<Quality>(quality);
      if(!valid_metric(m.metric)||!valid_origin(m.origin))return false;
    }
    for(std::size_t i=0;i<s.sources;++i){auto& c=s.cursors[i];unsigned has=0,gap=0;
      if(!(in>>tag>>c.id>>c.clock_domain>>c.last_sequence>>c.last_acquired_ns>>has>>gap>>c.epoch)||tag!="source"||has>1||gap>1)return false;
      c.has_sequence=has!=0;c.gap=gap!=0;
    }
    if(!(in>>tag>>next.count)||tag!="last"||next.count!=s.sources)return false;
    for(std::size_t i=0;i<next.count;++i){auto& o=next.last[i];unsigned metric=0,lane=0,origin=0;
      if(!(in>>tag>>o.colony>>o.animon>>o.source>>o.clock_domain>>metric>>lane>>origin>>o.value>>o.urgency>>o.importance>>
         o.sequence>>o.acquired_ns>>o.evidence_tag>>o.source_epoch)||tag!="observation"||metric>255||lane>255||origin>255)return false;
      o.metric=static_cast<Metric>(metric);o.lane=static_cast<Lane>(lane);o.origin=static_cast<Origin>(origin);
      if(!valid_observation(o)||!o.source_epoch)return false;
    }
    if(!(in>>tag)||tag!="end"||in>>extra)return false;
    std::string canonical;if(!encode(next,canonical,session)||canonical!=wire)return false;
    saved=next;return true;
  }
};
} // namespace anaiero::runtime
