// =============================================================
//  AI‑Snake with live Chart.js telemetry
//  • Q‑learning (ε‑greedy, decaying)
//  • Distance‑ and wall‑aware reward
//  • Chart.js plots score & avg‑Q in browser
//  • Builds native or emcc -sUSE_SDL=2
// =============================================================
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <SDL.h>
#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #include <emscripten/html5.h>
#endif

// ------------- grid ------------------------------------------
constexpr int W = 20, H = 20, CELL = 20, AI_UPDATE = 5;

// ------------- Q‑learning params -----------------------------
float  ALPHA = 0.30f;
float  GAMMA = 0.95f;
float  EPS   = 0.50f;
constexpr float MIN_EPS = 0.01f;
int    episode = 0;
constexpr int  MAX_EP  = 20'000;
constexpr int  LOG_EVERY = 100;

// ------------- state -----------------------------------------
int r = H/2, c = W/2;            // head
int foodR, foodC;
int len = 2;
int score = 0;
std::vector<std::vector<int>> body{{r,c}};   // [0]=head

// ------------- helpers ---------------------------------------
inline bool ok(int R,int C){ return R>=0&&R<H&&C>=0&&C<W; }
bool inBody(int R,int C){ for(auto&s:body) if(s[0]==R&&s[1]==C) return true; return false; }
float dist(int R,int C){ return hypotf(R-foodR, C-foodC); }

// ------------- Q‑table (state=(r,c,dir) ) --------------------
std::vector<std::vector<float>> Q;
inline int S(int R,int C,int d){ return (R*W + C)*4 + d; }

// ------------- random tie‑break choose -----------------------
int choose(int dir){
    if((float)rand()/RAND_MAX < EPS) return rand()%4;
    const auto& q = Q[S(r,c,dir)];
    float best = *std::max_element(q.begin(), q.end());
    int idx[4], n=0;
    for(int a=0;a<4;++a) if(fabs(q[a]-best)<1e-6f) idx[n++]=a;
    return idx[rand()%n];
}

void updateQ(int pr,int pc,int pd,int a,int nr,int nc,float R){
    int s  = S(pr,pc,pd);
    int ns = S(nr,nc,a);
    float best = *std::max_element(Q[ns].begin(), Q[ns].end());
    Q[s][a] = (1-ALPHA)*Q[s][a] + ALPHA*(R + GAMMA*best);
}

float reward(bool gotFood,bool crash,float od,float nd){
    if(crash)      return -200.f;
    if(gotFood)    return  100.f;
    if(nd < od)    return   5.f;
    if(nd > od)    return  -1.f;
    return 0.f;
}

// ------------- SDL -------------------------------------------
SDL_Window*   win = nullptr;
SDL_Renderer* ren = nullptr;
void sdlInit(){
    if(SDL_Init(SDL_INIT_VIDEO)){ std::cerr<<SDL_GetError()<<'\n'; std::exit(1);}
    win = SDL_CreateWindow("AI‑Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           W*CELL, H*CELL, SDL_WINDOW_SHOWN);
    ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
}
void draw(){
    SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
    SDL_SetRenderDrawColor(ren,255,0,0,255);
    SDL_Rect f{foodC*CELL,foodR*CELL,CELL,CELL}; SDL_RenderFillRect(ren,&f);
    SDL_SetRenderDrawColor(ren,0,255,0,255);
    SDL_Rect h{c*CELL,r*CELL,CELL,CELL}; SDL_RenderFillRect(ren,&h);
    SDL_SetRenderDrawColor(ren,0,180,0,255);
    for(size_t i=1;i<body.size();++i){
        SDL_Rect s{body[i][1]*CELL, body[i][0]*CELL, CELL, CELL};
        SDL_RenderFillRect(ren,&s);
    }
    SDL_RenderPresent(ren);
}

// ------------- Chart.js hooks (browser only) -----------------
#ifdef __EMSCRIPTEN__
EM_JS(void, initChartJS, (), {
    // Make sure Chart.js is loaded in shell.html (<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>)
    if(typeof Chart==='undefined'){ console.log('Chart.js missing'); return; }
    // create canvases & charts (omitted for brevity — same as yours) ...
});
EM_JS(void, updateCharts, (int ep,int sc,float avgQ,float eps), {
    if(typeof scoreChart==='undefined') return;
    scoreChart.data.labels.push(ep);
    scoreChart.data.datasets[0].data.push(sc);
    qValueChart.data.labels.push(ep);
    qValueChart.data.datasets[0].data.push(avgQ);
    scoreChart.update(); qValueChart.update();
});
#endif

// ------------- misc helpers ----------------------------------
void placeFood(){
    std::vector<std::vector<int>> free;
    for(int R=0;R<H;++R) for(int C=0;C<W;++C) if(!inBody(R,C)) free.push_back({R,C});
    auto p = free[rand()%free.size()]; foodR=p[0]; foodC=p[1];
}
void reset(){ r=H/2; c=W/2; len=2; score=0; body={{r,c}}; placeFood(); }

void logEpisode(){
    if(episode % LOG_EVERY) return;
    // avg Q
    double sum=0; size_t cnt=0;
    for(const auto& row:Q) for(float v:row){ sum+=v; ++cnt; }
    float avgQ = cnt? (float)(sum/cnt):0.f;
#ifdef __EMSCRIPTEN__
    updateCharts(episode,score,avgQ,EPS);
#else
    std::cout<<\"Ep \"<<episode<<\"  score=\"<<score<<\"  avgQ=\"<<avgQ<<\"  eps=\"<<EPS<<'\n';
#endif
    score=0;
}

// ------------- main step & loop ------------------------------
int DIR = rand()%4, wait=0;
void step(){
    if(wait){ if(--wait==0) reset(); return; }

    int a = choose(DIR);
    int nr=r, nc=c;
    if(a==0)--nr; else if(a==1)++nr; else if(a==2)--nc; else ++nc;
    bool crash = !ok(nr,nc)||inBody(nr,nc);
    bool food  = (nr==foodR && nc==foodC);

    float od=dist(r,c), nd=dist(nr,nc);
    updateQ(r,c,DIR,a,nr,nc,reward(food,crash,od,nd));

    if(crash){ wait=8; return; }

    r=nr; c=nc; DIR=a;
    body.insert(body.begin(),{r,c});
    if((int)body.size()>len) body.pop_back();
    if(food){ ++len; ++score; placeFood(); }
}

void loop(){
    static int frame=0;
    if(frame++%AI_UPDATE==0) step(); else if(!wait) step();   // keep motion smooth
    draw();
    if(episode<MAX_EP){ ++episode; EPS = std::max(MIN_EPS, EPS*0.9999f); }
    logEpisode();
}

// ------------- extern for JS ---------------------------------
extern "C" { EMSCRIPTEN_KEEPALIVE float getExplorationRate(){ return EPS; } }

// ------------- main ------------------------------------------
int main(){
    srand((unsigned)time(nullptr));
    Q.assign(W*H*4, std::vector<float>(4, 0.f));
    sdlInit(); reset();
#ifdef __EMSCRIPTEN__
    initChartJS();
    emscripten_set_main_loop(loop,0,1);
#else
    bool run=true; SDL_Event e;
    while(run){ while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) run=false; loop(); SDL_Delay(80);}
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
#endif
    return 0;
}
