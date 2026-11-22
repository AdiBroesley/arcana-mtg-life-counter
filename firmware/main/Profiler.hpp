#pragma once
#include "platform/TimingStats.hpp"
#include "platform/IProfiler.hpp"
#include "esp_timer.h"
#include <cstdio>
#include <array>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>

class Profiler final : public mtg::IProfiler {
public:
 enum Metric {Ui,Frame,DrawWork,Lcd,TouchQueue,WheelQueue,HistoryRecord,UndoApply,UndoLabel,WheelToFrame,FramePixels,Count};
 void begin(){fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL,0)|O_NONBLOCK);}
 void diagnostics(void (*handler)(void*,char),void* context){diagnosticHandler_=handler;diagnosticContext_=context;}
 void backlight(unsigned requested,unsigned output,bool dimmed){requested_=requested;output_=output;dimmed_=dimmed;}
 bool recording()const override{return active_;}
 void record(mtg::Work work,std::uint64_t micros)override{
  add(work==mtg::Work::HistoryRecord?HistoryRecord:work==mtg::Work::UndoApply?UndoApply:UndoLabel,micros);
 }
 void add(Metric metric,std::uint64_t micros){if(active_)stats_[metric].add(static_cast<std::uint32_t>(std::min<std::uint64_t>(micros,UINT32_MAX)));}
 void poll(){
  char input[16];auto n=read(STDIN_FILENO,input,sizeof(input));
  for(int i=0;i<n;++i){
   if(input[i]=='p'){
    stats_={};active_=true;until_=esp_timer_get_time()+30000000;
    printf("PROFILE START 30 seconds match_saving=removed\n");
   }
   else if(input[i]=='r'&&active_)report();
   else if(diagnosticHandler_&&(input[i]=='o'||input[i]=='e'||input[i]=='i'||input[i]=='b'||input[i]=='d'||input[i]=='t'||input[i]=='u'||input[i]=='x'||input[i]=='N'||input[i]=='H'||input[i]=='J'||input[i]=='K'||input[i]=='S'||input[i]=='A'||input[i]=='D'||input[i]=='Q'))diagnosticHandler_(diagnosticContext_,input[i]);
  }
  if(active_&&esp_timer_get_time()>=until_)report();
 }
private:
 void report(){
  active_=false;
  static const char* names[]={"ui","frame","draw_work","lcd_transfer","touch_queue","wheel_queue","history_record","undo_apply","undo_label","wheel_to_frame","frame_pixels"};
  printf("PROFILE BEGIN units=us; frame_pixels units=pixels; p50/p95=latest128; avg/max=whole_capture\n");
  for(unsigned i=0;i<Count;++i){const auto& s=stats_[i];printf("PROFILE %s n=%lu avg=%llu p50=%lu p95=%lu max=%lu\n",names[i],static_cast<unsigned long>(s.count()),static_cast<unsigned long long>(s.mean()),static_cast<unsigned long>(s.percentile(50)),static_cast<unsigned long>(s.percentile(95)),static_cast<unsigned long>(s.maximum()));}
  printf("PROFILE backlight requested=%u output=%u dimmed=%u\n",requested_,output_,dimmed_?1u:0u);
  printf("PROFILE match_saving=removed\n");
  printf("PROFILE END\n");fflush(stdout);
 }
 std::array<mtg::TimingStats,Count> stats_{};
 std::int64_t until_=0;bool active_=false;
 unsigned requested_=0,output_=0;bool dimmed_=false;
 void (*diagnosticHandler_)(void*,char)=nullptr;void* diagnosticContext_=nullptr;
};
