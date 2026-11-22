#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mtg {
inline std::string wifiSetupQr(const std::string& network,const std::string& password){
 auto escape=[](const std::string& value){
  std::string result;
  for(char c:value){if(c=='\\'||c==';'||c==','||c=='"'||c==':')result+='\\';result+=c;}
  return result;
 };
 return "WIFI:T:WPA;S:"+escape(network)+";P:"+escape(password)+";;";
}

// Setup-only DNS: one ordinary question, bounded names/packets, no compression
// traversal. IPv4 names resolve to the knob; IPv6/other types return no data.
inline std::vector<std::uint8_t> captiveDnsReply(const std::uint8_t* query,std::size_t size,const std::array<std::uint8_t,4>& address){
 if(size<17||size>512||(query[2]&0xfe)||(query[3]&0xcf)||query[4]||query[5]!=1||query[6]||query[7]||query[8]||query[9])return {};
 std::size_t end=12;
 while(true){
  if(end>=size)return {};
  auto length=query[end++];
  if(length>63||end+length>size||end+length-12>255)return {};
  if(!length)break;
  end+=length;
 }
 if(end+4>size||query[end+2]||query[end+3]!=1)return {};
 const bool ipv4=query[end]==0&&query[end+1]==1;
 end+=4;
 std::vector<std::uint8_t> reply(query,query+end);
 reply[2]=0x84|(query[2]&1);reply[3]=0;
 reply[6]=0;reply[7]=ipv4?1:0;reply[8]=reply[9]=reply[10]=reply[11]=0;
 if(ipv4){
  const std::uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4};
  reply.insert(reply.end(),answer,answer+sizeof(answer));
  reply.insert(reply.end(),address.begin(),address.end());
 }
 return reply;
}
}
