#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>
#include "sprite_idle0.h"
#include "sprite_walk0.h"

#define SCREEN_W 960.0f
#define SCREEN_H 544.0f
#define WORLD_W 3600.0f
#define GRAVITY 0.72f
#define MOVE_SPEED 4.8f
#define JUMP_SPEED 13.5f

typedef struct { float x,y,w,h; } Rect;
typedef struct { Rect r; int collected; } Coin;
typedef struct { Rect r; float vx,min_x,max_x; int alive; } Enemy;

static int intersects(Rect a, Rect b){ return a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y; }
static float clampf(float v,float lo,float hi){ if(v<lo)return lo; if(v>hi)return hi; return v; }
static float stick_x(unsigned char v){ int c=(int)v-128; if(abs(c)<18)return 0.0f; return c/127.0f; }

static int b64v(char c){
 if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26;
 if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; return -1;
}
static unsigned char *decode64(const char *s,size_t *outn){
 size_t n=strlen(s),oi=0; unsigned char *out=malloc(n*3/4+4); if(!out)return NULL;
 unsigned int acc=0; int bits=0;
 for(size_t i=0;i<n;i++){ if(s[i]=='=')break; int v=b64v(s[i]); if(v<0)continue; acc=(acc<<6)|(unsigned int)v; bits+=6; if(bits>=8){ bits-=8; out[oi++]=(unsigned char)((acc>>bits)&255u); if(bits==0)acc=0; else acc&=(1u<<bits)-1u; }}
 *outn=oi; return out;
}
static vita2d_texture *load64(const char *s){ size_t n=0; unsigned char *p=decode64(s,&n); if(!p)return NULL; vita2d_texture *t=vita2d_load_PNG_buffer(p); free(p); return t; }

static void draw_sprite(vita2d_texture *t,Rect p,float cam,int left){
 if(!t){ vita2d_draw_rectangle(p.x-cam,p.y,p.w,p.h,RGBA8(70,105,235,255)); return; }
 float tw=(float)vita2d_texture_get_width(t), th=(float)vita2d_texture_get_height(t);
 float h=112.0f, sc=h/th, w=tw*sc; float x=p.x-cam+(p.w-w)*0.5f, y=p.y+p.h-h+4.0f;
 if(left) vita2d_draw_texture_scale(t,x+w,y,-sc,sc); else vita2d_draw_texture_scale(t,x,y,sc,sc);
}

static void reset_game(Rect *p,float *vx,float *vy,int *ground,int *dead,int *won,int *gold,Coin *coins,int nc,Enemy *en,int ne){
 p->x=120;p->y=360;*vx=*vy=0;*ground=*dead=*won=*gold=0; for(int i=0;i<nc;i++)coins[i].collected=0; for(int i=0;i<ne;i++)en[i].alive=1;
}

int main(void){
 sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG); vita2d_init(); vita2d_set_clear_color(RGBA8(120,190,255,255));
 vita2d_pgf *font=vita2d_load_default_pgf();
 vita2d_texture *idle=load64(idle0_png_b64); vita2d_texture *walk=load64(walk0_png_b64);
 Rect player={120,360,52,66}; float vx=0,vy=0; int ground=0;
 Rect platforms[]={{0,470,900,74},{980,470,520,74},{1600,470,720,74},{2420,470,1180,74},{420,385,180,28},{760,320,180,28},{1120,365,210,28},{1450,300,180,28},{1800,365,210,28},{2130,290,180,28},{2590,385,190,28},{2920,320,180,28},{3260,255,170,28}};
 const int np=sizeof(platforms)/sizeof(platforms[0]);
 Coin coins[]={{{470,345,24,24},0},{{820,280,24,24},0},{{1180,325,24,24},0},{{1500,260,24,24},0},{{1880,325,24,24},0},{{2180,250,24,24},0},{{2660,345,24,24},0},{{2980,280,24,24},0},{{3320,215,24,24},0}}; const int nc=sizeof(coins)/sizeof(coins[0]);
 Enemy en[]={{{680,428,48,42},1.6f,620,850,1},{{1700,428,48,42},1.8f,1650,2150,1},{{2760,428,48,42},2.0f,2600,3200,1}}; const int ne=sizeof(en)/sizeof(en[0]);
 Rect goal={3480,350,56,120}; int gold=0,dead=0,won=0,intro=1,running=1,left=0; float cam=0; unsigned int tick=0,old=0; SceCtrlData pad; memset(&pad,0,sizeof(pad));
 while(running){ tick++; sceCtrlPeekBufferPositive(0,&pad,1); unsigned int pressed=pad.buttons&~old; if(pressed&SCE_CTRL_START)running=0;
  if(intro){ if(pressed&SCE_CTRL_CROSS)intro=0; }
  else if(dead||won){ if(pressed&SCE_CTRL_CROSS){ reset_game(&player,&vx,&vy,&ground,&dead,&won,&gold,coins,nc,en,ne);cam=0; } }
  else{
   float ix=stick_x(pad.lx); vx=ix*MOVE_SPEED; if(ix<-.08f)left=1;else if(ix>.08f)left=0;
   if((pressed&SCE_CTRL_CROSS)&&ground){vy=-JUMP_SPEED;ground=0;}
   player.x+=vx;player.x=clampf(player.x,0,WORLD_W-player.w);
   for(int i=0;i<np;i++)if(intersects(player,platforms[i])){if(vx>0)player.x=platforms[i].x-player.w;else if(vx<0)player.x=platforms[i].x+platforms[i].w;}
   float oy=player.y;vy+=GRAVITY;if(vy>18)vy=18;player.y+=vy;ground=0;
   for(int i=0;i<np;i++)if(intersects(player,platforms[i])){if(vy>0&&oy+player.h<=platforms[i].y+8){player.y=platforms[i].y-player.h;vy=0;ground=1;}else if(vy<0&&oy>=platforms[i].y+platforms[i].h-8){player.y=platforms[i].y+platforms[i].h;vy=0;}}
   if(player.y>SCREEN_H+140)dead=1;
   for(int i=0;i<nc;i++)if(!coins[i].collected&&intersects(player,coins[i].r)){coins[i].collected=1;gold++;}
   for(int i=0;i<ne;i++)if(en[i].alive){en[i].r.x+=en[i].vx;if(en[i].r.x<=en[i].min_x||en[i].r.x+en[i].r.w>=en[i].max_x)en[i].vx*=-1;if(intersects(player,en[i].r)){if(vy>0&&oy+player.h<=en[i].r.y+12){en[i].alive=0;vy=-9;}else dead=1;}}
   if(intersects(player,goal)&&gold==nc)won=1;float target=player.x-300;cam+=(target-cam)*.12f;cam=clampf(cam,0,WORLD_W-SCREEN_W);
  }
  vita2d_start_drawing();vita2d_clear_screen();
  if(intro){vita2d_draw_rectangle(100,80,760,360,RGBA8(0,0,0,190));vita2d_pgf_draw_text(font,250,145,RGBA8(255,255,255,255),1.7f,"PLATFORMER TEST");vita2d_pgf_draw_text(font,220,220,RGBA8(235,235,235,255),1.0f,"LEFT STICK: MOVE");vita2d_pgf_draw_text(font,220,265,RGBA8(235,235,235,255),1.0f,"X: JUMP / STOMP ENEMIES");vita2d_pgf_draw_text(font,345,390,RGBA8(200,215,235,255),1.0f,"PRESS X TO START");}
  else{
   for(int i=0;i<np;i++){float x=platforms[i].x-cam;if(x+platforms[i].w<0||x>SCREEN_W)continue;vita2d_draw_rectangle(x,platforms[i].y,platforms[i].w,platforms[i].h,RGBA8(90,175,75,255));vita2d_draw_rectangle(x,platforms[i].y,platforms[i].w,8,RGBA8(65,125,55,255));}
   for(int i=0;i<nc;i++)if(!coins[i].collected)vita2d_draw_rectangle(coins[i].r.x-cam,coins[i].r.y,coins[i].r.w,coins[i].r.h,RGBA8(255,220,45,255));
   for(int i=0;i<ne;i++)if(en[i].alive)vita2d_draw_rectangle(en[i].r.x-cam,en[i].r.y,en[i].r.w,en[i].r.h,RGBA8(220,70,70,255));
   float gx=goal.x-cam;vita2d_draw_rectangle(gx,goal.y,8,goal.h,RGBA8(230,230,230,255));vita2d_draw_rectangle(gx+8,goal.y,48,34,gold==nc?RGBA8(80,220,110,255):RGBA8(130,130,130,255));
   vita2d_texture *frame=idle; if(!ground)frame=walk; else if(fabsf(vx)>.3f)frame=((tick/7)%2)?walk:idle; draw_sprite(frame,player,cam,left);
   char hud[64];snprintf(hud,sizeof(hud),"GOLD %d/%d",gold,nc);vita2d_pgf_draw_text(font,24,38,RGBA8(255,255,255,255),1.0f,hud);
   if(dead){vita2d_draw_rectangle(245,185,470,170,RGBA8(0,0,0,215));vita2d_pgf_draw_text(font,370,245,RGBA8(255,100,100,255),1.5f,"YOU DIED");vita2d_pgf_draw_text(font,325,305,RGBA8(255,255,255,255),1.0f,"PRESS X TO RESTART");}
   if(won){vita2d_draw_rectangle(210,170,540,190,RGBA8(0,0,0,215));vita2d_pgf_draw_text(font,315,235,RGBA8(255,225,80,255),1.5f,"LEVEL COMPLETE!");}
  }
  vita2d_end_drawing();vita2d_swap_buffers();old=pad.buttons;
 }
 if(idle)vita2d_free_texture(idle);if(walk)vita2d_free_texture(walk);vita2d_free_pgf(font);vita2d_fini();sceKernelExitProcess(0);return 0;
}
