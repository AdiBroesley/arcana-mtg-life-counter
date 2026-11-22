#pragma once
#include <cstdint>
#include <limits>
namespace mtg {
struct RotationBatch {std::int32_t delta=0;std::uint64_t sampledAt=0;};
// The hardware adapter protects add/take with one short critical section.
// Timestamp the oldest pending pulse, rather than overwriting it on each click.
class EncoderBatch {
public:
 void add(int direction,std::uint64_t now){
  if(direction!=1&&direction!=-1)return;
  if((direction>0&&pending_.delta==std::numeric_limits<std::int32_t>::max())||
     (direction<0&&pending_.delta==std::numeric_limits<std::int32_t>::min()))return;
  if(!pending_.delta)pending_.sampledAt=now;
  pending_.delta+=direction;
 }
 RotationBatch take(){auto result=pending_;pending_={};return result;}
private:RotationBatch pending_;
};
}
