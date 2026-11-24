#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace mtg::ble {
inline constexpr std::size_t headerSize=12,maxMessage=8192;
inline std::uint16_t read16(const char* p){return static_cast<unsigned char>(p[0])|(static_cast<unsigned char>(p[1])<<8);}
inline void write16(char* p,std::uint16_t value){p[0]=static_cast<char>(value);p[1]=static_cast<char>(value>>8);}
inline std::string fragment(const std::string& data,std::uint16_t sequence,std::size_t offset,std::size_t payload){
 if(data.empty()||data.size()>maxMessage||offset>=data.size()||payload<=headerSize)return {};
 auto count=std::min(data.size()-offset,payload-headerSize);std::string frame(headerSize+count,'\0');
 frame[0]='A';frame[1]='B';frame[2]=1;write16(&frame[4],sequence);write16(&frame[6],static_cast<std::uint16_t>(data.size()));write16(&frame[8],static_cast<std::uint16_t>(offset));write16(&frame[10],static_cast<std::uint16_t>(count));frame.replace(headerSize,count,data,offset,count);return frame;
}
class Assembler {
public:
 std::optional<std::string> receive(const std::string& frame,std::uint64_t now){
  if(!data_.empty()&&now-updated_>2000)reset();
  if(frame.size()<headerSize||frame[0]!='A'||frame[1]!='B'||frame[2]!=1||frame[3]!=0){reset();return {};}
  auto seq=read16(frame.data()+4),total=read16(frame.data()+6),offset=read16(frame.data()+8),count=read16(frame.data()+10);
  if(!total||total>maxMessage||!count||frame.size()!=headerSize+count||offset>=total||count>total-offset){reset();return {};}
  if(offset==0){data_.clear();sequence_=seq;total_=total;}
  if(seq!=sequence_||total!=total_||offset!=data_.size()){reset();return {};}
  data_.append(frame,headerSize,count);updated_=now;
  if(data_.size()==total_){auto result=std::move(data_);reset();return result;}return {};
 }
 void reset(){data_.clear();total_=0;sequence_=0;updated_=0;}
 std::size_t pending()const{return data_.size();}
private:std::string data_;std::uint16_t sequence_=0,total_=0;std::uint64_t updated_=0;
};
}
