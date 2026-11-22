#include "core/FirmwareRelease.hpp"
#include <iostream>
#include <stdexcept>
using namespace mtg;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 check(releaseVersion("v0.15.10")>releaseVersion("0.15.9"),"Versions compare numerically");
 for(const char* bad:{"","1.2","1.2.3.4","1.2.-1","v1.2.3-beta","1.2.3/evil"}){bool rejected=false;try{releaseVersion(bad);}catch(...){rejected=true;}check(rejected,"Malformed versions are rejected");}
 using J=nlohmann::json;
 J release={{"tag_name","v0.15.1"},{"draft",false},{"prerelease",false},{"assets",J::array({{{"name",appInfo::firmwareAsset},{"browser_download_url",std::string(appInfo::repositoryUrl)+"/releases/download/v0.15.1/"+appInfo::firmwareAsset},{"size",2000000},{"digest","sha256:"+std::string(64,'a')}}})}};
 check(FirmwareRelease::parse(release.dump(),0x370000).newerThan("0.15.0"),"Compatible newer binary is accepted");
 for(unsigned mode=0;mode<6;++mode){auto invalid=release;
  if(mode==0)invalid["draft"]=true;
  if(mode==1)invalid["assets"][0]["name"]="another-board.bin";
  if(mode==2)invalid["assets"][0]["size"]=9000000;
  if(mode==3)invalid["assets"][0]["digest"]="sha256:bad";
  if(mode==4)invalid["assets"][0]["browser_download_url"]="https://example.com/firmware.bin";
  if(mode==5)invalid["prerelease"]=true;
  bool rejected=false;try{FirmwareRelease::parse(invalid.dump(),0x370000);}catch(...){rejected=true;}
  check(rejected,"Wrong model, size, digest, source and unstable releases must be rejected");
 }
 std::cout<<"Firmware release compatibility and integrity checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
