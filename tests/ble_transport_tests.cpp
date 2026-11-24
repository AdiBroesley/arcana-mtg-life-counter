#include "platform/BleFraming.hpp"
#include "platform/RadioTransport.hpp"
#include "core/Multiplayer.hpp"
#include <map>
#include <deque>
#include <stdexcept>
#include <iostream>
using namespace mtg;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Bus {std::map<std::string,std::deque<PeerPacket>> packets;bool drop=false;unsigned mtu=244;};
class FramedWire final:public IPeerTransport {
public:
 Bus& bus;std::string address,empty;uint16_t sequence=0;bool stopped=true;
 std::map<std::string,ble::Assembler> assemblers;
 FramedWire(Bus& b,std::string a):bus(b),address(std::move(a)){bus.packets[address];}
 const char* description()const override{return "BLE test wire";}
 bool hostNetwork(const std::string&)override{stopped=false;return true;}
 void browseNetworks()override{stopped=false;}
 void stopNetwork()override{stopped=true;assemblers.clear();}
 bool send(const std::string& endpoint,const std::string& data)override{
  ++sequence;auto deliver=[&](const std::string& destination){
   // Emulate GATT chunks with an optional lost middle fragment, per link.
   ble::Assembler receiver;size_t offset=0;unsigned index=0;
   while(offset<data.size()){auto frame=ble::fragment(data,sequence,offset,bus.mtu);if(frame.empty())return false;offset+=frame.size()-ble::headerSize;
    if(bus.drop&&index++==1)continue;
    if(auto complete=receiver.receive(frame,10))bus.packets[destination].push_back({address,*complete});
   }return true;
  };
  if(endpoint=="discovery"){for(auto& [target,queue]:bus.packets)if(target!=address)deliver(target);return true;}return deliver(endpoint);
 }
 std::vector<PeerPacket> receive()override{auto& queue=bus.packets[address];std::vector<PeerPacket> result;while(!queue.empty()){result.push_back(std::move(queue.front()));queue.pop_front();}return result;}
 const std::string& error()const override{return empty;}
};
void framing(){
 for(unsigned mtu:{20u,64u,244u,514u})for(unsigned length:{1u,8u,232u,1000u,8192u}){
  std::string message(length,'a');message.front()='\0';message.back()='z';ble::Assembler assembler;size_t offset=0;unsigned completions=0;
  while(offset<message.size()){auto frame=ble::fragment(message,65535,offset,mtu);check(!frame.empty()&&frame.size()<=mtu,"Frame respects negotiated MTU");offset+=frame.size()-ble::headerSize;
   if(auto result=assembler.receive(frame,100)){check(*result==message&&offset==message.size(),"Reassembly preserves binary payload and message boundaries");++completions;}}
  check(completions==1&&!assembler.pending(),"Exactly one complete packet per message");
 }
 check(ble::fragment(std::string(8193,'x'),1,0,244).empty()&&ble::fragment("x",1,0,12).empty(),"Oversize messages and impossible MTUs rejected");
 auto first=ble::fragment(std::string(40,'x'),10,0,20),second=ble::fragment(std::string(40,'x'),10,8,20),last=ble::fragment(std::string(40,'x'),10,32,20);
 ble::Assembler a;a.receive(first,0);check(a.pending()==8,"First fragment retained");check(!a.receive(last,1)&&!a.pending(),"Missing middle fragment discards partial message");
 a.receive(first,0);check(!a.receive(second,2001)&&!a.pending(),"Stale fragments cannot complete after timeout");
 a.receive(first,0);auto bad=second;bad[4]=11;check(!a.receive(bad,1)&&!a.pending(),"Different message sequence cannot splice payloads");
 a.receive(first,0);bad=second;bad[10]=99;check(!a.receive(bad,1)&&!a.pending(),"Invalid lengths rejected before appending");
 a.receive(first,0);a.receive(first,1);a.receive(second,2);check(a.pending()==16,"A retry starting at offset zero restarts a packet safely");
 ble::Assembler b;b.receive(ble::fragment(std::string(40,'y'),10,0,20),0);check(b.pending()==8&&a.pending()==16,"Independent connections keep separate reassembly state");
 auto complete=a.receive(ble::fragment("fresh",0,0,244),5);check(complete&&*complete=="fresh"&&!a.pending(),"Sequence wrap and new packet recover after interrupted message");
}
void radioAndSync(){
 Bus bus;FramedWire hostBle(bus,"host"),guestBle(bus,"guest"),lateBle(bus,"late"),hostWifi(bus,"wifi-host"),guestWifi(bus,"wifi-guest");
 RadioTransport hostRadio(hostWifi,hostBle),guestRadio(guestWifi,guestBle);
 DeviceProfile hp,gp,lp;hp.setName("ALICE");gp.setName("BOB");lp.setName("CAROL");GameState hg,gg,lg;
 Multiplayer host(hg,hp,hostRadio),guest(gg,gp,guestRadio),late(lg,lp,lateBle);
 check(hostRadio.radio()==PeerRadio::Bluetooth,"BLE default with explicit Wi-Fi fallback");
 check(host.host(GameFormat::Commander,4)&&host.start(),"Host starts with unoccupied player slots");
 check(!host.selectRadio(PeerRadio::Wifi),"Cannot swap a live match to another radio");hg.adjustLife(-7);
 guest.discover();host.tick(1);guest.tick(1);check(guest.join(0),"Discover and join over fragmented BLE packets");
 for(unsigned t=20;t<200;t+=20){host.tick(t);guest.tick(t);}
 check(guest.connected()&&guest.started()&&host.opponents()[0].life==40&&guest.opponents()[0].life==33,"Late admission preserves host state and initializes guest");
 bus.mtu=20;bus.drop=true;gg.adjustLife(-6);guest.tick(200);host.tick(200);check(host.opponents()[0].life==40,"Lost BLE fragment never produces a partial life edit");
 bus.drop=false;for(unsigned t=220;t<1000;t+=20){host.tick(t);guest.tick(t);}
 check(host.opponents()[0].life==34&&gg.player().life==34,"Existing command retry recovers lost fragmented edit exactly once at minimum MTU");
 late.discover();host.tick(1000);late.tick(1000);check(late.join(0),"Another knob can join an already running game");
 for(unsigned t=1020;t<1300;t+=20){host.tick(t);guest.tick(t);late.tick(t);}
 check(host.players().size()==3&&guest.players().size()==3&&late.connected()&&late.opponents()[0].life==33,"Late join updates the shared roster and preserves edited life");
 guest.leave();host.leave();late.leave();check(hostBle.stopped&&guestBle.stopped,"Leaving shuts down selected Bluetooth transport");
 check(host.selectRadio(PeerRadio::Wifi)&&guest.selectRadio(PeerRadio::Wifi),"Wi-Fi can be selected after leaving");
 check(host.host(GameFormat::Classic,2)&&host.start()&&!hostWifi.stopped&&hostBle.stopped,"Wi-Fi fallback uses original transport without starting BLE");
 guest.discover();host.tick(1400);guest.tick(1400);check(guest.join(0),"Wi-Fi fallback still discovers games");
 for(unsigned t=1420;t<1600;t+=20){host.tick(t);guest.tick(t);}
 check(guest.connected()&&gg.player().life==20,"Wi-Fi fallback retains normal game admission");host.leave();guest.leave();
}
int main(){try{framing();radioAndSync();std::cout<<"BLE framing, lost packet recovery, late joins and Wi-Fi fallback passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
