#pragma once
#include <string>
#include <vector>
namespace mtg {
struct PeerPacket {std::string endpoint,data;};
struct PeerNetwork {std::string id,name;};
class IPeerTransport {
public:virtual ~IPeerTransport()=default;virtual bool send(const std::string& endpoint,const std::string& data)=0;virtual std::vector<PeerPacket> receive()=0;virtual const std::string& error()const=0;
 // Desktop transports already share a LAN. Radios can establish one on demand.
 virtual bool hostNetwork(const std::string&){return true;}
 virtual void browseNetworks(){}
 virtual bool joinNetwork(const std::string&){return false;}
 virtual void stopNetwork(){}
 virtual void pollNetwork(){}
 virtual bool radioDiscovery()const{return false;}
 virtual bool networkReady()const{return true;}
 virtual std::vector<PeerNetwork> networks()const{return {};}
};
}
