#include "qualification.hpp"
#include <charconv>
#include <iomanip>
#include <map>
#include <sstream>

namespace anaiero::qualification {
namespace {
std::string quote(std::string_view text){
  std::ostringstream out;out<<'"';
  for(const unsigned char c:text){
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';return out.str();
}
}
sdk::Result parse_device_declaration(std::string_view bytes,DeviceDeclaration& output){
  if(bytes.empty()||bytes.size()>16384||bytes.find('\0')!=std::string_view::npos)
    return {sdk::Code::invalid,"device declaration exceeds bounded text format"};
  std::istringstream input{std::string(bytes)};std::string line;
  std::getline(input,line);if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIERODEVICE/1")return {sdk::Code::invalid,"unsupported device declaration version"};
  std::map<std::string,std::string> fields;
  while(std::getline(input,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line.front()=='#')continue;
    const auto split=line.find('=');
    if(split==std::string::npos||split==0||split+1==line.size()||line.size()>640||
       !fields.emplace(line.substr(0,split),line.substr(split+1)).second)
      return {sdk::Code::invalid,"malformed or duplicate device declaration field"};
  }
  DeviceDeclaration result;
  const std::map<std::string,std::string*> strings={{"name",&result.name},{"device_class",&result.device_class},
    {"machine",&result.machine},{"processor",&result.processor},{"storage_device",&result.storage_device},
    {"storage_firmware",&result.storage_firmware},{"storage_controller",&result.storage_controller},
    {"filesystem",&result.filesystem},{"mount_options",&result.mount_options},{"durability",&result.durability},
    {"dependencies",&result.dependencies},{"concurrent_work",&result.concurrent_work}};
  if(fields.size()!=strings.size()+1)return {sdk::Code::invalid,"device declaration requires every field and no unknown fields"};
  for(const auto& [name,target]:strings){
    const auto field=fields.find(name);
    if(field==fields.end()||field->second.empty()||field->second.size()>512)
      return {sdk::Code::invalid,"missing or oversized device declaration field: "+name};
    for(const unsigned char c:field->second)if(c<32||c==127)
      return {sdk::Code::invalid,"device declaration contains a control character"};
    *target=field->second;
  }
  const auto memory=fields.find("memory_bytes");
  if(memory==fields.end())return {sdk::Code::invalid,"missing memory_bytes device declaration"};
  const auto& text=memory->second;
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result.memory_bytes);
  if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||std::to_string(result.memory_bytes)!=text||
     result.memory_bytes<1048576||result.memory_bytes>(1ULL<<60)||!sdk::valid_token(result.name,96)||
     (result.device_class!="shared"&&result.device_class!="dedicated"))
    return {sdk::Code::invalid,"invalid device class, name or canonical memory size"};
  output=std::move(result);return {};
}
std::string device_declaration_json(const DeviceDeclaration& d){
  if(d.name.empty())return "null";
  std::ostringstream out;
  out<<"{\"schema\":\"anaiero.device-declaration/1\",\"scope\":\"operator declaration; hardware exclusivity and firmware are not independently attested\""
     <<",\"name\":"<<quote(d.name)<<",\"device_class\":"<<quote(d.device_class)<<",\"machine\":"<<quote(d.machine)
     <<",\"processor\":"<<quote(d.processor)<<",\"memory_bytes\":"<<d.memory_bytes<<",\"storage_device\":"<<quote(d.storage_device)
     <<",\"storage_firmware\":"<<quote(d.storage_firmware)<<",\"storage_controller\":"<<quote(d.storage_controller)
     <<",\"filesystem\":"<<quote(d.filesystem)<<",\"mount_options\":"<<quote(d.mount_options)
     <<",\"durability\":"<<quote(d.durability)<<",\"dependencies\":"<<quote(d.dependencies)
     <<",\"concurrent_work\":"<<quote(d.concurrent_work)<<'}';
  return out.str();
}
std::string disk_usage_json(const DiskUsage& d){
  std::ostringstream out;out<<"{\"files\":"<<d.files<<",\"logical_bytes\":"<<d.logical_bytes<<",\"allocated_bytes\":"<<d.allocated_bytes
    <<",\"allocated_bytes_available\":"<<(d.allocated_bytes_available?"true":"false")<<",\"directories\":"<<d.directories
    <<",\"directory_allocated_bytes\":"<<d.directory_allocated_bytes<<",\"directory_allocation_available\":"<<(d.directory_allocation_available?"true":"false")
    <<",\"allocation_unit_bytes\":"<<d.allocation_unit_bytes<<",\"filesystem_total_bytes\":"<<d.filesystem_total_bytes
    <<",\"filesystem_available_bytes\":"<<d.filesystem_available_bytes<<",\"filesystem_file_nodes_available\":"<<d.filesystem_file_nodes_available
    <<",\"file_nodes_available\":"<<(d.file_nodes_available?"true":"false")
    <<",\"scope\":\"regular files plus separately reported directory allocation; excludes filesystem journal, inode-table and external metadata; filesystem free space is volume-wide\"}";
  return out.str();
}
}
