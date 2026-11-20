#pragma once
// Pure CEVMESH/2 application-control and identifier codecs. TLS I/O, scheduling
// and authorization stay in the native transport and mesh service.
#include "anaiero/sdk/native_mesh.hpp"
namespace anaiero::sdk::mesh::protocol {
Bytes response(const Result&);
Result remote_error(const std::vector<std::string>&);
std::string hex(std::string_view);
bool unhex(std::string_view,Bytes&);
}
