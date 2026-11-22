#pragma once
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "driver/usb_serial_jtag.h"
#include <cstdio>

// Keep rendering at the accepted 240 MHz speed. Only idle time is slowed/slept.
class RuntimePower {
public:
 void begin(){
  ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX,0,"arcana_ui",&cpu_));
  ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP,0,"arcana_host",&host_));
  ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP,0,"arcana_usb_power",&external_));
  ESP_ERROR_CHECK(esp_pm_lock_acquire(external_));
  esp_pm_config_t config{};config.max_freq_mhz=240;config.min_freq_mhz=80;config.light_sleep_enable=true;
  ESP_ERROR_CHECK(esp_pm_configure(&config));
  esp_pm_sleep_cbs_register_config_t callbacks{};
  callbacks.exit_cb=[](int64_t us,void* context)->esp_err_t{
   auto& self=*static_cast<RuntimePower*>(context);
   // Called by the idle task; no allocation, logging, or blocking here.
   if(us>0){
    // IDF also invokes the exit callback after rejected/too-short attempts.
    // In IDF 5.5 the public wake cause is undefined after such a rejection.
    auto cause=esp_sleep_get_wakeup_cause();
    if(cause!=ESP_SLEEP_WAKEUP_UNDEFINED){
     self.sleeps_=self.sleeps_+1;self.sleepMicros_=self.sleepMicros_+static_cast<uint64_t>(us);
     if(cause==ESP_SLEEP_WAKEUP_GPIO)self.gpioWakes_=self.gpioWakes_+1;
    }else self.rejected_=self.rejected_+1;
   }
   return ESP_OK;
  };
  callbacks.exit_cb_user_arg=this;
  ESP_ERROR_CHECK(esp_pm_light_sleep_register_cbs(&callbacks));
  boost();
 }
 void boost(){if(!boosted_){ESP_ERROR_CHECK(esp_pm_lock_acquire(cpu_));boostedAt_=esp_timer_get_time();boosted_=true;}}
 void idle(){if(boosted_){boostMicros_+=esp_timer_get_time()-boostedAt_;ESP_ERROR_CHECK(esp_pm_lock_release(cpu_));boosted_=false;}}
 void hosting(bool value){if(value==hosting_)return;
  ESP_ERROR_CHECK(value?esp_pm_lock_acquire(host_):esp_pm_lock_release(host_));hosting_=value;
 }
 void externalPower(bool value){if(value==externallyPowered_)return;
  // A suspended Windows USB port can stop SOF packets even with its cable
  // attached. The charger rail is a separate, conservative sleep guard.
  ESP_ERROR_CHECK(value?esp_pm_lock_acquire(external_):esp_pm_lock_release(external_));externallyPowered_=value;
 }
 void diagnose()const{
  // Holding the CPU lock here excludes light sleep, so 64-bit counters are stable.
  printf("POWER min_mhz=80 max_mhz=240 auto_sleep=1 host_guard=%u external_guard=%u usb=%u sleeps=%llu sleep_us=%llu gpio_wakes=%llu rejected=%llu ui_boost_us=%llu uptime_us=%llu\n",
   hosting_?1u:0u,externallyPowered_?1u:0u,usb_serial_jtag_is_connected()?1u:0u,
   static_cast<unsigned long long>(sleeps_),static_cast<unsigned long long>(sleepMicros_),
   static_cast<unsigned long long>(gpioWakes_),static_cast<unsigned long long>(rejected_),
   static_cast<unsigned long long>(boostMicros_),
   static_cast<unsigned long long>(esp_timer_get_time()));
  esp_pm_dump_locks(stdout);printf("POWER END\n");fflush(stdout);
 }
private:
 esp_pm_lock_handle_t cpu_=nullptr,host_=nullptr,external_=nullptr;
 bool boosted_=false,hosting_=false,externallyPowered_=true;
 uint64_t boostedAt_=0,boostMicros_=0;
 volatile uint64_t sleeps_=0,sleepMicros_=0,gpioWakes_=0,rejected_=0;
};
