#pragma once
// Included by session_batch_tests.cpp after its CHECK definition.
#include "anaiero/application/native_session_application.hpp"
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
inline void scientific_session_batch(const std::filesystem::path& repo,
    const std::filesystem::path& provider,const std::filesystem::path& root){
  namespace fs=std::filesystem;namespace sci=anaiero::science;namespace session=anaiero::session;
  namespace application=anaiero::application;namespace runtime=anaiero::runtime;
  fs::remove_all(root);fs::create_directories(root);
  const auto model=root/"passive.cevmodel",solver=root/"passive.cevsolver",config=root/"passive.cevsession",state=root/"state";
  const auto executable=root/("provider"+provider.extension().string());
  fs::copy_file(repo/"examples/scientific/passive-membrane.cevmodel",model,fs::copy_options::overwrite_existing);
  fs::copy_file(provider,executable,fs::copy_options::overwrite_existing);
  sci::PassiveMembraneModel parsed{};std::string model_sha;
  CHECK(sci::files::read_passive_model(model,parsed,model_sha));
  {std::ofstream file(solver,std::ios::binary);
    file<<"CEVSOLVER/1\nprovider native-passive-analytic\nmodel "<<std::quoted(model.string())<<' '<<model_sha
      <<"\nbehavior "<<christeye::living::plans::sha256_hex(sci::kPassiveAnalyticBehavior)
      <<"\nexecutable "<<std::quoted(executable.string())
      <<"\nstep_ns 25000\nmax_steps 1000000\ntimeout_ms 2000\ninput_proxy 1000\nunit 2\ncheckpoint_schema 1\narguments 1\nmodel_argument\nend\n";
  }
  {std::ofstream file(config,std::ios::binary);
    file<<"CEVSESSION/2\nname scientific-batch\nperiod_ms 100\nstorage segmented 16\nengine 301 scientific 81 9 1 "
      <<std::quoted(solver.string())<<" \"-\" 0 0 0 0\nend\n";
  }
  session::Command step{};step.operation=session::Operation::step;step.operation_id=1;step.count=40;
  std::array<char,65> first_state_hash{};runtime::MetricValue first_voltage{};runtime::SourceCursor first_cursor{};
  std::uint64_t first_revision=0;
  {
    auto app=std::make_unique<application::NativeSessionApplication>();
    const bool opened=app->open(config,state);if(!opened)std::cerr<<app->error()<<'\n';CHECK(opened);
    CHECK(app->settle()&&app->driver().execute(step,nullptr)&&app->settle());
    const auto view=app->driver().view();CHECK(view.cycles==40&&view.engine_count==1&&view.projected.count==1);
    CHECK(view.engines[0].kind==anaiero::engine::Kind::scientific);
    CHECK(view.engines[0].capabilities.tick_ns==25000&&view.engines[0].capabilities.clock_domain==81);
    CHECK(view.engines[0].capabilities.summaries[2]==anaiero::engine::SummarySemantic::membrane_voltage_microvolt);
    CHECK(!view.engines[0].capabilities.evaluate_candidates&&!view.engines[0].capabilities.apply_changes);
    const auto& voltage=view.projected.values[0];
    CHECK(voltage.metric==runtime::Metric::membrane_voltage&&voltage.source==81&&voltage.source_epoch==9);
    CHECK(voltage.acquired_ns==1000000&&voltage.sequence==41&&voltage.value<0);
    CHECK(view.projected.cursors[0].clock_domain==81&&view.projected.sequence_gaps==0);
    const auto exact=-65000.0+79577472.0*20.0/1000000.0*(1-std::exp(-1.0));
    CHECK(std::abs(static_cast<double>(voltage.value)-exact)<=1.0);
    CHECK(view.engines[0].published.state_sha256==view.engines[0].working.state_sha256);
    first_state_hash=view.engines[0].working.state_sha256;first_voltage=voltage;first_cursor=view.projected.cursors[0];first_revision=view.revision;
    CHECK(app->driver().operation_view().entries[0].state==anaiero::operations::State::completed);
  }
  {
    auto app=std::make_unique<application::NativeSessionApplication>();
    const bool opened=app->open(config,state);if(!opened)std::cerr<<app->error()<<'\n';CHECK(opened&&app->settle());
    const auto recovered=app->driver().view();
    CHECK(recovered.revision==first_revision&&recovered.engines[0].working.state_sha256==first_state_hash&&recovered.cycles==40);
    CHECK(recovered.projected.count==1&&recovered.projected.sequence_gaps==0);
    CHECK(recovered.projected.values[0].value==first_voltage.value&&recovered.projected.values[0].metric==first_voltage.metric&&
      recovered.projected.values[0].evidence_tag==first_voltage.evidence_tag&&recovered.projected.values[0].sequence==first_voltage.sequence&&
      recovered.projected.values[0].acquired_ns==first_voltage.acquired_ns);
    CHECK(recovered.projected.cursors[0].clock_domain==first_cursor.clock_domain&&
      recovered.projected.cursors[0].last_sequence==first_cursor.last_sequence&&recovered.projected.cursors[0].epoch==first_cursor.epoch);
    CHECK(app->driver().execute(step,nullptr)&&app->settle()&&app->driver().view().cycles==40);
    step.operation_id=2;step.count=20;CHECK(app->driver().execute(step,nullptr)&&app->settle());
    const auto view=app->driver().view();CHECK(view.cycles==60&&view.projected.values[0].sequence==61);
    CHECK(view.projected.values[0].acquired_ns==1500000&&view.projected.sequence_gaps==0);
    const auto exact=-65000.0+79577472.0*20.0/1000000.0*(1-std::exp(-1.5));
    CHECK(std::abs(static_cast<double>(view.projected.values[0].value)-exact)<=1.0);
  }
  // A changed nested executable changes session identity even though the
  // .cevsession/.cevsolver files and declared model digest are unchanged.
  session::Configuration previously_pinned;std::string pin_error;
  CHECK(session::files::read_configuration(config,previously_pinned,pin_error));
  {std::ofstream changed(executable,std::ios::binary|std::ios::app);changed<<'\0';}
  CHECK(!application::SessionEngineFactoryRegistry::create(previously_pinned.engines[0],root/"unused-engine",pin_error));
  CHECK(pin_error=="scientific configuration changed before engine construction");
  {
    auto app=std::make_unique<application::NativeSessionApplication>();CHECK(!app->open(config,state));
    CHECK(app->error().find("identity")!=std::string::npos);
  }
  fs::copy_file(provider,executable,fs::copy_options::overwrite_existing);
  {std::ofstream changed(model,std::ios::binary|std::ios::app);changed<<' ';}
  {
    auto app=std::make_unique<application::NativeSessionApplication>();CHECK(!app->open(config,state));
    CHECK(app->error().find("digest mismatch")!=std::string::npos);
  }
}
