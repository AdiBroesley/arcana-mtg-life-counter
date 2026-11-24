#pragma once
#include "platform/IPeerTransport.hpp"
#include <memory>
namespace mtg {
// Experimental BLE GATT transport. No access point, router or phone required.
class BluetoothTransport final : public IPeerTransport {
public:
 BluetoothTransport();~BluetoothTransport();
 bool hostNetwork(const std::string&)override;
 void browseNetworks()override;
 bool joinNetwork(const std::string&)override;
 void stopNetwork()override;
 void pollNetwork()override;
 bool radioDiscovery()const override{return true;}
 bool networkReady()const override;
 std::vector<PeerNetwork> networks()const override;
 bool send(const std::string&,const std::string&)override;
 std::vector<PeerPacket> receive()override;
 const std::string& error()const override;
 const char* description()const override{return "Bluetooth (test)";}
 void diagnose()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
