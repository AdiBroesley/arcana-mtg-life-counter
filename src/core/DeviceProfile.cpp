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
  id_=id;name_=name;faceDesign_=setting("face",faceDesignCount-1,defaultFaceDesign);brightness_=setting("brightness",100,defaultBrightness);brightnessSaved_=j.contains("brightness")&&j["brightness"].is_number_integer()&&j["brightness"].get<std::int64_t>()>=0&&j["brightness"].get<std::int64_t>()<=100;error_.clear();return true;
 }catch(...){error_="Device profile is invalid";return false;}
}
bool DeviceProfile::save(const std::string& name,unsigned face,unsigned brightness,bool brightnessSaved){
 if(storage_){auto data=nlohmann::json{{"version",1},{"id",id_},{"name",name},{"face",face}};if(brightnessSaved)data["brightness"]=brightness;if(!storage_->save(data.dump(2),error_))return false;}
 name_=name;faceDesign_=face;brightness_=brightness;brightnessSaved_=brightnessSaved;error_.clear();return true;
}
bool DeviceProfile::setName(const std::string& value){auto name=value;while(!name.empty()&&name.back()==' ')name.pop_back();while(!name.empty()&&name.front()==' ')name.erase(0,1);if(!validName(name)){error_="Use 1-12 letters or digits";return false;}return save(name,faceDesign_,brightness_,brightnessSaved_);}
bool DeviceProfile::setFaceDesign(unsigned value){if(value>=faceDesignCount){error_="Unknown watch face";return false;}if(value==faceDesign_)return true;return save(name_,value,brightness_,brightnessSaved_);}
bool DeviceProfile::setBrightness(unsigned value){if(value>100){error_="Brightness must be 0-100%";return false;}if(value==brightness_&&brightnessSaved_)return true;return save(name_,faceDesign_,value,true);}
}
