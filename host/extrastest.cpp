// Shared firmware compositions and the exact library press regions, without board access.
// extrastest DATA OUT; compile with -lz, and -DPOCKET_CYD28 for the small Settings layout.
#include <cassert>
#include <fstream>
namespace textdatum_t { constexpr int middle_center=4, top_left=0; }
#include "canvas.h"
#include "cardmsg.h"
#include "pressrect.h"
#include "library.h"
#include "stdiolines.h"
struct Shot : Canvas {
  using Canvas::Canvas;
  void pushImage(int x,int y,int w,int h,const uint16_t* p) {
    for(int j=0;j<h;j++)for(int i=0;i<w;i++)put(x+i,y+j,p[j*w+i]);
  }
};
int main(int argc,char** argv) {
  assert(argc==3);
  linesDir=argv[1];std::string out=argv[2],w=std::to_string(scr::W);
  Shot g(scr::W,scr::H);
  auto save=[&](const std::string& name){writePng(out+"/extras-"+name+"-"+w+".png",g.W,g.H,rgbOf(g));};
  prs::Ctx c;c.W=g.W;c.H=g.H;c.tiles=6;c.back=c.prev=c.next=c.action=true;
  // Every pixel in each rendered target must have the same press identity; gaps do nothing.
  for(int i=0;i<6;i++){
    auto t=pk::tile(g.W,i);assert(t.h>=36&&t.y>=pk::header(g.W)&&t.y+t.h<=pk::footer(g.W));
    for(int y=t.y;y<t.y+t.h;y++)for(int x=t.x;x<t.x+t.w;x++){
      prs::Rect r;assert(pk::hitTile(g.W,x,y)==i&&prs::under(c,x,y,r));
      assert(r.x0==t.x&&r.y0==t.y&&r.x1==t.x+t.w&&r.y1==t.y+t.h);
    }
  }
  for(int x=0;x<g.W;x++)for(int y=0;y<pk::header(g.W);y++){
    prs::Rect r;bool live=pk::hitBack(g.W,x)||pk::hitSearch(g.W,x)||pk::hitSettings(g.W,x);
    assert(prs::under(c,x,y,r)==live);
    if(live)assert(r.y1==pk::header(g.W)&&r.x1-r.x0>=36);
  }
  for(int y=pk::footer(g.W);y<g.H;y++)for(int x=0;x<g.W;x++){
    prs::Rect r;bool live=pk::pageArrow(g.W,x)!=0||pk::hitAction(g.W,x);
    assert(prs::under(c,x,y,r)==live);
    if(live)assert(r.y0==pk::footer(g.W)&&r.y1-r.y0>=36);
  }
  // Acquisition gutters must be inert: a finger landing between different actions
  // cannot choose one merely because the drawing left a gap.
  for(int x=g.W-44;x<g.W-40;x++) {
    prs::Rect r;assert(!prs::under(c,x,10,r));
  }
  for(int x=54;x<58;x++) {
    prs::Rect r;assert(!prs::under(c,x,g.H-10,r));
  }
  for(int k=0;k<3;k++)for(int x=k*g.W/3;x<(k+1)*g.W/3;x++) {
    int action=pk::keyboardAction(g.W,x);
    assert(action==(x>=k*g.W/3+2&&x<(k+1)*g.W/3-2?k:-1));
  }
  assert(pk::keyboardAction(g.W,-1)==-1&&pk::keyboardAction(g.W,g.W)==-1);
  assert(pk::SEARCH_HEADER_HIT_H>=36&&pk::SEARCH_HEADER_HIT_H<=58);
  // The isolated failure action gains slop without changing its drawing.
  auto fail=pk::failButton(g.W,g.H);
  assert(pk::inFailButton(g.W,g.H,fail.x,fail.y-4));
  assert(pk::inFailButton(g.W,g.H,fail.x,fail.y+fail.h+3));
  assert(!pk::inFailButton(g.W,g.H,fail.x,fail.y-5));
  assert(!pk::inFailButton(g.W,g.H,fail.x,fail.y+fail.h+4));
  c.emptyCheck=true;c.tiles=0;
  auto eb=pk::emptyButton(g.W);
  for(int y=pk::header(g.W);y<pk::footer(g.W);y++)for(int x=0;x<g.W;x++){
    prs::Rect r;assert(prs::under(c,x,y,r)==pk::inEmptyButton(g.W,x,y));
  }
  c.emptyCheck=false;c.screen=prs::SC_DETAILS;
  for(int boundary : {g.W/3,2*g.W/3})for(int x=boundary-4;x<boundary;x++) {
    prs::Rect r;assert(!prs::under(c,x,g.H-10,r));
  }
  for(int mask=0;mask<8;mask++){
    c.pages=mask;
    for(int k=0,x0,x1;k<3;k++)if(pk::detailsPageSpan(g.W,mask,k,x0,x1)){
      prs::Rect r;assert(prs::under(c,x0,pk::detailsPagesTop(g.H),r));
      assert(r.x0==x0&&r.x1==x1&&r.y1-r.y0>=36);
    }
  }
  pk::clearTiles(g,g.W);pk::libraryFrame(g,g.W,g.H,"Pocket Player",0,2,nullptr,false);
  const char* names[]={"Continue","Recently played","Favorites","Starters","Sandbox","Bizarre"};
  for(int i=0;i<6;i++)pk::tileLabel(g,g.W,i,names[i],false,i==0?0:i==1?19:i==2?20:i==3?6:i==4?9:10);
  pk::drawCardChip(g,g.W,pk::CARD_NONE,0);save("collections");
  pk::clearTiles(g,g.W);pk::libraryFrame(g,g.W,g.H,"First Steps",0,2,"Details");
  std::ifstream f(linesDir+"/library.jsonl");std::string line;int n=0;
  while(n<6&&std::getline(f,line)){
    pk::Meta m;if(!pk::metadata(line.c_str(),line.size(),m,true)||strcmp(m.root,"starter")||!m.thumb[0])continue;
    FILE* tf=fopen((linesDir+"/thumbs.pak").c_str(),"rb");assert(tf);
    gm::SmallPak pak;pak.user=tf;pak.readAt=pakReadAt;fseek(tf,0,SEEK_END);pak.size=ftell(tf);assert(pak.begin());
    uint32_t off,len;assert(pak.find(m.thumb,&off,&len));std::vector<uint8_t> packed(len),raw(pk::THUMB_RAW);assert(pakReadAt(tf,off,packed.data(),len));fclose(tf);
    z_stream z={};assert(inflateInit2(&z,-15)==Z_OK);z.next_in=packed.data()+4;z.avail_in=len-4;z.next_out=raw.data();z.avail_out=raw.size();assert(inflate(&z,Z_FINISH)==Z_STREAM_END);inflateEnd(&z);
    pk::thumbnail(g,g.W,n,raw.data(),m.title,n==1,false,false,m.title,m.course);n++;
  }
  assert(n==6);save("games");
  pk::detailsScreen(g,g.W,g.H,"Moving and Pushing","Starters  /  Built-in Starter","Plays well on touch",7,true);save("details");
  pk::detailsScreen(g,g.W,g.H,"A very long game title that wraps across the available display width","Imported  /  microSD","Needs quick, exact jumps",0,false);save("details-long");
  for(auto state:{pk::CARD_NONE,pk::CARD_NOFOLDER,pk::CARD_UNREADABLE,pk::CARD_READING}){
    pk::clearTiles(g,g.W);pk::libraryFrame(g,g.W,g.H,"Pocket Player",0,1,nullptr,false);pk::drawCardChip(g,g.W,state,0);
    int end=pk::drawEmptyNotice(g,g.W,state,0);assert(end<=(state==pk::CARD_READING?pk::footer(g.W):eb.y));save("empty-"+std::to_string(state));
  }
  pk::clearTiles(g,g.W);pk::libraryFrame(g,g.W,g.H,"Search: All games",0,1,"Clear");pk::emptyResults(g,g.W,true);save("search-empty");
  pk::clearTiles(g,g.W);pk::libraryFrame(g,g.W,g.H,"Imported",0,1,nullptr);pk::emptyResults(g,g.W,false);save("category-empty");
  for(bool hidden:{false,true}){
    pk::clearTiles(g,g.W);pk::libraryFrame(g,g.W,g.H,"Favorites",0,1,nullptr);pk::emptyFavorites(g,g.W,hidden);save(hidden?"favorites-card-out":"favorites-empty");
  }
  st::Values vals;ctl::Need need;ctl::Layout L=ctl::makeLayout(vals.s,need);
  for(auto tab:{st::T_DISPLAY,st::T_SOUND,st::T_CARD}){
    st::View v;v.tab=tab;st::CardInfo card;card.builtIn=106;strcpy(card.state,"No card");strcpy(card.flash,"Built-in: 380 KB free");strcpy(card.board,"Board C fw 28428f4+ 2026-11-10");
    st::screen(g,v,vals,card,L,need,0,0,0);save(st::tabName(tab));
  }
  printf("extrastest %s: passed\n",w.c_str());
}
