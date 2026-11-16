#include "christeye/living/lab_vlm_import.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
namespace lab=christeye::living::lab;
namespace sim=christeye::living::sim3;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " line "<<__LINE__<<'\n';std::exit(1);}}while(false)
int main(int argc,char**argv){
  CHECK(argc==2);
  auto source=std::filesystem::path(argv[1])/"examples/connected-gardens.cev";
  sim::Configuration cfg;CHECK(sim::Configuration::load(source,cfg)==sim::Result::ok);
  CHECK(cfg.colonies.size()==2);
  auto imported=lab::read_overlay(cfg,source,cfg.colonies[0]);
  CHECK(imported.status==lab::Code::ok);
  CHECK(imported.source_sha256==cfg.colonies[0].source_sha);
  CHECK(imported.projection_json.find("\"format\":\"CEVLMOVERLAY/1\"")!=std::string::npos);
  CHECK(imported.projection_json.find("\"fields\":")!=std::string::npos);
  CHECK(imported.projection_json.find("\"molecules\":")!=std::string::npos);
  CHECK(imported.projection_json.find("\"active_processes\":")!=std::string::npos);
  auto bad=cfg;bad.colonies[0].source_sha=std::string(64,'0');
  CHECK(lab::read_overlay(bad,source,bad.colonies[0]).status==lab::Code::invalid_config);
  std::cout<<"SIM3.1 SHA-pinned CEVLM2 overlay tests passed\n";
}
