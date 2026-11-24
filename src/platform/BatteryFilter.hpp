#pragma once
#include <cstdint>

namespace mtg {
// Approximate Li-ion state of charge from the divided VCC_OUT voltage. The
// board has no fuel gauge, so this is an estimate (see docs/hardware.md).
inline unsigned batteryPercentFromMillivolts(int mv){
 constexpr int volts[]={3300,3500,3650,3750,3850,4000,4200};
 constexpr unsigned levels[]={0,5,20,40,60,80,100};
 if(mv>=volts[6])return 100;
 for(unsigned i=1;i<7;++i)if(mv>=volts[i-1]&&mv<volts[i])
  return levels[i-1]+static_cast<unsigned>(mv-volts[i-1])*(levels[i]-levels[i-1])/static_cast<unsigned>(volts[i]-volts[i-1]);
 return 0;
}

// Smooths the 5-second battery samples so the ring and battery icon do not
// flicker around thresholds (for example in and out of the <10% red pulse).
// Voltage is exponentially averaged (1/4 weight per sample). The reported
// percentage follows drops immediately but only rises after a 3-point margin,
// because the voltage recovers briefly whenever the load (radio, backlight) drops.
class BatteryFilter {
public:
 static constexpr unsigned riseMargin=3;
 void reset(){millivolts_=-1;percent_=-1;}
 unsigned update(int millivolts){
  millivolts_=millivolts_<0?millivolts:(millivolts_*3+millivolts+2)/4;
  const auto estimate=batteryPercentFromMillivolts(millivolts_);
  if(percent_<0||estimate<static_cast<unsigned>(percent_)||estimate>=static_cast<unsigned>(percent_)+riseMargin)percent_=static_cast<int>(estimate);
  return static_cast<unsigned>(percent_);
 }
 int millivolts()const{return millivolts_;}
private:
 int millivolts_=-1,percent_=-1;
};
}
