#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mtg {
inline constexpr int faceSize=360,faceRadius=176;
struct FaceSpan {int begin=0,end=0;};
inline constexpr auto faceSpans=[] {
 std::array<FaceSpan,faceSize> spans{};
 for(int y=0;y<faceSize;++y){
  const int dy=2*y+1-faceSize;
  for(int x=0;x<faceSize/2;++x){
   const int dx=2*x+1-faceSize;
   if(dx*dx+dy*dy<=4*faceRadius*faceRadius){spans[y]={x,faceSize-x};break;}
  }
 }
 return spans;
}();

// Apply at the final transfer, after all artwork/transitions. Black is zero in
// both RGB565 and XRGB8888. Row spans avoid extra LVGL masks or per-pixel work.
inline void blackOutsideFace(void* pixels,int x,int y,int width,int height,std::size_t pixelBytes,std::size_t stride=0){
 if(width<=0||height<=0||!pixelBytes||x<0||y<0||x+width>faceSize||y+height>faceSize)return;
 const auto top=faceSpans[y],bottom=faceSpans[y+height-1];
 if(top.end&&bottom.end&&x>=std::max(top.begin,bottom.begin)&&x+width<=std::min(top.end,bottom.end))return;
 if(!stride)stride=static_cast<std::size_t>(width)*pixelBytes;
 auto* row=static_cast<std::uint8_t*>(pixels);
 for(int i=0;i<height;++i,row+=stride){
  const auto span=faceSpans[y+i];
  const int left=std::clamp(span.begin-x,0,width),right=std::clamp(span.end-x,0,width);
  if(left)std::memset(row,0,left*pixelBytes);
  if(right<width)std::memset(row+right*pixelBytes,0,(width-right)*pixelBytes);
 }
}
}
