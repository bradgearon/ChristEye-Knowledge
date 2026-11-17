#pragma once
// Bounded half-duplex text framing for native stdio or an OS-specific pipe.
// The transport is not a secure authenticated network endpoint.
#include "anaiero/contracts/live_solver.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
namespace anaiero::science {
inline bool valid_sha256(std::string_view digest) noexcept {
  if(digest.size()!=64)return false;
  for(char c:digest)
    if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline bool parse_number(std::string_view value,std::uint64_t& result) noexcept {
  if(value.empty())return false;
  const auto [end,code]=std::from_chars(value.data(),value.data()+value.size(),result);
  return code==std::errc{}&&end==value.data()+value.size();
}
class StdioLiveSolverExchange final:public ILiveSolverExchange {
  std::istream& in_;
  std::ostream& out_;
  bool failed_=false;
  static char kind(Channel channel) noexcept {
    return channel==Channel::electrical_activity?'E':
           channel==Channel::environment_proxy?'M':'?';
  }
  bool read_bounded(std::string& line) noexcept {
    line.clear();
    for(unsigned i=0;i<255;++i){
      const int c=in_.get();
      if(c==std::char_traits<char>::eof())return false;
      if(c=='\n')return true;
      if(c=='\r'||c<32||c>126)return false;
      line.push_back(static_cast<char>(c));
    }
    return false;
  }
public:
  StdioLiveSolverExchange(std::istream& in,std::ostream& out) noexcept:
    in_(in),out_(out){}
  bool advance(const LiveRequest& req,LiveResponse& result) noexcept override {
    if(failed_)return false;
    if(req.epoch==0||req.sequence==0||req.sequence>4096||
       (kind(req.channel)=='?')||req.step_ns==0||req.step_ns>1000000000ULL||
       req.sequence>UINT64_MAX/req.step_ns||
       req.input_proxy<0||req.input_proxy>kProxyMax||
       !valid_sha256(req.model_sha256)){
      failed_=true;return false;
    }
    out_<<"STEP "<<kind(req.channel)<<' '<<req.epoch<<' '<<req.sequence<<' '
        <<req.step_ns<<' '<<req.input_proxy<<' '<<req.model_sha256<<'\n';
    out_.flush();
    if(!out_){failed_=true;return false;}
    std::string line;
    if(!read_bounded(line)){failed_=true;return false;}
    std::istringstream tokens(line);
    std::string word[9],extra;
    for(std::string& field:word)
      if(!(tokens>>field)){failed_=true;return false;}
    if(tokens>>extra){failed_=true;return false;}
    // SAMPLE <E/M> <epoch> <seq> <elapsed_ns> <feature> <uncertainty_ns> <sha> OK
    if(word[0]!="SAMPLE"||word[1]!=std::string(1,kind(req.channel))||
       word[7]!=req.model_sha256||word[8]!="OK"){
      failed_=true;return false;
    }
    std::uint64_t epoch=0,sequence=0,elapsed=0,value=0,uncertainty=0;
    if(!parse_number(word[2],epoch)||!parse_number(word[3],sequence)||
       !parse_number(word[4],elapsed)||!parse_number(word[5],value)||
       !parse_number(word[6],uncertainty)||epoch!=req.epoch||
       sequence!=req.sequence||elapsed!=req.sequence*req.step_ns||
       value>kProxyMax||uncertainty>req.step_ns){
      failed_=true;return false;
    }
    result={req.channel,req.epoch,req.sequence,elapsed,uncertainty,
            static_cast<std::int32_t>(value)};
    return true;
  }
  bool failed()const noexcept{return failed_;}
};
} // namespace anaiero::science
