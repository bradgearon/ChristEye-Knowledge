#include "native_mesh_protocol.hpp"
#include <charconv>
namespace anaiero::sdk::mesh::protocol {
namespace {
Result fail(Code code,std::string why){return {code,std::move(why)};}
std::string number(std::uint64_t value){return std::to_string(value);}
bool integer(std::string_view text,std::uint64_t& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&number(value)==text;
}
}
Bytes response(const Result& result){return pack({"error",number(static_cast<std::uint32_t>(result.code)),result.message});}
Result remote_error(const std::vector<std::string>& fields){
  std::uint64_t code=0;if(fields.size()!=3||fields[0]!="error"||!integer(fields[1],code)||code>static_cast<std::uint32_t>(Code::deadline))
    return fail(Code::corrupt,"invalid canonical mesh control response");
  return {static_cast<Code>(code),fields[2]};
}
std::string hex(std::string_view value){static constexpr char alphabet[]="0123456789abcdef";std::string out;out.reserve(value.size()*2);for(unsigned char c:value){out.push_back(alphabet[c>>4]);out.push_back(alphabet[c&15]);}return out;}
bool unhex(std::string_view value,Bytes& out){
  out.clear();if(value.size()%2)return false;
  auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  for(std::size_t i=0;i<value.size();i+=2){const auto high=nibble(value[i]),low=nibble(value[i+1]);if(high<0||low<0)return false;out.push_back(static_cast<char>(high*16+low));}return true;
}
}
