#include "DeviceProfile.hpp"
#include "third_party/json.hpp"
#include <random>
#ifdef ESP_PLATFORM
#include "esp_random.h"
#endif
namespace mtg {
DeviceProfile::DeviceProfile(IStorage* storage):storage_(storage){
#ifdef ESP_PLATFORM
 auto random=[](){return esp_random();};
#else
 std::random_device random;
#endif
 const char* hex="0123456789abcdef";for(int i=0;i<32;++i)id_+=hex[random()%16];}
bool DeviceProfile::validName(const std::string& value){return !value.empty()&&value.size()<=12&&value.front()!=' '&&value.back()!=' '&&value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_")==std::string::npos;}
bool DeviceProfile::load(){
 if(!storage_)return true;std::string data;if(!storage_->load(data,error_))return false;if(data.empty())return true;
 try{
  auto j=nlohmann::json::parse(data);auto id=j.at("id").get<std::string>(),name=j.at("name").get<std::string>();
  if(j.at("version")!=1||id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos||(!name.empty()&&!validName(name)))throw 1;
  auto setting=[&](const char* key,unsigned maximum,unsigned fallback){auto it=j.find(key);if(it==j.end()||!it->is_number_integer())return fallback;auto value=it->get<std::int64_t>();return value>=0&&value<=maximum?static_cast<unsigned>(value):fallback;};
  id_=id;name_=name;faceDesign_=setting("face",faceDesignCount-1,defaultFaceDesign);brightness_=setting("brightness",100,defaultBrightness);brightnessSaved_=j.contains("brightness")&&j["brightness"].is_number_integer()&&j["brightness"].get<std::int64_t>()>=0&&j["brightness"].get<std::int64_t>()<=100;display_={};radio_=static_cast<PeerRadio>(setting("radio",1,0));
  auto d=j.find("display");if(d!=j.end()&&d->is_object()){
   auto number=[&](const char* key,unsigned maximum,unsigned fallback){auto it=d->find(key);if(it==d->end()||!it->is_number_integer())return fallback;auto n=it->get<std::int64_t>();return n>=0&&n<=maximum?static_cast<unsigned>(n):fallback;};
   display_.dimAfterSeconds=number("dim_after",300,5);display_.idleBrightness=number("idle_brightness",100,10);
   for(auto item:{std::pair<const char*,bool*>{"life_only",&display_.lifeOnly},{"wake_protection",&display_.wakeProtection}}){auto it=d->find(item.first);if(it!=d->end()&&it->is_boolean())*item.second=it->get<bool>();}
  }
  error_.clear();return true;
 }catch(...){error_="Device profile is invalid";return false;}
}
bool DeviceProfile::save(const std::string& name,unsigned face,unsigned brightness,bool brightnessSaved,const DisplaySettings& display,PeerRadio radio){
 if(storage_){auto data=nlohmann::json{{"version",1},{"id",id_},{"name",name},{"face",face}};if(brightnessSaved)data["brightness"]=brightness;data["display"]={{"dim_after",display.dimAfterSeconds},{"idle_brightness",display.idleBrightness},{"life_only",display.lifeOnly},{"wake_protection",display.wakeProtection}};data["radio"]=static_cast<unsigned>(radio);if(!storage_->save(data.dump(2),error_))return false;}
 name_=name;faceDesign_=face;brightness_=brightness;brightnessSaved_=brightnessSaved;display_=display;radio_=radio;error_.clear();return true;
}
bool DeviceProfile::setName(const std::string& value){auto name=value;while(!name.empty()&&name.back()==' ')name.pop_back();while(!name.empty()&&name.front()==' ')name.erase(0,1);if(!validName(name)){error_="Use 1-12 letters or digits";return false;}return save(name,faceDesign_,brightness_,brightnessSaved_,display_,radio_);}
bool DeviceProfile::setFaceDesign(unsigned value){if(value>=faceDesignCount){error_="Unknown watch face";return false;}if(value==faceDesign_)return true;return save(name_,value,brightness_,brightnessSaved_,display_,radio_);}
bool DeviceProfile::setBrightness(unsigned value){if(value>100){error_="Brightness must be 0-100%";return false;}if(value==brightness_&&brightnessSaved_)return true;return save(name_,faceDesign_,value,true,display_,radio_);}
bool DeviceProfile::setDisplaySettings(const DisplaySettings& value){if(!value.valid()){error_="Invalid display settings";return false;}if(value==display_)return true;return save(name_,faceDesign_,brightness_,brightnessSaved_,value,radio_);}
bool DeviceProfile::setRadio(PeerRadio value){if(static_cast<unsigned>(value)>1){error_="Unknown connection mode";return false;}if(value==radio_)return true;return save(name_,faceDesign_,brightness_,brightnessSaved_,display_,value);}

}
