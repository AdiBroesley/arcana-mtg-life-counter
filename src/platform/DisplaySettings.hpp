#pragma once
#include <cstdint>
namespace mtg {
struct DisplaySettings {
 unsigned dimAfterSeconds=5,idleBrightness=10;
 bool lifeOnly=false,wakeProtection=false;
 bool valid()const{return dimAfterSeconds<=300&&idleBrightness<=100;}
 bool operator==(const DisplaySettings& other)const{return dimAfterSeconds==other.dimAfterSeconds&&idleBrightness==other.idleBrightness&&lifeOnly==other.lifeOnly&&wakeProtection==other.wakeProtection;}
 bool operator!=(const DisplaySettings& other)const{return !(*this==other);}
};
enum class PeerRadio : unsigned {Bluetooth,Wifi};
}
