#pragma once
#include <cstdint>
#include <limits>
namespace mtg {
struct RotationBatch {
 std::int32_t delta=0;
 std::uint64_t sampledAt=0,firstSampledAt=0,lastSampledAt=0;
 bool hasInput=false;
};
// The hardware adapter protects add/take with one short critical section.
// Timestamp the oldest pending pulse, rather than overwriting it on each click.
class EncoderBatch {
public:
 void add(int direction,std::uint64_t now){
  if(direction!=1&&direction!=-1)return;
  if((direction>0&&pending_.delta==std::numeric_limits<std::int32_t>::max())||
     (direction<0&&pending_.delta==std::numeric_limits<std::int32_t>::min()))return;
  // Keep physical bounds even when opposite pulses cancel. Wake protection
  // must not mistake time spent drawing for a pause in the user's gesture.
  if(!pending_.hasInput){pending_.firstSampledAt=now;pending_.hasInput=true;}
  pending_.lastSampledAt=now;
  if(!pending_.delta)pending_.sampledAt=now;
  pending_.delta+=direction;
 }
 RotationBatch take(){auto result=pending_;pending_={};return result;}
private:RotationBatch pending_;
};
}
