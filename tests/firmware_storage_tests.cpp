#include "FlashStorage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() {
 try {
  // CTest runs in build; never use the device's real files or user's saved session.
  fs::path directory=fs::current_path()/"firmware-storage-test";
  fs::create_directories(directory);auto base=(directory/"session").string();
  for(unsigned i=0;i<2;++i)fs::remove(base+"."+std::to_string(i));
  std::string data,error;mtg::FlashStorage storage(base);
  require(storage.load(data,error)&&data.empty(),"Missing save must load empty");
  require(storage.save("first match",error),"First save failed");
  require(storage.save("second match",error),"Second save failed");
  mtg::FlashStorage reloaded(base);
  require(reloaded.load(data,error)&&data=="second match","Newest committed slot not selected");
  // Power cut partway through replacing the older slot must retain last commit.
  std::ofstream(base+".0",std::ios::binary|std::ios::trunc)<<"ARCANA1 3 100 123\npartial";
  mtg::FlashStorage interrupted(base);
  require(interrupted.load(data,error)&&data=="second match","Interrupted save lost previous commit");
  require(interrupted.save("third match",error),"Recovery write failed");
  mtg::FlashStorage recovered(base);
  require(recovered.load(data,error)&&data=="third match","Recovered save did not reload");
  // A complete-length corrupted payload is rejected by its checksum.
  {std::fstream file(base+".0",std::ios::in|std::ios::out|std::ios::binary);file.seekp(-1,std::ios::end);file.put('!');}
  mtg::FlashStorage corrupted(base);
  require(corrupted.load(data,error)&&data=="second match","Corrupt payload did not fall back");
  std::ofstream(base+".1",std::ios::trunc)<<"broken";
  mtg::FlashStorage bothBroken(base);
  require(!bothBroken.load(data,error)&&!error.empty(),"Both damaged slots must report an error");
  require(!storage.save(std::string(2*1024*1024+1,'x'),error),"Oversized save accepted");
  for(unsigned i=0;i<2;++i)fs::remove(base+"."+std::to_string(i));
  fs::remove(directory);std::cout<<"Firmware storage recovery tests passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
