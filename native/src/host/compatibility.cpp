#include "compatibility.hpp"
#include "deployment.hpp"
#include "anaiero/sdk/session_runner.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace anaiero::deployment {
namespace {
namespace ev=evidence;
Result invalid(std::string text){return {Code::corrupt,std::move(text)};}
Result normalized(std::string_view input,Bytes& output){
  if(input.empty()||input.size()>1024U*1024U)return {Code::capacity,"build witness is empty or exceeds 1 MiB"};
  output.clear();output.reserve(input.size());
  for(std::size_t i=0;i<input.size();++i){
    if(input[i]=='\r'){if(i+1==input.size()||input[i+1]!='\n')return invalid("build witness contains a bare CR");continue;}
    if(input[i]=='\0')return invalid("build witness contains NUL");
    output+=input[i];
  }
  if(output.back()!='\n')return invalid("build witness has no final newline");
  return {};
}
struct Manifest {std::vector<std::string> metadata,suffix;std::map<std::string,Digest> sources;};
Result manifest(std::string_view raw,std::string_view header,Manifest& out){
  out={};std::vector<std::string> lines;std::size_t at=0;
  while(at<raw.size()){
    const auto end=raw.find('\n',at);if(end==raw.npos||lines.size()==8192)return invalid("build witness line bound");
    lines.emplace_back(raw.substr(at,end-at));at=end+1;
  }
  if(lines.size()<14||lines[0]!=header)return invalid("unsupported build witness schema");
  out.metadata.assign(lines.begin()+1,lines.begin()+11);std::string previous;
  bool suffix=false;
  for(std::size_t i=11;i<lines.size();++i){const auto& line=lines[i];
    const auto split=line.rfind(' ');const bool entry=split!=line.npos&&valid_digest(std::string_view(line).substr(split+1));
    if(entry&&!suffix){
      const auto name=line.substr(0,split);
      if(name.empty()||name.front()=='/'||name.find("..")!=name.npos||name.find('\\')!=name.npos||name.find(':')!=name.npos||
         (!previous.empty()&&name<=previous))return invalid("unsafe or unsorted build witness dependency");
      previous=name;out.sources.emplace(name,line.substr(split+1));
    }else{suffix=true;out.suffix.push_back(line);}
  }
  if(out.sources.empty()||out.suffix.size()<3||out.suffix.size()>5)return invalid("incomplete build witness");
  return {};
}
Bytes witness_wire(const Witness& w){return pack({w.build_id,w.compatibility_id,w.build_sha256,w.compatibility_sha256});}
Result witness_read(std::string_view bytes,Witness& w){
  std::vector<std::string> f;if(!unpack(bytes,f,4,1024)||f.size()!=4)return invalid("invalid binding witness");
  for(const auto& field:f)if(!valid_digest(field))return invalid("invalid binding witness digest");
  w={};w.build_id=f[0];w.compatibility_id=f[1];w.build_sha256=f[2];w.compatibility_sha256=f[3];return {};
}
Result hydrate(ev::ICommitStore& store,Witness& witness){
  const auto expected_build=witness.build_sha256,expected_compatibility=witness.compatibility_sha256;
  if(auto r=store.get_blob(expected_build,witness.build_manifest);!r)
    return r.code==Code::not_found?invalid("canonical binding's build witness is missing"):r;
  if(auto r=store.get_blob(expected_compatibility,witness.compatibility_manifest);!r)
    return r.code==Code::not_found?invalid("canonical binding's compatibility witness is missing"):r;
  if(sha256(witness.build_manifest)!=expected_build||sha256(witness.compatibility_manifest)!=expected_compatibility)
    return invalid("canonical compatibility witness bytes differ");
  return validate_witness(witness);
}
}
Result validate_witness(Witness& witness){
  Bytes full,closure;if(auto r=normalized(witness.build_manifest,full);!r)return r;
  if(auto r=normalized(witness.compatibility_manifest,closure);!r)return r;
  if(!valid_digest(witness.build_id)||sha256(full)!=witness.build_id)return invalid("whole-build witness differs from exact compiled identity");
  const auto compatible=sha256(closure);
  if(!witness.compatibility_id.empty()&&witness.compatibility_id!=compatible)return invalid("compatibility witness digest differs");
  Manifest build,contract;if(auto r=manifest(full,"Anaiero builtins/1",build);!r)return r;
  if(auto r=manifest(closure,"Anaiero engine compatibility/1",contract);!r)return r;
  if(contract.metadata!=build.metadata||contract.suffix.size()!=build.suffix.size()+1||
     !std::equal(build.suffix.begin(),build.suffix.end(),contract.suffix.begin())||
     contract.suffix.back()!="Contracts: session/2;engine-state/1;canonical-root/1;compatibility-binding/1")
    return {Code::unsupported,"compiler/platform/configuration or state contract differs"};
  for(const auto& [name,digest]:contract.sources){const auto found=build.sources.find(name);
    if(found==build.sources.end()||found->second!=digest)return invalid("engine dependency differs from exact whole-build witness: "+name);
  }
  witness.compatibility_id=compatible;witness.build_sha256=sha256(witness.build_manifest);
  witness.compatibility_sha256=sha256(witness.compatibility_manifest);return {};
}
Bytes encode_binding(const Binding& binding){
  std::vector<std::string> origins;for(const auto& witness:binding.origins)origins.push_back(witness_wire(witness));
  return pack({"anaiero.compatibility-binding/1",witness_wire(binding.target),pack(origins)});
}
Result decode_binding(std::string_view bytes,Binding& out){
  std::vector<std::string> f,origins;Binding next;
  if(!unpack(bytes,f,3,32768)||f.size()!=3||f[0]!="anaiero.compatibility-binding/1"||!unpack(f[2],origins,16,16384))
    return invalid("invalid bounded compatibility binding");
  if(auto r=witness_read(f[1],next.target);!r)return r;
  next.target_build=next.target.build_id;next.compatibility_id=next.target.compatibility_id;Digest previous;
  for(const auto& raw:origins){Witness value;if(auto r=witness_read(raw,value);!r)return r;
    if(value.compatibility_id!=next.compatibility_id||value.build_id==next.target_build||(!previous.empty()&&value.build_id<=previous))
      return invalid("duplicate, unordered or incompatible binding origin");
    previous=value.build_id;next.origins.push_back(std::move(value));
  }
  out=std::move(next);return {};
}
Result session_modules(ev::ICommitStore& store,std::vector<Digest>& modules,bool quiescent){
  modules.clear();ev::StoredRecord record;
  if(auto r=store.latest("session-definition","main",record);!r)return r;
  SessionDefinition definition;if(record.record.tombstone||!decode_session_definition(record.record.payload,definition))
    return invalid("invalid retained session definition");
  std::set<Digest> unique;
  for(const auto& scheduled:definition.engines)unique.insert(scheduled.identity.module_sha256);
  if(auto r=store.latest("session-progress","main",record);!r)return r;
  std::vector<std::string> progress;
  if(record.record.tombstone||!unpack(record.record.payload,progress,4)||progress.size()!=4||progress[0]!="session-progress/2"||
     (progress[2]!="0"&&progress[2]!="1"))return invalid("invalid retained cycle progress");
  if(quiescent&&progress[2]=="1")return {Code::busy,"finish the pending durable cycle before package migration"};
  std::string after;bool more=true;std::size_t count=0;
  while(more){std::vector<ev::StoredRecord> page;
    if(auto r=store.scan_latest("lifecycle.source",{},after,64,page,more);!r)return r;
    if(more&&page.empty())return invalid("source witness scan did not advance");
    for(const auto& source:page){after=source.record.key;
      if(++count>4096)return {Code::capacity,"migration admits at most 4096 retained lifecycle identities"};
      if(source.record.tombstone)return invalid("lifecycle identity cannot be silently omitted during migration");
      std::vector<std::string> fields;EngineIdentity identity;
      if(!unpack(source.record.payload,fields,11)||fields.size()!=11||fields[0]!="source/3"||!decode_identity(fields[2],identity)||
         source.record.key!=sha256(identity.source_uid)+":"+std::to_string(identity.epoch))return invalid("invalid retained lifecycle identity");
      unique.insert(identity.module_sha256);
    }
    if(unique.size()>16)return {Code::capacity,"migration admits at most 16 exact retained build identities"};
  }
  modules.assign(unique.begin(),unique.end());return {};
}
Result load_binding(ev::ICommitStore& store,Binding& binding){
  ev::StoredRecord record;if(auto r=store.latest("operator.deployment-binding","active",record);!r)return r;
  if(record.record.tombstone)return {Code::expired,"active compatibility binding is retired"};
  if(auto r=decode_binding(record.record.payload,binding);!r)return r;
  std::set<Digest> expected{binding.target.build_sha256,binding.target.compatibility_sha256};
  for(const auto& origin:binding.origins){expected.insert(origin.build_sha256);expected.insert(origin.compatibility_sha256);}
  if(record.record.references!=std::vector<Digest>(expected.begin(),expected.end()))return invalid("canonical compatibility binding does not protect its complete exact witnesses");
  if(auto r=hydrate(store,binding.target);!r)return r;
  for(auto& origin:binding.origins)if(auto r=hydrate(store,origin);!r)return r;
  return {};
}
Result authorize_binding(ev::ICommitStore& store,const Witness& executing,std::vector<Digest>& aliases){
  aliases.clear();Binding binding;const auto found=load_binding(store,binding);if(found.code==Code::not_found)return {};if(!found)return found;
  Service deployment(store);Selection selection;if(auto r=deployment.selection(selection);!r)return r;
  if(!selection.revision||selection.current.module_sha256!=binding.target_build)return invalid("canonical selection and active compatibility binding disagree");
  Witness selected;const auto source=deployment.witness(selection.current,selected);
  if(!source)return source.code==Code::not_found?invalid("selected package lacks its canonical build witness"):source;
  if(selected.build_sha256!=binding.target.build_sha256||selected.compatibility_sha256!=binding.target.compatibility_sha256)
    return invalid("canonical binding target is not the exact selected package source witness");
  auto current=executing;if(auto r=validate_witness(current);!r)return r;
  if(binding.target_build!=current.build_id)return {Code::unsupported,"canonical compatibility binding selects another exact build"};
  if(binding.compatibility_id!=current.compatibility_id||binding.target.build_id!=current.build_id)
    return {Code::unsupported,"executing engine closure differs from the reviewed binding"};
  for(const auto& origin:binding.origins){
    if(origin.compatibility_id!=current.compatibility_id)return {Code::unsupported,"retained source closure differs"};
    aliases.push_back(origin.build_id);
  }
  return {};
}
Result prepare_binding(ev::ICommitStore& store,const Witness& requested,const Witness& previous,Binding& output){
  auto target=requested,current=previous;
  if(auto r=validate_witness(target);!r)return r;
  if(auto r=validate_witness(current);!r)return r;
  if(target.compatibility_id!=current.compatibility_id)return {Code::unsupported,"complete engine closure/toolchain changed; semantic migration is unsupported"};
  std::vector<Digest> modules;if(auto r=session_modules(store,modules,true);!r)return r;
  Binding old;const auto existing=load_binding(store,old);if(!existing&&existing.code!=Code::not_found)return existing;
  if(existing&&(old.target_build!=current.build_id||old.compatibility_id!=current.compatibility_id))
    return {Code::conflict,"selected package differs from active compatibility binding"};
  Binding next;next.target=std::move(target);next.target_build=next.target.build_id;next.compatibility_id=next.target.compatibility_id;
  for(const auto& module:modules){
    if(module==next.target_build)continue;
    if(module==current.build_id){next.origins.push_back(current);continue;}
    const auto found=std::find_if(old.origins.begin(),old.origins.end(),[&](const auto& witness){return witness.build_id==module;});
    if(!existing||found==old.origins.end())return {Code::unsupported,"retained scheduled/retired engine identity lacks an exact compatibility witness"};
    next.origins.push_back(*found);
  }
  output=std::move(next);return {};
}
Result stage_binding(ev::ICommitStore& store,const Binding& binding,ev::CommitBatch& batch){
  ev::StoredRecord previous;const auto found=store.latest("operator.deployment-binding","active",previous);
  if(!found&&found.code!=Code::not_found)return found;
  std::vector<Digest> references;
  const auto add=[&](const Witness& witness){references.push_back(witness.build_sha256);references.push_back(witness.compatibility_sha256);};
  add(binding.target);for(const auto& origin:binding.origins)add(origin);
  std::sort(references.begin(),references.end());references.erase(std::unique(references.begin(),references.end()),references.end());
  batch.guards.push_back({"operator.deployment-binding","active",found?previous.revision:0,found?previous.digest:Digest{}});
  batch.records.push_back({"operator.deployment-binding","active",encode_binding(binding),std::move(references),false});return {};
}
}
