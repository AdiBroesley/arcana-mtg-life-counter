#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace mtg {
class TimingStats {
public:
 void add(std::uint32_t micros){samples_[count_%samples_.size()]=micros;++count_;sum_+=micros;maximum_=std::max(maximum_,micros);}
 std::uint32_t count()const{return count_;}
 std::uint64_t mean()const{return count_?sum_/count_:0;}
 std::uint32_t maximum()const{return maximum_;}
 // Exact percentile of the latest 128 samples; average/max cover the whole capture.
 std::uint32_t percentile(unsigned percent)const{
  auto n=std::min<std::size_t>(count_,samples_.size());if(!n)return 0;
  auto sorted=samples_;std::sort(sorted.begin(),sorted.begin()+n);
  auto rank=std::max<std::size_t>(1,(n*std::min(percent,100u)+99)/100);
  return sorted[rank-1];
 }
private:
 std::array<std::uint32_t,128> samples_{};
 std::uint32_t count_=0,maximum_=0;std::uint64_t sum_=0;
};
}
