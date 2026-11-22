#include "platform/IdleBacklight.hpp"
#include "platform/TimingStats.hpp"
#include "platform/BatteryRing.hpp"
#include "platform/EncoderBatch.hpp"
#include <stdexcept>
#include <iostream>
void check(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
int main(){try{
 mtg::EncoderBatch encoder;check(encoder.take().delta==0,"Empty encoder batch");
 encoder.add(1,1000);encoder.add(1,2000);auto clockwise=encoder.take();
 check(clockwise.delta==2&&clockwise.sampledAt==1000,"Encoder batches must keep the oldest pending pulse time");
 check(encoder.take().delta==0,"Taking a batch must consume every pending click exactly once");
 encoder.add(-1,3000);encoder.add(-1,4000);auto counterclockwise=encoder.take();
 check(counterclockwise.delta==-2&&counterclockwise.sampledAt==3000,"Counterclockwise direction and batch time");
 encoder.add(1,5000);encoder.add(-1,6000);encoder.add(1,UINT64_C(0x100000123));auto reversed=encoder.take();
 check(reversed.delta==1&&reversed.sampledAt==UINT64_C(0x100000123),"Reversed net-zero batches must reset latency and preserve 64-bit timestamps");
 mtg::IdleBacklight light;light.setBrightness(75,100);
 light.tick(5099);check(light.output()==75,"Dimmed too soon");light.tick(5100);check(light.output()==10&&light.brightness()==75,"Idle dim must preserve selection");
 check(!light.touch(5200,true)&&light.output()==75,"Wake press must restore brightness and be consumed");
 check(!light.touch(6200,true),"Held wake touch must remain consumed");check(!light.touch(6300,false),"Wake release must not click");
 check(light.touch(6400,true)&&light.touch(6500,false),"Next tap must work normally");
 light.tick(11500);check(light.dimmed(),"Must dim five seconds after release");
 check(!light.rotation(11600)&&light.output()==75,"Wake rotation must not edit values");
 check(!light.rotation(11700),"Wake rotation burst must remain consumed");check(light.rotation(12000),"Next independent rotation must work");
 light.touch(12500,true);light.tick(30000,true);check(!light.dimmed(),"Never dim while held");light.touch(30001,false);light.tick(35001);check(light.dimmed(),"Dim countdown must resume after release");
 light.setBrightness(5,36000);light.tick(50000);check(light.output()==5&&!light.dimmed(),"Idle policy must not brighten an already low selection");
 light.setBrightness(0,51000);light.tick(60000);check(light.output()==0&&!light.dimmed(),"Respect deliberate zero brightness");
 light.setBrightness(90,61000);light.tick(66000);light.tick(66001,true);check(light.dimmed(),"Raw held sample must not bypass wake suppression");check(!light.touch(66002,true),"Queued wake must still be consumed");
 mtg::TimingStats stats;check(stats.percentile(95)==0,"Empty metric");for(unsigned i=1;i<=100;++i)stats.add(i);
 check(stats.count()==100&&stats.mean()==50&&stats.percentile(50)==50&&stats.percentile(95)==95&&stats.maximum()==100,"Timing summary incorrect");
 for(unsigned i=0;i<128;++i)stats.add(200);check(stats.percentile(95)==200&&stats.maximum()==200,"Rolling percentile incorrect");
 auto batteryColor=[](unsigned percent){mtg::BatteryStatus battery;battery.available=true;battery.percent=percent;return mtg::batteryRingColor(battery);};
 check(batteryColor(0)==mtg::RingColor{5,0,0}&&batteryColor(29)==mtg::RingColor{5,0,0},"Ring must be about two percent red below 30 percent");
 check(batteryColor(30)==mtg::RingColor{0,5,0}&&batteryColor(31)==mtg::RingColor{0,5,0}&&batteryColor(100)==mtg::RingColor{0,5,0},"Ring must be dim green at/above 30 percent");
 check(mtg::batteryRingColor({})==mtg::RingColor{},"Unknown battery must not display a fabricated status");
 mtg::BatteryStatus usb;usb.available=true;usb.externalPower=true;usb.percent=80;
 check(mtg::batteryRingColor(usb)==mtg::RingColor{},"USB supply estimate must not select a battery color");
 auto fullRing=mtg::ringFrame(batteryColor(29));
 check(fullRing.size()==64*3,"Ring frame must reach longer hardware revisions");
 for(unsigned i=0;i<64;++i)check(fullRing[i*3]==0&&fullRing[i*3+1]==5&&fullRing[i*3+2]==0,"Every LED slot, including beyond the demo's 13, must receive identical dim red in GRB order");
 auto greenRing=mtg::ringFrame(batteryColor(80));auto offRing=mtg::ringFrame({});
 for(unsigned i=0;i<64;++i){check(greenRing[i*3]==5&&greenRing[i*3+1]==0&&greenRing[i*3+2]==0,"Entire ring must receive green");check(offRing[i*3]==0&&offRing[i*3+1]==0&&offRing[i*3+2]==0,"USB/off must clear every LED slot");}
 std::cout<<"Idle backlight, battery ring and timing statistics passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
