// =========================  AI‑Snake  =========================
// A minimal Q‑learning snake that compiles both natively and
// with Emscripten (-sUSE_SDL=2).
// --------------------------------------------------------------
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <queue>
#include <cmath>
#include <SDL.h>               // <SDL.h> works on desktop & Web
#ifdef __EMSCRIPTEN__
 #include <emscripten.h>
#endif

using namespace std;

// --------------------------------------------------------------
// Board‑level constants
// --------------------------------------------------------------
const int WIDTH  = 20;          // columns
const int HEIGHT = 20;          // rows
const int CELL   = 20;          // pixel size of each grid cell
const int AI_UPDATE_INTERVAL = 5;   // RL update cadence (frames)

// --------------------------------------------------------------
// RL parameters
// --------------------------------------------------------------
float ALPHA   = 0.10f;          // learning‑rate
float GAMMA   = 0.90f;          // discount
float EPS     = 0.30f;          // ε‑greedy (decays)
const int MAX_EP = 1000;        // training episodes for decay

// --------------------------------------------------------------
// Game state
// --------------------------------------------------------------
int  headR, headC;              // head row/col
int  foodR, foodC;              // food row/col
int  length = 2;                // body length (incl. head)
int  score  = 0;                // food eaten

vector<vector<int>> body;       // 0 = head, rest tail
vector<vector<int>> allCells;   // cached list of every {r,c}

// --------------------------------------------------------------
// Q‑table:  state = (row, col, prev‑dir)  => 4 action‑values
// --------------------------------------------------------------
vector<vector<float>> Q;        // size HEIGHT*WIDTH*4 × 4

inline int stateIdx(int r,int c,int d){ return (r*WIDTH + c)*4 + d; }

// --------------------------------------------------------------
// Helpers
// --------------------------------------------------------------
inline bool valid(int r,int c){ return r>=0 && r<HEIGHT && c>=0 && c<WIDTH; }

bool inBody(int r,int c){
    for(auto &s:body) if(s[0]==r && s[1]==c) return true;
    return false;
}

vector<vector<int>> freeCells(){
    vector<vector<int>> f=allCells;
    for(auto &s:body)   f.erase(remove(f.begin(),f.end(),s),f.end());
    return f;
}

float dist(int r,int c){ return hypotf(float(r-foodR), float(c-foodC)); }

// --------------------------------------------------------------
// Q‑learning helpers
// --------------------------------------------------------------
int chooseAction(int r,int c,int dir)
{
    // ε‑greedy with random tie‑break
    if( (float)rand()/static_cast<float>(RAND_MAX) < EPS ) return rand()%4;

    const auto &q = Q[stateIdx(r,c,dir)];
    float best = *max_element(q.begin(),q.end());
    int bestA[4], n=0;
    for(int a=0;a<4;++a) if(fabs(q[a]-best)<1e-6f) bestA[n++]=a;
    return bestA[rand()%n];
}

void updateQ(int r,int c,int dir,int a,int nr,int nc,float reward)
{
    int s  = stateIdx(r,c,dir);
    int ns = stateIdx(nr,nc,a);
    float bestNext = *max_element(Q[ns].begin(), Q[ns].end());
    Q[s][a] = (1-ALPHA)*Q[s][a] + ALPHA*(reward + GAMMA*bestNext);
}

float rewardFn(bool gotFood,bool crash,float oldD,float newD)
{
    if(crash)      return -50.f;
    if(gotFood)    return 100.f;
    if(newD < oldD) return  1.f;   // moved closer
    if(newD > oldD) return -1.f;   // moved farther
    return 0.f;
}

// --------------------------------------------------------------
// SDL handles
// --------------------------------------------------------------
SDL_Window  *win  = nullptr;
SDL_Renderer* ren = nullptr;

void sdlInit()
{
    if(SDL_Init(SDL_INIT_VIDEO)) { cerr<<"SDL_Init: "<<SDL_GetError()<<"\n"; exit(1);}    
    win = SDL_CreateWindow("AI‑Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           WIDTH*CELL, HEIGHT*CELL, SDL_WINDOW_SHOWN);
    if(!win){ cerr<<"CreateWindow: "<<SDL_GetError()<<"\n"; exit(1);}    
    ren = SDL_CreateRenderer(win,-1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){ cerr<<"CreateRenderer: "<<SDL_GetError()<<"\n"; exit(1);}    
}

void draw()
{
    SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
    // border
    SDL_SetRenderDrawColor(ren,40,40,40,255);
    SDL_Rect b{0,0, WIDTH*CELL, HEIGHT*CELL}; SDL_RenderDrawRect(ren,&b);
    // food
    SDL_SetRenderDrawColor(ren,255,0,0,255);
    SDL_Rect f{foodC*CELL, foodR*CELL, CELL, CELL}; SDL_RenderFillRect(ren,&f);
    // head
    SDL_SetRenderDrawColor(ren,0,255,0,255);
    SDL_Rect h{headC*CELL, headR*CELL, CELL, CELL}; SDL_RenderFillRect(ren,&h);
    // tail
    SDL_SetRenderDrawColor(ren,0,180,0,255);
    for(int i=1;i<length && i<(int)body.size();++i){
        SDL_Rect s{ body[i][1]*CELL, body[i][0]*CELL, CELL, CELL};
        SDL_RenderFillRect(ren,&s);
    }
    SDL_RenderPresent(ren);
}

// --------------------------------------------------------------
// Game reset
// --------------------------------------------------------------
void resetGame()
{
    length=2; score=0; headR=HEIGHT/2; headC=WIDTH/2;
    body.clear(); body.push_back({headR,headC});

    // random food
    auto freeT = freeCells();
    auto pick  = freeT[rand()%freeT.size()];
    foodR=pick[0]; foodC=pick[1];
}

// --------------------------------------------------------------
// Main AI step – returns true on crash
// --------------------------------------------------------------
bool step(int &dir)
{
    // choose / update every n frames (simple throttle)
    static int frame=0; ++frame;
    if(frame%AI_UPDATE_INTERVAL==0 || EPS>0.1f){
        int act = chooseAction(headR,headC,dir);
        dir = act;
    }

    int nr=headR, nc=headC;
    if(dir==0) --nr; else if(dir==1) ++nr; else if(dir==2) --nc; else if(dir==3) ++nc;

    bool crash = !valid(nr,nc) || inBody(nr,nc);
    bool gotFood = (nr==foodR && nc==foodC);

    float oldD = dist(headR,headC), newD = dist(nr,nc);
    float r = rewardFn(gotFood,crash,oldD,newD);
    updateQ(headR,headC,dir,dir,nr,nc,r);

    if(crash) return true;

    headR=nr; headC=nc;
    body.insert(body.begin(),{headR,headC});
    if((int)body.size()>length) body.pop_back();

    if(gotFood){
        ++score; ++length;
        auto freeT = freeCells();
        auto pick  = freeT[rand()%freeT.size()];
        foodR=pick[0]; foodC=pick[1];
    }
    return false;
}

// --------------------------------------------------------------
// Global variables for loop
// --------------------------------------------------------------
int DIR = rand()%4;    // 0 up,1 down,2 left,3 right
int wait=0;            // frames to wait after crash
int episode=0;

void loop()
{
    if(wait){ if(--wait==0) resetGame(); return; }

    bool crash = step(DIR);
    draw();

    if(episode<MAX_EP){ ++episode; EPS = max(0.05f, EPS-0.0005f);}    
    if(crash) wait=10;
}

// --------------------------------------------------------------
// Main
// --------------------------------------------------------------
int main()
{
    srand((unsigned)time(nullptr));

    // build cell cache & Q‑table
    allCells.resize(WIDTH*HEIGHT);
    int k=0; for(int r=0;r<HEIGHT;++r) for(int c=0;c<WIDTH;++c) allCells[k++]={r,c};
    Q.assign(WIDTH*HEIGHT*4, vector<float>(4,0.f));

    sdlInit(); resetGame();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    bool run=true; SDL_Event e;
    while(run){
        while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) run=false;
        loop(); SDL_Delay(120); }
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
#endif
    return 0;
}
