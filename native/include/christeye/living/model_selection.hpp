#pragma once
// CE-SW-15 / SIM3.3: deterministic, digital-only prespecified law selection.
// No biology, health inference, genotype prediction or physical actuation.
// Training and final test data are kept in different sealed files; the
// selection entrypoint has no holdout-file argument and never opens it.
#include "christeye/living/model_fidelity.hpp"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace christeye::living::selection {
namespace fs = std::filesystem;
namespace sim = christeye::living::sim3;
namespace fid = christeye::living::fidelity;
namespace pl = christeye::living::plans;
inline constexpr std::size_t kMaxCandidates=16;
inline constexpr std::uint32_t kMaxHorizon=256;
inline constexpr std::uint64_t kScale=1000000;

enum class Error : std::uint8_t {
 ok, malformed, bad_seal, tampered_artifact, bad_configuration, bad_dataset,
 incompatible_models, missing_checkpoint, bad_snapshot, unauthorized_test,
 mismatch, capacity, io_error
};
struct Row {std::uint32_t tick=0;std::string colony,metric;std::uint32_t observed=0,radius=1;};
struct DataSet {
 std::string name,sha256,source_sha256,partition;
 std::vector<Row> rows;
 static Error parse(std::string_view contents,std::string_view expected_partition,DataSet& output) {
   std::vector<std::vector<std::string>> lines;
   auto status=fid::sealed_lines(contents,lines);
   if(status.code!=fid::Status::ok)return Error::bad_seal;
   if(lines.size()<9||lines[0]!=std::vector<std::string>{"CEVSPLIT/1"}||
      lines[1].size()!=2||lines[1][0]!="name"||!sim::ident(lines[1][1])||
      lines[2]!=std::vector<std::string>{"partition",std::string(expected_partition)}||
      lines[3]!=std::vector<std::string>{"origin","SYNTHETIC_FIXTURE"}||
      lines[4].size()!=2||lines[4][0]!="source_sha256"||!sim::digest(lines[4][1])||
      lines[5]!=std::vector<std::string>{"units","DIMENSIONLESS_PROXY"}||
      lines.back()!=std::vector<std::string>{"end"})return Error::malformed;
   DataSet next;next.name=lines[1][1];next.partition=std::string(expected_partition);
   next.source_sha256=lines[4][1];next.sha256=pl::sha256_hex(contents);
   std::set<std::string> keys;
   for(std::size_t i=6;i+1<lines.size();++i){
     const auto& t=lines[i];std::uint32_t tick=0,value=0,radius=0;
     if(t.size()!=6||t[0]!="row"||!sim::parse_u32(t[1],tick)||tick>kMaxHorizon||
        !(t[2]=="all"||sim::ident(t[2]))||!fid::is_metric(t[3])||
        !sim::parse_u32(t[4],value)||!sim::parse_u32(t[5],radius)||
        radius==0||radius>100000||next.rows.size()>=fid::kMaxRecords||
        !keys.insert(t[1]+"/"+t[2]+"/"+t[3]).second)return Error::bad_dataset;
     next.rows.push_back({tick,t[2],t[3],value,radius});
   }
   if(next.rows.size()<2)return Error::bad_dataset;
   output=std::move(next);return Error::ok;
 }
};
struct Candidate {
 std::string name,config_path,config_sha256,law_path,law_sha256;
};
struct Manifest {
 std::string name,source_sha256,training_path,training_sha256,test_sha256,sha256;
 std::uint32_t horizon=0;
 std::vector<Candidate> candidates;
 static Error parse(std::string_view bytes,Manifest& output){
   std::vector<std::vector<std::string>> lines;
   if(fid::sealed_lines(bytes,lines).code!=fid::Status::ok)return Error::bad_seal;
   if(lines.size()<10||lines[0]!=std::vector<std::string>{"CEVSELECT/1"}||
      lines[1].size()!=2||lines[1][0]!="name"||!sim::ident(lines[1][1])||
      lines[2]!=std::vector<std::string>{"domain","SYNTHETIC_ONLY"}||
      lines[3].size()!=2||lines[3][0]!="source_sha256"||!sim::digest(lines[3][1])||
      lines[4].size()!=2||lines[4][0]!="horizon"||
      lines[5].size()!=3||lines[5][0]!="train"||!sim::safe_relative(lines[5][1])||!sim::digest(lines[5][2])||
      lines[6].size()!=2||lines[6][0]!="reserved_test_sha256"||!sim::digest(lines[6][1])||
      lines.back()!=std::vector<std::string>{"end"})return Error::malformed;
   Manifest next;next.name=lines[1][1];next.source_sha256=lines[3][1];
   next.training_path=lines[5][1];next.training_sha256=lines[5][2];next.test_sha256=lines[6][1];
   if(!sim::parse_u32(lines[4][1],next.horizon)||next.horizon==0||next.horizon>kMaxHorizon||
      next.test_sha256==next.training_sha256)return Error::bad_dataset;
   std::set<std::string> names,configs;
   for(std::size_t i=7;i+1<lines.size();++i){const auto& t=lines[i];
     if(t.size()!=6||t[0]!="candidate"||!sim::ident(t[1])||
        !sim::safe_relative(t[2])||!sim::digest(t[3])||
        !sim::safe_relative(t[4])||!sim::digest(t[5])||
        !names.insert(t[1]).second||!configs.insert(t[3]).second||
        next.candidates.size()>=kMaxCandidates)return Error::malformed;
     next.candidates.push_back({t[1],t[2],t[3],t[4],t[5]});
   }
   if(next.candidates.size()<2)return Error::malformed;
   next.sha256=pl::sha256_hex(bytes);output=std::move(next);return Error::ok;
 }
};
struct Entry {
 std::string id,config_hash,law_hash,tip_hash;
 std::uint64_t normalized_sum=0;
 std::uint64_t absolute_sum=0;
 std::size_t count=0;
};
struct Selection {
 std::string winner,report,sha256,manifest_sha256,training_sha256,test_sha256;
 std::vector<Entry> ranked;
};
inline bool load_pinned(const fs::path& root,const std::string& relative,
                        const std::string& digest,std::string& contents){
 return fid::pinned_file(root,relative,digest,contents);
}
inline std::string topology(const sim::Configuration& c) {
 std::ostringstream out;
 out<<c.material_role<<' '<<c.material_sha<<' '<<c.name<<'\n';
 for(const auto& co:c.colonies)out<<co.id<<' '<<co.source_sha<<'\n';
 for(const auto& s:c.seeds)out<<s.colony<<' '<<s.id<<' '<<s.parent<<' '<<s.kind<<' '<<s.x<<' '<<s.y<<' '<<s.z<<'\n';
 for(const auto& l:c.links)out<<l.from<<' '<<l.to<<' '<<sim::channel_text(l.channel)<<' '<<l.units<<'\n';
 for(const auto& p:c.processes)out<<p.colony<<' '<<sim::process_text(p.kind)<<' '<<p.subject<<' '<<p.value<<' '<<p.at<<'\n';
 return pl::sha256_hex(out.str());
}
inline std::string snapshot_name(std::uint32_t tick){
 std::ostringstream name;name<<"tick-"<<std::setfill('0')<<std::setw(6)<<tick<<".state";return name.str();
}
inline bool load_history(const sim::Configuration& config,const fs::path& run_dir,
                         std::uint32_t horizon,std::map<std::uint32_t,sim::Snapshot>& snapshots,
                         std::string& tip_sha){
 for(std::uint32_t tick=0;tick<=horizon;++tick){
   std::string bytes;
   if(!sim::bounded_read(run_dir/"history"/snapshot_name(tick),sim::kMaxSnapshotBytes,bytes))return false;
   sim::Snapshot snap;
   if(sim::decode_snapshot(bytes,config,snap)!=sim::Result::ok||snap.tick!=tick)return false;
   if(tick==horizon)tip_sha=pl::sha256_hex(bytes);
   snapshots.emplace(tick,std::move(snap));
 }
 return true;
}
inline Error candidate_config(const Candidate& candidate,const fs::path& root,
                              std::string_view source_hash,sim::Configuration& config){
 std::string bytes;
 if(!load_pinned(root,candidate.config_path,candidate.config_sha256,bytes))return Error::tampered_artifact;
 fs::path full;
 if(!sim::bound_path(root,candidate.config_path,full))return Error::bad_configuration;
 sim::Configuration loaded;
 if(sim::Configuration::load(full,loaded)!=sim::Result::ok||loaded.hash!=candidate.config_sha256||
    loaded.material_sha!=source_hash)return Error::bad_configuration;
 if(!load_pinned(root,candidate.law_path,candidate.law_sha256,bytes))return Error::tampered_artifact;
 fid::LawProfile law;
 if(fid::LawProfile::parse(bytes,law).code!=fid::Status::ok||!law.matches(loaded))return Error::bad_configuration;
 config=std::move(loaded);return Error::ok;
}
inline Error load_training(const Manifest& manifest,const fs::path& root,DataSet& data){
 std::string bytes;
 if(!load_pinned(root,manifest.training_path,manifest.training_sha256,bytes))return Error::tampered_artifact;
 if(auto r=DataSet::parse(bytes,"TRAIN_ONLY",data);r!=Error::ok)return r;
 if(data.source_sha256!=manifest.source_sha256)return Error::bad_dataset;
 for(const auto& row:data.rows)if(row.tick>manifest.horizon)return Error::bad_dataset;
 return Error::ok;
}
inline Error select(const Manifest& manifest,const fs::path& root,const fs::path& runs,
                    Selection& output) {
 DataSet train;
 if(auto err=load_training(manifest,root,train);err!=Error::ok)return err;
 std::vector<Entry> entries;
 std::string topology_pin;
 for(const auto& candidate:manifest.candidates){
   sim::Configuration config;
   if(auto err=candidate_config(candidate,root,manifest.source_sha256,config);err!=Error::ok)return err;
   const auto fingerprint=topology(config);
   if(!topology_pin.empty() && topology_pin!=fingerprint)return Error::incompatible_models;
   topology_pin=fingerprint;
   std::map<std::uint32_t,sim::Snapshot> history;
   std::string tip;
   if(!load_history(config,runs/candidate.name,manifest.horizon,history,tip))return Error::missing_checkpoint;
   Entry score;score.id=candidate.name;score.config_hash=candidate.config_sha256;
   score.law_hash=candidate.law_sha256;score.tip_hash=tip;
   for(const auto& row:train.rows){
     const fid::Observation observation{row.tick,row.colony,row.metric,"train",row.observed,row.radius};
     std::int64_t amount=0;
     if(!fid::predicted(history.at(row.tick),observation,amount))return Error::bad_dataset;
     const auto residual=static_cast<std::uint64_t>(std::llabs(amount-static_cast<std::int64_t>(row.observed)));
     if(residual>1000000)return Error::capacity;
     const auto value=(residual*kScale+row.radius/2u)/row.radius;
     score.normalized_sum+=value;
     score.absolute_sum+=residual;
     ++score.count;
   }
   entries.push_back(std::move(score));
 }
 std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){
   return a.normalized_sum!=b.normalized_sum?a.normalized_sum<b.normalized_sum:a.id<b.id;
 });
 std::ostringstream text;
 text<<"CEVWINNER/1\nname "<<manifest.name<<"\nmanifest_sha256 "<<manifest.sha256
     <<"\ntraining_sha256 "<<train.sha256<<"\nreserved_test_sha256 "<<manifest.test_sha256
     <<"\nwinner "<<entries.front().id<<"\n";
 for(const auto& e:entries)text<<"rank "<<e.id<<' '<<e.normalized_sum<<' '<<e.absolute_sum
    <<' '<<e.count<<' '<<e.config_hash<<' '<<e.law_hash<<' '<<e.tip_hash<<'\n';
 text<<"end\n";const auto body=text.str();text<<"seal "<<pl::sha256_hex(body)<<'\n';
 output={entries.front().id,text.str(),pl::sha256_hex(text.str()),manifest.sha256,train.sha256,manifest.test_sha256,std::move(entries)};
 return Error::ok;
}
inline Error read_winner(std::string_view bytes,const Manifest& manifest,Selection& out){
 std::vector<std::vector<std::string>> lines;
 if(fid::sealed_lines(bytes,lines).code!=fid::Status::ok)return Error::bad_seal;
 if(lines.size()!=7+manifest.candidates.size()||lines[0]!=std::vector<std::string>{"CEVWINNER/1"}||
    lines[1]!=std::vector<std::string>{"name",manifest.name}||
    lines[2]!=std::vector<std::string>{"manifest_sha256",manifest.sha256}||
    lines[3]!=std::vector<std::string>{"training_sha256",manifest.training_sha256}||
    lines[4]!=std::vector<std::string>{"reserved_test_sha256",manifest.test_sha256}||
    lines[5].size()!=2||lines[5][0]!="winner"||lines.back()!=std::vector<std::string>{"end"})return Error::mismatch;
 Selection result;result.winner=lines[5][1];result.sha256=pl::sha256_hex(bytes);
 result.report=std::string(bytes);result.manifest_sha256=manifest.sha256;
 result.training_sha256=manifest.training_sha256;result.test_sha256=manifest.test_sha256;
 std::set<std::string> seen;
 for(std::size_t i=6;i+1<lines.size();++i){const auto& t=lines[i];
   if(t.size()!=8||t[0]!="rank"||!seen.insert(t[1]).second)return Error::mismatch;
   Entry e;e.id=t[1];e.config_hash=t[5];e.law_hash=t[6];e.tip_hash=t[7];
   if(!sim::digest(e.config_hash)||!sim::digest(e.law_hash)||!sim::digest(e.tip_hash)||
      !sim::parse_u64(t[2],e.normalized_sum)||!sim::parse_u64(t[3],e.absolute_sum))return Error::mismatch;
   std::uint64_t count=0;if(!sim::parse_u64(t[4],count)||count==0||count>fid::kMaxRecords)return Error::mismatch;
   e.count=static_cast<std::size_t>(count);
   result.ranked.push_back(std::move(e));
 }
 if(result.ranked.front().id!=result.winner)return Error::mismatch;
 out=std::move(result);return Error::ok;
}
struct FinalReport {std::string json,sha256;std::uint64_t count=0;double normalized_mae=0,mae=0,band_coverage=0;};
inline Error final_test(const Manifest& manifest,const fs::path& root,const fs::path& runs,
                        std::string_view locked_winner,const fs::path& test_file,FinalReport& output) {
 Selection provided;
 if(auto err=read_winner(locked_winner,manifest,provided);err!=Error::ok)return err;
 // Recompute selection from TRAIN ONLY, checking every ranked score and history tip.
 // This proves the chosen winner was selected by the code and pinned training set.
 Selection reproducible;
 if(auto err=select(manifest,root,runs,reproducible);err!=Error::ok)return err;
 if(provided.report!=reproducible.report)return Error::mismatch;
 // Only here does the runner open the reserved, previously unseen test file.
 std::string test_bytes;
 if(!sim::bounded_read(test_file,fid::kMaxManifestBytes,test_bytes)||
    pl::sha256_hex(test_bytes)!=manifest.test_sha256)return Error::unauthorized_test;
 DataSet test;
 if(auto err=DataSet::parse(test_bytes,"TEST_ONLY",test);err!=Error::ok)return err;
 if(test.source_sha256!=manifest.source_sha256)return Error::bad_dataset;
 DataSet train;
 if(auto err=load_training(manifest,root,train);err!=Error::ok)return err;
 std::set<std::string> train_keys,train_metrics;
 for(const auto& row:train.rows){train_keys.insert(std::to_string(row.tick)+"/"+row.colony+"/"+row.metric);
   train_metrics.insert(row.colony+"/"+row.metric);}
 for(const auto& row:test.rows){
   if(row.tick>manifest.horizon||train_keys.contains(std::to_string(row.tick)+"/"+row.colony+"/"+row.metric)||
      !train_metrics.contains(row.colony+"/"+row.metric))return Error::bad_dataset;
 }
 auto it=std::find_if(manifest.candidates.begin(),manifest.candidates.end(),
                      [&](const Candidate& x){return x.name==provided.winner;});
 if(it==manifest.candidates.end())return Error::mismatch;
 sim::Configuration config;
 if(auto err=candidate_config(*it,root,manifest.source_sha256,config);err!=Error::ok)return err;
 std::map<std::uint32_t,sim::Snapshot> history;std::string tip;
 if(!load_history(config,runs/it->name,manifest.horizon,history,tip))return Error::missing_checkpoint;
 if(tip!=provided.ranked.front().tip_hash)return Error::mismatch;
 long double normalized=0,abs_error=0,within=0;
 std::ostringstream rows;
 for(std::size_t i=0;i<test.rows.size();++i){const auto& row=test.rows[i];
   const fid::Observation observation{row.tick,row.colony,row.metric,"test",row.observed,row.radius};
   std::int64_t value=0;
   if(!fid::predicted(history.at(row.tick),observation,value))return Error::bad_dataset;
   const auto residual=value-static_cast<std::int64_t>(row.observed);
   const auto magnitude=std::llabs(residual);
   normalized+=static_cast<long double>(magnitude)/row.radius;
   abs_error+=static_cast<long double>(magnitude);
   if(static_cast<std::uint64_t>(magnitude)<=row.radius)within+=1;
   if(i)rows<<',';
   rows<<"{\"tick\":"<<row.tick<<",\"colony\":\""<<row.colony<<"\",\"metric\":\""<<row.metric
       <<"\",\"observed\":"<<row.observed<<",\"predicted\":"<<value<<",\"radius\":"<<row.radius
       <<",\"residual\":"<<residual<<'}';
 }
 const auto n=static_cast<long double>(test.rows.size());
 std::ostringstream json;json<<std::fixed<<std::setprecision(6);
 json<<"{\"schema\":\"CEVFINAL/1\",\"classification\":\"SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION\","
     <<"\"independent_holdout_verified\":false,\"selection_locked\":true,\"model\":\""<<provided.winner
     <<"\",\"selection_sha256\":\""<<provided.sha256<<"\",\"manifest_sha256\":\""<<manifest.sha256
     <<"\",\"training_sha256\":\""<<train.sha256<<"\",\"test_sha256\":\""<<test.sha256
     <<"\",\"source_sha256\":\""<<manifest.source_sha256<<"\",\"config_sha256\":\""<<it->config_sha256
     <<"\",\"law_sha256\":\""<<it->law_sha256<<"\",\"test\":{\"count\":"<<test.rows.size()
     <<",\"mae\":"<<static_cast<double>(abs_error/n)
     <<",\"normalized_mae\":"<<static_cast<double>(normalized/n)
     <<",\"declared_band_coverage\":"<<static_cast<double>(within/n)<<"},\"rows\":["<<rows.str()<<"]}\n";
 FinalReport final;final.json=json.str();final.sha256=pl::sha256_hex(final.json);
 final.count=test.rows.size();final.mae=static_cast<double>(abs_error/n);
 final.normalized_mae=static_cast<double>(normalized/n);final.band_coverage=static_cast<double>(within/n);
 output=std::move(final);return Error::ok;
}
} // namespace christeye::living::selection
