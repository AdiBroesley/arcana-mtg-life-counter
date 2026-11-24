#include "Bluetooth.hpp"
#include "platform/BleFraming.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "nimble/nimble_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <cstring>
#include <atomic>

namespace mtg {
namespace {
// c2f00001-5a6c-4b9d-8e70-415243414e41 (RX 0002, TX 0003).
const ble_uuid128_t serviceUuid=BLE_UUID128_INIT(0x41,0x4e,0x41,0x43,0x52,0x41,0x70,0x8e,0x9d,0x4b,0x6c,0x5a,0x01,0x00,0xf0,0xc2);
const ble_uuid128_t rxUuid=BLE_UUID128_INIT(0x41,0x4e,0x41,0x43,0x52,0x41,0x70,0x8e,0x9d,0x4b,0x6c,0x5a,0x02,0x00,0xf0,0xc2);
const ble_uuid128_t txUuid=BLE_UUID128_INIT(0x41,0x4e,0x41,0x43,0x52,0x41,0x70,0x8e,0x9d,0x4b,0x6c,0x5a,0x03,0x00,0xf0,0xc2);
uint64_t millis(){return esp_timer_get_time()/1000;}
std::string addressId(const ble_addr_t& addr){char id[24];snprintf(id,sizeof(id),"%u:%02x%02x%02x%02x%02x%02x",addr.type,addr.val[0],addr.val[1],addr.val[2],addr.val[3],addr.val[4],addr.val[5]);return id;}
}
struct BluetoothTransport::Impl {
 enum class Mode {Off,Host,Browse,Join};
 struct Outgoing {std::string data;uint16_t sequence;size_t offset=0;};
 struct Link {bool subscribed=false;ble::Assembler assembler;std::deque<Outgoing> outgoing;size_t queuedBytes=0;};
 struct Found {ble_addr_t address{};std::string name;bool service=false;uint64_t seen=0;};
 mutable std::recursive_mutex mutex;
 Mode mode=Mode::Off;bool initialized=false,synced=false,connecting=false,ready=false,closing=false,drainControl=false;
 std::atomic<bool> hostReturned{false};
 TaskHandle_t hostTaskHandle=nullptr;
 uint8_t ownAddress=0;uint16_t txHandle=0,hostHandle=BLE_HS_CONN_HANDLE_NONE,remoteRx=0,remoteTx=0,svcStart=0,svcEnd=0,cccd=0,sequence=0,lastFlushed=0;
 uint64_t sentFrames=0,receivedMessages=0;unsigned queueDrops=0;
 std::string name,error;ble_addr_t target{};uint64_t retryAt=0,connectedAt=0;
 std::map<uint16_t,Link> links;std::map<std::string,Found> found;std::deque<PeerPacket> incoming;
 ble_gatt_chr_def characteristics[3]{};ble_gatt_svc_def services[2]{};
 static Impl* instance;
 using Guard=std::lock_guard<std::recursive_mutex>;
 void fail(const char* operation,int code){error=std::string(operation)+" ("+std::to_string(code)+")";ESP_LOGW("ARCANA_BLE","%s",error.c_str());}
 static void hostTask(void* context){auto* self=static_cast<Impl*>(context);nimble_port_run();self->hostReturned.store(true);vTaskSuspend(nullptr);}
 static void sync(){auto* p=instance;if(!p)return;Guard guard(p->mutex);int rc=ble_hs_util_ensure_addr(0);if(!rc)rc=ble_hs_id_infer_auto(0,&p->ownAddress);p->synced=rc==0;if(rc)p->fail("Bluetooth address failed",rc);}
 static void reset(int reason){auto* p=instance;if(!p)return;Guard guard(p->mutex);p->synced=false;p->ready=false;p->connecting=false;p->links.clear();p->hostHandle=BLE_HS_CONN_HANDLE_NONE;p->fail("Bluetooth reset",reason);}
 bool begin(){
  if(initialized)return !closing;
  closing=false;hostReturned.store(false);instance=this;
  auto rc=nimble_port_init();if(rc!=ESP_OK){fail("Bluetooth unavailable",rc);instance=nullptr;return false;}
  initialized=true;ble_hs_cfg.sync_cb=sync;ble_hs_cfg.reset_cb=reset;
  ble_svc_gap_init();ble_svc_gatt_init();ble_att_set_preferred_mtu(247);
  ble_svc_gap_device_name_set(("ARCANA-"+name).c_str());
  characteristics[0]={};characteristics[0].uuid=&rxUuid.u;characteristics[0].access_cb=access;characteristics[0].arg=this;characteristics[0].flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP;
  characteristics[1]={};characteristics[1].uuid=&txUuid.u;characteristics[1].access_cb=access;characteristics[1].arg=this;characteristics[1].flags=BLE_GATT_CHR_F_NOTIFY;characteristics[1].val_handle=&txHandle;
  services[0]={};services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services[0].uuid=&serviceUuid.u;services[0].characteristics=characteristics;
  int result=ble_gatts_count_cfg(services);if(!result)result=ble_gatts_add_svcs(services);
  if(result){fail("Bluetooth service failed",result);nimble_port_deinit();initialized=false;instance=nullptr;return false;}
  if(xTaskCreatePinnedToCore(hostTask,"arcana_ble",CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE,this,configMAX_PRIORITIES-4,&hostTaskHandle,CONFIG_BT_NIMBLE_PINNED_TO_CORE)!=pdPASS){
   fail("Not enough memory for Bluetooth",BLE_HS_ENOMEM);nimble_port_deinit();initialized=false;instance=nullptr;return false;
  }
  return true;
 }
 static int access(uint16_t handle,uint16_t,ble_gatt_access_ctxt* context,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);
  if(p.closing||p.mode!=Mode::Host||context->op!=BLE_GATT_ACCESS_OP_WRITE_CHR)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
  auto it=p.links.find(handle);if(it==p.links.end())return BLE_ATT_ERR_UNLIKELY;
  p.accept(handle,context->om);return 0;
 }
 void accept(uint16_t handle,os_mbuf* buffer){
  auto it=links.find(handle);if(it==links.end())return;auto size=OS_MBUF_PKTLEN(buffer);
  if(size<ble::headerSize||size>514)return;
  std::string frame(size,'\0');if(os_mbuf_copydata(buffer,0,size,frame.data()))return;
  auto message=it->second.assembler.receive(frame,millis());
  if(message&&incoming.size()<32){incoming.push_back({mode==Mode::Host?"ble:"+std::to_string(handle):"host",std::move(*message)});++receivedMessages;}
 }
 void advertising(){
  if(ble_gap_adv_active()||links.size()>=9)return;
  ble_hs_adv_fields fields{};fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;fields.uuids128=const_cast<ble_uuid128_t*>(&serviceUuid);fields.num_uuids128=1;fields.uuids128_is_complete=1;
  int rc=ble_gap_adv_set_fields(&fields);
  std::string advertised="ARCANA-"+name;ble_hs_adv_fields response{};response.name=reinterpret_cast<const uint8_t*>(advertised.data());response.name_len=advertised.size();response.name_is_complete=1;
  if(!rc)rc=ble_gap_adv_rsp_set_fields(&response);
  ble_gap_adv_params params{};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;params.itvl_min=480;params.itvl_max=800; // 300-500 ms, also while a game runs.
  if(!rc)rc=ble_gap_adv_start(ownAddress,nullptr,BLE_HS_FOREVER,&params,event,this);
  if(rc){fail("Bluetooth advertising failed",rc);retryAt=millis()+1500;}else{ready=true;error.clear();}
 }
 void scanning(){
  if(ble_gap_disc_active())return;
  ble_gap_disc_params params{};params.passive=0;params.itvl=160;params.window=80;params.filter_duplicates=0;
  int rc=ble_gap_disc(ownAddress,BLE_HS_FOREVER,&params,event,this);if(rc){fail("Bluetooth scan failed",rc);retryAt=millis()+1500;}
 }
 void connect(){
  if(connecting||hostHandle!=BLE_HS_CONN_HANDLE_NONE)return;
  ble_gap_disc_cancel();ble_gap_conn_params params{};params.scan_itvl=160;params.scan_window=80;params.itvl_min=24;params.itvl_max=48;params.latency=0;params.supervision_timeout=400;params.min_ce_len=0;params.max_ce_len=0;
  int rc=ble_gap_connect(ownAddress,&target,8000,&params,event,this);
  if(rc){fail("Bluetooth connection failed",rc);retryAt=millis()+1500;}else connecting=true;
 }
 void drop(uint16_t handle,const char* why,int code){fail(why,code);ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);}
 static int subscribed(uint16_t handle,const ble_gatt_error* result,ble_gatt_attr*,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);if(p.closing||p.mode!=Mode::Join||handle!=p.hostHandle)return 0;
  if(result->status)p.drop(handle,"Bluetooth subscription failed",result->status);
  else{p.ready=true;p.error.clear();p.links[handle].subscribed=true;}return 0;
 }
 static int descriptor(uint16_t handle,const ble_gatt_error* result,uint16_t,const ble_gatt_dsc* dsc,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);if(p.closing||handle!=p.hostHandle)return 0;
  if(!result->status){if(ble_uuid_u16(&dsc->uuid.u)==BLE_GATT_DSC_CLT_CFG_UUID16)p.cccd=dsc->handle;return 0;}
  if(result->status!=BLE_HS_EDONE||!p.cccd){p.drop(handle,"Bluetooth notification descriptor missing",result->status);return 0;}
  uint8_t value[2]={1,0};int rc=ble_gattc_write_flat(handle,p.cccd,value,2,subscribed,&p);if(rc)p.drop(handle,"Bluetooth subscription failed",rc);return 0;
 }
 static int characteristic(uint16_t handle,const ble_gatt_error* result,const ble_gatt_chr* chr,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);if(p.closing||handle!=p.hostHandle)return 0;
  if(!result->status){if(!ble_uuid_cmp(&chr->uuid.u,&rxUuid.u))p.remoteRx=chr->val_handle;if(!ble_uuid_cmp(&chr->uuid.u,&txUuid.u))p.remoteTx=chr->val_handle;return 0;}
  if(result->status!=BLE_HS_EDONE||!p.remoteRx||!p.remoteTx){p.drop(handle,"Incompatible Bluetooth host",result->status);return 0;}
  int rc=ble_gattc_disc_all_dscs(handle,p.remoteTx,p.svcEnd,descriptor,&p);if(rc)p.drop(handle,"Bluetooth descriptor lookup failed",rc);return 0;
 }
 static int service(uint16_t handle,const ble_gatt_error* result,const ble_gatt_svc* svc,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);if(p.closing||handle!=p.hostHandle)return 0;
  if(!result->status){p.svcStart=svc->start_handle;p.svcEnd=svc->end_handle;return 0;}
  if(result->status!=BLE_HS_EDONE||!p.svcStart){p.drop(handle,"ARCANA Bluetooth service missing",result->status);return 0;}
  int rc=ble_gattc_disc_all_chrs(handle,p.svcStart,p.svcEnd,characteristic,&p);if(rc)p.drop(handle,"Bluetooth characteristic lookup failed",rc);return 0;
 }
 static int mtu(uint16_t handle,const ble_gatt_error*,uint16_t,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);if(p.closing||handle!=p.hostHandle)return 0;
  int rc=ble_gattc_disc_svc_by_uuid(handle,&serviceUuid.u,service,&p);if(rc)p.drop(handle,"Bluetooth service lookup failed",rc);return 0;
 }
 static int event(ble_gap_event* ev,void* arg){
  auto& p=*static_cast<Impl*>(arg);Guard guard(p.mutex);if(p.closing)return 0;
  switch(ev->type){
   case BLE_GAP_EVENT_DISC:{
    if(p.mode!=Mode::Browse)break;
    ble_hs_adv_fields f{};if(ble_hs_adv_parse_fields(&f,ev->disc.data,ev->disc.length_data))break;
    auto id=addressId(ev->disc.addr);bool service=false;for(unsigned i=0;i<f.num_uuids128;++i)service|=!ble_uuid_cmp(&f.uuids128[i].u,&serviceUuid.u);
    auto it=p.found.find(id);if(it==p.found.end()){if(!service&&!(f.name_len>7&&std::memcmp(f.name,"ARCANA-",7)==0))break;if(p.found.size()>=24)break;it=p.found.emplace(id,Found{}).first;}
    auto& found=it->second;found.address=ev->disc.addr;found.service|=service;found.seen=millis();
    if(f.name_len>7&&std::memcmp(f.name,"ARCANA-",7)==0)found.name.assign(reinterpret_cast<const char*>(f.name)+7,std::min<size_t>(12,f.name_len-7));
    break;
   }
   case BLE_GAP_EVENT_CONNECT:
    p.connecting=false;
    if(ev->connect.status){p.fail("Bluetooth host not reached",ev->connect.status);p.retryAt=millis()+1500;break;}
    p.links.emplace(ev->connect.conn_handle,Link{});
    if(p.mode==Mode::Join){p.hostHandle=ev->connect.conn_handle;p.connectedAt=millis();p.remoteRx=p.remoteTx=p.svcStart=p.svcEnd=p.cccd=0;int rc=ble_gattc_exchange_mtu(p.hostHandle,mtu,&p);if(rc)mtu(p.hostHandle,nullptr,23,&p);}
    break;
   case BLE_GAP_EVENT_DISCONNECT:
    p.links.erase(ev->disconnect.conn.conn_handle);
    if(p.mode==Mode::Join){p.ready=false;p.hostHandle=BLE_HS_CONN_HANDLE_NONE;p.retryAt=millis()+1500;p.error="Bluetooth reconnecting";}break;
   case BLE_GAP_EVENT_SUBSCRIBE:{auto it=p.links.find(ev->subscribe.conn_handle);if(it!=p.links.end()&&ev->subscribe.attr_handle==p.txHandle){it->second.subscribed=ev->subscribe.cur_notify;if(!it->second.subscribed){it->second.outgoing.clear();it->second.queuedBytes=0;}}break;}
   case BLE_GAP_EVENT_NOTIFY_RX:
    if(p.mode==Mode::Join&&ev->notify_rx.conn_handle==p.hostHandle&&ev->notify_rx.attr_handle==p.remoteTx)p.accept(ev->notify_rx.conn_handle,ev->notify_rx.om);
    break;
   default:break;
  }return 0;
 }
 bool enqueue(uint16_t handle,const std::string& data){
  auto it=links.find(handle);if(it==links.end()||!it->second.subscribed)return false;auto& link=it->second;
  bool control=data.find("\"t\":\"closed\"")!=std::string::npos||data.find("\"t\":\"leave\"")!=std::string::npos;
  if(control){for(auto pending=link.outgoing.begin();pending!=link.outgoing.end();)if(!pending->offset){link.queuedBytes-=pending->data.size();pending=link.outgoing.erase(pending);}else ++pending;}
  // Authoritative snapshots supersede an older snapshot that has not started
  // transmission. Finish any partially sent packet before starting the next.
  if(data.find("\"t\":\"state\"")!=std::string::npos){
   for(auto& pending:link.outgoing)if(!pending.offset&&pending.data.find("\"t\":\"state\"")!=std::string::npos){if(link.queuedBytes-pending.data.size()+data.size()>32768){++queueDrops;return false;}link.queuedBytes-=pending.data.size();pending.data=data;link.queuedBytes+=data.size();return true;}
  }
  if(link.outgoing.size()>=8||link.queuedBytes+data.size()>32768){++queueDrops;return false;}
  link.outgoing.push_back({data,++sequence,0});link.queuedBytes+=data.size();drainControl|=control;return true;
 }
 void flush(){
  // At most four chunks per peer and 2 ms per UI pass. NimBLE owns transmission.
  auto until=esp_timer_get_time()+2000;
  for(size_t visited=0;visited<links.size()&&esp_timer_get_time()<until;++visited){
   auto it=links.upper_bound(lastFlushed);if(it==links.end())it=links.begin();auto handle=it->first;auto& link=it->second;lastFlushed=handle;
   for(unsigned n=0;n<4&&!link.outgoing.empty()&&esp_timer_get_time()<until;++n){
   auto mtu=ble_att_mtu(handle);if(mtu<23)break;
   auto& message=link.outgoing.front();auto frame=ble::fragment(message.data,message.sequence,message.offset,mtu-3);
   if(frame.empty()){link.queuedBytes-=message.data.size();link.outgoing.pop_front();break;}
   int rc;if(mode==Mode::Host){auto* buffer=ble_hs_mbuf_from_flat(frame.data(),frame.size());if(!buffer)break;rc=ble_gatts_notify_custom(handle,txHandle,buffer);}
   else rc=ble_gattc_write_no_rsp_flat(handle,remoteRx,frame.data(),frame.size());
   if(rc)break;
   ++sentFrames;message.offset+=frame.size()-ble::headerSize;
   if(message.offset==message.data.size()){link.queuedBytes-=message.data.size();link.outgoing.pop_front();}
  }}
 }
};
BluetoothTransport::Impl* BluetoothTransport::Impl::instance=nullptr;
BluetoothTransport::BluetoothTransport():impl_(new Impl){}
BluetoothTransport::~BluetoothTransport(){stopNetwork();}
bool BluetoothTransport::hostNetwork(const std::string& name){stopNetwork();auto& p=*impl_;Impl::Guard guard(p.mutex);p.mode=Impl::Mode::Host;p.name=name.substr(0,12);return p.begin();}
void BluetoothTransport::browseNetworks(){auto& p=*impl_;{Impl::Guard guard(p.mutex);if(p.mode==Impl::Mode::Browse)return;}stopNetwork();Impl::Guard guard(p.mutex);p.mode=Impl::Mode::Browse;p.begin();}
bool BluetoothTransport::joinNetwork(const std::string& id){
 // Multiplayer deliberately stops discovery before joining. The address is
 // carried in the selected network ID, rather than a now-discarded scan cache.
 auto& p=*impl_;Impl::Guard guard(p.mutex);
 if(id.size()!=14||(id[0]!='0'&&id[0]!='1')||id[1]!=':'||id.find_first_not_of("0123456789abcdef",2)!=std::string::npos){p.error="Invalid Bluetooth host address";return false;}
 p.target={};p.target.type=id[0]-'0';auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};for(unsigned i=0;i<6;++i)p.target.val[i]=(hex(id[2+i*2])<<4)|hex(id[3+i*2]);
 p.mode=Impl::Mode::Join;p.ready=false;p.retryAt=0;p.error.clear();return p.begin();
}
void BluetoothTransport::stopNetwork(){
 auto& p=*impl_;bool drain;{Impl::Guard guard(p.mutex);drain=p.drainControl&&p.initialized&&p.ready&&!p.closing;}
 if(drain){
  // Give End Game/Leave a bounded opportunity to reach peers before tearing
  // down the radio. Normal navigation and inactive-radio stops do not wait.
  auto deadline=millis()+250;
  do{bool pending=false;{Impl::Guard guard(p.mutex);p.flush();for(const auto& [handle,link]:p.links)pending|=!link.outgoing.empty();}if(!pending)break;vTaskDelay(pdMS_TO_TICKS(5));}while(millis()<deadline);
  vTaskDelay(pdMS_TO_TICKS(120)); // two maximum connection intervals on air
 }
 bool initialized;{Impl::Guard guard(p.mutex);p.closing=true;p.mode=Impl::Mode::Off;p.ready=false;initialized=p.initialized;}
 // Never wait for the host task while holding the state mutex.
 if(initialized){
  int rc=nimble_port_stop();if(rc){Impl::Guard guard(p.mutex);p.fail("Bluetooth shutdown failed",rc);return;}
  // Finish and delete the old host before another start can reuse the stack.
  // The parked task performs no further stack accesses.
  while(!p.hostReturned.load())vTaskDelay(1);
  vTaskDelete(p.hostTaskHandle);p.hostTaskHandle=nullptr;
  rc=nimble_port_deinit();if(rc){Impl::Guard guard(p.mutex);p.fail("Bluetooth deinit failed",rc);return;}
 }
 Impl::Guard guard(p.mutex);p.initialized=p.synced=p.connecting=p.drainControl=false;p.closing=false;p.links.clear();p.incoming.clear();p.found.clear();p.hostHandle=BLE_HS_CONN_HANDLE_NONE;p.retryAt=0;p.error.clear();Impl::instance=nullptr;
}
void BluetoothTransport::pollNetwork(){
 auto& p=*impl_;Impl::Guard guard(p.mutex);if(!p.initialized||!p.synced||p.closing)return;
 auto now=millis();if(now>=p.retryAt){if(p.mode==Impl::Mode::Host)p.advertising();else if(p.mode==Impl::Mode::Browse)p.scanning();else if(p.mode==Impl::Mode::Join)p.connect();}
 if(p.mode==Impl::Mode::Join&&p.hostHandle!=BLE_HS_CONN_HANDLE_NONE&&!p.ready&&now-p.connectedAt>12000){p.connectedAt=now;p.drop(p.hostHandle,"Bluetooth setup timed out",BLE_HS_ETIMEOUT);}
 for(auto it=p.found.begin();it!=p.found.end();)if(now-it->second.seen>8000)it=p.found.erase(it);else ++it;
 p.flush();
}
bool BluetoothTransport::networkReady()const{auto& p=*impl_;Impl::Guard guard(p.mutex);return p.ready;}
std::vector<PeerNetwork> BluetoothTransport::networks()const{auto& p=*impl_;Impl::Guard guard(p.mutex);std::vector<PeerNetwork> result;for(const auto& [id,f]:p.found)if(f.service&&!f.name.empty())result.push_back({id,f.name});return result;}
bool BluetoothTransport::send(const std::string& endpoint,const std::string& data){
 if(data.empty()||data.size()>ble::maxMessage)return false;
 auto& p=*impl_;Impl::Guard guard(p.mutex);if(!p.ready)return false;
 if(p.mode==Impl::Mode::Join)return p.enqueue(p.hostHandle,data);
 bool sent=false;if(endpoint=="discovery"){for(const auto& [handle,link]:p.links)if(link.subscribed)sent=p.enqueue(handle,data)||sent;return sent;}
 for(const auto& [handle,link]:p.links)if(endpoint=="ble:"+std::to_string(handle))return p.enqueue(handle,data);
 return false;
}
std::vector<PeerPacket> BluetoothTransport::receive(){auto& p=*impl_;Impl::Guard guard(p.mutex);std::vector<PeerPacket> result;while(!p.incoming.empty()){result.push_back(std::move(p.incoming.front()));p.incoming.pop_front();}return result;}
const std::string& BluetoothTransport::error()const{auto& p=*impl_;Impl::Guard guard(p.mutex);static thread_local std::string result;result=p.error;return result;}
void BluetoothTransport::diagnose()const{auto& p=*impl_;Impl::Guard guard(p.mutex);printf("BLE mode=%u initialized=%u synced=%u ready=%u links=%u found=%u rx=%u frames=%llu messages=%llu queue_drops=%u error=%s\n",static_cast<unsigned>(p.mode),p.initialized,p.synced,p.ready,static_cast<unsigned>(p.links.size()),static_cast<unsigned>(p.found.size()),static_cast<unsigned>(p.incoming.size()),static_cast<unsigned long long>(p.sentFrames),static_cast<unsigned long long>(p.receivedMessages),p.queueDrops,p.error.c_str());}
}
