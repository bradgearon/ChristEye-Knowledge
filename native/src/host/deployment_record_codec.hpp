#pragma once
// Exact native deployment binary plan and package codecs; no filesystem
// access, package admission, supervisor operations or selection side effects.
#include "deployment.hpp"
namespace anaiero::deployment::recordcodec {
bool valid(const Package&);
Bytes package_wire(const Package&);
Result package_read(std::string_view,Package&,bool empty=false);
Bytes body(const Plan&);
Result validate(const Plan&);
std::string operation(const Plan&);
Result decode_selection(std::string_view,std::string_view expected_store,Selection&);
}
