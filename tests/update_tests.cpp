#include "core/FirmwareRelease.hpp"
#include "core/WifiSetup.hpp"
#include <iostream>
#include <stdexcept>
using namespace mtg;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 check(wifiSetupQr("ARCANA-SETUP-1234ABCD","001122334455")=="WIFI:T:WPA;S:ARCANA-SETUP-1234ABCD;P:001122334455;;","Wi-Fi QR includes the temporary network and password");
 check(wifiSetupQr("A;B:C,D\"E\\F","x;y:z")=="WIFI:T:WPA;S:A\\;B\\:C\\,D\\\"E\\\\F;P:x\\;y\\:z;;","Wi-Fi QR escapes reserved characters");
 const std::array<std::uint8_t,4> ip{{192,168,4,1}};
 const std::vector<std::uint8_t> query{0x12,0x34,1,0,0,1,0,0,0,0,0,0,7,'c','a','p','t','i','v','e',5,'a','p','p','l','e',3,'c','o','m',0,0,1,0,1};
 auto reply=captiveDnsReply(query.data(),query.size(),ip);
 check(reply.size()==query.size()+16&&reply[0]==0x12&&reply[1]==0x34&&reply[2]==0x85&&reply[7]==1,"Captive DNS preserves the transaction and returns one answer");
 check(std::equal(reply.end()-4,reply.end(),ip.begin())&&std::equal(query.begin()+12,query.end(),reply.begin()+12),"Captive DNS answers with the AP address and echoes the question");
 for(std::size_t length=0;length<query.size();++length)check(captiveDnsReply(query.data(),length,ip).empty(),"Truncated DNS packets are dropped");
 auto ipv6=query;ipv6[ipv6.size()-3]=28;reply=captiveDnsReply(ipv6.data(),ipv6.size(),ip);
 check(reply.size()==ipv6.size()&&reply[7]==0,"IPv6 probes receive a valid empty response");
 auto edns=query;edns[11]=1;edns.insert(edns.end(),{0,0,41,4,208,0,0,0,0,0,0});reply=captiveDnsReply(edns.data(),edns.size(),ip);
 check(reply.size()==query.size()+16&&reply[11]==0,"EDNS additions are not reflected into the captive reply");
 for(unsigned mode=0;mode<6;++mode){auto invalid=query;
  if(mode==0)invalid[2]=0x81;
  if(mode==1)invalid[5]=2;
  if(mode==2)invalid[12]=0xc0;
  if(mode==3)invalid[12]=64;
  if(mode==4)invalid.back()=3;
  if(mode==5)invalid.resize(513);
  check(captiveDnsReply(invalid.data(),invalid.size(),ip).empty(),"Response packets, multiple questions, compression, bad labels/classes and oversized DNS are dropped");
 }
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
