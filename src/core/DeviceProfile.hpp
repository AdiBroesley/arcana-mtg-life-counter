#pragma once
#include "platform/IStorage.hpp"
#include "DeviceSettings.hpp"
#include "platform/DisplaySettings.hpp"
#include <string>
namespace mtg {
class DeviceProfile {
public:
 explicit DeviceProfile(IStorage* storage=nullptr);
 bool load();bool setName(const std::string& name);
 bool setFaceDesign(unsigned value);bool setBrightness(unsigned value);
 unsigned faceDesign()const{return faceDesign_;}unsigned brightness()const{return brightness_;}
 bool hasBrightness()const{return brightnessSaved_;}
 const DisplaySettings& displaySettings()const{return display_;}
 PeerRadio radio()const{return radio_;}
 bool setDisplaySettings(const DisplaySettings& value);bool setRadio(PeerRadio value);
 const std::string& name()const{return name_;}const std::string& id()const{return id_;}const std::string& error()const{return error_;}
 static bool validName(const std::string& value);
private:
 bool save(const std::string& name,unsigned face,unsigned brightness,bool brightnessSaved,const DisplaySettings& display,PeerRadio radio);
 IStorage* storage_;std::string id_,name_,error_;
 unsigned faceDesign_=defaultFaceDesign,brightness_=defaultBrightness;
 bool brightnessSaved_=false;
 DisplaySettings display_;PeerRadio radio_=PeerRadio::Bluetooth;
};
}
