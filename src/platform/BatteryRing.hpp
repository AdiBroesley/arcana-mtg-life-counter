#pragma once
#include "IHardware.hpp"
#include <cstdint>
#include <array>

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

inline RingColor batteryRingColor(BatteryStatus battery){
 if(!battery.available||battery.externalPower)return {};
 return battery.percent<30?RingColor{5,0,0}:RingColor{0,5,0};
}
}
