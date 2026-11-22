#include "Wifi.hpp"
#include "LanTransport.hpp"
#include "core/DeviceProfile.hpp"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdio>
namespace mtg {
namespace {
constexpr const char* tag="ARCANA_WIFI";
constexpr const char* prefix="ARCANA-";
std::uint64_t millis(){return esp_timer_get_time()/1000;}
std::string hostName(const std::string& ssid){
 if(ssid.size()<15||ssid.compare(0,7,prefix)||ssid[ssid.size()-7]!='-')return {};
 auto suffix=ssid.substr(ssid.size()-6);
 if(suffix.find_first_not_of("0123456789abcdef")!=std::string::npos)return {};
 auto name=ssid.substr(7,ssid.size()-14);return DeviceProfile::validName(name)?name:std::string{};
}
}
struct WifiTransport::Impl {
 enum Mode {Off,Browse,Joining,Host,Internet,Setup};Mode mode=Off;
 bool initialized=false,running=false,scanBusy=false;
 std::atomic<bool> scanDone{false},stationConnected{false},connectRequested{false};
 esp_netif_t* station=nullptr;esp_netif_t* accessPoint=nullptr;
 esp_event_handler_instance_t wifiHandler{},ipHandler{};
 std::string error,ssid,ip;std::vector<PeerNetwork> found;
 std::unique_ptr<LanTransport> lan;
 std::uint64_t nextScan=0,nextConnect=0;
 static void event(void* context,esp_event_base_t base,int32_t id,void*){
  auto& self=*static_cast<Impl*>(context);
  if(base==WIFI_EVENT&&id==WIFI_EVENT_SCAN_DONE)self.scanDone=true;
  if(base==WIFI_EVENT&&(id==WIFI_EVENT_STA_DISCONNECTED||id==WIFI_EVENT_STA_STOP)){self.stationConnected=false;self.connectRequested=true;}
  if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP)self.stationConnected=true;
  if(base==IP_EVENT&&id==IP_EVENT_STA_LOST_IP)self.stationConnected=false;
 }
 bool check(esp_err_t result,const char* operation){
  if(result==ESP_OK)return true;
  ESP_LOGE(tag,"%s: %s",operation,esp_err_to_name(result));error="Wi-Fi unavailable";return false;
 }
 bool initialize(){
  if(initialized)return true;
  if(!check(esp_netif_init(),"network init"))return false;
  auto loop=esp_event_loop_create_default();if(loop!=ESP_ERR_INVALID_STATE&&!check(loop,"event loop"))return false;
  station=esp_netif_create_default_wifi_sta();accessPoint=esp_netif_create_default_wifi_ap();
  if(!station||!accessPoint){error="Wi-Fi unavailable";return false;}
  wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
  if(!check(esp_wifi_init(&config),"radio init"))return false;
  if(!check(esp_wifi_set_storage(WIFI_STORAGE_RAM),"RAM config")||
     !check(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,this,&wifiHandler),"Wi-Fi events")||
     !check(esp_event_handler_instance_register(IP_EVENT,ESP_EVENT_ANY_ID,event,this,&ipHandler),"IP events"))return false;
  initialized=true;return true;
 }
 bool setupDns(bool enabled){
  // Called with the AP stopped. Do not leave setup DNS advertised by later
  // multiplayer hosts using the same persistent network interface.
  auto stopped=esp_netif_dhcps_stop(accessPoint);
  if(stopped!=ESP_OK&&stopped!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)return check(stopped,"stop setup DHCP");
  esp_netif_ip_info_t info{};esp_netif_dns_info_t dns{};dns.ip.type=ESP_IPADDR_TYPE_V4;
  if(enabled&&esp_netif_get_ip_info(accessPoint,&info)!=ESP_OK)return false;
  dns.ip.u_addr.ip4.addr=enabled?info.ip.addr:0;
  uint8_t offer=enabled?1:0;
  bool configured=check(esp_netif_set_dns_info(accessPoint,ESP_NETIF_DNS_MAIN,&dns),"setup DNS address")&&
   check(esp_netif_dhcps_option(accessPoint,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer,sizeof(offer)),"setup DNS offer");
  // Explicit stop puts DHCP in STOPPED, which AP_START does not restart. Start
  // here also rearms its automatic start when the interface is still down.
  auto started=esp_netif_dhcps_start(accessPoint);
  return configured&&(started==ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED||check(started,"start setup DHCP"));
 }
 bool start(wifi_mode_t type){return check(esp_wifi_set_mode(type),"radio mode");}
 bool run(){if(!check(esp_wifi_start(),"radio start"))return false;running=true;esp_wifi_set_ps(WIFI_PS_MIN_MODEM);return true;}
 void refreshAddress(){
  esp_netif_ip_info_t info{};auto* netif=(mode==Host||mode==Setup)?accessPoint:station;
  bool ready=running&&(mode==Host||mode==Setup||((mode==Joining||mode==Internet)&&stationConnected.load()));
  std::string address;
  if(ready&&netif&&esp_netif_get_ip_info(netif,&info)==ESP_OK&&info.ip.addr){char buffer[16];snprintf(buffer,sizeof(buffer),IPSTR,IP2STR(&info.ip));address=buffer;}
  if(address!=ip){lan.reset();ip=address;if(!ip.empty()){error.clear();esp_netif_set_default_netif(netif);ESP_LOGI(tag,"%s ready at %s",mode==Host?"Host":"Guest",ip.c_str());}}
  if(!ip.empty()&&!lan&&(mode==Host||mode==Joining)){lan=std::make_unique<LanTransport>(42688,ip);if(!lan->error().empty())error=lan->error();}
 }
};
WifiTransport::WifiTransport():impl_(new Impl){}
WifiTransport::~WifiTransport(){
 stopNetwork();auto& p=*impl_;if(p.initialized){esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,p.wifiHandler);esp_event_handler_instance_unregister(IP_EVENT,ESP_EVENT_ANY_ID,p.ipHandler);esp_wifi_deinit();esp_netif_destroy_default_wifi(p.station);esp_netif_destroy_default_wifi(p.accessPoint);}
}
void WifiTransport::stopNetwork(){
 auto& p=*impl_;bool setup=p.mode==Impl::Setup;p.mode=Impl::Off;p.lan.reset();p.ip.clear();
 if(p.running){esp_wifi_stop();p.running=false;}
 if(setup)p.setupDns(false);
 p.scanBusy=false;p.scanDone=false;p.stationConnected=false;p.found.clear();p.ssid.clear();p.error.clear();
}
bool WifiTransport::hostNetwork(const std::string& name){
 stopNetwork();auto& p=*impl_;if(!DeviceProfile::validName(name)||!p.initialize()||!p.start(WIFI_MODE_AP))return false;
 uint8_t mac[6];if(!p.check(esp_wifi_get_mac(WIFI_IF_AP,mac),"host identity"))return false;
 char suffix[8];snprintf(suffix,sizeof(suffix),"-%02x%02x%02x",mac[3],mac[4],mac[5]);p.ssid=std::string(prefix)+name+suffix;
 wifi_config_t config{};memcpy(config.ap.ssid,p.ssid.data(),p.ssid.size());config.ap.ssid_len=p.ssid.size();
 config.ap.channel=6;config.ap.authmode=WIFI_AUTH_OPEN;config.ap.max_connection=10;config.ap.beacon_interval=100;
 if(!p.check(esp_wifi_set_config(WIFI_IF_AP,&config),"host config")||!p.run())return false;
 p.mode=Impl::Host;p.refreshAddress();ESP_LOGI(tag,"Hosting %s (open local network)",p.ssid.c_str());return true;
}
void WifiTransport::browseNetworks(){
 auto& p=*impl_;if(p.mode==Impl::Browse)return;
 stopNetwork();if(!p.initialize()||!p.start(WIFI_MODE_STA)||!p.run())return;
 p.mode=Impl::Browse;p.nextScan=0;ESP_LOGI(tag,"Scanning for ARCANA hosts");
}
bool WifiTransport::joinNetwork(const std::string& ssid){
 if(hostName(ssid).empty())return false;
 stopNetwork();auto& p=*impl_;if(!p.initialize()||!p.start(WIFI_MODE_STA))return false;
 wifi_config_t config{};memcpy(config.sta.ssid,ssid.data(),ssid.size());config.sta.scan_method=WIFI_ALL_CHANNEL_SCAN;config.sta.threshold.authmode=WIFI_AUTH_OPEN;
 if(!p.check(esp_wifi_set_config(WIFI_IF_STA,&config),"guest config")||!p.run())return false;
 p.mode=Impl::Joining;p.ssid=ssid;p.nextConnect=0;p.connectRequested=true;ESP_LOGI(tag,"Joining %s",ssid.c_str());return true;
}
bool WifiTransport::joinInternet(const std::string& ssid,const std::string& password){
 if(ssid.empty()||ssid.size()>32||(!password.empty()&&(password.size()<8||password.size()>63)))return false;
 stopNetwork();auto& p=*impl_;if(!p.initialize()||!p.start(WIFI_MODE_STA))return false;
 wifi_config_t config{};memcpy(config.sta.ssid,ssid.data(),ssid.size());memcpy(config.sta.password,password.data(),password.size());
 config.sta.scan_method=WIFI_ALL_CHANNEL_SCAN;config.sta.threshold.authmode=password.empty()?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK;
 if(!p.check(esp_wifi_set_config(WIFI_IF_STA,&config),"internet config")||!p.run())return false;
 p.mode=Impl::Internet;p.nextConnect=0;p.connectRequested=true;return true;
}
bool WifiTransport::setupNetwork(const std::string& ssid,const std::string& password){
 if(ssid.empty()||ssid.size()>32||password.size()<8||password.size()>63)return false;
 stopNetwork();auto& p=*impl_;if(!p.initialize()||!p.start(WIFI_MODE_AP))return false;
 wifi_config_t config{};memcpy(config.ap.ssid,ssid.data(),ssid.size());config.ap.ssid_len=ssid.size();
 memcpy(config.ap.password,password.data(),password.size());config.ap.authmode=WIFI_AUTH_WPA2_PSK;config.ap.channel=6;config.ap.max_connection=2;config.ap.beacon_interval=100;
 if(!p.check(esp_wifi_set_config(WIFI_IF_AP,&config),"setup config")||!p.setupDns(true)||!p.run()){p.setupDns(false);return false;}
 p.mode=Impl::Setup;return true;
}
bool WifiTransport::updateNetwork()const{return impl_->mode==Impl::Internet||impl_->mode==Impl::Setup;}
bool WifiTransport::internetReady()const{return impl_->mode==Impl::Internet&&impl_->stationConnected.load()&&!impl_->ip.empty();}
void WifiTransport::pollNetwork(){
 auto& p=*impl_;auto now=millis();
 if(p.mode==Impl::Browse){
  if(p.scanDone.exchange(false)){
   p.scanBusy=false;uint16_t count=0;
   if(p.check(esp_wifi_scan_get_ap_num(&count),"scan count")){
    std::vector<wifi_ap_record_t> records(count);
    if(!count||p.check(esp_wifi_scan_get_ap_records(&count,records.data()),"scan results")){
     p.found.clear();for(const auto& ap:records){std::string ssid(reinterpret_cast<const char*>(ap.ssid),strnlen(reinterpret_cast<const char*>(ap.ssid),sizeof(ap.ssid)));auto name=hostName(ssid);
      if(!name.empty()&&ap.authmode==WIFI_AUTH_OPEN&&p.found.size()<24&&std::none_of(p.found.begin(),p.found.end(),[&](const auto& n){return n.id==ssid;}))p.found.push_back({ssid,name});}
     std::sort(p.found.begin(),p.found.end(),[](const auto& a,const auto& b){return a.id<b.id;});p.error.clear();ESP_LOGI(tag,"Scan complete: %u ARCANA host(s)",static_cast<unsigned>(p.found.size()));
    }
   }
   p.nextScan=now+5000;
  }
  if(!p.scanBusy&&now>=p.nextScan){wifi_scan_config_t config{};config.scan_time.active.min=40;config.scan_time.active.max=100;
   if(p.check(esp_wifi_scan_start(&config,false),"scan start"))p.scanBusy=true;else p.nextScan=now+5000;}
 }
 if((p.mode==Impl::Joining||p.mode==Impl::Internet)&&!p.stationConnected.load()&&now>=p.nextConnect&&p.connectRequested.exchange(false)){auto result=esp_wifi_connect();if(result!=ESP_OK){p.connectRequested=true;p.check(result,"connect");}p.nextConnect=now+3000;}
 p.refreshAddress();
}
bool WifiTransport::networkReady()const{return impl_->lan&&impl_->lan->error().empty()&&!impl_->ip.empty();}
std::vector<PeerNetwork> WifiTransport::networks()const{return impl_->found;}
bool WifiTransport::send(const std::string& endpoint,const std::string& data){
 if(!networkReady())return false;
 // The selected host is the DHCP gateway. Unicast discovery also works on
 // radios that filter multicast while saving power.
 if(endpoint=="discovery"&&impl_->mode==Impl::Joining){esp_netif_ip_info_t info{};
  if(esp_netif_get_ip_info(impl_->station,&info)==ESP_OK&&info.gw.addr){char gateway[16];snprintf(gateway,sizeof(gateway),IPSTR,IP2STR(&info.gw));return impl_->lan->send(std::string(gateway)+":42688",data);}
 }
 return impl_->lan->send(endpoint,data);
}
std::vector<PeerPacket> WifiTransport::receive(){return networkReady()?impl_->lan->receive():std::vector<PeerPacket>{};}
const std::string& WifiTransport::error()const{return impl_->error;}
bool WifiTransport::hosting()const{return impl_->mode==Impl::Host||impl_->mode==Impl::Setup;}
void WifiTransport::diagnose()const{const auto& p=*impl_;printf("NETWORK mode=%u ready=%u ssid=%s ip=%s hosts=%u error=%s\n",static_cast<unsigned>(p.mode),networkReady()?1u:0u,p.ssid.c_str(),p.ip.c_str(),static_cast<unsigned>(p.found.size()),p.error.c_str());}
}

