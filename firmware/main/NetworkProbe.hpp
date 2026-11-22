#pragma once
#include "Wifi.hpp"
#include "core/Multiplayer.hpp"
#include "esp_timer.h"
#include <cstdio>
// USB-only diagnostics use an independent RAM profile/match. They cannot rename
// the device, overwrite a match, or write flash. Q returns to the normal UI.
class NetworkProbe {
public:
 NetworkProbe(mtg::WifiTransport& radio,mtg::GameState& liveGame,mtg::Multiplayer& liveSession):radio_(radio),liveGame_(liveGame),liveSession_(liveSession),session_(game_,profile_,radio){}
 bool active()const{return active_;}
 void tick(std::uint64_t now){session_.tick(now);if(now-startedAt_>120000)command('Q');}
 void command(char input){
  if(input=='N'){status();return;}
  if(input=='Q'){if(active_)session_.leave();active_=false;printf("NETTEST stopped\n");return;}
  if(!active_){if(liveGame_.state().active||liveSession_.attached()){printf("NETTEST busy: end the current match first\n");return;}active_=true;startedAt_=esp_timer_get_time()/1000;}
  bool ok=true;
  if(input=='H'){profile_.setName("USBHOST");ok=session_.host(mtg::GameFormat::Commander,4);}
  else if(input=='J'){profile_.setName("USBJOIN");session_.discover();}
  else if(input=='K'){ok=false;for(std::size_t i=0;i<session_.lobbies().size();++i)if(session_.lobbies()[i].name=="USBHOST"){ok=session_.join(i);break;}}
  else if(input=='S')ok=session_.start();
  else if(input=='A')ok=game_.adjustLife(1);
  else if(input=='D')ok=game_.adjustLife(-1);
  printf("NETTEST command=%c ok=%u error=%s\n",input,ok?1u:0u,session_.error().c_str());fflush(stdout);
 }
private:
 void status(){
  radio_.diagnose();printf("NETTEST active=%u attached=%u connected=%u started=%u players=%u lobbies=%u life=%lld error=%s\n",active_?1u:0u,session_.attached()?1u:0u,session_.connected()?1u:0u,session_.started()?1u:0u,static_cast<unsigned>(session_.players().size()),static_cast<unsigned>(session_.lobbies().size()),static_cast<long long>(game_.player().life),session_.error().c_str());
  for(const auto& lobby:session_.lobbies())printf("NETTEST lobby name=%s network=%s\n",lobby.name.c_str(),lobby.network.c_str());
  for(const auto& player:session_.players())printf("NETTEST player name=%s seat=%u life=%lld online=%u\n",player.name.c_str(),player.seat,static_cast<long long>(player.life),player.online?1u:0u);fflush(stdout);
 }
 mtg::WifiTransport& radio_;mtg::GameState& liveGame_;mtg::Multiplayer& liveSession_;
 mtg::GameState game_;mtg::DeviceProfile profile_;mtg::Multiplayer session_;
 bool active_=false;std::uint64_t startedAt_=0;
};
