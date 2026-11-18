// Offline bounded model/reference comparison. This does not launch a solver,
// configure an instrument, apply a candidate, or promote scientific authority.
#include "anaiero/application/environment_calibration_application.hpp"
#include <iostream>
int main(int argc,char** argv){
  if(argc!=3){std::cerr<<"usage: ce_scientific_calibrate <plan.cevcalibration> <new-report.json>\n";return 2;}
  const auto result=anaiero::application::calibrate_environment(argv[1],argv[2]);
  if(!result.error.empty()){std::cerr<<result.error<<'\n';return result.exit_code;}
  const auto& report=result.report;
  std::cout<<(report.passed?"PASS":"FAIL")<<" selected "<<report.candidates[report.selected_index].id
    <<" holdout_rmse_micromolar "<<anaiero::science::calibration::root_mean_squared_error(report.holdout)
    <<" holdout_max_absolute_error_micromolar "<<report.holdout.max_absolute_error_micromolar
    <<" report "<<argv[2]<<'\n';
  return result.exit_code;
}
