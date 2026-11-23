#include "FaceDesigns.hpp"
#include "ui/AppUI.hpp"
#include "ui/Widgets/DialWidgets.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
namespace mtg {
using namespace widgets;
namespace {
unsigned mix(unsigned a,unsigned b,double amount){
 unsigned result=0;for(unsigned shift:{0u,8u,16u})result|=static_cast<unsigned>(std::lround(((a>>shift)&255)*(1-amount)+((b>>shift)&255)*amount))<<shift;
 return result;
}
bool compact(unsigned design){return design!=1&&design!=4&&design!=10;}
}
void AppUI::setFaceDesign(unsigned design){
 faceDesign_=std::min<unsigned>(design,faceDesigns.size()-1);
 lifeHoldActive_=false;lifeHoldCancelled_=true;
 if(game_.state().active)goHome();else rebuild_=true;
}
void AppUI::decorateFaceConcept(){
 const auto& p=faceDesigns[faceDesign_];const unsigned id=faceDesign_;
 lv_obj_update_layout(screen_);lv_obj_set_style_bg_color(screen_,lv_color_hex(p.background),0);
 conceptBatteryState_=-2;
 unsigned tab=0;
 for(unsigned i=0;i<lv_obj_get_child_count(screen_);++i){
  auto* obj=lv_obj_get_child(screen_,i);int w=lv_obj_get_width(obj),h=lv_obj_get_height(obj),x=lv_obj_get_x(obj),y=lv_obj_get_y(obj);
  if(networkFace()&&lv_obj_check_type(obj,&lv_label_class)&&profile_->name()==lv_label_get_text(obj))lv_obj_set_style_text_color(obj,lv_color_hex(p.ink),0);
  if(lv_obj_check_type(obj,&lv_arc_class)){
   if(w==326&&lv_obj_get_style_arc_width(obj,LV_PART_MAIN)==29){
    auto color=id==2?mix(p.ink,tab%2?p.accent:p.secondary,.50):id==8?mix(p.surface,p.accent,.27+.07*(tab%3)):mix(p.surface,tab%2?p.accent:p.secondary,.45);
    lv_obj_set_style_arc_color(obj,lv_color_hex(color),LV_PART_MAIN);++tab;
   }else if(w==224){
    if(id==2||id==7||id==8||id==9||id==10)lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_set_style_arc_color(obj,lv_color_hex(mix(p.surface,p.accent,.65)),LV_PART_MAIN);
   }else if(w>=338)lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
  }
  if(lv_obj_check_type(obj,&lv_line_class)||(w==1&&h==108))lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
  if(w==224&&h==224&&x==68&&y==78&&!lv_obj_check_type(obj,&lv_arc_class)){
   lv_obj_set_style_bg_color(obj,lv_color_hex(p.surface),0);
   auto* circle=lv_obj_get_child(obj,0);lv_obj_set_style_bg_color(circle,lv_color_hex(p.surface),0);
   for(unsigned side=0;side<2;++side)lv_obj_set_style_bg_color(lv_obj_get_child(circle,side),lv_color_hex(mix(p.surface,side?p.gain:p.loss,id==2?.045:id==4?.20:id==8?.025:.075)),0);
  }
  if(w==108&&h==216&&y==82){
   for(unsigned j=0;j<lv_obj_get_child_count(obj);++j){auto* segment=lv_obj_get_child(obj,j);
    if(id!=1&&id!=4&&id!=10)lv_obj_add_flag(segment,LV_OBJ_FLAG_HIDDEN);
    else {lv_obj_set_style_arc_color(segment,lv_color_hex(x<180?p.loss:p.gain),LV_PART_MAIN);lv_obj_set_style_arc_width(segment,id==4?5:3,LV_PART_MAIN);}
   }
  }
  if(w==344&&h==344&&!lv_obj_check_type(obj,&lv_arc_class)){
   for(unsigned j=0;j<lv_obj_get_child_count(obj);++j){auto* control=lv_obj_get_child(obj,j);
    bool plus=lv_obj_get_y(control)<200,right=lv_obj_get_x(control)>100;auto color=plus?p.gain:p.loss;
    lv_obj_set_style_bg_color(control,lv_color_hex(mix(p.surface,color,id==2?.08:.13)),0);
    lv_obj_set_style_border_color(control,lv_color_hex(color),0);lv_obj_set_style_border_width(control,id==8?0:1,0);
    if(compact(id)){lv_obj_set_pos(control,right?292:12,plus?121:229);lv_obj_set_size(control,42,36);lv_obj_set_style_radius(control,id==3||id==7?6:16,0);}
    auto* glyph=lv_obj_get_child(control,0);lv_obj_set_style_text_color(glyph,lv_color_hex(color),0);if(compact(id))lv_obj_center(glyph);
   }
  }
  if(lv_obj_check_type(obj,&lv_button_class)&&w==44&&h==44){
   auto* content=lv_obj_get_child(obj,0);bool gear=lv_obj_check_type(content,&lv_label_class)&&!strcmp(lv_label_get_text(content),LV_SYMBOL_SETTINGS);
   unsigned fill=id==2&&!gear?0x203e4a:p.surface;
   lv_obj_set_style_bg_color(obj,lv_color_hex(fill),0);lv_obj_set_style_border_color(obj,lv_color_hex(gear?id==8?p.secondary:p.accent:mix(p.surface,p.accent,.65)),0);
   lv_obj_set_style_radius(obj,id==3||id==10?9:LV_RADIUS_CIRCLE,0);
   if(gear)lv_obj_set_style_text_color(content,lv_color_hex(id==8?p.secondary:p.ink),0);
  }
 }
 for(auto* count:{aux_,poison_}){
  auto* badge=lv_obj_get_parent(count);lv_obj_set_style_bg_color(badge,lv_color_hex(id==2?0x203e4a:p.surface),0);
  lv_obj_set_style_border_color(badge,lv_color_hex(p.accent),0);lv_obj_set_style_text_color(count,lv_color_hex(id==2?0xffffff:p.ink),0);
  if(id==3||id==10)lv_obj_set_style_radius(badge,5,0);
 }
 lv_obj_set_style_text_color(value_,lv_color_hex(p.ink),0);lv_obj_set_style_text_color(lifeRight_,lv_color_hex(p.ink),0);
 lv_obj_set_style_arc_width(undoArc_,id==2||id==8?18:id==10?31:25,LV_PART_MAIN);
 auto* layer=box(screen_,0,0,360,360,p.background);lv_obj_set_style_bg_opa(layer,0,0);lv_obj_set_style_radius(layer,0,0);lv_obj_remove_flag(layer,LV_OBJ_FLAG_CLICKABLE);lv_obj_move_background(layer);
 auto disk=[&](int cx,int cy,int diameter,unsigned color){auto* o=box(layer,cx-diameter/2,cy-diameter/2,diameter,diameter,color);lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;};
 auto line=[&](int x1,int y1,int x2,int y2,unsigned color,int thickness=1){
  tabEdgePoints_.push_back({lv_point_precise_t{x1,y1},lv_point_precise_t{x2,y2}});auto* o=lv_line_create(layer);lv_line_set_points(o,tabEdgePoints_.back().data(),2);
  lv_obj_set_style_line_color(o,lv_color_hex(color),0);lv_obj_set_style_line_width(o,thickness,0);lv_obj_set_style_line_rounded(o,true,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
 };
 auto arc=[&](int cx,int cy,int diameter,unsigned from,unsigned to,unsigned color,int thickness=1){
  auto* o=lv_arc_create(layer);lv_obj_set_size(o,diameter,diameter);lv_obj_set_pos(o,cx-diameter/2,cy-diameter/2);
  lv_obj_remove_style(o,nullptr,LV_PART_KNOB);lv_obj_set_style_arc_opa(o,0,LV_PART_INDICATOR);lv_obj_set_style_arc_width(o,thickness,LV_PART_MAIN);lv_obj_set_style_arc_color(o,lv_color_hex(color),LV_PART_MAIN);lv_obj_set_style_arc_rounded(o,false,LV_PART_MAIN);
  lv_arc_set_bg_angles(o,from,to);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;
 };
 constexpr double pi=3.141592653589793;
 auto radial=[&](double a,double r,int cy=180){double t=a*pi/180;return lv_point_t{static_cast<int32_t>(std::lround(180+r*std::cos(t))),static_cast<int32_t>(std::lround(cy+r*std::sin(t)))};};
 auto ticks=[&](unsigned count,int inner,int outer,unsigned color,int thickness,int cy=180){for(unsigned i=0;i<count;++i){auto a=radial(i*360.0/count,inner,cy),b=radial(i*360.0/count,outer,cy);line(a.x,a.y,b.x,b.y,color,thickness);}};
 auto polygon=[&](unsigned sides,int radius,int cy,unsigned color,int thickness,double offset){for(unsigned i=0;i<sides;++i){auto a=radial(offset+i*360.0/sides,radius,cy),b=radial(offset+(i+1)*360.0/sides,radius,cy);line(a.x,a.y,b.x,b.y,color,thickness);}};
 unsigned faint=mix(p.background,p.accent,.25);
 switch(id){
  case 1:
   arc(180,180,344,0,360,p.accent,3);arc(180,180,332,0,360,faint);ticks(60,165,169,p.secondary,1);
   ticks(48,115,123,mix(p.surface,p.accent,.58),1,190);arc(180,190,238,0,360,p.accent,1);
   for(unsigned i=0;i<8;++i){auto point=radial(22.5+i*45,171);disk(point.x,point.y,4,p.accent);}break;
  case 2:
   arc(180,180,344,0,360,mix(p.background,p.accent,.42));arc(180,190,236,0,360,p.accent,1);
   for(unsigned i=0;i<48;++i){auto point=radial(i*7.5,171);disk(point.x,point.y,i%4?2:3,mix(p.background,p.accent,i%4?.32:.7));}
   arc(180,190,248,23,157,mix(p.background,p.accent,.20),3);break;
  case 3:
   polygon(8,173,180,p.secondary,2,22.5);polygon(8,122,190,p.accent,2,22.5);
   for(unsigned i=0;i<12;++i){auto a=radial(i*30+15,126,190),b=radial(i*30+15,134,190);line(a.x,a.y,b.x,b.y,p.secondary,2);disk(b.x,b.y,4,p.accent);}break;
  case 4:
   arc(180,180,344,110,310,p.accent,6);arc(180,180,338,310,110,p.secondary,2);arc(180,190,240,110,280,p.accent,5);
   arc(180,180,326,5,65,mix(p.background,p.accent,.4),10);arc(180,180,326,115,175,mix(p.background,p.accent,.4),10);break;
  case 5:
   arc(180,180,344,0,360,p.accent,2);arc(180,187,324,15,165,p.secondary,2);
   arc(180,190,240,0,360,mix(p.surface,p.accent,.55));arc(165,188,267,25,155,mix(p.background,p.accent,.35),2);
   for(unsigned i=0;i<7;++i){auto point=radial(32+i*19,164);disk(point.x,point.y,3,p.accent);}break;
  case 6:
   arc(180,180,344,0,360,p.secondary,2);arc(180,180,336,0,360,faint);
   for(unsigned i=0;i<32;++i){auto point=radial(i*11.25,121,190);auto* leaf=box(layer,point.x-5,point.y-2,10,4,i%4?p.accent:p.secondary);lv_obj_set_style_radius(leaf,LV_RADIUS_CIRCLE,0);lv_obj_set_style_transform_pivot_x(leaf,5,0);lv_obj_set_style_transform_pivot_y(leaf,2,0);lv_obj_set_style_transform_rotation(leaf,(i*112+450)%3600,0);lv_obj_remove_flag(leaf,LV_OBJ_FLAG_CLICKABLE);}
   arc(180,190,232,0,360,mix(p.surface,p.accent,.4));break;
  case 7:
   for(unsigned i=0;i<6;++i)arc(180,180,342,i*60+2,i*60+58,mix(p.background,i%2?p.accent:p.secondary,.35),16);
   polygon(6,124,190,p.accent,2,30);polygon(6,130,190,mix(p.background,p.secondary,.4),1,30);
   for(unsigned i=0;i<6;++i){auto point=radial(30+i*60,171);disk(point.x,point.y,5,i%2?p.accent:p.secondary);}break;
  case 8:
   arc(180,180,344,195,345,p.accent);arc(180,180,344,15,165,p.accent);
   arc(180,190,232,45,135,mix(p.surface,p.accent,.33));arc(180,190,232,225,315,mix(p.surface,p.accent,.33));
   for(int a:{0,90,180,270}){auto aa=radial(a,166),bb=radial(a,171);line(aa.x,aa.y,bb.x,bb.y,p.secondary,2);}break;
  case 9:
   arc(180,180,344,0,360,p.accent,2);arc(180,180,334,0,360,faint);
   polygon(4,128,190,p.accent,1,0);polygon(4,134,190,faint,1,0);
   for(int a:{0,90,180,270})for(unsigned j=0;j<5;++j){auto aa=radial(a-8+j*4,163-j%2*3),bb=radial(a-8+j*4,172);line(aa.x,aa.y,bb.x,bb.y,p.accent,j==2?2:1);}break;
  case 10:
   arc(180,180,342,0,360,mix(p.background,p.accent,.45),5);ticks(48,165,171,p.accent,2);
   polygon(8,122,190,p.accent,3,22.5);polygon(8,126,190,faint,1,22.5);
   for(unsigned i=0;i<8;++i){auto point=radial(22.5+i*45,165);disk(point.x,point.y,9,p.background);disk(point.x,point.y,4,p.secondary);}break;
 }
 if(id==2){auto* marker=box(screen_,158,117,44,44,0x203e4a);lv_obj_set_style_radius(marker,LV_RADIUS_CIRCLE,0);lv_obj_remove_flag(marker,LV_OBJ_FLAG_CLICKABLE);lv_obj_move_foreground(lifeMarker_);}
 refreshFaceConcept();
}
void AppUI::refreshFaceConcept(){
 const auto& p=faceDesigns[faceDesign_];bool lethal=game_.player().life<=0||game_.poisonWarning()||game_.commanderWarning();
 textColor(value_,lv_color_hex(p.ink));textColor(lifeRight_,lv_color_hex(p.ink));
 textColor(status_,lv_color_hex(lethal?0xffffff:p.gain));textColor(lifeMarker_,lv_color_hex(lethal?0xffffff:p.loss));
 if(faceDesign_==2){lv_obj_set_style_bg_color(status_,lv_color_hex(0x203e4a),0);lv_obj_set_style_bg_opa(status_,lethal?LV_OPA_COVER:LV_OPA_TRANSP,0);lv_obj_set_style_radius(status_,6,0);}
 if(undoArc_){bool available=!game_.recentUndoLabel(lv_tick_get()).empty();lv_obj_set_style_arc_color(undoArc_,lv_color_hex(mix(p.surface,p.accent,available?.45:.18)),LV_PART_MAIN);}
 if(undo_)for(unsigned i=1;i<lv_obj_get_child_count(undo_);++i)textColor(lv_obj_get_child(undo_,i),lv_color_hex(p.ink));
 if(battery_&&conceptBatteryState_!=batteryState_){
  conceptBatteryState_=batteryState_;
  lv_obj_update_layout(battery_);
  for(unsigned i=0;i<lv_obj_get_child_count(battery_);++i){auto* item=lv_obj_get_child(battery_,i);
   if(lv_obj_check_type(item,&lv_label_class))textColor(item,lv_color_hex(p.ink));
   else if(lv_obj_get_width(item)==24){lv_obj_set_style_bg_color(item,lv_color_hex(p.surface),0);lv_obj_set_style_border_color(item,lv_color_hex(p.ink),0);}
   else if(lv_obj_get_x(item)==24)lv_obj_set_style_bg_color(item,lv_color_hex(p.ink),0);
  }
 }
}
}
