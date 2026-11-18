#pragma once
#include "anaiero/infrastructure/scientific_calibration_files.hpp"
#include "anaiero/services/environment_calibration_service.hpp"
#include <filesystem>
#include <string>
namespace anaiero::application {
struct EnvironmentCalibrationResult {
  int exit_code=2;
  science::calibration::Report report;
  std::string error;
};
inline EnvironmentCalibrationResult calibrate_environment(const std::filesystem::path& plan,
    const std::filesystem::path& report_path,science::calibration::IObserver* observer=nullptr,
    const science::calibration::ICancellation* cancellation=nullptr){
  namespace c=science::calibration;
  EnvironmentCalibrationResult result;c::Plan request;c::Reference training,holdout;
  if(!c::files::read_bundle(plan,request,training,holdout,result.error))return result;
  c::NativeEnvironmentTrial trial;c::AbsoluteFidelityPolicy policy;c::EnvironmentCalibrationService service(trial,policy);
  service.set_observer(observer);
  const auto code=service.run(request,training,holdout,result.report,cancellation);
  if(code!=c::Code::ok){result.error="calibration rejected with code "+std::to_string(static_cast<unsigned>(code));return result;}
  const auto bytes=c::files::serialize_report(result.report);
  if(!c::files::write_new_report(report_path,bytes,result.error))return result;
  result.exit_code=result.report.passed?0:3;return result;
}
} // namespace anaiero::application
