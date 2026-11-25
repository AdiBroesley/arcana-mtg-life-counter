#include "assets/Fonts.hpp"
#include "AppUI.hpp"
#include "FaceDesigns.hpp"
#include "Widgets/DialWidgets.hpp"
#include "src/core/lv_obj_event_private.h"
#include <cmath>
#include <tuple>
#include <vector>
namespace mtg {
using namespace widgets;
void AppUI::toolSectorHit(lv_event_t* event) {
 auto* binding=static_cast<Binding*>(lv_event_get_user_data(event));auto* hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));
 lv_area_t area;lv_obj_get_coords(binding->ui->screen_,&area);double x=hit->point->x-area.x1-180,y=hit->point->y-area.y1-180;
 double angle=std::atan2(y,x)*180/3.141592653589793;if(angle<0)angle+=360;
 double offset=std::fmod(angle-binding->sectorStart+720,360);
 hit->res=x*x+y*y>=46*46&&x*x+y*y<=150*150&&offset<=binding->sectorSpan;
}
void AppUI::brightnessChanged(lv_event_t* event) {auto* ui=static_cast<AppUI*>(lv_event_get_user_data(event));ui->hardware_.setBrightness(lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(event))));}
lv_obj_t* AppUI::radialSector(const std::string& title,unsigned index,unsigned count,Action action,std::uint32_t arg,unsigned color,ToolIcon icon) {
 double span=360.0/count,start=270-span/2+span*index;
 auto* sector=lv_arc_create(screen_);lv_obj_set_size(sector,300,300);lv_obj_set_pos(sector,30,30);
 lv_obj_remove_style(sector,nullptr,LV_PART_KNOB);lv_obj_set_style_arc_opa(sector,0,LV_PART_INDICATOR);
 lv_obj_set_style_arc_width(sector,104,LV_PART_MAIN);lv_obj_set_style_arc_color(sector,lv_color_hex(color),LV_PART_MAIN);lv_obj_set_style_arc_rounded(sector,false,LV_PART_MAIN);
 auto angle=[](double value){return static_cast<unsigned>(std::round(value+720))%360;};
 lv_arc_set_bg_angles(sector,angle(start+1),angle(start+span-1));lv_obj_remove_flag(sector,LV_OBJ_FLAG_CLICKABLE);
 auto* b=button(screen_,"",30,30,300,300,action,arg);lv_obj_clean(b);
 lv_obj_set_style_bg_opa(b,0,0);lv_obj_set_style_bg_opa(b,0,LV_STATE_PRESSED);lv_obj_set_style_border_width(b,0,0);lv_obj_set_style_radius(b,0,0);lv_obj_set_style_pad_all(b,0,0);
 bindings_.back().sectorStart=start;bindings_.back().sectorSpan=span;
 lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_add_event_cb(b,toolSectorHit,LV_EVENT_HIT_TEST,&bindings_.back());
 double theta=(270+span*index)*3.141592653589793/180,radius=count>=7?112:100;
 int x=150+radius*std::cos(theta),y=150+radius*std::sin(theta);
 drawToolIcon(b,icon,x-15,y-25,0xffffff);label(b,title,x-49,y+10,98,&lv_font_montserrat_14,0xffffff);
 return b;
}
void AppUI::radialBack(Action action) {
 auto* back=button(screen_,"Back",136,136,88,88,action,0,0x000000);lv_obj_set_style_radius(back,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(back,0,0);
 lv_obj_set_style_text_color(lv_obj_get_child(back,0),lv_color_hex(0xaaaaaa),0);
}
void AppUI::buildTools() {
 if(view_==View::Gear&&homeFace_) {
  // A replacement home face has no counter shortcuts, player tabs or Undo arc:
  // the menu carries Undo, Poison and commander damage instead.
  const bool commander=usesCommanderDamage(game_.state().format)&&game_.state().playerCount>1;
  std::vector<std::tuple<const char*,Action,std::uint32_t,ToolIcon,unsigned>> items{
   {"Undo",Undo,0,ToolIcon::Reset,0x3c5261},{"Poison",OpenCounter,static_cast<std::uint32_t>(Counter::Poison),ToolIcon::Poison,0x5a8a3a}};
  if(commander)items.push_back({"ComDmg",SelectPlayer,2,ToolIcon::Shield,violet});
  items.push_back({"Flip Coin",FlipCoin,0,ToolIcon::Coin,gold});items.push_back({"Roll",RollD20,0,ToolIcon::D20,violet});
  items.push_back({"Display",OpenDisplay,0,ToolIcon::Brightness,teal});items.push_back({"Colors",OpenFaces,0,ToolIcon::WatchFace,0x496c92});
  items.push_back({"End Game",RequestEnd,0,ToolIcon::End,red});
  const auto count=static_cast<unsigned>(items.size());
  for(unsigned i=0;i<count;++i){
   auto& [title,action,arg,icon,color]=items[i];auto* tile=radialSector(title,i,count,action,arg,color,icon);
   // Pull captions in the rightmost slice inwards so they are not clipped by the ring.
   const double theta=(270+360.0/count*i)*3.141592653589793/180;
   if(std::cos(theta)>0.9){auto* caption=lv_obj_get_child(tile,1);lv_obj_set_x(caption,static_cast<int>(150+(count>=7?112:100)*std::cos(theta))-49-16);}
  }
 }else if(view_==View::Gear) {
  const char* titles[]{"New Game","Flip Coin","Roll","End Game","Display","Watch Face"};
  const Action actions[]{AskReset,FlipCoin,RollD20,RequestEnd,OpenDisplay,OpenFaces};
  const ToolIcon icons[]{ToolIcon::Reset,ToolIcon::Coin,ToolIcon::D20,ToolIcon::End,ToolIcon::Brightness,ToolIcon::WatchFace};
  const unsigned colors[]{red,gold,violet,red,teal,0x496c92};
  for(unsigned i=0;i<6;++i)radialSector(titles[i],i,6,actions[i],0,colors[i],icons[i]);
 }else if(view_==View::Display) {
  const auto& settings=profile_->displaySettings();
  radialSector("Brightness",0,5,OpenBrightness,0,teal,ToolIcon::Brightness);
  radialSector("Dim After",1,5,OpenDimDelay,0,0x326e88,ToolIcon::Brightness);
  radialSector(std::string("Life Only: ")+(settings.lifeOnly?"ON":"OFF"),2,5,ToggleLifeOnly,0,settings.lifeOnly?0x159870:0x3c5261,ToolIcon::LifeLink);
  radialSector(std::string("Wake Protection: ")+(settings.wakeProtection?"ON":"OFF"),3,5,ToggleWakeProtection,0,settings.wakeProtection?0x159870:0x3c5261,ToolIcon::Shield);
  radialSector("Idle Brightness",4,5,OpenIdleBrightness,0,0x6d587d,ToolIcon::Brightness);
  for(unsigned i=0;i<lv_obj_get_child_count(screen_);++i){auto* item=lv_obj_get_child(screen_,i);if(lv_obj_check_type(item,&lv_button_class)&&lv_obj_get_child_count(item)>1){auto* caption=lv_obj_get_child(item,1);lv_obj_set_style_text_font(caption,&tab_font,0);lv_obj_set_height(caption,30);lv_label_set_long_mode(caption,LV_LABEL_LONG_WRAP);}}
 }else if(view_==View::DimDelay||view_==View::IdleBrightness){
  bool delay=view_==View::DimDelay;auto* top=radialSector("",0,3,NoAction,0,0x326e88,ToolIcon::Brightness);lv_obj_clean(top);
  label(screen_,delay?"DIM AFTER":"IDLE BRIGHTNESS",80,55,200,&lv_font_montserrat_14,ink);value_=label(screen_,"",95,85,170,&lv_font_montserrat_32,ink);
  radialSector("+",1,3,Plus,0,teal,ToolIcon::Plus);radialSector("-",2,3,Minus,0,red,ToolIcon::Minus);
  label(screen_,delay?"0 = Never / max 300 sec":"Capped at your selected brightness",70,296,220,&tab_font,ink);
 }else if(view_==View::FaceSelect) {
  unsigned first=facePage_*4,count=std::min<unsigned>(4,faceDesigns.size()-first);
  bool previous=facePage_>0,next=first+count<faceDesigns.size();unsigned sectors=count+previous+next;
  for(unsigned i=0;i<count;++i){const auto& face=faceDesigns[first+i];
   auto* tile=radialSector(face.name,i,sectors,ChooseFace,first+i,((face.accent&0xfefefe)>>1)+((face.background&0xfefefe)>>1),first+i==faceDesign_?ToolIcon::Confirm:ToolIcon::WatchFace);
   auto* caption=lv_obj_get_child(tile,1);lv_obj_set_style_text_font(caption,&tab_font,0);lv_obj_set_height(caption,30);lv_label_set_long_mode(caption,LV_LABEL_LONG_WRAP);
  }
  unsigned index=count;
  if(previous)radialSector("Previous",index++,sectors,FacePage,facePage_-1,0x3c5261,ToolIcon::Reset);
  if(next)radialSector("Next",index,sectors,FacePage,facePage_+1,0x3c5261,ToolIcon::Reset);
  if(!notice_.empty())label(screen_,"SAVE FAILED",124,111,112,&tab_font,0xffffff);
 }else if(view_==View::Brightness) {
  auto* top=radialSector("",0,3,NoAction,0,0x166853,ToolIcon::Brightness);lv_obj_clean(top);
  label(screen_,"BRIGHTNESS",105,49,150,&lv_font_montserrat_14,ink);
  value_=label(screen_,"",115,74,130,&lv_font_montserrat_32,ink);
  brightnessSlider_=lv_slider_create(screen_);lv_obj_set_pos(brightnessSlider_,105,117);lv_obj_set_size(brightnessSlider_,150,10);lv_slider_set_range(brightnessSlider_,0,100);lv_slider_set_value(brightnessSlider_,hardware_.brightness(),LV_ANIM_OFF);lv_obj_add_event_cb(brightnessSlider_,brightnessChanged,LV_EVENT_VALUE_CHANGED,this);
  radialSector("+",1,3,Plus,0,teal,ToolIcon::Brightness);radialSector("-",2,3,Minus,0,red,ToolIcon::Brightness);
 }else if(view_==View::ResetConfirm||view_==View::EndConfirm) {
  bool ending=view_==View::EndConfirm;
  auto* top=radialSector("",0,3,NoAction,0,0x773044,ending?ToolIcon::End:ToolIcon::Reset);lv_obj_clean(top);
  label(screen_,ending?"END GAME?":"NEW GAME?",95,57,170,&lv_font_montserrat_20,ink);
  label(screen_,ending?"Finish this match?":"Reset all stats?",95,99,170,&lv_font_montserrat_14,ink);
  radialSector(ending?"End game":"Yes",1,3,ending?FinishGame:ResetYes,0,red,ToolIcon::Confirm);
  radialSector("No",2,3,ending?CancelEnd:ResetNo,0,teal,ToolIcon::Cancel);
 }else if(view_==View::CoinResult) {
  auto* result=radialSector("",0,2,NoAction,0,0x93702f,ToolIcon::Coin);lv_obj_clean(result);
  drawToolIcon(screen_,ToolIcon::Coin,165,56,0xffffff);
  label(screen_,coinHeads_?"HEADS":"TAILS",95,94,170,&lv_font_montserrat_32,0xffffff);
  radialSector("Flip again",1,2,FlipCoin,0,gold,ToolIcon::Reset);
 }else if(view_==View::RollResult) {
  const char* prefix=game_.state().format==GameFormat::TwoHeadedGiant?"T":"P";
  const auto& rolls=starterRoll_.rounds.back();unsigned count=static_cast<unsigned>(rolls.size())+2;
  auto* result=radialSector("",0,count,NoAction,0,0x246e88,ToolIcon::D20);lv_obj_clean(result);
  label(screen_,starterRoll_.winner?std::string(prefix)+std::to_string(starterRoll_.winner):"TIE",130,54,100,&lv_font_montserrat_32,ink);
  label(screen_,starterRoll_.winner?"STARTS":"REROLL",144,99,72,&lv_font_montserrat_14,ink);
  for(unsigned i=0;i<rolls.size();++i){
   auto* tile=radialSector(std::string(prefix)+std::to_string(rolls[i].player)+": "+std::to_string(rolls[i].die),i+1,count,NoAction,0,rolls[i].player==starterRoll_.winner?0x159870:0x326381,ToolIcon::D20);
   auto* title=lv_obj_get_child(tile,1);lv_obj_set_style_text_font(title,&tab_font,0);lv_obj_set_height(title,tab_font.line_height);
  }
  radialSector("Roll again",count-1,count,RollD20,0,violet,ToolIcon::Reset);
 }
 radialBack();
 if(view_==View::FaceSelect)label(screen_,std::to_string(facePage_+1)+" / "+std::to_string((faceDesigns.size()+3)/4),151,195,58,&tab_font,0xaaaaaa);
}
}
