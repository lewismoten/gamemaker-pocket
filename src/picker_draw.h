// Picker rows, icons and title-search keyboard, rendered by both firmware and host/setupshots.cpp.
#pragma once
#include <stdint.h>
#include "picker.h"
#include "closex.h"
namespace pk {
constexpr uint16_t C_WHITE = 0xFFFF, C_BLACK = 0x10A5, C_YELLOW = 0xFEAA, C_GRAY = 0xA5B9, C_DARK = 0x1927;
// Search keyboard colors: action keys (Space, Del, Done, back, clear) are blue with a yellow frame; vowels are light green.
constexpr uint16_t C_ACTION = 0x2A9A, C_VOWEL = 0x87F0;
// on=false draws it gray: a button that would do nothing now (the first page's back arrow, Details with no game picked).
template <class G> void drawButton(G& g, int x0, int y0, int x1, int y1, const char* label, bool on = true) {
  g.fillRoundRect(x0, y0, x1 - x0, y1 - y0, 6, C_DARK);
  g.drawRoundRect(x0, y0, x1 - x0, y1 - y0, 6, on ? C_WHITE : C_GRAY);
  g.setTextSize(2); g.setTextColor(on ? C_WHITE : C_GRAY, C_DARK);
  g.setCursor((x0 + x1) / 2 - (int)strlen(label) * 6, (y0 + y1) / 2 - 8); g.print(label);
}
// An action key (Space, Del, Done, back, clear): blue with a double yellow frame, so it never reads as a letter.
template <class G> void drawActionButton(G& g, int x0, int y0, int x1, int y1, const char* label) {
  g.fillRoundRect(x0, y0, x1 - x0, y1 - y0, 6, C_ACTION);
  g.drawRoundRect(x0, y0, x1 - x0, y1 - y0, 6, C_YELLOW);
  g.drawRoundRect(x0 + 1, y0 + 1, x1 - x0 - 2, y1 - y0 - 2, 5, C_YELLOW);
  g.setTextSize(2); g.setTextColor(C_WHITE, C_ACTION);
  g.setCursor((x0 + x1) / 2 - (int)strlen(label) * 6, (y0 + y1) / 2 - 8); g.print(label);
}
// A letter key whose vowel is tinted (text and frame) so the five vowels stand out.
template <class G> void drawLetterKey(G& g, int x0, int y0, int x1, int y1, char c) {
  bool v = c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
  uint16_t ink = v ? C_VOWEL : C_WHITE;
  g.fillRoundRect(x0, y0, x1 - x0, y1 - y0, 6, C_DARK);
  g.drawRoundRect(x0, y0, x1 - x0, y1 - y0, 6, v ? C_VOWEL : C_WHITE);
  g.setTextSize(2); g.setTextColor(ink, C_DARK);
  char l[2] = {c, 0};
  g.setCursor((x0 + x1) / 2 - 6, (y0 + y1) / 2 - 8); g.print(l);
}
// Icon buttons are 44 pixels wide or larger on both boards.
template <class G> void drawIcon(G& tft, int slot, int W, int H, int footer) {
  int x0 = 6 + slot * (W / 4), x1 = x0 + W / 4 - 12;
  int cy = (footer + H) / 2, cx = (x0 + x1) / 2;
  drawButton(tft, x0, footer + 2, x1, H - 2, "");
  if (slot < 2) {
    int d = slot ? 1 : -1;
    tft.fillRect(cx - 12, cy - 2, 24, 4, C_WHITE);
    tft.fillTriangle(cx + d * 14, cy, cx, cy - 10, cx, cy + 10, C_WHITE);
  } else if (slot == 2) {
    // A three-pixel ring and a short 45-degree handle, centered as one silhouette.
    tft.fillCircle(cx - 3, cy - 3, 10, C_WHITE);
    tft.fillCircle(cx - 3, cy - 3, 7, C_DARK);
    for (int d = 4; d <= 11; d++) tft.fillRect(cx + d - 1, cy + d - 1, 3, 3, C_WHITE);
  } else {
    for (int i = -1; i <= 1; i += 2) {
      tft.fillRect(cx - 3, cy + i * 10 - 3, 6, 6, C_WHITE);
      tft.fillRect(cx + i * 10 - 3, cy - 3, 6, 6, C_WHITE);
      for (int j = -1; j <= 1; j += 2) tft.fillRect(cx + i * 7 - 2, cy + j * 7 - 2, 4, 4, C_WHITE);
    }
    tft.fillCircle(cx, cy, 10, C_WHITE); tft.fillCircle(cx, cy, 4, C_DARK);
  }
}
template <class G> void drawRow(G& tft, const char* text, bool sd, int W, int y, int height) {
    char l[96];
    pk::ellipsize(text, l, sizeof l, (W - 32 - (sd ? 36 : 0)) / 12);
    tft.fillRoundRect(6, y, W - 12, height, 6, C_DARK);
    tft.drawRoundRect(6, y, W - 12, height, 6, C_GRAY);
    tft.setTextColor(C_WHITE, C_DARK); tft.setTextSize(2);
    tft.setCursor(16, y + (height - 16) / 2); tft.print(l);
    if (sd) {
      tft.setTextColor(C_YELLOW, C_DARK);
      tft.setCursor(W - 40, y + (height - 16) / 2); tft.print("SD");
    }
}
// Repaint directly on the dark backing; only an empty slot needs erasing.
template <class G> void drawPageRow(G& tft, const char* text, bool sd, int W, int y, int height) {
  if (text) drawRow(tft, text, sd, W, y, height);
  else tft.fillRect(6, y, W - 12, height, C_BLACK);
}
// Preserve the common prefix and erase only the edited suffix and old caret. The keyboard and title are painted once on entry.
// Empty, the field shows "Type a title" in gray as a hint (four pixels in, so the white caret sits clear of it); the caret is
// always where the next character will appear.
constexpr int SEARCH_HINT_SHIFT = 4;
template <class G> void drawSearchText(G& tft, int titleH, const char* text, const char* previous = nullptr, const char* hint = "Type a title") {
  if (previous && !strcmp(text, previous)) return;
  bool hintNew = !text[0], hintOld = !(previous && previous[0]);
  size_t prefix = 0;
  if (previous && text[0] && previous[0])
    while (text[prefix] && text[prefix] == previous[prefix]) prefix++;
  int endNew = hintNew ? SEARCH_HINT_SHIFT + (int)strlen(hint) * 12 : (int)strlen(text) * 12 + 2;
  int endOld = hintOld ? SEARCH_HINT_SHIFT + (int)strlen(hint) * 12 : (int)strlen(previous) * 12 + 2;
  int end = endNew > endOld ? endNew : endOld;
  tft.fillRect(8 + prefix * 12, titleH + 4, end - (int)prefix * 12, 16, C_BLACK);
  tft.setTextSize(2);
  if (hintNew) { tft.setTextColor(C_GRAY, C_BLACK); tft.setCursor(8 + SEARCH_HINT_SHIFT, titleH + 4); tft.print(hint); }
  else { tft.setTextColor(C_WHITE, C_BLACK); tft.setCursor(8 + prefix * 12, titleH + 4); tft.print(text + prefix); }
  tft.fillRect(8 + (int)strlen(text) * 12, titleH + 4, 2, 16, C_WHITE);
}
// The search header is as tall as the Close X band (closex.h: 38 px on the 2.8 in, 42 on the 3.5 in); the query line is under it.
inline int searchTitleH(int W) { return cx::bandH(W); }
// Keep the footer's visible four-pixel gutters inert, including during Backspace repeat. Three keys: Space, Del, Done.
inline int keyboardAction(int W, int x) {
  if (x < 0 || x >= W) return -1;
  int k = x * 3 / W;
  return x >= k * W / 3 + 2 && x < (k + 1) * W / 3 - 2 ? k : -1;
}
// The header: the scope button, and the Close X at the right (it closes Search; Del, in the footer, removes letters).
constexpr int SEARCH_SCOPE_X0 = 4, SEARCH_SCOPE_GAP = 4;
inline int searchScopeX1(int W) { return cx::box(W).x0 - SEARCH_SCOPE_GAP; }
template <class G> void drawSearchHeader(G& tft, int W, bool all) {
  drawButton(tft, SEARCH_SCOPE_X0, cx::INSET, searchScopeX1(W), searchTitleH(W), all ? "All games" : "This category");
  cx::draw(tft, W);
}
template <class G> void drawKeyboard(G& tft, int W, int H, int titleH, const char* text, const char* hint = "Type a title", bool clear = true) {
  if (clear) tft.fillRect(0,0,W,H,C_BLACK);
  drawSearchText(tft, titleH, text, nullptr, hint);
  const char* keys = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int top = titleH + 26, step = (H - top - 38) / 4;
  for (int i = 0; i < 36; i++)
    drawLetterKey(tft, i % 9 * W / 9 + 2, top + i / 9 * step, (i % 9 + 1) * W / 9 - 2, top + (i / 9 + 1) * step - 2, keys[i]);
  for (int i = 0; i < 3; i++) drawActionButton(tft, i * W / 3 + 2, H - 36, (i + 1) * W / 3 - 2, H - 2, i == 0 ? "Space" : i == 1 ? "Del" : "Done");
}
}  // namespace pk

// Shared six-slot layout and clipped bitmap text: no font wrapping or page framebuffer.
#include "softfont.h"
#include "icons_data.h"
#include "favorites.h"
namespace pk {
inline int hitTile(int W,int x,int y) {for(int i=0;i<6;i++){auto r=tile(W,i);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)return i;}return -1;}
template<class G> void clippedText(G&g,const Tile&r,const char*s,int x,int y,int scale,uint16_t color) {
  for(;*s;s++,x+=6*scale)for(int yy=0;yy<8;yy++)for(int xx=0;xx<6;xx++)if(((unsigned char)*s>=32&&(unsigned char)*s<=126&&(SOFT_FONT[(unsigned char)*s-32][yy]&(1<<xx))))
    for(int dy=0;dy<scale;dy++)for(int dx=0;dx<scale;dx++){int px=x+xx*scale+dx,py=y+yy*scale+dy;if(px>=r.x&&px<r.x+r.w&&py>=r.y&&py<r.y+r.h)g.fillRect(px,py,1,1,color);}
}
// The chosen tile: a 3 px yellow frame (not color alone: it is also the thickest line on the page).
template<class G> void selFrame(G&g,const Tile&r) {g.drawRect(r.x+1,r.y+1,r.w-2,r.h-2,C_YELLOW);g.drawRect(r.x+2,r.y+2,r.w-4,r.h-4,C_YELLOW);}
// A category icon (icons_data.h) at 2x, centered on cx, top at y; transparent pixels take the tile color. panelOrder swaps bytes for the panel.
template<class G> void drawTileIcon(G&g,int icon,int cx,int y,bool panelOrder=false) {
  uint16_t row[32];
  for(int sy=0;sy<16;sy++) {
    for(int sx=0;sx<16;sx++){uint8_t v=ICONS[icon][sy*16+sx];uint16_t c=v?ICON_PALETTE[v]:C_DARK;if(panelOrder)c=(c<<8)|(c>>8);row[sx*2]=row[sx*2+1]=c;}
    g.pushImage(cx-16,y+sy*2,32,1,row);g.pushImage(cx-16,y+sy*2+1,32,1,row);
  }
}
// The same icon at 1x on the dark button backing, top left at x, y.
template<class G> void drawSmallIcon(G&g,int icon,int x,int y,bool panelOrder=false) {
  uint16_t row[16];
  for(int sy=0;sy<16;sy++){for(int sx=0;sx<16;sx++){uint8_t v=ICONS[icon][sy*16+sx];uint16_t c=v?ICON_PALETTE[v]:C_DARK;if(panelOrder)c=(c<<8)|(c>>8);row[sx]=c;}g.pushImage(x,y+sy,16,1,row);}
}
// A label broken at spaces into up to four lines that fit a tile.
inline int labelLines(const Tile&r,const char*label,int units,char lines[4][25]) {
  int cols=(r.w-8)/(3*units),count=0;if(cols>24)cols=24;size_t at=0,len=strlen(label);
  while(count<4&&at<len) {size_t n=len-at<(size_t)cols?len-at:cols;
    if(at+n<len){size_t word=n;while(word&&label[at+word]!=' ')word--;if(word)n=word;}
    memcpy(lines[count],label+at,n);lines[count][n]=0;count++;at+=n;while(label[at]==' ')at++;
  }
  return count;
}
// icon < 0: the name alone. The icon is dropped when the name needs its room.
template<class G> void tileLabel(G&g,int W,int i,const char*label,bool selected=false,int icon=-1,bool panelOrder=false) {
  auto r=tile(W,i);g.fillRect(r.x,r.y,r.w,r.h,C_DARK);g.drawRect(r.x,r.y,r.w,r.h,selected?C_YELLOW:C_GRAY);if(selected)selFrame(g,r);
  int units=W==320?3:4;char lines[4][25];int count=labelLines(r,label,units,lines);
  int text=count*5*units,icons=icon>=0&&icon<ICON_COUNT&&text+36<=r.h-4?36:0;
  int y=r.y+(r.h-text-icons)/2;
  if(icons){drawTileIcon(g,icon,r.x+r.w/2,y,panelOrder);y+=icons;}
  // 1.5x on the small board keeps whole words such as Favorites readable in a 96 px tile.
  // Integer pixel edges work through SoftGfx too (its built-in text sizes are whole numbers).
  for(int line=0;line<count;line++,y+=5*units) {
    int x=r.x+(r.w-(int)strlen(lines[line])*3*units)/2;
    for(const char* s=lines[line];*s;s++,x+=3*units)
      for(int yy=0;yy<8;yy++)for(int xx=0;xx<6;xx++)
        if((unsigned char)*s>=32&&(unsigned char)*s<=126&&(SOFT_FONT[(unsigned char)*s-32][yy]&(1<<xx)))
          g.fillRect(x+xx*units/2,y+yy*units/2,(xx+1)*units/2-xx*units/2,(yy+1)*units/2-yy*units/2,C_WHITE);
  }
}
// A name centered in a tile, in up to four lines broken at spaces (a game with no title picture shows its title this way).
template<class G> void tileName(G&g,const Tile&r,const char*name,int scale) {
  char lines[4][25];int count=labelLines(r,name,scale*2,lines),y=r.y+(r.h-count*10*scale)/2;
  for(int line=0;line<count;line++,y+=10*scale)clippedText(g,r,lines[line],r.x+(r.w-(int)strlen(lines[line])*6*scale)/2,y,scale,C_WHITE);
}
template<class G> void clearTiles(G&g,int W) {g.fillRect(0,0,W,footer(W),C_BLACK);}
// The Good on touch icon (16 = ICONS index) at 1x, no backing, with a dark outline so it reads over any picture: the mark of a game
// that scores 85 or more on the touch advisory. Bottom right of its tile, clear of the title art (the favorite's star is top right).
template<class G> void touchMark(G&g,int x,int y,bool panelOrder=false) {
  (void)panelOrder;  // fillRect takes plain colors
  auto on=[](int sx,int sy){return sx>=0&&sx<16&&sy>=0&&sy<16&&ICONS[16][sy*16+sx];};
  for(int sy=-1;sy<=16;sy++)for(int sx=-1;sx<=16;sx++){bool edge=false;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(on(sx+dx,sy+dy))edge=true;
    if(edge&&!on(sx,sy))g.fillRect(x+sx,y+sy,1,1,C_BLACK);}
  for(int sy=0;sy<16;sy++)for(int sx=0;sx<16;sx++)if(on(sx,sy))g.fillRect(x+sx,y+sy,1,1,ICON_PALETTE[ICONS[16][sy*16+sx]]);
}
// Details: one plain line about touch (a suggestion, from the advisory score) with the hand mark, drawn at y.
template<class G> void detailsTouch(G&g,int W,int y,const char*text) {
  Tile clip={8,y,W-16,18,0,0};g.fillRect(8,y,W-16,18,C_BLACK);touchMark(g,10,y+1);clippedText(g,clip,text,30,y+5,1,C_WHITE);
}
// A favorite's mark: a yellow star with a dark ring in the tile's top-right corner (over the picture's sky, never its middle).
template<class G> void starMark(G&g,int W,int i) {auto r=tile(W,i);int R=W==320?7:9;drawStar(g,r.x+r.w-R-3,r.y+R+3,R+2,C_BLACK);drawStar(g,r.x+r.w-R-3,r.y+R+3,R,C_YELLOW);}
// A Starter's caption under its picture: the course number in yellow, then the title in white, word-wrapped onto two lines
// of the 6 x 8 font, doubled on the 3.5 inch board's wider tiles (the title picture's own lettering is too small to read at tile size). Lines are cut with "..." to fit.
template<class G> void tileCaption(G&g,const Tile&r,int y,const char*title,int course) {
  int k2=r.w>=144?2:1,cols=(r.w-4)/(6*k2),x=r.x+2;char num[8]="";if(course>0)snprintf(num,sizeof num,"%d ",course);
  int used=(int)strlen(num);clippedText(g,r,num,x,y,k2,C_YELLOW);
  const char*t=title;while(*t==' ')t++;int len=(int)strlen(t),first=cols-used;
  if(first<1)first=1;
  int cut=len<=first?len:first;  // the first line breaks at the last space that fits
  if(len>first){for(int k=first;k>0;k--)if(t[k]==' '){cut=k;break;}}
  char a[64];int n=cut<(int)sizeof a-1?cut:(int)sizeof a-1;memcpy(a,t,n);a[n]=0;
  clippedText(g,r,a,x+used*6*k2,y,k2,C_WHITE);
  const char*rest=t+cut;while(*rest==' ')rest++;
  if(*rest){char b[64];ellipsize(rest,b,sizeof b,(size_t)cols);clippedText(g,r,b,x,y+9*k2,k2,C_WHITE);}
}
// caption: a Starter's title (with its course number, 0 for none) drawn as text under the picture, which then sits at the top.
// "On board": the game is kept in the board's own flash, not on the microSD card. A small black chip, bottom left of its tile.
template<class G> void boardMark(G&g,const Tile&r) {
  g.fillRect(r.x+1,r.y+r.h-11,8*6+4,10,C_BLACK);clippedText(g,r,"On board",r.x+3,r.y+r.h-10,1,C_YELLOW);
}
template<class G> void thumbnail(G&g,int W,int i,const uint8_t*raw,const char*dos,bool selected,bool panelOrder=false,bool pinned=false,
                                 const char*caption=nullptr,int course=0,bool touch=false,bool onBoard=false) {
  auto r=tile(W,i);g.fillRect(r.x,r.y,r.w,r.h,C_DARK);int y0=raw&&caption?r.y+1:r.y+(r.h-r.ph)/2;
  int shown=caption?r.ph*5/6:r.ph;  // a caption covers the picture's bottom sixth (a Starter's own "COURSE nnn" band)
  if(raw){uint16_t row[144];for(int y=0;y<shown;y++){
    for(int x=0;x<r.pw;x++){int idx=raw[524+(y*60/r.ph)*96+x*96/r.pw];uint16_t c=raw[12+idx*2]|(raw[13+idx*2]<<8);row[x]=panelOrder?(c<<8)|(c>>8):c;}
    g.pushImage(r.x,y0+y,r.pw,1,row);
  }if(caption)tileCaption(g,r,y0+shown+(r.w>=144?2:3),caption,course);
  }else tileName(g,r,dos,1);  // no title picture: the name main.cpp gives (the title, else the DOS name), wrapped; a picture is shown whole
  g.drawRect(r.x,r.y,r.w,r.h,selected?C_YELLOW:C_GRAY);if(selected)selFrame(g,r);
  if(selected){g.fillRect(r.x+4,r.y+4,52,12,C_YELLOW);clippedText(g,r,"SELECTED",r.x+6,r.y+6,1,C_BLACK);}
  if(pinned)starMark(g,W,i);
  if(touch)touchMark(g,r.x+r.w-18,r.y+r.h-18,panelOrder);
  if(onBoard)boardMark(g,r);
}
// Details' footer: the Favorite star (a toggle: outlined, or filled when the game is a favorite) and Play, in halves. Going back is
// the Close X in the header (closex.h), so the footer no longer repeats it. detailsButton: 1 Favorite, 2 Play.
inline int detailsButton(int W,int x) {return x<W/2?1:2;}
template<class G> void detailsFooter(G&g,int W,int H,bool pinned) {
  int y0=detailsFooterTop(H);g.fillRoundRect(W/2,y0,W-W/2,H-y0,6,C_YELLOW);
  Tile play={W/2,y0,W-W/2,H-y0,0,0};clippedText(g,play,"Play",(W/2+W)/2-24,y0+14,2,C_BLACK);drawButton(g,0,y0,W/2-4,H,"");
  bool s=W==320;int scale=s?1:2,R=s?9:11,tw=8*6*scale,x=(W/2-4-(2*R+6+tw))/2,cy=(y0+H)/2;
  drawStar(g,x+R,cy,R,C_YELLOW,!pinned,C_DARK);
  Tile clip={0,y0,W/2-4,H-y0,0,0};clippedText(g,clip,"Favorite",x+2*R+6,cy-4*scale,scale,pinned?C_YELLOW:C_WHITE);
}
// An empty Favorites page: how to star a game, or (hidden) that the starred games are on a card that is not in.
template<class G> void emptyFavorites(G&g,int W,bool hidden) {
  Tile clip={12,header(W)+12,W-24,120,0,0};int y=header(W)+28;
  drawStar(g,26,y+10,12,C_YELLOW,true,C_BLACK);
  clippedText(g,clip,hidden?"Card is out":"No favorites yet",48,y,2,C_WHITE);
  clippedText(g,clip,hidden?"Put your games' card back in.":"Open a game's Details",48,y+28,1,C_GRAY);
  if(!hidden)clippedText(g,clip,"and tap Favorite to save it here.",48,y+42,1,C_GRAY);
}
// Details: the buttons for the game's own pages (picker.h detailsPageSpan). Text is 6 by 8 on the small board, 12 by 16 on the large.
template<class G> void detailsPages(G&g,int W,int H,int mask) {
  int y0=detailsPagesTop(H),y1=detailsPagesBottom(H),scale=W==320?1:2;
  for(int k=0,x0,x1;k<DETAILS_PAGES;k++){if(!detailsPageSpan(W,mask,k,x0,x1))continue;
    g.fillRoundRect(x0,y0,x1-x0,y1-y0,6,C_DARK);g.drawRoundRect(x0,y0,x1-x0,y1-y0,6,C_WHITE);
    const char*l=detailsPageLabel(k);g.setTextSize(scale);g.setTextColor(C_WHITE,C_DARK);
    g.setCursor((x0+x1)/2-(int)strlen(l)*3*scale,(y0+y1)/2-4*scale);g.print(l);}
}
// A line under the title in Details: whether the game is a favorite, or that the list is full.
template<class G> void detailsNote(G&g,int W,int H,const char*note) {
  Tile clip={8,H-58,W-16,14,0,0};g.fillRect(8,H-58,W-16,14,C_BLACK);clippedText(g,clip,note,8,H-56,1,C_YELLOW);
}
// The home page's first tile, Continue: the game's title picture (raw: its thumbnail, or null), "Continue" and the level
// (0 based). cardOut: the game is on a card that is not in, so the tile says to put it back instead.
template<class G> void continueTile(G&g,int W,int i,const uint8_t*raw,int level,bool cardOut,bool panelOrder=false) {
  if(cardOut){tileLabel(g,W,i,"Put the card back to continue",false,-1,panelOrder);return;}
  auto r=tile(W,i);bool s=W==320;int scale=s?1:2,pw=s?64:96,ph=s?32:50,px0=r.x+(r.w-pw)/2,y=r.y+4;
  g.fillRect(r.x,r.y,r.w,r.h,C_DARK);
  if(raw){uint16_t row[144];for(int yy=0;yy<ph;yy++){
    for(int x=0;x<pw;x++){int idx=raw[524+(yy*60/ph)*96+x*96/pw];uint16_t c=raw[12+idx*2]|(raw[13+idx*2]<<8);row[x]=panelOrder?(c<<8)|(c>>8):c;}
    g.pushImage(px0,y+yy,pw,1,row);
  }}else{int cx=r.x+r.w/2,cy=y+ph/2,h=ph/3;g.fillTriangle(cx-h/2,cy-h,cx-h/2,cy+h,cx+h,cy,C_WHITE);}  // no picture: a play sign
  g.drawRect(px0-1,y-1,pw+2,ph+2,C_GRAY);
  y+=ph+4;char lv[16];snprintf(lv,sizeof lv,"Level %d",level+1);
  clippedText(g,r,"Continue",r.x+(r.w-8*6*scale)/2,y,scale,C_YELLOW);
  clippedText(g,r,lv,r.x+(r.w-(int)strlen(lv)*6*scale)/2,y+10*scale,scale,C_WHITE);
  g.drawRect(r.x,r.y,r.w,r.h,C_GRAY);
}
// The title bar's Search and Settings icons, 20 x 20 one-bit pictures (bit 19 is the left column), the same on both boards:
// a magnifier (a round lens with a thick handle at 45 degrees) and a gear with eight teeth and a round hole.
constexpr uint32_t MAGNIFIER20[20] = {0x00000,0x07E00,0x1FF80,0x3C3C0,0x381C0,0x700E0,0x700E0,0x700E0,0x700E0,0x700E0,
  0x700E0,0x381C0,0x3C3C0,0x1FFF0,0x07E78,0x0003C,0x0001E,0x0000F,0x00007,0x00003};
constexpr uint32_t GEAR20[20] = {0x00F00,0x00F00,0x18F18,0x3CF3C,0x3FFFC,0x1FFF8,0x0FFF0,0x0F0F0,0xFE07F,0xFE07F,
  0xFE07F,0xFE07F,0x0F0F0,0x0FFF0,0x1FFF8,0x3FFFC,0x3CF3C,0x18F18,0x00F00,0x00F00};
template<class G> void drawBits20(G&g,int x0,int y0,const uint32_t*rows,uint16_t c=C_WHITE) {
  for(int y=0;y<20;y++)for(int x=0;x<20;){if(!(rows[y]>>(19-x)&1)){x++;continue;}int e=x;while(e<20&&(rows[y]>>(19-e)&1))e++;g.fillRect(x0+x,y0+y,e-x,1,c);x=e;}
}
// A small castle badge for the home page's title bar (the UI pack's mark, drawn in code: no art in flash). 26 x 26 at x, y.
template<class G> void drawBadge(G&g,int x,int y) {
  const uint16_t SKY=0x4D9F,STONE=0xC618,SHADE=0x8410,GOLD=0xFE60,DOOR=0x4A49;
  g.fillRoundRect(x,y,26,26,5,SKY);g.drawRoundRect(x,y,26,26,5,C_WHITE);
  g.fillRect(x+4,y+10,18,12,STONE);g.fillRect(x+3,y+6,6,16,STONE);g.fillRect(x+17,y+6,6,16,STONE);
  for(int k=0;k<3;k++){g.fillRect(x+3+k*2+(k?0:0),y+4,2,3,STONE);g.fillRect(x+17+k*2,y+4,2,3,STONE);}
  g.fillRect(x+11,y+15,4,7,DOOR);g.fillRect(x+4,y+22,18,2,SHADE);g.fillRect(x+12,y+7,2,5,GOLD);g.fillRect(x+12,y+7,5,2,GOLD);
}
// Touch zones of the title bar and the footer, shared with main.cpp (the hit tests) so the drawing and the taps agree.
// Header, right to left: the Close X (closex.h; drawn only below the root page), the gear, the magnifier; 4 px gutters between.
inline int gearX0(int W) {return cx::box(W).x0-44;}
inline int searchX0(int W) {return gearX0(W)-44;}
inline bool hitClose(int W,int x) {return x>=cx::hitBox(W).x0&&x<cx::hitBox(W).x1;}
inline bool hitSearch(int W,int x) {return x>=searchX0(W)&&x<searchX0(W)+40;}
inline bool hitSettings(int W,int x) {return x>=gearX0(W)&&x<gearX0(W)+40;}
inline int pageArrow(int,int x) {return x<54?-1:(x>=58&&x<114)?1:0;}  // -1 back, 1 next, 0 neither
inline bool hitAction(int W,int x) {return x>=W-100;}
template<class G> void libraryFrame(G&g,int W,int H,const char*breadcrumb,int page,int pages,const char*action,bool back=true,bool picked=true) {
  // back=false on the root page (nothing above it); action=nullptr leaves the right-hand footer button off; the page arrows
  // go gray where there is no such page; picked=false (a game list, no game chosen yet) grays Details.
  // Header targets are 40 px wide and 40/44 px tall; the title is doubled when it fits.
  int bh=header(W);g.fillRect(0,0,W,bh,C_DARK);drawBadge(g,6,(bh-26)/2);if(back)cx::draw(g,W);
  int bx=38,room=searchX0(W)-4-bx,k=(int)strlen(breadcrumb)*12<=room?2:1;Tile clip={bx,0,room,bh,0,0};
  clippedText(g,clip,breadcrumb,bx,bh/2-4*k,k,C_WHITE);
  drawButton(g,searchX0(W),0,searchX0(W)+40,bh,"");drawButton(g,gearX0(W),0,gearX0(W)+40,bh,"");
  drawBits20(g,searchX0(W)+10,(bh-20)/2,MAGNIFIER20);drawBits20(g,gearX0(W)+10,(bh-20)/2,GEAR20);
  int f=footer(W);g.fillRect(0,f,W,H-f,C_BLACK);drawButton(g,0,f,54,H,"<",page>0);drawButton(g,58,f,114,H,">",page+1<pages);
  char count[24];snprintf(count,sizeof count,"%d/%d",page+1,pages?pages:1);
  Tile c={116,f,W-216,H-f,0,0};clippedText(g,c,count,122,f+(H-f-16)/2,2,C_WHITE);
  Tile section={12,bh,W-24,14,0,0};clippedText(g,section,action?"GAMES":"COLLECTIONS",12,bh+2,1,C_GRAY);
  bool details=action&&!strcmp(action,"Details");(void)picked;
  if(action)drawButton(g,W-100,f,W,H,action,!details||picked);
}
} // namespace pk
