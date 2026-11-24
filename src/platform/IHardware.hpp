#pragma once
#include <cstdint>
#include "DisplaySettings.hpp"
namespace mtg {
class IProfiler;
struct Touch { int x = 0; int y = 0; bool pressed = false; };
struct BatteryStatus { unsigned percent=0; bool charging=false; bool available=false; bool externalPower=false; };
class IHardware {
public:
 virtual ~IHardware() = default;
 virtual std::int64_t takeRotation() = 0;
 // A consumed wheel batch changed game state; used for input-to-display timing.
 virtual void rotationApplied(bool) {}
 virtual Touch touch() const = 0;
 virtual bool powered() const = 0;
 virtual unsigned brightness() const { return 100; }
 virtual void setBrightness(unsigned) {}
 virtual void setDisplaySettings(const DisplaySettings&) {}
 virtual void updateIdle(std::uint32_t) {}
 virtual bool idle()const{return false;}
 virtual unsigned outputBrightness()const{return brightness();}
 virtual BatteryStatus battery() const { return {}; }
 virtual IProfiler* profiler()const {return nullptr;}
};
}
