#pragma once
#include "IPeerTransport.hpp"
namespace mtg {
// One explicit multiplayer radio at a time. Wi-Fi itself remains available to OTA.
class RadioTransport final:public IPeerTransport {
public:
 RadioTransport(IPeerTransport& wifi,IPeerTransport& bluetooth):wifi_(wifi),bluetooth_(bluetooth){}
 bool selectRadio(PeerRadio radio)override{
  if(static_cast<unsigned>(radio)>1||(active_&&radio!=radio_)){error_="Leave the game before changing connection";return false;}
  radio_=radio;error_.clear();return true;
 }
 const char* description()const override{return current().description();}
 bool hostNetwork(const std::string& name)override{active_=true;return current().hostNetwork(name);}
 void browseNetworks()override{active_=true;current().browseNetworks();}
 bool joinNetwork(const std::string& id)override{active_=true;return current().joinNetwork(id);}
 void stopNetwork()override{current().stopNetwork();active_=false;error_.clear();}
 void pollNetwork()override{current().pollNetwork();}
 bool radioDiscovery()const override{return current().radioDiscovery();}
 bool networkReady()const override{return current().networkReady();}
 std::vector<PeerNetwork> networks()const override{return current().networks();}
 bool send(const std::string& endpoint,const std::string& data)override{return current().send(endpoint,data);}
 std::vector<PeerPacket> receive()override{return current().receive();}
 const std::string& error()const override{return error_.empty()?current().error():error_;}
 PeerRadio radio()const{return radio_;}
private:
 IPeerTransport& current()const{return radio_==PeerRadio::Bluetooth?bluetooth_:wifi_;}
 IPeerTransport& wifi_;IPeerTransport& bluetooth_;PeerRadio radio_=PeerRadio::Bluetooth;bool active_=false;std::string error_;
};
}
