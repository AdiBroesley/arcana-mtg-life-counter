#include "Board.hpp"
#include "PanelInit.hpp"
#include "platform/RoundDisplay.hpp"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_sleep.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace mtg {
namespace {
constexpr int width=360,rows=40;

}
bool Board::transferDone(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void* context) {
 auto* board=static_cast<Board*>(context);BaseType_t wake=pdFALSE;
 xSemaphoreGiveFromISR(board->txDone_,&wake);return wake==pdTRUE;
}
void Board::setBrightness(unsigned value) {
 backlight_.setBrightness(value,esp_timer_get_time()/1000);applyBacklight();
}
void Board::setDisplaySettings(const DisplaySettings& value){backlight_.configure(value,esp_timer_get_time()/1000);applyBacklight();}
void Board::applyBacklight(){
 auto value=backlight_.output();if(value==appliedBrightness_)return;
 ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,value*1023/100));
 ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0));
 appliedBrightness_=value;
}
void Board::beginRing(bool dma){
 if(ringChannel_){
  if(ringEnabled_){rmt_disable(ringChannel_);ringEnabled_=false;}
  auto error=rmt_del_channel(ringChannel_);
  if(error!=ESP_OK){ringError_=error;ringFailed_=true;ESP_LOGW("RING","Channel cleanup failed: %s",esp_err_to_name(error));return;}
  ringChannel_=nullptr;
 }
 if(ringEncoder_){rmt_del_encoder(ringEncoder_);ringEncoder_=nullptr;}
 ringFailed_=false;ringApplied_=false;ringError_=ESP_OK;ringDma_=dma;
 // GPIO/order match the vendor. Send a padded uniform frame for longer rings.
 if(!ringSymbols_)ringSymbols_=static_cast<rmt_symbol_word_t*>(heap_caps_calloc(ringFramePixels*24+1,sizeof(rmt_symbol_word_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
 if(!ringSymbols_){ringError_=ESP_ERR_NO_MEM;ringFailed_=true;ESP_LOGW("RING","RGB buffer unavailable");return;}
 rmt_tx_channel_config_t config{};config.gpio_num=GPIO_NUM_0;config.clk_src=RMT_CLK_SRC_DEFAULT;
 // Entire 64-pixel frame plus latch/EOF fits, below the DMA descriptor limit.
 config.resolution_hz=10000000;config.mem_block_symbols=dma?1600:192;config.trans_queue_depth=1;config.flags.with_dma=dma;config.intr_priority=3;
 auto error=rmt_new_tx_channel(&config,&ringChannel_);
 if(error==ESP_OK){
  rmt_copy_encoder_config_t encoder{};error=rmt_new_copy_encoder(&encoder,&ringEncoder_);
  if(error!=ESP_OK){rmt_del_channel(ringChannel_);ringChannel_=nullptr;}
 }
 if(error!=ESP_OK){ringError_=error;ringFailed_=true;ESP_LOGW("RING","RGB unavailable: %s",esp_err_to_name(error));return;}
 updateRing(); // Explicitly clear any light left latched by previous firmware.
}
void Board::updateRing(){
 if(!ringChannel_||ringFailed_)return;
 auto color=ringTestUntil_?RingColor{5,0,0}:batteryRingColor(battery_);
 if(ringApplied_&&color==ringColor_)return;
 size_t index=0;
 for(auto byte:ringFrame(color)){
  for(unsigned bit=0x80;bit;bit>>=1){
   auto& symbol=ringSymbols_[index++];bool one=(byte&bit)!=0;
   // ESP-IDF WS2812 example: 0.3/0.9 us for zero, 0.9/0.3 us for one.
   symbol.level0=1;symbol.duration0=one?9:3;symbol.level1=0;symbol.duration1=one?3:9;
  }
 }
 auto& reset=ringSymbols_[index];reset.level0=reset.level1=0;
 reset.duration0=reset.duration1=1500; // 300 us reset/latch, then stay low.
 auto error=rmt_enable(ringChannel_);
 if(error==ESP_OK){
  ringEnabled_=true;
  rmt_transmit_config_t transmit{};
  error=rmt_transmit(ringChannel_,ringEncoder_,ringSymbols_,(ringFramePixels*24+1)*sizeof(rmt_symbol_word_t),&transmit);
  if(error==ESP_OK)error=rmt_tx_wait_all_done(ringChannel_,20);
  auto disabled=rmt_disable(ringChannel_); // Release the active peripheral/clock lock.
  if(disabled==ESP_OK)ringEnabled_=false;
  if(error==ESP_OK)error=disabled;
 }
 ringError_=error;
 if(error!=ESP_OK){ringFailed_=true;ESP_LOGW("RING","RGB update failed: %s",esp_err_to_name(error));return;}
 ringColor_=color;ringApplied_=true;++ringFrames_;
 ESP_LOGI("RING","%u pixel frame: %s, channel drive %u/255",ringFramePixels,color.green?"green":color.red?"red":"off",static_cast<unsigned>(color.green?color.green:color.red));
}
void Board::diagnoseRing(char command){
 if(command=='t'||command=='u'){
  ringTestUntil_=esp_timer_get_time()/1000+15000;
  beginRing(command=='t');
  printf("RING TEST START mode=%s red=5/255 seconds=15 error=%s\n",ringDma_?"DMA":"RMT",esp_err_to_name(ringError_));
 }else if(command=='x'){
  ringTestUntil_=0;ringApplied_=false;updateRing();printf("RING TEST RESTORED\n");
 }
 if(command=='d'||command=='t'||command=='u'){
  printf("RING STATUS mode=%s failed=%u error=%s frames=%u applied=%u rgb=%u,%u,%u override=%u\n",ringDma_?"DMA":"RMT",ringFailed_?1u:0u,esp_err_to_name(ringError_),ringFrames_,ringApplied_?1u:0u,static_cast<unsigned>(ringColor_.red),static_cast<unsigned>(ringColor_.green),static_cast<unsigned>(ringColor_.blue),ringTestUntil_?1u:0u);
  unsigned count=std::min(batterySampleCount_,16u);
  for(unsigned i=0;i<count;++i){auto& sample=batterySamples_[(batterySampleCount_-count+i)%16];
   printf("RING BATTERY t_ms=%llu mv=%d available=%u external=%u usb=%u percent=%u\n",static_cast<unsigned long long>(sample.time),sample.millivolts,sample.status.available?1u:0u,sample.status.externalPower?1u:0u,sample.usb?1u:0u,sample.status.percent);
  }
  printf("RING STATUS END\n");
 }
 fflush(stdout);
}
void Board::pollIdle(){
 auto now=esp_timer_get_time()/1000;
 if(ringTestUntil_&&static_cast<uint64_t>(now)>=ringTestUntil_){ringTestUntil_=0;ringApplied_=false;updateRing();printf("RING TEST RESTORED\n");fflush(stdout);}
 backlight_.tick(now,(sampledTouch_.load()&(1u<<18))!=0);applyBacklight();
}
std::int64_t Board::takeRotation(){
 rotationConsumedAt_=0;
 portENTER_CRITICAL(&knobMux_);auto batch=rotation_.take();portEXIT_CRITICAL(&knobMux_);
 if(!batch.delta)return 0;
 auto now=esp_timer_get_time();
 if(profiler_)profiler_->add(Profiler::WheelQueue,now-batch.sampledAt);
 bool deliver=backlight_.rotation(now/1000);applyBacklight();
 if(deliver)rotationConsumedAt_=batch.sampledAt;
 return deliver?batch.delta:0;
}
void Board::discardPendingInput(){
 portENTER_CRITICAL(&knobMux_);rotation_.take();portEXIT_CRITICAL(&knobMux_);
 xQueueReset(touchEvents_);touch_={};rawTouchDown_=false;
 lv_indev_reset(nullptr,nullptr);
}
void Board::rotationApplied(bool accepted){
 // Render an accepted wheel edit on the next handler call instead of waiting
 // for the normal 33 ms refresh timer. Idle redraws remain disabled.
 if(accepted)lv_timer_ready(lv_display_get_refr_timer(lv_display_get_default()));
 if(accepted&&rotationConsumedAt_&&!wheelFrameAt_&&profiler_&&profiler_->recording())wheelFrameAt_=rotationConsumedAt_;
 rotationConsumedAt_=0;
}
void Board::armInput(gpio_num_t pin){
 // The wake API also sets the interrupt type: use a level interrupt for both.
 ESP_ERROR_CHECK(gpio_wakeup_enable(pin,GPIO_INTR_LOW_LEVEL));
 ESP_ERROR_CHECK(gpio_intr_enable(pin));
}
void Board::knobReady(void* context){
 auto& input=*static_cast<KnobInterrupt*>(context);gpio_intr_disable(input.pin);
 BaseType_t wake=pdFALSE;vTaskNotifyGiveFromISR(input.board->knobTask_,&wake);
 if(wake)portYIELD_FROM_ISR();
}
void Board::touchReady(esp_lcd_touch_handle_t handle){
 auto& b=*static_cast<Board*>(handle->config.user_data);gpio_intr_disable(GPIO_NUM_7);
 if(b.touchTask_){BaseType_t wake=pdFALSE;vTaskNotifyGiveFromISR(b.touchTask_,&wake);if(wake)portYIELD_FROM_ISR();}
}
void Board::sampleKnob(void* context){
 auto& b=*static_cast<Board*>(context);
 const gpio_num_t pins[]={GPIO_NUM_2,GPIO_NUM_1};
 for(;;){
  bool active=false;
  for(unsigned i=0;i<2;++i){
   if(!gpio_get_level(pins[i])){
    gpio_intr_disable(pins[i]);gpio_wakeup_disable(pins[i]);
    b.lowTicks_[i]=std::min(100u,b.lowTicks_[i]+1);active=true;
   }else{
    if(b.lowTicks_[i]>=2){auto now=esp_timer_get_time();
     portENTER_CRITICAL(&b.knobMux_);b.rotation_.add(i==0?-1:1,now);portEXIT_CRITICAL(&b.knobMux_);b.wakeUi();
    }
    b.lowTicks_[i]=0;
    b.armInput(pins[i]);
   }
  }
  // Retain the proven 3 ms pulse filter during motion, with no idle polling.
  if(active)vTaskDelay(pdMS_TO_TICKS(3));else ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
 }
}
void Board::prepareInput(){
 if(uxQueueMessagesWaiting(touchEvents_))lv_indev_read(input_);
}
void Board::waitForActivity(unsigned milliseconds){
 ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(std::max(1u,milliseconds)));
}
void Board::diagnoseBatteryHardware(){
 // Read-only address probes: no configuration writes to any suspected gauge.
 const uint8_t addresses[]={0x36,0x55,0x62};
 for(auto address:addresses){
  auto command=i2c_cmd_link_create();i2c_master_start(command);
  i2c_master_write_byte(command,(address<<1)|I2C_MASTER_WRITE,true);i2c_master_stop(command);
  auto result=i2c_master_cmd_begin(I2C_NUM_0,command,pdMS_TO_TICKS(8));i2c_cmd_link_delete(command);
  printf("BATTERY PROBE address=0x%02x ack=%u error=%s\n",address,result==ESP_OK?1u:0u,esp_err_to_name(result));
 }
 printf("BATTERY SOURCE calibrated_ADC_GPIO6 VCC_OUT_divider not_coulomb_counter\n");
 diagnoseRing('d');
}
void Board::readTouch(lv_indev_t* input,lv_indev_data_t* data) {
 auto& b=*static_cast<Board*>(lv_indev_get_user_data(input));
 InputEvent queued;bool fromQueue=xQueueReceive(b.touchEvents_,&queued,0)==pdTRUE;
 auto event=queued.point;auto now=esp_timer_get_time()/1000;
 if(!fromQueue){auto packed=b.sampledTouch_.load();event={static_cast<int>(packed&511),static_cast<int>((packed>>9)&511),(packed&(1u<<18))!=0};}
 if(fromQueue&&event.pressed&&!b.rawTouchDown_&&b.profiler_)b.profiler_->add(Profiler::TouchQueue,1000u*(static_cast<uint32_t>(now)-queued.sampledAt));
 b.rawTouchDown_=event.pressed;
 bool deliver=b.backlight_.touch(now,event.pressed);b.applyBacklight();
 if(!deliver)event.pressed=false;
 b.touch_=event;data->point.x=event.x;data->point.y=event.y;
 data->state=event.pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
 data->continue_reading=uxQueueMessagesWaiting(b.touchEvents_)>0;
}
void Board::sampleTouch(void* context) {
 auto& b=*static_cast<Board*>(context);Touch previous;
 for(;;){
 uint16_t x=0,y=0;
 // CST816S register 0x02: finger count, X high/low, Y high/low.
 // Bypass the generic LCD I2C adapter, which uses portMAX_DELAY.
 gpio_intr_disable(GPIO_NUM_7);gpio_wakeup_disable(GPIO_NUM_7);
 uint8_t reg=2,point[5]{};
 bool pressed=i2c_master_write_read_device(I2C_NUM_0,0x15,&reg,1,point,sizeof(point),pdMS_TO_TICKS(8))==ESP_OK&&(point[0]&0xf)>0;
 if(pressed){x=((point[1]&0xf)<<8)|point[2];y=((point[3]&0xf)<<8)|point[4];
#ifdef CONFIG_ARCANA_ROTATE_180
 x=359-x;y=359-y;
#endif
 pressed=x<360&&y<360;}
 Touch event{pressed?static_cast<int>(x):previous.x,pressed?static_cast<int>(y):previous.y,pressed};
 b.sampledTouch_.store(static_cast<uint32_t>(event.x)|(static_cast<uint32_t>(event.y)<<9)|(pressed?(1u<<18):0));
 if(event.pressed!=previous.pressed||event.x!=previous.x||event.y!=previous.y){
  InputEvent queued{event,static_cast<uint32_t>(esp_timer_get_time()/1000)};
  if(xQueueSend(b.touchEvents_,&queued,0)!=pdTRUE){xQueueReset(b.touchEvents_);xQueueSend(b.touchEvents_,&queued,0);}
  b.wakeUi();
 }
 previous=event;
 if(event.pressed||gpio_get_level(GPIO_NUM_7)==0)vTaskDelay(pdMS_TO_TICKS(5));
 else {b.armInput(GPIO_NUM_7);ulTaskNotifyTake(pdTRUE,portMAX_DELAY);}
 }
}
void Board::pollBattery() {
 auto now=static_cast<uint64_t>(esp_timer_get_time()/1000);
 if(lastBattery_&&now-lastBattery_<5000)return;
 bool first=lastBattery_==0;lastBattery_=now;battery_={};batteryMillivolts_=-1;
 struct RingUpdate {Board* board;~RingUpdate(){
  auto& b=*board;b.batterySamples_[b.batterySampleCount_++%16]={b.lastBattery_,b.batteryMillivolts_,b.battery_,usb_serial_jtag_is_connected()};b.updateRing();
 }} ringUpdate{this};
 if(!adc_||!calibration_)return;
 int sum=0;
 for(int i=0;i<16;++i){int raw=0;if(adc_oneshot_read(adc_,ADC_CHANNEL_5,&raw)!=ESP_OK)return;sum+=raw;}
 int mv=0;if(adc_cali_raw_to_voltage(calibration_,sum/16,&mv)!=ESP_OK)return;
 mv*=2;batteryMillivolts_=mv; // GPIO6 divider is 10k / 10k from VCC_OUT, not directly BAT+.
 battery_.externalPower=usb_serial_jtag_is_connected()||mv>4350;
 if(!battery_.externalPower&&mv>=2800&&mv<=4350){
  constexpr int volts[]={3300,3500,3650,3750,3850,4000,4200};
  constexpr unsigned levels[]={0,5,20,40,60,80,100};
  battery_.available=true;battery_.percent=mv>=4200?100:0;
  for(unsigned i=1;i<7;++i)if(mv>=volts[i-1]&&mv<volts[i])battery_.percent=levels[i-1]+(mv-volts[i-1])*(levels[i]-levels[i-1])/(volts[i]-volts[i-1]);
 }
 if(first)ESP_LOGI("BATTERY","Supply %d mV; %s; estimate %u%%",mv,battery_.externalPower?"external power":battery_.available?"battery":"unavailable",battery_.percent);
}
void Board::flush(lv_display_t* display,const lv_area_t* area,uint8_t* pixels) {
 auto& b=*static_cast<Board*>(lv_display_get_user_data(display));
 auto started=esp_timer_get_time();
 int w=area->x2-area->x1+1;
 for(int y=area->y1;y<=area->y2;y+=rows){
  int height=std::min(rows,static_cast<int>(area->y2-y+1));size_t bytes=w*height*2;
  std::memcpy(b.dmaBuffer_,pixels+(y-area->y1)*w*2,bytes);
  blackOutsideFace(b.dmaBuffer_,area->x1,y,w,height,2);
  lv_draw_sw_rgb565_swap(b.dmaBuffer_,w*height);
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(b.panel_,area->x1,y,area->x2+1,y+height,b.dmaBuffer_));
  ESP_ERROR_CHECK(xSemaphoreTake(b.txDone_,pdMS_TO_TICKS(100))==pdTRUE?ESP_OK:ESP_ERR_TIMEOUT);
 }
 b.transferPixels_+=w*(area->y2-area->y1+1);
 if(lv_display_flush_is_last(display)&&b.wheelFrameAt_){if(b.profiler_)b.profiler_->add(Profiler::WheelToFrame,esp_timer_get_time()-b.wheelFrameAt_);b.wheelFrameAt_=0;}
 lv_display_flush_ready(display);
 b.transferMicros_+=esp_timer_get_time()-started;
}
void Board::begin() {
 uiTask_=xTaskGetCurrentTaskHandle();
 ledc_timer_config_t timer{};timer.speed_mode=LEDC_LOW_SPEED_MODE;timer.duty_resolution=LEDC_TIMER_10_BIT;
 timer.timer_num=LEDC_TIMER_0;timer.freq_hz=5000;timer.clk_cfg=LEDC_USE_RC_FAST_CLK;
 ESP_ERROR_CHECK(ledc_timer_config(&timer));
 ledc_channel_config_t channel{};channel.gpio_num=21;channel.speed_mode=LEDC_LOW_SPEED_MODE;
 channel.channel=LEDC_CHANNEL_0;channel.timer_sel=LEDC_TIMER_0;
 channel.sleep_mode=LEDC_SLEEP_MODE_KEEP_ALIVE;
 ESP_ERROR_CHECK(ledc_channel_config(&channel)); // Keep backlight off until first frame.

 lv_init();auto* display=lv_display_create(360,360);
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
 constexpr size_t bufferSize=width*rows*2;
 // A whole-frame draw buffer avoids repainting transformed layers once per strip.
 // Keep only the small transfer staging buffer in precious internal DMA RAM.
 auto* buffer=heap_caps_malloc(width*width*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);assert(buffer);
 dmaBuffer_=static_cast<uint8_t*>(heap_caps_malloc(bufferSize,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));assert(dmaBuffer_);
 txDone_=xSemaphoreCreateBinary();assert(txDone_);
 lv_display_set_buffers(display,buffer,nullptr,width*width*2,LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_user_data(display,this);lv_display_set_flush_cb(display,flush);
 spi_bus_config_t bus{};bus.sclk_io_num=11;bus.data0_io_num=13;bus.data1_io_num=14;
 bus.data2_io_num=15;bus.data3_io_num=16;bus.max_transfer_sz=bufferSize;
 ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
 esp_lcd_panel_io_spi_config_t ioConfig=ST77916_PANEL_IO_QSPI_CONFIG(12,transferDone,this);
 ioConfig.pclk_hz=40000000;
 esp_lcd_panel_io_handle_t io=nullptr;
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST),&ioConfig,&io));
 st77916_vendor_config_t vendor{};vendor.init_cmds=lcd_commands;
 vendor.init_cmds_size=sizeof(lcd_commands)/sizeof(lcd_commands[0]);vendor.flags.use_qspi_interface=1;
 esp_lcd_panel_dev_config_t config{};config.reset_gpio_num=17;
 config.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB;config.bits_per_pixel=16;config.vendor_config=&vendor;
 ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(io,&config,&panel_));
 ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
 ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_,true));
#ifdef CONFIG_ARCANA_ROTATE_180
 ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_,true,true));
#endif
 // LCD RAM can retain arbitrary/stale edge pixels outside LVGL's rounded
 // screen. Clear the complete square address space before lighting the panel.
 std::memset(dmaBuffer_,0,bufferSize);
 for(int y=0;y<width;y+=rows){
  const int height=std::min(rows,width-y);
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_,0,y,width,y+height,dmaBuffer_));
  ESP_ERROR_CHECK(xSemaphoreTake(txDone_,pdMS_TO_TICKS(100))==pdTRUE?ESP_OK:ESP_ERR_TIMEOUT);
 }
 ESP_LOGI("DISPLAY","Cleared 360x360 LCD; outside face radius %d stays black",faceRadius);
 ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_,true));

 i2c_config_t i2c{};i2c.mode=I2C_MODE_MASTER;i2c.sda_io_num=9;i2c.scl_io_num=10;
 i2c.sda_pullup_en=GPIO_PULLUP_ENABLE;i2c.scl_pullup_en=GPIO_PULLUP_ENABLE;i2c.master.clk_speed=400000;
 ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0,&i2c));
 ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0,I2C_MODE_MASTER,0,0,0));
 esp_lcd_panel_io_i2c_config_t touchIoConfig{};
 touchIoConfig.dev_addr=ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
 touchIoConfig.control_phase_bytes=1;touchIoConfig.lcd_cmd_bits=8;
 touchIoConfig.flags.disable_control_phase=1;
 esp_lcd_panel_io_handle_t touchIo=nullptr;
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(static_cast<esp_lcd_i2c_bus_handle_t>(I2C_NUM_0),&touchIoConfig,&touchIo));
 esp_lcd_touch_config_t tc{};tc.x_max=360;tc.y_max=360;tc.rst_gpio_num=GPIO_NUM_8;tc.int_gpio_num=GPIO_NUM_7;
 tc.interrupt_callback=touchReady;tc.user_data=this;
#ifdef CONFIG_ARCANA_ROTATE_180
 tc.flags.mirror_x=1;tc.flags.mirror_y=1;
#endif
 ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(touchIo,&tc,&touchHandle_));
 touchEvents_=xQueueCreate(64,sizeof(InputEvent));assert(touchEvents_);
 input_=lv_indev_create();lv_indev_set_type(input_,LV_INDEV_TYPE_POINTER);
 lv_indev_set_user_data(input_,this);lv_indev_set_read_cb(input_,readTouch);
 gpio_config_t knob{};knob.pin_bit_mask=(1ULL<<1)|(1ULL<<2);knob.mode=GPIO_MODE_INPUT;knob.pull_up_en=GPIO_PULLUP_ENABLE;
 ESP_ERROR_CHECK(gpio_config(&knob));
 // The CST816S driver has already installed the shared GPIO ISR service.
 for(auto& input:knobInterrupts_)ESP_ERROR_CHECK(gpio_isr_handler_add(input.pin,knobReady,&input));
 ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
 adc_oneshot_unit_init_cfg_t adcConfig{};adcConfig.unit_id=ADC_UNIT_1;
 if(adc_oneshot_new_unit(&adcConfig,&adc_)==ESP_OK){
  adc_oneshot_chan_cfg_t channelConfig{};channelConfig.atten=ADC_ATTEN_DB_12;channelConfig.bitwidth=ADC_BITWIDTH_DEFAULT;
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_,ADC_CHANNEL_5,&channelConfig));
  adc_cali_curve_fitting_config_t cal{};cal.unit_id=ADC_UNIT_1;cal.chan=ADC_CHANNEL_5;cal.atten=ADC_ATTEN_DB_12;cal.bitwidth=ADC_BITWIDTH_DEFAULT;
  if(adc_cali_create_scheme_curve_fitting(&cal,&calibration_)!=ESP_OK)calibration_=nullptr;
 }
 beginRing();pollBattery();
 // Keep peripheral outputs driven while automatic sleep isolates unused pins.
 for(auto pin:{GPIO_NUM_0,GPIO_NUM_8,GPIO_NUM_9,GPIO_NUM_10,GPIO_NUM_11,GPIO_NUM_12,
               GPIO_NUM_13,GPIO_NUM_14,GPIO_NUM_15,GPIO_NUM_16,GPIO_NUM_17,GPIO_NUM_21})
  ESP_ERROR_CHECK(gpio_sleep_sel_dis(pin));
 BaseType_t started=xTaskCreatePinnedToCore(sampleTouch,"arcana_touch",4096,this,3,&touchTask_,1);
 assert(started==pdPASS);
 started=xTaskCreatePinnedToCore(sampleKnob,"arcana_knob",3072,this,4,&knobTask_,1);assert(started==pdPASS);
}
}

