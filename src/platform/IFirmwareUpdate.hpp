#pragma once
#include <cstdint>
#include <string>
namespace mtg {
enum class UpdatePhase { Offline, Connecting, Checking, Current, Available, Installing, Ready, Setup, Error };
struct UpdateStatus {
 UpdatePhase phase=UpdatePhase::Offline;
 std::string latest,message="Connect Wi-Fi in About",setupNetwork,setupPassword;
 unsigned progress=0;
 std::uint64_t revision=0;
 bool busy()const{return phase==UpdatePhase::Connecting||phase==UpdatePhase::Checking||phase==UpdatePhase::Installing;}
};
class IFirmwareUpdate {
public:
 virtual ~IFirmwareUpdate()=default;
 virtual UpdateStatus status()const=0;
 virtual void check()=0;
 virtual void install()=0;
 virtual void setup()=0;
 virtual void cancel()=0;
};
}
