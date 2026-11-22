#pragma once
#include "platform/IPeerTransport.hpp"
#include <memory>
namespace mtg {
// An on-demand, passwordless network hosted by an ARCANA; no router required.
class WifiTransport final:public IPeerTransport {
public:
 WifiTransport();~WifiTransport();
 bool hostNetwork(const std::string& name)override;
 void browseNetworks()override;
 bool joinNetwork(const std::string& id)override;
 void stopNetwork()override;void pollNetwork()override;
 bool radioDiscovery()const override{return true;}
 bool networkReady()const override;
 std::vector<PeerNetwork> networks()const override;
 bool send(const std::string& endpoint,const std::string& data)override;
 std::vector<PeerPacket> receive()override;
 const std::string& error()const override;
 bool hosting()const;
 bool joinInternet(const std::string& ssid,const std::string& password);
 bool setupNetwork(const std::string& ssid,const std::string& password);
 bool updateNetwork()const;
 bool internetReady()const;
 void diagnose()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}

