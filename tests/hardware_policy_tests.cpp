#include <algorithm>
#include "platform/IdleBacklight.hpp"
#include "platform/TimingStats.hpp"
#include "platform/BatteryRing.hpp"
#include "platform/BatteryFilter.hpp"
#include "platform/EncoderBatch.hpp"
#include "platform/RoundDisplay.hpp"
#include <vector>
#include <stdexcept>
#include <iostream>
void check(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
int main(){try{
 for(unsigned bytes:{2u,4u})for(const auto& area:std::array<std::array<int,4>,5>{{{0,0,360,360},{0,177,12,6},{170,4,20,9},{82,100,196,180},{170,347,20,13}}}){
  int x=area[0],y=area[1],w=area[2],h=area[3];std::size_t stride=w*bytes+6;
  std::vector<std::uint8_t> pixels(17+stride*h+17,0xab);
  mtg::blackOutsideFace(pixels.data()+17,x,y,w,h,bytes,stride);
  for(int row=0;row<h;++row){
   for(int col=0;col<w;++col){int dx=2*(x+col)+1-360,dy=2*(y+row)+1-360;bool outside=dx*dx+dy*dy>4*176*176;
    for(unsigned byte=0;byte<bytes;++byte)check(pixels[17+row*stride+col*bytes+byte]==(outside?0:0xab),"Perimeter mask must clear outside pixels and preserve inside pixels for full/partial RGB565 and RGB8888 transfers");
   }
   for(unsigned pad=0;pad<6;++pad)check(pixels[17+row*stride+w*bytes+pad]==0xab,"Perimeter mask must preserve row padding");
  }
  for(unsigned guard=0;guard<17;++guard)check(pixels[guard]==0xab&&pixels[pixels.size()-1-guard]==0xab,"Perimeter mask must not write outside the transfer buffer");
 }
 mtg::EncoderBatch encoder;check(encoder.take().delta==0,"Empty encoder batch");
 encoder.add(1,1000);encoder.add(1,2000);auto clockwise=encoder.take();
 check(clockwise.delta==2&&clockwise.sampledAt==1000,"Encoder batches must keep the oldest pending pulse time");
 check(encoder.take().delta==0,"Taking a batch must consume every pending click exactly once");
 encoder.add(-1,3000);encoder.add(-1,4000);auto counterclockwise=encoder.take();
 check(counterclockwise.delta==-2&&counterclockwise.sampledAt==3000,"Counterclockwise direction and batch time");
 encoder.add(1,5000);encoder.add(-1,6000);encoder.add(1,UINT64_C(0x100000123));auto reversed=encoder.take();
 check(reversed.delta==1&&reversed.sampledAt==UINT64_C(0x100000123),"Reversed net-zero batches must reset latency and preserve 64-bit timestamps");
 mtg::IdleBacklight light;mtg::DisplaySettings protectedWake;protectedWake.wakeProtection=true;light.configure(protectedWake,100);light.setBrightness(75,100);
 light.tick(5099);check(light.output()==75,"Dimmed too soon");light.tick(5100);check(light.output()==10&&light.brightness()==75,"Idle dim must preserve selection");
 check(!light.touch(5200,true)&&light.output()==75,"Wake press must restore brightness and be consumed");
 check(!light.touch(6200,true),"Held wake touch must remain consumed");check(!light.touch(6300,false),"Wake release must not click");
 check(light.touch(6400,true)&&light.touch(6500,false),"Next tap must work normally");
 light.tick(11500);check(light.dimmed(),"Must dim five seconds after release");
 check(!light.rotation(11600)&&light.output()==75,"Wake rotation must not edit values");
 check(!light.rotation(11700),"Wake rotation burst must remain consumed");check(light.rotation(12000),"Next independent rotation must work");
 light.touch(12500,true);light.tick(30000,true);check(!light.dimmed(),"Never dim while held");light.touch(30001,false);light.tick(35001);check(light.dimmed(),"Dim countdown must resume after release");
 light.setBrightness(5,36000);light.tick(50000);check(light.output()==5&&light.dimmed(),"Idle policy must not brighten an already low selection");
 light.setBrightness(0,51000);light.tick(60000);check(light.output()==0&&light.dimmed(),"Respect deliberate zero brightness");
 light.setBrightness(90,61000);light.tick(66000);light.tick(66001,true);check(light.dimmed(),"Raw held sample must not bypass wake suppression");check(!light.touch(66002,true),"Queued wake must still be consumed");
 mtg::IdleBacklight immediate;immediate.setBrightness(6,0);immediate.tick(5000);
 check(immediate.dimmed()&&immediate.output()==6,"Idle state must be tracked even below 10 percent");
 check(immediate.rotation(5001)&&!immediate.dimmed(),"Wake protection defaults off: first wheel turn edits immediately");
 immediate.tick(10001);check(immediate.touch(10002,true)&&immediate.touch(10003,false),"Unprotected first tap and release must both pass");
 mtg::DisplaySettings custom;custom.dimAfterSeconds=12;custom.idleBrightness=3;immediate.configure(custom,11000);immediate.setBrightness(80,11000);immediate.tick(22999);check(!immediate.dimmed(),"Custom delay respected");immediate.tick(23000);check(immediate.output()==3,"Custom idle brightness respected");
 custom.dimAfterSeconds=0;immediate.configure(custom,24000);immediate.tick(999999);check(!immediate.dimmed()&&immediate.output()==80,"Never disables idle dimming");
 custom.dimAfterSeconds=1;custom.wakeProtection=true;immediate.configure(custom,1000000);immediate.setBrightness(4,1000000);immediate.tick(1001000);check(!immediate.touch(1001001,true)&&!immediate.touch(1001002,false),"Wake protection applies even below 10 percent");
 mtg::IdleBacklight delayed;delayed.configure(protectedWake,0);delayed.tick(5000);
 check(!delayed.rotation(5010,5000,5005),"First queued wake batch is suppressed");
 check(!delayed.rotation(5700,5100,5250),"A continuous wake burst queued during redraw stays suppressed after the processing-time deadline");
 check(!delayed.rotation(6000,5300,5490),"Delayed protected batches extend the guard using the latest physical pulse");
 check(delayed.rotation(6100,5800,5805),"A genuinely separate gesture after a physical pause is accepted even if processed late");
 mtg::EncoderBatch cancelled;cancelled.add(1,100);cancelled.add(-1,180);auto zero=cancelled.take();
 check(zero.hasInput&&zero.delta==0&&zero.firstSampledAt==100&&zero.lastSampledAt==180,"Net-zero batches retain physical activity for wake protection");
 check(!cancelled.take().hasInput,"Taking a batch clears physical activity too");
 cancelled.add(1,1000);cancelled.add(-1,1100);cancelled.add(1,1400);auto bounds=cancelled.take();
 check(bounds.firstSampledAt==1000&&bounds.lastSampledAt==1400&&bounds.sampledAt==1400,"Physical gesture bounds coexist with the existing net-pulse latency timestamp");
 mtg::TimingStats stats;check(stats.percentile(95)==0,"Empty metric");for(unsigned i=1;i<=100;++i)stats.add(i);
 check(stats.count()==100&&stats.mean()==50&&stats.percentile(50)==50&&stats.percentile(95)==95&&stats.maximum()==100,"Timing summary incorrect");
 for(unsigned i=0;i<128;++i)stats.add(200);check(stats.percentile(95)==200&&stats.maximum()==200,"Rolling percentile incorrect");
 auto batteryColor=[](unsigned percent,std::uint64_t now=0){mtg::BatteryStatus battery;battery.available=true;battery.percent=percent;return mtg::batteryRingColor(battery,now);};
 check(batteryColor(100)==mtg::RingColor{0,mtg::ringPeak,0}&&mtg::ringPeak==13,"Full battery is green at about 5 percent luminosity");
 check(batteryColor(60)==mtg::RingColor{mtg::ringPeak,mtg::ringPeak,0},"Half-way battery is yellow");
 check(batteryColor(20)==mtg::RingColor{mtg::ringPeak,0,0}&&batteryColor(15)==mtg::RingColor{mtg::ringPeak,0,0}&&batteryColor(10)==mtg::RingColor{mtg::ringPeak,0,0},"20 to 10 percent is steady red");
 unsigned lastGreen=0;bool monotonic=true;for(unsigned p=20;p<=100;++p){auto c=batteryColor(p);monotonic=monotonic&&c.green>=lastGreen&&c.blue==0&&std::max(c.red,c.green)==mtg::ringPeak;lastGreen=c.green;}
 check(monotonic,"Gradient moves steadily towards green and keeps the brighter channel at peak");
 mtg::BatteryStatus low;low.available=true;low.percent=9;
 check(mtg::batteryRingPulses(low)&&!mtg::batteryRingPulses([]{mtg::BatteryStatus b;b.available=true;b.percent=10;return b;}()),"Only below 10 percent pulses on battery");
 auto lowDim=mtg::batteryRingColor(low,0),lowBright=mtg::batteryRingColor(low,mtg::ringLowPulsePeriodMs/2);
 check(lowDim.red>0&&lowDim.red<lowBright.red&&lowBright==mtg::RingColor{mtg::ringPeak,0,0}&&lowDim.green==0&&lowDim.blue==0,"Low battery pulses slowly in red without switching fully off");
 check(mtg::batteryRingColor(low,mtg::ringLowPulsePeriodMs)==lowDim&&mtg::ringLowPulsePeriodMs==8000,"Low-battery pulse repeats every 8 seconds");
 check(mtg::batteryRingColor({})==mtg::RingColor{},"Unknown battery must not display a fabricated status");
 mtg::BatteryStatus usb;usb.externalPower=true;
 auto usbDim=mtg::batteryRingColor(usb,0),usbBright=mtg::batteryRingColor(usb,mtg::ringPowerPulsePeriodMs/2);
 check(mtg::batteryRingPulses(usb)&&usbDim.red==0&&usbDim.green==0&&usbDim.blue>0&&usbBright==mtg::RingColor{0,0,mtg::ringPeak}&&mtg::batteryRingColor(usb,mtg::ringPowerPulsePeriodMs)==usbDim&&mtg::ringPowerPulsePeriodMs==8000,"External power pulses blue every 8 seconds");
 auto fullRing=mtg::ringFrame(batteryColor(15));
 check(fullRing.size()==64*3,"Ring frame must reach longer hardware revisions");
 for(unsigned i=0;i<64;++i)check(fullRing[i*3]==0&&fullRing[i*3+1]==mtg::ringPeak&&fullRing[i*3+2]==0,"Every LED slot, including beyond the demo's 13, must receive identical red in GRB order");
 auto greenRing=mtg::ringFrame(batteryColor(100));auto offRing=mtg::ringFrame({});
 for(unsigned i=0;i<64;++i){check(greenRing[i*3]==mtg::ringPeak&&greenRing[i*3+1]==0&&greenRing[i*3+2]==0,"Entire ring must receive green");check(offRing[i*3]==0&&offRing[i*3+1]==0&&offRing[i*3+2]==0,"Off must clear every LED slot");}
 check(mtg::batteryPercentFromMillivolts(4250)==100&&mtg::batteryPercentFromMillivolts(4200)==100&&mtg::batteryPercentFromMillivolts(3500)==5&&mtg::batteryPercentFromMillivolts(3200)==0&&mtg::batteryPercentFromMillivolts(3575)==12,"Voltage curve keeps its calibrated points");
 mtg::BatteryFilter filter;check(filter.update(3580)==mtg::batteryPercentFromMillivolts(3580),"First sample seeds the filter");
 auto seeded=filter.update(3580);bool steadyRing=true;
 for(int i=0;i<40;++i){auto p=filter.update(i%2?3560:3600);steadyRing=steadyRing&&p<=seeded&&p+3>=seeded;}
 check(steadyRing,"Load jitter of +/-20 mV keeps the estimate steady");
 mtg::BatteryFilter dip;for(int i=0;i<8;++i)dip.update(3540);auto lowEstimate=dip.update(3540);
 for(int i=0;i<3;++i)dip.update(3555);
 check(dip.update(3555)<lowEstimate+mtg::BatteryFilter::riseMargin,"Brief voltage recovery after a load drop does not raise the estimate");
 for(int i=0;i<20;++i)dip.update(3700);check(dip.update(3700)>=lowEstimate+mtg::BatteryFilter::riseMargin,"A sustained rise (e.g. after charging) is followed");
 mtg::BatteryFilter falling;falling.update(3800);for(int i=0;i<30;++i)falling.update(3480);check(falling.update(3480)<5,"Drops are followed promptly");
 falling.reset();check(falling.update(4100)==mtg::batteryPercentFromMillivolts(4100),"Reset re-seeds after unplugging");
 std::cout<<"Idle backlight, battery ring and timing statistics passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
