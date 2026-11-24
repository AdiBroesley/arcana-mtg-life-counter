#pragma once
#include <algorithm>
#include <cstdint>
#include "DisplaySettings.hpp"

namespace mtg {
// Owned by the UI thread; sampling tasks only queue input.
class IdleBacklight {
public:
 void configure(const DisplaySettings& settings,std::uint64_t now){if(settings.valid()){settings_=settings;activity(now);wheelGuardUntil_=0;}}
 void setBrightness(unsigned value,std::uint64_t now){requested_=std::min(value,100u);activity(now);}
 unsigned brightness()const{return requested_;}
 unsigned output()const{return dimmed_?std::min(requested_,settings_.idleBrightness):requested_;}
 bool dimmed()const{return dimmed_;}
 void tick(std::uint64_t now,bool physicalTouchHeld=false){
  if(touchDown_||physicalTouchHeld)lastActivity_=now;
  if(!touchDown_&&!physicalTouchHeld&&settings_.dimAfterSeconds&&now-lastActivity_>=static_cast<std::uint64_t>(settings_.dimAfterSeconds)*1000)dimmed_=true;
 }
 // False suppresses an entire wake gesture, including its release/click.
 bool touch(std::uint64_t now,bool pressed){
  if(pressed){if(!touchDown_)ignoreTouch_=dimmed_&&settings_.wakeProtection;activity(now);}
  else if(touchDown_)activity(now);
  touchDown_=pressed;bool deliver=!ignoreTouch_;
  if(!pressed)ignoreTouch_=false;
  return deliver;
 }
 bool rotation(std::uint64_t now){return rotation(now,now,now);}
 bool rotation(std::uint64_t now,std::uint64_t firstSampledAt,std::uint64_t lastSampledAt){
  bool waking=dimmed_;activity(now);
  // Classify queued pulses by acquisition time, not delayed UI consumption.
  // A batch beginning in the protected burst stays protected after a redraw.
  if(settings_.wakeProtection&&(waking||(wheelGuardUntil_&&firstSampledAt<wheelGuardUntil_))){wheelGuardUntil_=std::max(wheelGuardUntil_,lastSampledAt+250);return false;}
  wheelGuardUntil_=0;return true;
 }
private:
 void activity(std::uint64_t now){lastActivity_=now;dimmed_=false;}
 unsigned requested_=70;
 DisplaySettings settings_;
 std::uint64_t lastActivity_=0,wheelGuardUntil_=0;
 bool dimmed_=false,touchDown_=false,ignoreTouch_=false;
};
}
