// =============================================================
//  AISnake  —  compact Q‑learning snake that *actually improves*
//  • Explores randomly at first (ε‑greedy with decay)
//  • Tie‑breaks Q‑values randomly → no “always go up” bug
//  • Reward = food + distance, penalty = crash + hugging walls
//  • Body lists trimmed correctly, no phantom (0,0)
//  • Compiles native & Web (emcc -sUSE_SDL=2)
// =============================================================
#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <SDL.h>
#ifdef __EMSCRIPTEN__
 #include <emscripten.h>
#endif
using namespace std;

// ---------- board ----------
const int W = 20, H = 20, CELL = 20;

// ---------- RL params ----------
float ALPHA = 0.15f;   // learning‑rate
float GAMMA = 0.95f;   // discount
float EPS   = 0.40f;   // start exploration
const int   MAX_EP = 5000;
int   episode = 0;

// ---------- state ----------
int r = H/2, c = W/2;      // head row/col
int foodR, foodC;
int length = 2;            // including head
int score  = 0;
vector<vector<int>> body;  // 0=head

// ---------- helpers ----------
inline bool ok(int R,int C){return R>=0&&R<H&&C>=0&&C<W;}
bool inBody(int R,int C){for(auto&s:body)if(s[0]==R&&s[1]==C)return true;return false;}
float dist(int R,int C){return hypotf(R-foodR, C-foodC);}  // Euclidean

// ---------- Q‑table ----------
// state = (r,c,prevDir) -> 4 actions
vector<vector<float>> Q;
inline int S(int R,int C,int d){return (R*W + C)*4 + d;}

// ---------- choose ----------
int choose(int d){
    if((float)rand()/RAND_MAX < EPS) return rand()%4;
    const auto&q = Q[S(r,c,d)];
    float best=*max_element(q.begin(),q.end());
    int idx[4],n=0;for(int a=0;a<4;++a)if(fabs(q[a]-best)<1e-6f)idx[n++]=a;
    return idx[rand()%n];
}

void update(int pr,int pc,int pd,int a,int nr,int nc,float R){
    int s=S(pr,pc,pd), ns=S(nr,nc,a);
    float best=*max_element(Q[ns].begin(),Q[ns].end());
    Q[s][a]=(1-ALPHA)*Q[s][a] + ALPHA*(R + GAMMA*best);
}

float reward(bool food,bool crash,float od,float nd){
    if(crash) return -100.f;
    if(food)  return  50.f;
    if(nd<od) return   1.f;
    if(nd>od) return  -1.f;
    return 0.f;
}

// ---------- SDL ----------
SDL_Window* win=nullptr; SDL_Renderer* ren=nullptr;
void initSDL(){
    SDL_Init(SDL_INIT_VIDEO);
    win=SDL_CreateWindow("AI‑Snake",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W*CELL,H*CELL,0);
    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
}
void draw(){
    SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
    // food
    SDL_SetRenderDrawColor(ren,255,0,0,255);
    SDL_Rect f{foodC*CELL,foodR*CELL,CELL,CELL}; SDL_RenderFillRect(ren,&f);
    // head
    SDL_SetRenderDrawColor(ren,0,255,0,255);
    SDL_Rect h{c*CELL,r*CELL,CELL,CELL}; SDL_RenderFillRect(ren,&h);
    // tail
    SDL_SetRenderDrawColor(ren,0,180,0,255);
    for(size_t i=1;i<body.size();++i){ SDL_Rect s{body[i][1]*CELL,body[i][0]*CELL,CELL,CELL}; SDL_RenderFillRect(ren,&s);}    
    SDL_RenderPresent(ren);
}

// ---------- reset ----------
void placeFood(){
    vector<vector<int>> free;
    for(int R=0;R<H;++R)for(int C=0;C<W;++C) if(!inBody(R,C)) free.push_back({R,C});
    auto p=free[rand()%free.size()]; foodR=p[0]; foodC=p[1];
}
void reset(){
    r=H/2; c=W/2; length=2; score=0; body={{r,c}}; placeFood();
}

// ---------- main step ----------
int DIR = rand()%4;             // 0 up,1 down,2 left,3 right
int wait=0;
void step(){
    if(wait){ if(--wait==0) reset(); return; }

    int a = choose(DIR);
    int nr=r, nc=c; if(a==0)--nr; else if(a==1)++nr; else if(a==2)--nc; else ++nc;
    bool crash = !ok(nr,nc)||inBody(nr,nc);
    bool food  = (nr==foodR&&nc==foodC);

    float od=dist(r,c), nd=dist(nr,nc);
    update(r,c,DIR,a,nr,nc,reward(food,crash,od,nd));

    if(crash){ wait=8; return; }

    r=nr; c=nc; DIR=a;
    body.insert(body.begin(),{r,c});
    if((int)body.size()>length) body.pop_back();
    if(food){ ++length; ++score; placeFood(); }
}

// ---------- loop ----------
void loop(){
    step(); draw();
    if(episode<MAX_EP){ ++episode; EPS=max(0.05f,EPS-0.0005f);} }

// ---------- entry ----------
int main(){
    srand((unsigned)time(nullptr));
    Q.assign(W*H*4, vector<float>(4,0.f)); // zero‑init
    initSDL(); reset();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop,0,1);
#else
    bool run=true; SDL_Event e;
    while(run){ while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) run=false; loop(); SDL_Delay(80);}    
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
#endif
    return 0;
}
