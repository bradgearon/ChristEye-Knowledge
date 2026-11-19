#include "qualification.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using anaiero::qualification::Histogram;
using Samples=std::vector<std::uint64_t>;
void expect(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}

// Independent oracle: retain and sort samples only in this test, then select
// the exact nearest-rank quantile. Production retains just 65 bucket counters.
Histogram check_sorted_oracle(Samples samples,const std::string& label){
  Histogram histogram;
  for(const auto sample:samples)histogram.add(sample);
  std::sort(samples.begin(),samples.end());
  expect(histogram.count==samples.size(),label+": exact sample count");
  const auto minimum=samples.empty()?0:samples.front();
  const auto maximum=samples.empty()?0:samples.back();
  expect(histogram.minimum_ns==minimum&&histogram.maximum_ns==maximum,label+": exact extrema");
  for(unsigned percent=1;percent<=100;++percent){
    const auto upper=histogram.percentile_upper_ns(percent);
    if(samples.empty()){
      expect(upper==0,label+": empty percentile is zero");
      continue;
    }
    const auto rank=(samples.size()*percent+99)/100;
    const auto exact=samples[rank-1];
    expect(upper>=exact,label+": upper bound understates exact p"+std::to_string(percent));
    expect(upper<=maximum,label+": upper bound exceeds observed maximum at p"+std::to_string(percent));
    // Derive the inclusive power-of-two ceiling without consulting the
    // production buckets or bit-width calculation. Zero and one share bucket 0.
    std::uint64_t ceiling=1;
    while(ceiling<exact){
      if(ceiling>std::numeric_limits<std::uint64_t>::max()/2){
        ceiling=std::numeric_limits<std::uint64_t>::max();
        break;
      }
      ceiling*=2;
    }
    expect(upper==std::min(ceiling,maximum),label+": incorrect selected bucket at p"+std::to_string(percent));
  }
  return histogram;
}

void edges_and_rounding(){
  check_sorted_oracle({},"empty");
  check_sorted_oracle({0},"single zero");
  check_sorted_oracle(Samples(257,0),"all zero");
  Samples edges{0,0,1};
  for(unsigned bit=0;bit<64;++bit){
    const auto power=std::uint64_t{1}<<bit;
    for(const auto sample:{power-1,power,power+1}){
      check_sorted_oracle({sample},"singleton power edge "+std::to_string(bit));
      edges.push_back(sample);
    }
  }
  const auto maximum=std::numeric_limits<std::uint64_t>::max();
  check_sorted_oracle({maximum},"UINT64_MAX singleton");
  edges.push_back(maximum-1);edges.push_back(maximum);
  check_sorted_oracle(edges,"all power edges and UINT64_MAX");
  std::reverse(edges.begin(),edges.end());
  check_sorted_oracle(edges,"reverse insertion order");
  // Small and non-multiple-of-100 counts expose nearest-rank rounding. Mixed
  // zeros and nonzero samples also exercise bucket 0 with a nonzero maximum.
  for(std::size_t count=1;count<=103;++count){
    Samples values;
    for(std::size_t i=0;i<count;++i)values.push_back(((i*17+count)%37)*1000003);
    check_sorted_oracle(std::move(values),"sample count "+std::to_string(count));
  }
}

void unchanged_two_second_gate(){
  constexpr std::uint64_t budget_ns=2000000000;
  constexpr std::uint64_t observed_maximum=1656422900;
  Samples below(1538,300000000);below.insert(below.end(),50,observed_maximum);
  const auto below_histogram=check_sorted_oracle(std::move(below),"1.656-second observed maximum");
  expect(below_histogram.percentile_upper_ns(99)==observed_maximum&&
         below_histogram.percentile_upper_ns(99)<=budget_ns,
         "a coarse 2.147-second bucket cannot fail when every sample is below two seconds");

  const auto at_budget=check_sorted_oracle(Samples(100,budget_ns),"exact two-second boundary");
  expect(at_budget.percentile_upper_ns(99)==budget_ns,"the declared inclusive budget remains inclusive");

  Samples over(95,100000000);over.insert(over.end(),5,2010000001);
  const auto over_histogram=check_sorted_oracle(std::move(over),"actual over-budget p99 tail");
  expect(over_histogram.percentile_upper_ns(99)>budget_ns,
         "a real p99 above two seconds must still reject under the unchanged gate");

  Samples outlier(99,100000000);outlier.push_back(3000000000);
  const auto outlier_histogram=check_sorted_oracle(std::move(outlier),"outlier beyond p99");
  expect(outlier_histogram.percentile_upper_ns(99)<budget_ns&&
         outlier_histogram.percentile_upper_ns(100)>budget_ns,
         "percentiles retain their selected ranks instead of becoming the maximum");
}
}

int main(){
  try{
    edges_and_rounding();unchanged_two_second_gate();
    std::cout<<"bounded percentile oracle, integer edges, nearest-rank rounding and unchanged latency gate passed\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
