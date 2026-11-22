#pragma once
#include "platform/IStorage.hpp"
#include <cstdint>
namespace mtg {
class FlashStorage final : public IStorage {
public:
 explicit FlashStorage(std::string path):path_(std::move(path)){}
 bool load(std::string& data,std::string& error) override;
 bool save(const std::string& data,std::string& error) override;
private:
 std::string path_;
 std::uint64_t generation_=0;
 unsigned current_=1;
 bool loaded_=false;
};
}

