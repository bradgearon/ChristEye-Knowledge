#include "qualification.hpp"
#include "native_paths.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
namespace q=anaiero::qualification;
namespace sdk=anaiero::sdk;
namespace {
std::string utf8(const fs::path& path){const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
sdk::Result run(int argc,char** argv){
  if(argc==2&&(std::string_view(argv[1])=="help"||std::string_view(argv[1])=="--help")){
    std::cout<<"Usage: ce_native_qualification run PROFILE NEW_OUTPUT_DIRECTORY\n"
      "Runs the exact predeclared synthetic evidence/lifecycle profile and retains\n"
      "profile bytes, a canonical store and measured report.json. Existing output\n"
      "directories are never reused or deleted. Budget failure returns nonzero.\n";return {};
  }
  if(argc!=4||std::string_view(argv[1])!="run")return {sdk::Code::invalid,"expected run PROFILE NEW_OUTPUT_DIRECTORY"};
  const fs::path profile_path=q::native_path(argv[2]),output=q::native_path(argv[3]);
  std::error_code error;const auto status=fs::symlink_status(profile_path,error);
  if(error)return {sdk::Code::invalid,"cannot inspect qualification profile "+utf8(profile_path)+": "+error.message()};
  if(!fs::is_regular_file(status)||fs::file_size(profile_path)>16384)
    return {sdk::Code::invalid,"qualification profile must be a regular text file within 16 KiB: "+utf8(profile_path)};
  std::ifstream input(profile_path,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(input),{}};
  if(input.bad())return {sdk::Code::io_error,"cannot read qualification profile"};
  q::Profile profile;if(auto r=q::parse_profile(bytes,profile);!r)return r;
  if(!fs::create_directory(output,error)||error)return {sdk::Code::conflict,"qualification output directory already exists or cannot be created"};
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"profile.cevqualification"),bytes);!r)return r;
  q::Report report;report.profile_sha256=sdk::sha256(bytes);
  const auto result=q::run_profile(profile,utf8(output),report,[next=std::uint64_t{1000}](std::uint64_t observations)mutable{
    if(observations>=next){
      std::cerr<<"qualification committed_observations="<<observations<<'\n'<<std::flush;
      next=(observations/1000+1)*1000;
    }
  });const auto json=q::report_json(report);
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"report.json"),json);!r)return r;
  std::cout<<json;return result;
}
}
int main(int argc,char** argv){
  try{
    const auto result=run(argc,argv);
    if(!result){std::cerr<<sdk::code_name(result.code)<<": "<<result.message<<'\n';return 2;}
    return 0;
  }catch(const std::exception& error){std::cerr<<"io_error: "<<error.what()<<'\n';return 2;}
}
