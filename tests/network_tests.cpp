#include "core/Multiplayer.hpp"
#include "LanTransport.hpp"
#include "FileStorage.hpp"
#include "third_party/json.hpp"
#include <map>
#include <deque>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
using namespace mtg;
void check(bool ok,const char* description){if(!ok){std::cerr<<description<<'\n';std::exit(1);}}
class Memory final:public IStorage {public:std::string data;bool fail=false;bool load(std::string& out,std::string&)override{out=data;return true;}bool save(const std::string& value,std::string& error)override{if(fail){error="Disk full";return false;}data=value;return true;}};
struct Bus {std::map<std::string,std::deque<PeerPacket>> queues;bool duplicate=false,dropCommands=false;std::string lastCommand;};
class Wire final:public IPeerTransport {public:Bus& bus;std::string address,empty;Wire(Bus& bus,std::string address):bus(bus),address(address){bus.queues[address];}bool send(const std::string& destination,const std::string& data)override{if(data.find("\"t\":\"cmd\"")!=std::string::npos)bus.lastCommand=data;if(bus.dropCommands&&data.find("\"t\":\"cmd\"")!=std::string::npos)return true;auto deliver=[&](const std::string& target){bus.queues[target].push_back({address,data});if(bus.duplicate)bus.queues[target].push_back({address,data});};if(destination=="discovery"){for(const auto& entry:bus.queues)if(entry.first!=address)deliver(entry.first);}else deliver(destination);return true;}std::vector<PeerPacket> receive()override{std::vector<PeerPacket> result;auto& queue=bus.queues[address];while(!queue.empty()){result.push_back(queue.front());queue.pop_front();}return result;}const std::string& error()const override{return empty;}};
class Radio final:public IPeerTransport {
public:
 Wire wire;bool ready=false,browsing=false,stopped=true;std::string selected;
 std::vector<PeerNetwork> visible;
 Radio(Bus& bus,std::string name):wire(bus,name){}
 bool hostNetwork(const std::string&)override{ready=true;stopped=false;return true;}
 void browseNetworks()override{browsing=true;stopped=false;}
 bool joinNetwork(const std::string& id)override{selected=id;stopped=false;return true;}
 void stopNetwork()override{ready=browsing=false;selected.clear();stopped=true;}
 bool networkReady()const override{return ready;}
 bool radioDiscovery()const override{return true;}
 std::vector<PeerNetwork> networks()const override{return visible;}
 bool send(const std::string& endpoint,const std::string& data)override{return ready&&wire.send(endpoint,data);}
 std::vector<PeerPacket> receive()override{return ready?wire.receive():std::vector<PeerPacket>{};}
 const std::string& error()const override{return wire.error();}
};
void powerNetworkTests(){
 Bus bus;Wire a(bus,"power-host"),b(bus,"power-guest");
 DeviceProfile pa,pb;pa.setName("HOST");pb.setName("GUEST");GameState ga,gb;
 Multiplayer host(ga,pa,a),guest(gb,pb,b);
 check(host.host(GameFormat::Commander,4)&&host.start(),"power test creates a running game");
 guest.discover();host.tick(10);guest.tick(10);check(guest.join(0),"power test joins");
 host.tick(20);guest.tick(20);check(guest.connected(),"power test admitted");
 // Empty the discovery reply and verify that idle traffic no longer runs at 300 ms.
 a.receive();b.receive();host.tick(21);guest.tick(21);a.receive();b.receive();
 for(unsigned now=40;now<1000;now+=20){host.tick(now);guest.tick(now);}
 check(a.receive().empty()&&b.receive().empty(),"idle heartbeat must wait a full second");
 auto revision=guest.revision();host.tick(1020);guest.tick(1020);
 check(guest.revision()==revision,"unchanged heartbeat must not redraw the guest UI");
 ga.adjustLife(-2);host.tick(1021);guest.tick(1021);
 check(guest.opponents()[0].life==38,"host edit broadcasts on its first tick");
 gb.adjustLife(-3);guest.tick(1022);host.tick(1022);guest.tick(1022);
 check(host.opponents()[0].life==37,"guest edit sends on its first tick, even just after a heartbeat");
 gb.adjustLife(-1);guest.tick(1023);host.tick(1023);guest.tick(1023);
 check(host.opponents()[0].life==36,"next acknowledged command sends without waiting for a retry timer");
 bus.dropCommands=true;gb.adjustLife(-4);guest.tick(1024);host.tick(1024);
 bus.dropCommands=false;
 guest.tick(1323);check(a.receive().empty(),"lost edits must not retry before 300 ms");
 guest.tick(1324);host.tick(1324);guest.tick(1324);
 check(host.opponents()[0].life==32&&gb.player().life==32,"lost edit retries after 300 ms and converges");
 host.leave();guest.leave();
}
void radioTests(){
 Bus bus;Radio a(bus,"radio-host"),b(bus,"radio-guest");DeviceProfile pa,pb;pa.setName("ALICE");pb.setName("BOB");GameState ga,gb;Multiplayer host(ga,pa,a),guest(gb,pb,b);
 check(host.host(GameFormat::Commander,4)&&a.ready,"host creates radio network");check(host.start(),"radio host can start with open slots");ga.adjustLife(-3);
 guest.discover();b.visible={{"ARCANA-ALICE-aabbcc","ALICE"}};guest.tick(100);
 check(b.browsing&&guest.lobbies().size()==1,"unconnected radio discovers SSID host names");
 check(guest.join(0)&&guest.attached()&&!guest.connected(),"radio join remains attached while DHCP is pending");
 check(b.selected=="ARCANA-ALICE-aabbcc","join selects the advertised radio network");
 guest.tick(500);check(!guest.connected(),"no gameplay admission before radio connection");
 b.ready=true;
 for(std::uint64_t now=1000;now<4000;now+=100){host.tick(now);guest.tick(now);}
 check(guest.connected()&&guest.started()&&host.players().size()==2,"radio handshake resolves real game ID after DHCP");
 gb.adjustLife(-5);
 for(std::uint64_t now=4000;now<6000;now+=100){host.tick(now);guest.tick(now);}
 check(ga.player().life==37&&gb.player().life==35&&host.opponents()[0].life==35,"late radio join preserves host life and synchronizes edits");
 b.ready=false;guest.tick(6000);gb.adjustLife(-2);guest.tick(6100);
 b.wire.address="radio-guest-reconnected";bus.queues[b.wire.address];b.ready=true;
 for(std::uint64_t now=6200;now<8200;now+=100){host.tick(now);guest.tick(now);}
 check(guest.connected()&&host.players().size()==2&&gb.player().life==33&&host.opponents()[0].life==33,"radio reconnect replaces the endpoint and preserves queued edits without claiming another seat");
 guest.leave();check(b.stopped&&!guest.attached(),"leaving powers off guest radio");
 guest.discover();guest.tick(8300);check(guest.join(0),"radio can reconnect to same SSID");guest.leave();
 b.ready=true;guest.tick(9000);check(!guest.attached()&&!guest.connected(),"cancelled network join cannot admit a stale reply");
 host.leave();check(a.stopped,"leaving powers off host radio");
}
int main(int argc,char** argv){
 powerNetworkTests();radioTests();
 if(argc>1&&(std::string(argv[1])=="--lan-host"||std::string(argv[1])=="--lan-client")){
  bool server=std::string(argv[1])=="--lan-host";LanTransport transport(42690);check(transport.error().empty(),"process LAN socket initialization");DeviceProfile profile;profile.setName(server?"NETTESTHOST":"NETTESTJOIN");GameState game;Multiplayer session(game,profile,transport);if(server){check(session.host(GameFormat::Classic,2),"process host lobby");check(session.start(),"process host starts before client arrives");game.adjustLife(-3);}else session.discover();bool changed=false,wasStarted=false;
  auto beginning=std::chrono::steady_clock::now();
  for(;;){auto elapsed=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-beginning).count());if(elapsed>10000){std::cerr<<"Separate-process network timeout\n";return 1;}session.tick(elapsed);
   if(server){if(!session.started()&&session.players().size()==2)session.start();if(session.started()&&!session.opponents().empty()&&session.opponents()[0].life==15){session.leave();std::cout<<"Separate host verified remote life\n";return 0;}}
   else {if(!session.attached())for(std::size_t i=0;i<session.lobbies().size();++i)if(session.lobbies()[i].name=="NETTESTHOST"){session.join(i);break;}if(session.started()){wasStarted=true;if(!changed){game.adjustLife(-5);changed=true;}}if(wasStarted&&!game.state().active){std::cout<<"Separate client verified host shutdown\n";return 0;}}
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
 }
 if(argc>1&&std::string(argv[1])=="--lan"){
  LanTransport a(42689),b(42689);check(a.error().empty()&&b.error().empty(),"LAN sockets and multicast initialize");DeviceProfile pa,pb;pa.setName("ALICE");pb.setName("BOB");GameState ga,gb;Multiplayer host(ga,pa,a),guest(gb,pb,b);check(host.host(GameFormat::Classic,2),"LAN host starts");check(host.start(),"LAN host starts with open slots");ga.adjustLife(-3);guest.discover();std::uint64_t now=0;
  auto pump=[&](int cycles){for(int i=0;i<cycles;++i){now+=20;host.tick(now);guest.tick(now);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
  pump(60);check(!guest.lobbies().empty(),"real UDP discovers hosted game");check(guest.join(0),"real UDP joins discovered game");pump(60);check(guest.connected()&&host.players().size()==2,"real UDP admission and roster");check(host.start(),"LAN host starts match");pump(60);gb.adjustLife(-5);pump(80);check(ga.player().life==17&&gb.player().life==15&&host.opponents()[0].life==15,"real UDP late join preserves running host life");host.leave();pump(10);check(!gb.state().active,"real UDP host end reaches client");std::cout<<"Real LAN discovery, join and life sync passed\n";return 0;
 }
 Memory memory;DeviceProfile profile(&memory);check(profile.load()&&profile.name().empty(),"empty identity profile");check(profile.setName(" ALICE "),"trim and persist name");auto identity=profile.id();DeviceProfile rebooted(&memory);check(rebooted.load()&&rebooted.id()==identity&&rebooted.name()=="ALICE","name and device ID survive restart");memory.fail=true;check(!profile.setName("BOB")&&profile.name()=="ALICE","failed save keeps old name");memory.fail=false;check(!profile.setName("")&&!profile.setName("NAME-TOO-LONG-123")&&!profile.setName("<bad>"),"invalid names rejected");
 // Appearance settings upgrade existing v1 profiles, including before a name is entered.
 Memory appearance;DeviceProfile settings(&appearance);check(settings.setFaceDesign(7)&&settings.setBrightness(35),"save appearance without a player name");
 DeviceProfile restored(&appearance);check(restored.load()&&restored.faceDesign()==7&&restored.brightness()==35&&restored.name().empty()&&restored.id()==settings.id(),"appearance survives restart before naming");
 check(restored.setName("MICHAEL"),"name save preserves appearance");DeviceProfile renamed(&appearance);check(renamed.load()&&renamed.faceDesign()==7&&renamed.brightness()==35,"name and appearance coexist");
 appearance.fail=true;check(!restored.setFaceDesign(8)&&!restored.setBrightness(42)&&restored.faceDesign()==7&&restored.brightness()==35,"failed appearance saves retain previous settings");appearance.fail=false;
 check(!restored.setFaceDesign(faceDesignCount)&&!restored.setBrightness(101),"appearance rejects invalid ranges");
 auto old=nlohmann::json::parse(appearance.data);old.erase("face");old.erase("brightness");appearance.data=old.dump();DeviceProfile legacy(&appearance);check(legacy.load()&&legacy.name()=="MICHAEL"&&legacy.faceDesign()==0&&legacy.brightness()==100,"old firmware profile upgrades with original face and default brightness");
 old["face"]=-1;old["brightness"]=1000;appearance.data=old.dump();DeviceProfile invalid(&appearance);check(invalid.load()&&invalid.id()==restored.id()&&invalid.faceDesign()==0&&invalid.brightness()==100,"invalid optional settings do not discard identity");
 check(invalid.setBrightness(0),"zero brightness is valid");DeviceProfile dark(&appearance);check(dark.load()&&dark.brightness()==0,"zero brightness survives restart");
 Memory policyMemory;DeviceProfile policy(&policyMemory);
 check(policy.displaySettings()==DisplaySettings{}&&policy.radio()==PeerRadio::Bluetooth,"new profile uses five-second dim, both toggles off and experimental BLE");
 DisplaySettings display;display.lifeOnly=true;display.wakeProtection=true;display.dimAfterSeconds=27;display.idleBrightness=4;
 check(policy.setDisplaySettings(display)&&policy.setRadio(PeerRadio::Wifi)&&policy.setName("SAVED")&&policy.setBrightness(45),"display, connection, name and brightness save together");
 DeviceProfile policyReboot(&policyMemory);check(policyReboot.load()&&policyReboot.displaySettings()==display&&policyReboot.radio()==PeerRadio::Wifi&&policyReboot.brightness()==45,"display and radio survive restart");
 policyMemory.fail=true;auto invalidDisplay=display;invalidDisplay.dimAfterSeconds=301;
 check(!policy.setDisplaySettings(invalidDisplay)&&!policy.setDisplaySettings(DisplaySettings{})&&!policy.setRadio(PeerRadio::Bluetooth)&&policy.displaySettings()==display&&policy.radio()==PeerRadio::Wifi,"invalid or failed saves preserve all display and connection settings");policyMemory.fail=false;
 auto legacyPolicy=nlohmann::json::parse(policyMemory.data);legacyPolicy.erase("display");legacyPolicy.erase("radio");policyMemory.data=legacyPolicy.dump();DeviceProfile migrated(&policyMemory);
 check(migrated.load()&&migrated.name()=="SAVED"&&migrated.displaySettings()==DisplaySettings{}&&migrated.radio()==PeerRadio::Bluetooth,"legacy identity migrates safely with requested defaults");
 legacyPolicy["display"]={{"dim_after",-1},{"idle_brightness",101},{"life_only","yes"},{"wake_protection",1}};legacyPolicy["radio"]=10;policyMemory.data=legacyPolicy.dump();DeviceProfile badPolicy(&policyMemory);
 check(badPolicy.load()&&badPolicy.name()=="SAVED"&&badPolicy.displaySettings()==DisplaySettings{}&&badPolicy.radio()==PeerRadio::Bluetooth,"malformed optional settings keep identity and use safe defaults");
 Bus bus;Wire wa(bus,"host"),wb(bus,"guest"),wc(bus,"third");DeviceProfile pa,pb,pc;pa.setName("ALICE");pb.setName("BOB");pc.setName("CAROL");GameState ga,gb,gc;Multiplayer host(ga,pa,wa),guest(gb,pb,wb),third(gc,pc,wc);std::uint64_t now=0;
 auto pump=[&](int cycles){for(int i=0;i<cycles;++i){now+=20;host.tick(now);guest.tick(now);third.tick(now);}};
 check(host.host(GameFormat::Classic,2),"create host lobby");guest.discover();third.discover();pump(10);check(guest.lobbies().size()==1&&guest.lobbies()[0].name=="ALICE","discovery shows host name");guest.join(0);pump(20);check(guest.connected()&&!guest.started()&&host.players().size()==2,"join requires no password");third.join(0);pump(20);check(!third.connected()&&third.error()=="Game is full","capacity enforced");check(host.start(),"host starts admitted players");pump(20);check(guest.started()&&gb.player().life==20,"shared format starts at correct life");
 bus.duplicate=true;gb.adjustLife(-5);pump(60);check(gb.player().life==15&&host.opponents()[0].life==15&&ga.player().life==20,"duplicate commands do not duplicate life changes");wb.send("host",bus.lastCommand);pump(20);check(host.opponents()[0].life==15,"replayed command ignored");ga.adjustLife(3);pump(30);check(guest.opponents()[0].life==23,"host life reaches guest");gb.adjustCounter(Counter::Poison,2);pump(40);check(host.opponents()[0].counters[0]==2,"counter values synchronize");gb.undo();pump(40);check(host.opponents()[0].counters[0]==0,"local Undo sends synchronized correction");
 bus.dropCommands=true;for(int i=0;i<200;++i){gb.adjustLife(-1);pump(1);}check(gb.player().life==-185&&host.opponents()[0].life==15,"edits stay local while packets are lost");bus.dropCommands=false;pump(100);check(gb.player().life==-185&&host.opponents()[0].life==-185,"rapid burst is coalesced and delivered after packet loss");gb.adjustLife(200);pump(80);
 wc.send("host","{broken");wc.send("host",std::string(17000,'x'));wc.send("host",nlohmann::json{{"v",1},{"t","cmd"},{"game",guest.lobbies()[0].id},{"id",pb.id()},{"seq",99},{"life",-100},{"counters",std::array<int,8>{}}}.dump());pump(10);check(host.opponents()[0].life==15,"malformed packets and another endpoint cannot edit a player");host.leave();pump(10);check(!guest.connected()&&!gb.state().active,"host end closes guest match");guest.leave();third.leave();bus.duplicate=false;
 Wire wd(bus,"fourth");DeviceProfile pd;pd.setName("DAVE");GameState gd;Multiplayer fourth(gd,pd,wd);
 auto all=[&](int cycles){for(int i=0;i<cycles;++i){now+=20;host.tick(now);guest.tick(now);third.tick(now);fourth.tick(now);}};
 check(host.host(GameFormat::TwoHeadedGiant,4),"host team game");guest.discover();third.discover();fourth.discover();all(20);
 auto joinHost=[&](Multiplayer& client){for(std::size_t i=0;i<client.lobbies().size();++i)if(client.lobbies()[i].name=="ALICE"&&client.lobbies()[i].format==GameFormat::TwoHeadedGiant){client.join(i);return;}check(false,"new team lobby found");};
 joinHost(guest);all(20);check(host.start(),"team game starts with two of four devices");all(20);ga.adjustLife(-3);check(host.adjustOpenSeat(3,-7),"host tracks an unclaimed team seat");all(30);check(ga.player().life==37&&gb.player().life==37&&host.opponents()[1].life==33,"partial-game edits preserve both teams");
 joinHost(third);all(30);check(third.started()&&gc.player().life==33&&ga.player().life==37&&gb.player().life==37,"late device inherits slot life without resetting existing players");joinHost(fourth);all(30);check(gd.player().life==33&&host.players().size()==4&&host.opponents()[1].name=="CAROL","late joins replace the free seats with registered names");gc.adjustLife(7);ga.adjustLife(3);all(60);
 gb.adjustLife(-5);all(40);check(ga.player().life==35&&gb.player().life==35&&gc.player().life==40&&gd.player().life==40,"teammate life shared without changing other team");gc.adjustLife(-4);all(40);check(gc.player().life==36&&gd.player().life==36&&ga.player().life==35,"other team shares independent life");gb.adjustCounter(Counter::Poison,1);all(40);check(ga.player().counters[0]==1&&gb.player().counters[0]==1&&gc.player().counters[0]==0,"poison shared by team only");ga.adjustLife(-2);gb.adjustLife(-3);all(80);check(ga.player().life==30&&gb.player().life==30,"simultaneous teammate edits converge without lost updates");
 auto before=gb.player().life;now+=7000;guest.tick(now);check(!guest.error().empty()&&gb.player().life==before,"host timeout preserves local stats");all(40);check(guest.error().empty(),"heartbeat recovery clears timeout");host.leave();all(10);
 FileStorage disk(std::filesystem::current_path()/"network-profile-test.json");DeviceProfile saved(&disk);check(saved.setName("MICHAEL"),"profile saves atomically on disk");DeviceProfile loaded(&disk);check(loaded.load()&&loaded.name()=="MICHAEL"&&loaded.id()==saved.id(),"profile survives a new reader");std::filesystem::remove(std::filesystem::current_path()/"network-profile-test.json");
 std::cout<<"Identity, discovery, admission, replay handling, team sync and recovery checks passed\n";
}
