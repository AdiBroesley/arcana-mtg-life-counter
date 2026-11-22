#include "FlashStorage.hpp"
#include <cstdio>
#include <cerrno>
#include <sstream>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace mtg {
namespace {
constexpr size_t maximum=2*1024*1024;
std::uint64_t checksum(const std::string& data) {
 std::uint64_t hash=14695981039346656037ULL;
 for(unsigned char byte:data){hash^=byte;hash*=1099511628211ULL;}return hash;
}
enum class Slot { Missing, Invalid, Valid };
Slot read(const std::string& path,std::string& data,std::uint64_t& generation) {
 FILE* file=fopen(path.c_str(),"rb");if(!file)return errno==ENOENT?Slot::Missing:Slot::Invalid;
 char header[128]{};
 if(!fgets(header,sizeof(header),file)){fclose(file);return Slot::Invalid;}
 std::string magic;size_t length=0;std::uint64_t expected=0;
 std::istringstream stream(header);stream>>magic>>generation>>length>>expected;
 if(!stream||magic!="ARCANA1"||length>maximum||!generation){fclose(file);return Slot::Invalid;}
 data.resize(length);bool ok=fread(data.data(),1,length,file)==length;
 ok=ok&&fgetc(file)==EOF&&!ferror(file);fclose(file);
 return ok&&checksum(data)==expected?Slot::Valid:Slot::Invalid;
}
}
bool FlashStorage::load(std::string& data,std::string& error) {
 error.clear();data.clear();generation_=0;current_=1;loaded_=true;
 bool invalid=false;
 for(unsigned slot=0;slot<2;++slot) {
  std::string candidate;std::uint64_t version=0;
  auto state=read(path_+"."+std::to_string(slot),candidate,version);
  invalid|=state==Slot::Invalid;
  if(state==Slot::Valid&&version>generation_){data=std::move(candidate);generation_=version;current_=slot;}
 }
 if(!generation_&&invalid){error="Saved flash data is damaged";return false;}
 return true;
}
bool FlashStorage::save(const std::string& data,std::string& error) {
 error.clear();
 if(!loaded_){std::string old;load(old,error);if(!error.empty())return false;}
 if(data.size()>maximum||generation_==UINT64_MAX){error="Saved game too large";return false;}
 unsigned slot=1-current_;auto next=generation_+1;auto path=path_+"."+std::to_string(slot);
 // Keep the last committed slot intact until its replacement has been verified.
 FILE* file=fopen(path.c_str(),"wb");if(!file){error="Flash save failed";return false;}
 auto header="ARCANA1 "+std::to_string(next)+" "+std::to_string(data.size())+" "+std::to_string(checksum(data))+"\n";
 bool ok=fwrite(header.data(),1,header.size(),file)==header.size()&&fwrite(data.data(),1,data.size(),file)==data.size();
 if(fflush(file)!=0)ok=false;
#ifdef _WIN32
 if(_commit(_fileno(file))!=0)ok=false;
#else
 if(fsync(fileno(file))!=0)ok=false;
#endif
 if(fclose(file)!=0)ok=false;
 std::string verify;std::uint64_t version=0;
 if(!ok||read(path,verify,version)!=Slot::Valid||version!=next||verify!=data){error="Flash save failed";return false;}
 generation_=next;current_=slot;return true;
}
}

