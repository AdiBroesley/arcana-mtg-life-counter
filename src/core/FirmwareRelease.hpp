#pragma once
#include "AppInfo.hpp"
#include "third_party/json.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace mtg {
inline std::array<unsigned,3> releaseVersion(std::string text){
 if(!text.empty()&&text[0]=='v')text.erase(0,1);
 std::array<unsigned,3> result{};std::size_t offset=0;
 for(unsigned part=0;part<3;++part){
  auto end=text.find('.',offset);if(part==2)end=text.size();
  if(end==std::string::npos||end==offset||end-offset>6)throw std::runtime_error("Invalid release version");
  auto value=text.substr(offset,end-offset);if(value.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid release version");
  result[part]=static_cast<unsigned>(std::stoul(value));offset=end+1;
 }
 return result;
}
struct FirmwareRelease {
 std::string version,url,sha256;std::uint32_t bytes=0;
 bool newerThan(const std::string& current)const{return releaseVersion(version)>releaseVersion(current);}
 static FirmwareRelease parse(const std::string& body,std::uint32_t capacity){
  auto j=nlohmann::json::parse(body);
  if(j.value("draft",true)||j.value("prerelease",true))throw std::runtime_error("No stable release");
  FirmwareRelease release;auto tag=j.at("tag_name").get<std::string>();releaseVersion(tag);
  release.version=tag[0]=='v'?tag.substr(1):tag;
  for(const auto& asset:j.at("assets"))if(asset.at("name")==appInfo::firmwareAsset){
   release.url=asset.at("browser_download_url").get<std::string>();
   auto prefix=std::string(appInfo::repositoryUrl)+"/releases/download/"+tag+"/";
   if(release.url!=prefix+appInfo::firmwareAsset)throw std::runtime_error("Unexpected firmware URL");
   auto bytes=asset.at("size").get<std::uint64_t>();if(bytes<256||bytes>capacity)throw std::runtime_error("Firmware does not fit this device");release.bytes=static_cast<std::uint32_t>(bytes);
   auto digest=asset.value("digest",std::string{});
   if(digest.size()!=71||digest.compare(0,7,"sha256:")||digest.substr(7).find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Release has no valid SHA-256 digest");
   release.sha256=digest.substr(7);return release;
  }
  throw std::runtime_error("No firmware for this ARCANA model");
 }
};
}
