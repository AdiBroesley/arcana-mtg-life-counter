#pragma once
#include "IHardware.hpp"
#include <cstdint>
#include <array>
#include <cmath>

namespace mtg {
struct RingColor {
 std::uint8_t red=0,green=0,blue=0;
 bool operator==(const RingColor& other)const{return red==other.red&&green==other.green&&blue==other.blue;}
};

// Uniform padding also reaches longer ring revisions. Extra pixels are simply
// forwarded past the last LED of a shorter WS2812 chain.
constexpr unsigned ringFramePixels=64;
inline std::array<std::uint8_t,ringFramePixels*3> ringFrame(RingColor color){
 std::array<std::uint8_t,ringFramePixels*3> frame{};
 for(unsigned i=0;i<ringFramePixels;++i){frame[i*3]=color.green;frame[i*3+1]=color.red;frame[i*3+2]=color.blue;}
 return frame;
}

// Peak LED level is about 5% (13/255). The ring shows an approximate battery
// gradient (green at 100%, through yellow, red at 20% and below), pulses red
// below 10%, and pulses blue while external/USB power is present (both 8 s cycles). The
// board cannot tell charging from full, so any external power selects blue.
constexpr std::uint8_t ringPeak=13;
constexpr std::uint32_t ringLowPulsePeriodMs=8000;
constexpr std::uint32_t ringPowerPulsePeriodMs=8000;
inline bool batteryRingPulses(BatteryStatus battery){
 return battery.externalPower||(battery.available&&battery.percent<10);
}
// Smooth 0..peak..0 pulse that never fully switches off (floor is 1/8 of peak).
inline std::uint8_t ringPulseLevel(std::uint64_t nowMs,std::uint32_t periodMs){
 const auto phase=static_cast<double>(nowMs%periodMs)/periodMs;
 const double wave=(1.0-std::cos(phase*2.0*3.14159265358979323846))/2.0;
 const double floor=ringPeak/8.0;
 return static_cast<std::uint8_t>(floor+(ringPeak-floor)*wave+0.5);
}
inline RingColor batteryRingColor(BatteryStatus battery,std::uint64_t nowMs=0){
 if(battery.externalPower)return {0,0,ringPulseLevel(nowMs,ringPowerPulsePeriodMs)};
 if(!battery.available)return {};
 if(battery.percent<10)return {ringPulseLevel(nowMs,ringLowPulsePeriodMs),0,0};
 if(battery.percent<=20)return {ringPeak,0,0};
 // 20%..100% maps red -> yellow -> green with the brighter channel held at peak.
 const unsigned span=battery.percent>=100?80:battery.percent-20;
 const unsigned red=span>=40?ringPeak*(80-span)/40:ringPeak;
 const unsigned green=span>=40?ringPeak:ringPeak*span/40;
 return {static_cast<std::uint8_t>(red),static_cast<std::uint8_t>(green),0};
}
}
