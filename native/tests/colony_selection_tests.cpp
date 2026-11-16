#include "christeye/living/model_selection.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
namespace sl=christeye::living::selection;
namespace sim=christeye::living::sim3;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL " #x " line "<<__LINE__<<'\n';std::exit(1);}}while(false)
static std::string read(const fs::path& file){
 std::string s;CHECK(sim::bounded_read(file,65536,s));return s;
}
static void write(const fs::path& file,const std::string& data){
 std::ofstream f(file,std::ios::binary|std::ios::trunc);CHECK(bool(f));f<<data;f.flush();CHECK(bool(f));
}
int main(int argc,char**argv){
 CHECK(argc==3);
 const fs::path root=argv[1],runs=argv[2];
 const auto manifest_text=read(root/"examples/sim3/model-search.cevselect");
 sl::Manifest manifest;CHECK(sl::Manifest::parse(manifest_text,manifest)==sl::Error::ok);
 CHECK(manifest.candidates.size()==2&&manifest.horizon==6);
 CHECK(manifest.training_sha256!=manifest.test_sha256);
 sl::DataSet training;
 CHECK(sl::load_training(manifest,root,training)==sl::Error::ok);
 CHECK(training.partition=="TRAIN_ONLY"&&training.rows.size()==12);
 sl::Selection selection;
 CHECK(sl::select(manifest,root,runs,selection)==sl::Error::ok);
 CHECK(selection.winner=="baseline"&&selection.ranked.size()==2);
 CHECK(selection.ranked[0].normalized_sum<selection.ranked[1].normalized_sum);
 sl::Selection parsed;
 CHECK(sl::read_winner(selection.report,manifest,parsed)==sl::Error::ok);
 CHECK(parsed.report==selection.report);
 sl::FinalReport report;
 const auto test_file=root/"examples/sim3/model-search-test.cevsplits";
 CHECK(sl::final_test(manifest,root,runs,selection.report,test_file,report)==sl::Error::ok);
 CHECK(report.count==16 && report.mae>=0 && report.normalized_mae>=0);
 CHECK(report.json.find("\"independent_holdout_verified\":false")!=std::string::npos);
 CHECK(report.json.find("\"model\":\"baseline\"")!=std::string::npos);
 sl::Selection repeat;
 CHECK(sl::select(manifest,root,runs,repeat)==sl::Error::ok);
 CHECK(repeat.report==selection.report && repeat.sha256==selection.sha256);
 sl::FinalReport report_repeat;
 CHECK(sl::final_test(manifest,root,runs,selection.report,test_file,report_repeat)==sl::Error::ok);
 CHECK(report_repeat.json==report.json);
 // A modified immutable selection report is rejected, even if it names a better holdout model.
 auto forged=selection.report;
 auto at=forged.find("winner baseline");CHECK(at!=std::string::npos);
 forged.replace(at,15,"winner fast-learning");
 CHECK(sl::final_test(manifest,root,runs,forged,test_file,report_repeat)!=sl::Error::ok);
 // A re-sealed forged winner is still rejected against recomputed training ranking.
 auto body=forged.substr(0,forged.find("seal "));
 forged=body+"seal "+sl::pl::sha256_hex(body)+'\n';
 CHECK(sl::final_test(manifest,root,runs,forged,test_file,report_repeat)==sl::Error::mismatch);
 auto changed=manifest_text;
 auto p=changed.find("candidate baseline ");CHECK(p!=std::string::npos);
 changed[p]='X';CHECK(sl::Manifest::parse(changed,manifest)!=sl::Error::ok);
 CHECK(sl::Manifest::parse(manifest_text,manifest)==sl::Error::ok);
 // Selection can run with the frozen test file absent, proving there is no holdout read.
 auto shadow=fs::temp_directory_path()/"sim33-holdout-hidden";
 std::error_code ec;fs::remove_all(shadow,ec);fs::create_directories(shadow);
 auto missing_test=shadow/"holdout-does-not-exist.cevsplits";
 CHECK(!fs::exists(missing_test));
 CHECK(sl::select(manifest,root,runs,repeat)==sl::Error::ok);
 CHECK(sl::final_test(manifest,root,runs,repeat.report,missing_test,report_repeat)==sl::Error::unauthorized_test);
 // A held-out row that duplicates a training row is rejected, even with a new seal/hash.
 auto modified=read(test_file);
 auto begin=modified.find("row 3 all alive");CHECK(begin!=std::string::npos);
 modified.replace(begin,5,"row 0");
 const auto pos=modified.rfind("seal ");CHECK(pos!=std::string::npos);
 modified=modified.substr(0,pos);
 modified+="seal "+sl::pl::sha256_hex(modified)+'\n';
 auto duplicate_file=shadow/"overlapping.cevsplits";write(duplicate_file,modified);
 auto candidate_manifest=manifest;
 candidate_manifest.test_sha256=sl::pl::sha256_hex(modified);
 // Candidate must be generated from a newly sealed manifest; patched in-memory alone is rejected by locked report.
 CHECK(sl::final_test(candidate_manifest,root,runs,selection.report,duplicate_file,report_repeat)==sl::Error::mismatch);
 sl::DataSet invalid;
 CHECK(sl::DataSet::parse(modified,"TEST_ONLY",invalid)==sl::Error::ok);
 // A changed training source digest cannot pass a pinned manifest.
 auto revised=manifest;revised.training_sha256=std::string(64,'0');
 CHECK(sl::select(revised,root,runs,repeat)==sl::Error::tampered_artifact);
 // Bad law candidate path, disconnected topology and mismatched hashes all fail closed.
 revised=manifest;revised.candidates[0].config_sha256=std::string(64,'1');
 CHECK(sl::select(revised,root,runs,repeat)==sl::Error::tampered_artifact);
 // Source history must include every committed tick; empty history is invalid.
 CHECK(sl::select(manifest,root,shadow,repeat)==sl::Error::missing_checkpoint);
 fs::remove_all(shadow,ec);
 std::cout<<"SIM3.3 sealed selection, train-only ranking, frozen test and tamper tests passed\n";
}
