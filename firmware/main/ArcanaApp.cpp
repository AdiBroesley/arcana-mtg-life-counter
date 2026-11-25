#include "ArcanaApp.hpp"
#include "FlashStorage.hpp"
#include "Wifi.hpp"
#include "Bluetooth.hpp"
#include "platform/RadioTransport.hpp"
#include "NetworkProbe.hpp"
#include "RuntimePower.hpp"
#include "FirmwareUpdate.hpp"
#include "LanTransport.hpp"
#include "ui/AppUI.hpp"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>
#include <algorithm>

namespace mtg {
unsigned HardwareRandom::uniform(unsigned lower,unsigned upper) {
 if(upper<=lower)return lower;
 const uint32_t range=upper-lower+1;
 if(!range)return esp_random();
 const uint32_t threshold=static_cast<uint32_t>(-range)%range;
 uint32_t value;do{value=esp_random();}while(value<threshold);
 return lower+value%range;
}
void bootPlatform(DevicePlatform& platform) {
 // Do not silently erase NVS or filesystem if an existing partition cannot mount.
 ESP_ERROR_CHECK(nvs_flash_init());
 esp_vfs_spiffs_conf_t fs{};fs.base_path="/arcana";fs.partition_label="storage";fs.max_files=6;
 // Format only a completely erased partition. Never auto-format existing data.
 const auto* partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"storage");
 bool erased=partition!=nullptr;uint8_t probe[1024];
 if(partition)for(size_t offset=0;erased&&offset<partition->size;offset+=sizeof(probe)) {
  size_t length=std::min<size_t>(sizeof(probe),partition->size-offset);
  if(esp_partition_read(partition,offset,probe,length)!=ESP_OK){erased=false;break;}
  for(size_t i=0;i<length;++i)if(probe[i]!=0xff){erased=false;break;}
 }
 fs.format_if_mount_failed=erased;
 platform.storage=esp_vfs_spiffs_register(&fs);
 if(platform.storage!=ESP_OK)ESP_LOGE("ARCANA","Storage unavailable (%s); format the new storage partition using the installation helper",esp_err_to_name(platform.storage));
 platform.power.begin();
 platform.profiler.begin();platform.board.setProfiler(&platform.profiler);platform.board.begin();
}
void runArcana(DevicePlatform& platform,const ArcanaOptions& options) {
 auto& power=platform.power;auto& profiler=platform.profiler;auto& board=platform.board;auto& random=platform.random;
 const auto mounted=platform.storage;
 GameState game;game.setProfiler(&profiler);FlashStorage profileStorage("/arcana/profile.json");
 DeviceProfile profile(&profileStorage);profile.load();
 WifiTransport wifi;BluetoothTransport bluetooth;RadioTransport transport(wifi,bluetooth);
 Multiplayer multiplayer(game,profile,transport);
 FirmwareUpdate updates(wifi,game);
 NetworkProbe networkProbe(wifi,game,multiplayer);
 struct Diagnostics {Board& board;NetworkProbe& network;RuntimePower& power;FirmwareUpdate& updates;BluetoothTransport& bluetooth;RadioTransport& radio;AppUI* ui=nullptr;};Diagnostics diagnostics{board,networkProbe,power,updates,bluetooth,transport};
 profiler.diagnostics([](void* context,char command){auto& d=*static_cast<Diagnostics*>(context);
  if(command=='o'){d.updates.diagnose();return;}
  if(command=='z'){d.bluetooth.diagnose();return;}
  if(command=='e'){d.power.diagnose();return;}
  if(command=='i'){d.board.diagnoseBatteryHardware();return;}
  if(command=='b'){
   if(!d.ui||d.network.active()){printf("RENDER_BENCH BUSY\n");return;}
   printf("RENDER_BENCH START phases=live,frozen,live samples=24\n");fflush(stdout);
   auto trials=d.ui->benchmarkUndoDrawing();d.board.discardPendingInput();
   if(trials.empty()){printf("RENDER_BENCH BUSY open Single ARCANA main life face\n");return;}
   for(unsigned phase=0;phase<3;++phase){TimingStats stats;for(const auto& trial:trials)if(trial.phase==phase)stats.add(trial.micros);
    printf("RENDER_BENCH phase=%u caption=%s n=%lu avg=%llu p50=%lu p95=%lu max=%lu units=us\n",phase,phase==1?"frozen":"live",static_cast<unsigned long>(stats.count()),static_cast<unsigned long long>(stats.mean()),static_cast<unsigned long>(stats.percentile(50)),static_cast<unsigned long>(stats.percentile(95)),static_cast<unsigned long>(stats.maximum()));}
   printf("RENDER_BENCH END restored=1\n");fflush(stdout);
  }else if(command>='A'&&command<='Z'){if(d.radio.radio()==PeerRadio::Wifi)d.network.command(command);else printf("NETWORK_PROBE select Wi-Fi first; z prints BLE status\n");}else d.board.diagnoseRing(command);},&diagnostics);
 auto* screen=lv_screen_active();lv_obj_set_style_pad_all(screen,0,0);lv_obj_set_style_border_width(screen,0,0);
 lv_obj_set_style_text_font(screen,&lv_font_montserrat_14,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
 AppUI ui(game,board,screen,random,&profile,&multiplayer,&updates);
 if(options.exitToLauncher)ui.setExitHandler(options.exitToLauncher);
 if(options.homeFace)ui.setHomeFace(options.homeFace);
 if(options.title)ui.setTitle(options.title,options.subtitle?options.subtitle:"");
 diagnostics.ui=&ui;
 // Autosave the single-player match so a reset or brownout can resume it.
 FlashStorage matchStorage(options.matchFile);
 if(mounted==ESP_OK)ui.setMatchStorage(&matchStorage);
 lv_timer_handler();board.takeTransferMicros();board.setBrightness(board.brightness());
 bool bootConfirmed=false;
 const auto confirmBootAt=esp_timer_get_time()/1000+5000;
 updates.begin();
 uint64_t previous=esp_timer_get_time()/1000;
 ESP_LOGI("ARCANA","JC3636K718 firmware started; BLE experimental / Wi-Fi fallback; radios on demand");
 ESP_LOGI("ARCANA","Power v1: event-driven inputs; 80/240 MHz DFS; automatic light sleep; visuals unchanged");
 for(;;) {
  power.boost();
  uint64_t now=esp_timer_get_time()/1000;lv_tick_inc(static_cast<uint32_t>(now-previous));previous=now;
  board.prepareInput();board.pollBattery();board.tickRing();updates.poll();
  if(!bootConfirmed&&mounted==ESP_OK&&now>=static_cast<uint64_t>(confirmBootAt)){FirmwareUpdate::confirmBoot();bootConfirmed=true;}
  if(board.takeRedrawRequest())lv_obj_invalidate(lv_screen_active());
  auto t=esp_timer_get_time();if(networkProbe.active())networkProbe.tick(now);else ui.update();auto u=esp_timer_get_time();uint32_t nextTimer=networkProbe.active()?20:lv_timer_handler();auto r=esp_timer_get_time();
  auto transfer=board.takeTransferMicros();
  auto transferredPixels=board.takeTransferPixels();
  if(ui.lastUpdateDidWork())profiler.add(Profiler::Ui,u-t);
  if(transfer){profiler.add(Profiler::Frame,r-u);profiler.add(Profiler::Lcd,transfer);profiler.add(Profiler::DrawWork,static_cast<uint64_t>(r-u)>transfer?r-u-transfer:0);profiler.add(Profiler::FramePixels,transferredPixels);}
  board.pollIdle();profiler.backlight(board.brightness(),board.outputBrightness(),board.dimmed());
  profiler.poll();
  power.hosting(wifi.hosting());
  power.externalPower(board.battery().externalPower||!board.battery().available);
  unsigned wait=board.touchHeld()?5:multiplayer.attached()||networkProbe.active()||updates.active()?20:50;
  wait=std::min<unsigned>(wait,nextTimer);
  // Dimmed and nothing animating: LVGL's ~30 ms timers have nothing to do, and
  // knob/touch interrupts end the wait immediately. 500 ms cuts idle wake-ups
  // from ~33/s to 2/s; battery polling (5 s) and Undo expiry stay timely enough.
  if(board.dimmed()&&!board.touchHeld()&&!board.ringAnimating()&&!ui.animating()&&!ui.lastUpdateDidWork()&&
     !multiplayer.attached()&&!networkProbe.active()&&!updates.active())wait=500;
  power.idle();board.waitForActivity(wait);
 }
}
}
