#pragma once
// Exact rational conversion: only compatible dimensions and full precision.
#include "anaiero/models/scientific_quantity.hpp"
#include <cstdint>
#include <limits>
namespace anaiero::science {
class UnitConversionService final {
  static bool valid_digest(const std::array<char,65>& sha,bool empty=false)noexcept {
    if(sha.back()!='\0')return false;
    bool all_empty=true;
    for(std::size_t i=0;i<64;++i)all_empty=all_empty&&sha[i]=='\0';
    if(all_empty)return empty;
    for(std::size_t i=0;i<64;++i)
      if(!((sha[i]>='0'&&sha[i]<='9')||(sha[i]>='a'&&sha[i]<='f')))return false;
    return true;
  }
  static int dimension(Unit u)noexcept {
    switch(u){
      case Unit::dimensionless_proxy:return 1;
      case Unit::microvolt:case Unit::millivolt:return 2;
      case Unit::nanosecond:case Unit::microsecond:case Unit::millisecond:return 3;
      case Unit::micromolar:case Unit::millimolar:return 4;
    }
    return 0;
  }
  static std::int64_t scale(Unit u)noexcept {
    switch(u){
      case Unit::dimensionless_proxy:case Unit::microvolt:
      case Unit::nanosecond:case Unit::micromolar:return 1;
      case Unit::millivolt:case Unit::microsecond:
      case Unit::millimolar:return 1000;
      case Unit::millisecond:return 1000000;
    }
    return 0;
  }
  static bool safe_mul(std::int64_t x,std::int64_t factor,std::int64_t& out)noexcept {
    if(x>std::numeric_limits<std::int64_t>::max()/factor||
       x<std::numeric_limits<std::int64_t>::min()/factor)return false;
    out=x*factor;return true;
  }
public:
  bool convert(const Quantity& original,Unit destination,Quantity& output)const noexcept {
    if(original.uncertainty<0||original.source==0||original.epoch==0||
       !valid_digest(original.model_sha256,true)||
       !dimension(original.unit)||dimension(original.unit)!=dimension(destination))
      return false;
    std::int64_t x=0,u=0;
    if(!safe_mul(original.value,scale(original.unit),x)||
       !safe_mul(original.uncertainty,scale(original.unit),u))return false;
    const auto target=scale(destination);
    if(target==0||x%target||u%target)return false;
    output=original;output.value=x/target;output.uncertainty=u/target;
    output.unit=destination;return true;
  }
  bool conservation(const ConservationBudget& budget,
                    const ConservationContext* context=nullptr)const noexcept {
    if(budget.absolute_tolerance<0||budget.before.source==0)return false;
    const auto target=budget.before.unit;
    Quantity a{},b{},c{},d{};
    if(!convert(budget.before,target,a)||!convert(budget.inflow,target,b)||
       !convert(budget.outflow,target,c)||!convert(budget.after,target,d))
       return false;
    if(context){
      if(!valid_digest(context->transform_sha256))return false;
      const std::array<const Quantity*,4> terms{{&a,&b,&c,&d}};
      for(std::size_t i=0;i<terms.size();++i){
        const auto& expected=context->terms[i];const auto& term=*terms[i];
        if(!valid_digest(expected.model_sha256)||term.source!=expected.source||
           term.epoch!=expected.epoch||term.model_sha256!=expected.model_sha256)return false;
      }
    }else if(a.source!=b.source||a.source!=c.source||a.source!=d.source||
       a.epoch!=b.epoch||a.epoch!=c.epoch||a.epoch!=d.epoch||
       a.model_sha256!=b.model_sha256||a.model_sha256!=c.model_sha256||
       a.model_sha256!=d.model_sha256)return false;
    // Conservative numeric guard; no double, no silent overflow.
    constexpr auto bound=std::numeric_limits<std::int64_t>::max()/8;
    if(a.value>bound||a.value<-bound||b.value>bound||b.value<-bound||
       c.value>bound||c.value<-bound||d.value>bound||d.value<-bound||
       budget.absolute_tolerance>bound)return false;
    const auto residual=a.value+b.value-c.value-d.value;
    return residual>=-budget.absolute_tolerance&&
           residual<=budget.absolute_tolerance;
  }
};
} // namespace anaiero::science
