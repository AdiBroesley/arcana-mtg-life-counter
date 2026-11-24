#pragma once
#include "platform/IHardware.hpp"
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_timer.h"
#include <atomic>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "platform/IdleBacklight.hpp"
#include "platform/BatteryRing.hpp"
#include "platform/EncoderBatch.hpp"
#include "driver/rmt_tx.h"
#include <array>
#include "Profiler.hpp"

namespace mtg {
class Board final : public IHardware {
public:
 void begin();
 void pollBattery();
 void pollIdle();
 void diagnoseRing(char command);
 void discardPendingInput();
 void prepareInput();
 void waitForActivity(unsigned milliseconds);
 bool touchHeld()const{return (sampledTouch_.load()&(1u<<18))!=0;}
 void diagnoseBatteryHardware();
 void setProfiler(Profiler* profiler){profiler_=profiler;}
 IProfiler* profiler()const override{return profiler_;}
 bool dimmed()const{return backlight_.dimmed();}
 unsigned outputBrightness()const override{return backlight_.output();}
 bool idle()const override{return backlight_.dimmed();}
 void setDisplaySettings(const DisplaySettings& value)override;
 uint64_t takeTransferMicros(){auto time=transferMicros_;transferMicros_=0;return time;}
 uint32_t takeTransferPixels(){auto pixels=transferPixels_;transferPixels_=0;return pixels;}
 BatteryStatus battery() const override {return battery_;}
 std::int64_t takeRotation() override;
 void rotationApplied(bool accepted)override;
 Touch touch() const override { return touch_; }
 bool powered() const override { return true; }
 unsigned brightness() const override { return backlight_.brightness(); }
 void setBrightness(unsigned value) override;
private:
 static bool transferDone(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*);
 static void sampleKnob(void* arg);
 static void knobReady(void* arg);
 static void touchReady(esp_lcd_touch_handle_t handle);
 void wakeUi(){xTaskNotifyGive(uiTask_);}
 void armInput(gpio_num_t pin);
 static void readTouch(lv_indev_t*,lv_indev_data_t*);
 static void sampleTouch(void*);
 static void flush(lv_display_t*,const lv_area_t*,uint8_t*);
 void applyBacklight();
 void beginRing(bool dma=true);
 void updateRing();
 struct InputEvent {Touch point;uint32_t sampledAt=0;};
 esp_lcd_panel_handle_t panel_=nullptr;
 esp_lcd_touch_handle_t touchHandle_=nullptr;
 TaskHandle_t uiTask_=nullptr,knobTask_=nullptr,touchTask_=nullptr;
 lv_indev_t* input_=nullptr;
 struct KnobInterrupt {Board* board;gpio_num_t pin;};
 KnobInterrupt knobInterrupts_[2]={{this,GPIO_NUM_2},{this,GPIO_NUM_1}};
 EncoderBatch rotation_;
 portMUX_TYPE knobMux_=portMUX_INITIALIZER_UNLOCKED;
 unsigned lowTicks_[2]={0,0};
 IdleBacklight backlight_;
 unsigned appliedBrightness_=UINT32_MAX;
 bool rawTouchDown_=false;
 Profiler* profiler_=nullptr;
 uint64_t transferMicros_=0;
 uint32_t transferPixels_=0;
 uint64_t rotationConsumedAt_=0,wheelFrameAt_=0;
 Touch touch_;
 adc_oneshot_unit_handle_t adc_=nullptr;
 adc_cali_handle_t calibration_=nullptr;
 BatteryStatus battery_;
 uint64_t lastBattery_=0;
 SemaphoreHandle_t txDone_=nullptr;
 uint8_t* dmaBuffer_=nullptr;
 QueueHandle_t touchEvents_=nullptr;
 std::atomic<uint32_t> sampledTouch_{0};
 rmt_channel_handle_t ringChannel_=nullptr;
 rmt_encoder_handle_t ringEncoder_=nullptr;
 rmt_symbol_word_t* ringSymbols_=nullptr;
 RingColor ringColor_;
 bool ringApplied_=false,ringFailed_=false,ringEnabled_=false;
 bool ringDma_=true;uint64_t ringTestUntil_=0;
 esp_err_t ringError_=ESP_OK;unsigned ringFrames_=0;
 int batteryMillivolts_=-1;
 struct BatterySample {uint64_t time;int millivolts;BatteryStatus status;bool usb;};
 std::array<BatterySample,16> batterySamples_{};unsigned batterySampleCount_=0;
};
}

