#pragma once
#include "platform/IHardware.hpp"
#include "platform/IdleBacklight.hpp"
namespace mtg {
class SimulatorHardware final : public IHardware {
public:
 SimulatorHardware(){backlight_.setBrightness(100,0);}
 void rotate(int steps) { if(on_&&backlight_.rotation(now_))pending_+=steps; }
 std::int64_t takeRotation() override { auto result = pending_; pending_ = 0; return result; }
 Touch touch() const override { return touch_; }
 void setTouch(Touch value) {rawTouch_=value;bool deliver=backlight_.touch(now_,value.pressed);touch_=value;if(!deliver)touch_.pressed=false;}
 bool powered() const override { return on_; }
 unsigned brightness() const override { return backlight_.brightness(); }
 void setBrightness(unsigned value) override {backlight_.setBrightness(value,now_);}
 void setDisplaySettings(const DisplaySettings& value)override{backlight_.configure(value,now_);}
 void updateIdle(std::uint32_t now)override{now_=now;backlight_.tick(now_,rawTouch_.pressed);}
 bool idle()const override{return backlight_.dimmed();}
 unsigned outputBrightness()const override{return backlight_.output();}
 BatteryStatus battery() const override { return battery_; }
 void setBattery(unsigned percent,bool charging=false) { battery_={percent>100?100:percent,charging,true}; }
 void togglePower() { on_ = !on_; pending_ = 0; touch_.pressed = false;rawTouch_.pressed=false;backlight_.touch(now_,false); }
private:
 std::int64_t pending_ = 0;
 Touch touch_;
 bool on_ = true;
 IdleBacklight backlight_;std::uint64_t now_=0;Touch rawTouch_;
 BatteryStatus battery_{75,false,true};
};
}
