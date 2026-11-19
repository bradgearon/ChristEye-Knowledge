#include "qualification.hpp"
#include "mixed_qualification.hpp"
#include "scale_qualification.hpp"
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
sdk::Result read_text(const fs::path& path,std::string& bytes){
  std::error_code error;
  if(!fs::is_regular_file(fs::symlink_status(path,error))||error)
    return {sdk::Code::invalid,"qualification input is not a regular file: "+utf8(path)};
  const auto size=fs::file_size(path,error);
  if(error||size>16384)return {sdk::Code::invalid,"qualification input exceeds 16 KiB: "+utf8(path)};
  std::ifstream input(path,std::ios::binary);
  if(!input)return {sdk::Code::io_error,"cannot open qualification input"};
  bytes.assign(static_cast<std::size_t>(size),'\0');
  if((size!=0&&!input.read(bytes.data(),static_cast<std::streamsize>(size)))||
     input.peek()!=std::char_traits<char>::eof()||input.bad())
    return {sdk::Code::io_error,"qualification input changed or was truncated during its bounded read"};
  return {};
}
sdk::Result mixed(int argc,char** argv){
  if(argc!=5)return {sdk::Code::invalid,"expected mixed PROFILE SESSION NEW_OUTPUT_DIRECTORY"};
  const auto profile_path=q::native_path(argv[2]),session_path=q::native_path(argv[3]),output=q::native_path(argv[4]);
  std::string profile_bytes,session_bytes;
  if(auto r=read_text(profile_path,profile_bytes);!r)return r;
  if(auto r=read_text(session_path,session_bytes);!r)return r;
  q::MixedProfile profile;if(auto r=q::parse_mixed_profile(profile_bytes,profile);!r)return r;
  sdk::EngineModuleCatalog catalog;if(auto r=anaiero::engines::register_builtin_engines(catalog);!r)return r;
  if(auto r=catalog.freeze();!r)return r;
  sdk::SessionDefinition definition;
  if(auto r=anaiero::engines::read_session_definition(utf8(session_path),catalog,definition);!r)return r;
  if(auto r=q::validate_mixed_definition(definition,profile);!r)return r;
  std::string verified_text;if(auto r=read_text(session_path,verified_text);!r)return r;
  if(verified_text!=session_bytes)return {sdk::Code::conflict,"session text changed during qualification admission"};
  const auto definition_bytes=sdk::encode_session_definition(definition);
  std::error_code error;
  if(!fs::create_directory(output,error)||error)return {sdk::Code::conflict,"mixed qualification output already exists or cannot be created"};
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"profile.cevqualification"),profile_bytes);!r)return r;
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"session-input.cevsession"),session_bytes);!r)return r;
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"session.definition"),definition_bytes);!r)return r;
  q::MixedReport report;report.profile_sha256=sdk::sha256(profile_bytes);report.session_text_sha256=sdk::sha256(session_bytes);
  report.session_definition_sha256=sdk::sha256(definition_bytes);
  const auto result=q::run_mixed_profile(profile,definition,utf8(output),report,[](std::uint64_t cycles){
    std::cerr<<"mixed qualification completed_cycles="<<cycles<<'\n'<<std::flush;
  });
  const auto json=q::mixed_report_json(report);
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"report.json"),json);!r)return r;
  std::cout<<json;return result;
}
sdk::Result campaign(int argc,char** argv){
  const std::string_view command=argv[1];
  if(command=="campaign-status"){
    if(argc!=3)return {sdk::Code::invalid,"expected campaign-status DIRECTORY"};
    std::string json;if(auto r=q::mixed_campaign_status(argv[2],json);!r)return r;
    std::cout<<json;return {};
  }
  if(command=="campaign-resume"){
    if(argc!=3)return {sdk::Code::invalid,"expected campaign-resume DIRECTORY"};
    q::MixedReport report;const auto result=q::run_mixed_campaign(argv[2],report,{},[](std::uint64_t cycles){
      std::cerr<<"campaign completed_cycles="<<cycles<<'\n'<<std::flush;
    });std::cout<<q::mixed_report_json(report);return result;
  }
  if(argc!=5&&argc!=6)return {sdk::Code::invalid,"expected campaign-create PROFILE SESSION NEW_DIRECTORY [DEVICE_DECLARATION]"};
  std::string profile_bytes,session_bytes;
  if(auto r=read_text(q::native_path(argv[2]),profile_bytes);!r)return r;
  if(auto r=read_text(q::native_path(argv[3]),session_bytes);!r)return r;
  q::MixedProfile profile;if(auto r=q::parse_mixed_profile(profile_bytes,profile);!r)return r;
  sdk::EngineModuleCatalog catalog;if(auto r=anaiero::engines::register_builtin_engines(catalog);!r)return r;
  if(auto r=catalog.freeze();!r)return r;
  sdk::SessionDefinition definition;if(auto r=anaiero::engines::read_session_definition(argv[3],catalog,definition);!r)return r;
  if(auto r=q::validate_mixed_definition(definition,profile);!r)return r;
  std::string reread;if(auto r=read_text(q::native_path(argv[3]),reread);!r)return r;
  if(reread!=session_bytes)return {sdk::Code::conflict,"session text changed during campaign admission"};
  std::string device_bytes;if(argc==6){if(auto r=read_text(q::native_path(argv[5]),device_bytes);!r)return r;}
  if(auto r=q::create_mixed_campaign(profile_bytes,session_bytes,definition,argv[4],device_bytes);!r)return r;
  std::string json;if(auto r=q::mixed_campaign_status(argv[4],json);!r)return r;
  std::cout<<json;return {};
}
using IndexLayout=sdk::evidence::IndexLayout;

sdk::Result parse_layout_arguments(int& argc,char** argv,IndexLayout& layout){
  if(argc<4)return {sdk::Code::invalid,"expected PROFILE NEW_OUTPUT_DIRECTORY"};
  if(argc==4||argc==5)return {};
  if(argc!=7||std::string_view(argv[5])!="--index-layout")
    return {sdk::Code::invalid,"expected DEVICE_DECLARATION --index-layout node-files|packed-pages"};
  const std::string_view value=argv[6];
  if(value=="node-files")layout=IndexLayout::single_node;
  else if(value=="packed-pages")layout=IndexLayout::packed_pages;
  else return {sdk::Code::invalid,"unknown derived index layout"};
  argc=5;
  return {};
}
sdk::Result scale(int argc,char** argv){
  IndexLayout layout=IndexLayout::automatic;
  if(auto r=parse_layout_arguments(argc,argv,layout);!r)return r;
  std::string bytes,device_bytes;if(auto r=read_text(q::native_path(argv[2]),bytes);!r)return r;
  q::ScaleProfile profile;if(auto r=q::parse_scale_profile(bytes,profile);!r)return r;
  q::DeviceDeclaration device;
  if(argc==5){if(auto r=read_text(q::native_path(argv[4]),device_bytes);!r)return r;
    if(auto r=q::parse_device_declaration(device_bytes,device);!r)return r;}
  const auto output=q::native_path(argv[3]);std::error_code error;
  if(!fs::create_directory(output,error)||error)return {sdk::Code::conflict,"scale output exists or cannot be created"};
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"profile.cevqualification"),bytes);!r)return r;
  q::ScaleReport report;report.profile_sha256=sdk::sha256(bytes);
  if(!device_bytes.empty()){
    if(auto r=anaiero::engines::write_new_artifact(utf8(output/"device.cevdevice"),device_bytes);!r)return r;
    report.device=std::move(device);report.device_sha256=sdk::sha256(device_bytes);
  }
  const auto result=q::run_scale_profile(profile,utf8(output),report,[next=std::uint64_t{1000}](std::uint64_t observations)mutable{
    if(observations>=next){std::cerr<<"scale committed_observations="<<observations<<'\n'<<std::flush;next=(observations/1000+1)*1000;}
  },layout);const auto json=q::scale_report_json(report);
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"report.json"),json);!r)return r;
  std::cout<<json;return result;
}
sdk::Result run(int argc,char** argv){
  if(argc==2&&(std::string_view(argv[1])=="help"||std::string_view(argv[1])=="--help")){
    std::cout<<"Usage: ce_native_qualification run PROFILE NEW_OUTPUT_DIRECTORY [DEVICE_DECLARATION [--index-layout node-files|packed-pages]]\n"
      "       ce_native_qualification mixed PROFILE SESSION NEW_OUTPUT_DIRECTORY\n"
      "       ce_native_qualification scale PROFILE NEW_OUTPUT_DIRECTORY [DEVICE_DECLARATION [--index-layout node-files|packed-pages]]\n"
      "       ce_native_qualification campaign-create PROFILE SESSION NEW_DIRECTORY [DEVICE_DECLARATION]\n"
      "       ce_native_qualification campaign-resume DIRECTORY\n"
      "       ce_native_qualification campaign-status DIRECTORY\n"
      "run measures synthetic evidence/lifecycle; mixed executes five engine families\n"
      "through the continuous host. Both retain exact inputs, store and report.json.\n"
      "Existing output\n"
      "directories are never reused or deleted. Budget failure returns nonzero.\n";return {};
  }
  if(argc>=2&&std::string_view(argv[1])=="mixed")return mixed(argc,argv);
  if(argc>=2&&std::string_view(argv[1])=="scale")return scale(argc,argv);
  if(argc>=2&&(std::string_view(argv[1])=="campaign-create"||std::string_view(argv[1])=="campaign-resume"||std::string_view(argv[1])=="campaign-status"))return campaign(argc,argv);
  if(argc<2||std::string_view(argv[1])!="run")return {sdk::Code::invalid,"expected run PROFILE NEW_OUTPUT_DIRECTORY"};
  IndexLayout layout=IndexLayout::automatic;
  if(auto r=parse_layout_arguments(argc,argv,layout);!r)return r;
  const fs::path profile_path=q::native_path(argv[2]),output=q::native_path(argv[3]);
  std::error_code error;const auto status=fs::symlink_status(profile_path,error);
  if(error)return {sdk::Code::invalid,"cannot inspect qualification profile "+utf8(profile_path)+": "+error.message()};
  if(!fs::is_regular_file(status)||fs::file_size(profile_path)>16384)
    return {sdk::Code::invalid,"qualification profile must be a regular text file within 16 KiB: "+utf8(profile_path)};
  std::string bytes;if(auto r=read_text(profile_path,bytes);!r)return r;
  q::Profile profile;if(auto r=q::parse_profile(bytes,profile);!r)return r;
  std::string device_bytes;q::DeviceDeclaration device;
  if(argc==5){
    if(auto r=read_text(q::native_path(argv[4]),device_bytes);!r)return r;
    if(auto r=q::parse_device_declaration(device_bytes,device);!r)return r;
  }else if(profile.latency_gate==q::threshold_method)return {sdk::Code::invalid,"version 2 qualification requires a device declaration"};
  if(!fs::create_directory(output,error)||error)return {sdk::Code::conflict,"qualification output directory already exists or cannot be created"};
  if(auto r=anaiero::engines::write_new_artifact(utf8(output/"profile.cevqualification"),bytes);!r)return r;
  q::Report report;report.profile_sha256=sdk::sha256(bytes);
  if(!device_bytes.empty()){
    if(auto r=anaiero::engines::write_new_artifact(utf8(output/"device.cevdevice"),device_bytes);!r)return r;
    report.device=std::move(device);report.device_sha256=sdk::sha256(device_bytes);
  }
  const auto result=q::run_profile(profile,utf8(output),report,[next=std::uint64_t{1000}](std::uint64_t observations)mutable{
    if(observations>=next){
      std::cerr<<"qualification committed_observations="<<observations<<'\n'<<std::flush;
      next=(observations/1000+1)*1000;
    }
  },layout);const auto json=q::report_json(report);
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
