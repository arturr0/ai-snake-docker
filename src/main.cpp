#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <queue>
#include <cmath>
#include <SDL.h>                // works on desktop + -sUSE_SDL=2
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace std;

// --------------------------------------------------------------
// Game constants
// --------------------------------------------------------------
const int WIDTH  = 20;
const int HEIGHT = 20;
const int CELL_SIZE = 20;
const int AI_UPDATE_INTERVAL = 5;      // frames between RL updates

// --------------------------------------------------------------
// Game state
// --------------------------------------------------------------
int ai_row   = HEIGHT / 2;              // head row
int ai_col   = WIDTH  / 2;              // head col
int ai_score = 0;                       // score
int ai_length = 2;                      // body length (head+tail)

int food_row, food_col;                 // food position
bool ai_collision = false;              // crash flag
int frame_delay_ms = 200;               // desktop delay

// --------------------------------------------------------------
// Snake data structures
// --------------------------------------------------------------
vector<vector<int>> body;       // visible body (index‑0 = head)
vector<vector<int>> logicBody;  // helper list (same coords)

// --------------------------------------------------------------
// Pre‑generated list of every cell on the board
// --------------------------------------------------------------
vector<vector<int>> allCells(WIDTH * HEIGHT, vector<int>(2));

// --------------------------------------------------------------
// Q‑learning tables & params
// --------------------------------------------------------------
vector<vector<float>> q_table;           // [state] -> 4 action‑values
float lr     = 0.1f;
float gamma  = 0.9f;
float eps    = 0.3f;                     // exploration rate (decays)
int   episodes = 0;
const int MAX_EPISODES = 1000;

// --------------------------------------------------------------
// SDL handles
// --------------------------------------------------------------
SDL_Window*   window   = nullptr;
SDL_Renderer* renderer = nullptr;

// --------------------------------------------------------------
// SDL initialisation
// --------------------------------------------------------------
void initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO)) {
        cerr << "SDL_Init Error: " << SDL_GetError() << '\n';
        exit(1);
    }

    window = SDL_CreateWindow("AI Snake",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WIDTH * CELL_SIZE,
                              HEIGHT * CELL_SIZE,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "SDL_CreateWindow Error: " << SDL_GetError() << '\n';
        SDL_Quit();
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }
}

// --------------------------------------------------------------
// Draw one frame
// --------------------------------------------------------------
void draw()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // border
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect border{0,0, WIDTH*CELL_SIZE, HEIGHT*CELL_SIZE};
    SDL_RenderDrawRect(renderer, &border);

    // food
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect food{ food_col*CELL_SIZE, food_row*CELL_SIZE, CELL_SIZE, CELL_SIZE };
    SDL_RenderFillRect(renderer, &food);

    // head
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect head{ ai_col*CELL_SIZE, ai_row*CELL_SIZE, CELL_SIZE, CELL_SIZE };
    SDL_RenderFillRect(renderer, &head);

    // body (skip head)
    for (int i=1; i<ai_length && i<(int)body.size(); ++i) {
        SDL_Rect seg{ body[i][1]*CELL_SIZE, body[i][0]*CELL_SIZE, CELL_SIZE, CELL_SIZE };
        SDL_RenderFillRect(renderer, &seg);
    }

    SDL_RenderPresent(renderer);
}

// --------------------------------------------------------------
// Helpers for grid
// --------------------------------------------------------------
vector<vector<int>> generateAll()
{
    int k=0;
    for (int r=0;r<HEIGHT;++r)
        for (int c=0;c<WIDTH;++c,++k)
            allCells[k] = {r,c};
    return allCells;
}

vector<vector<int>> freeCells(const vector<vector<int>>& snake, vector<vector<int>> cells)
{
    for (const auto& s: snake)
        cells.erase(remove(cells.begin(), cells.end(), s), cells.end());
    return cells;
}

inline bool isValid(int r,int c){ return r>=0 && r<HEIGHT && c>=0 && c<WIDTH; }

bool isBody(int r,int c){ for(auto& s:body) if(s[0]==r && s[1]==c) return true; return false; }

// --------------------------------------------------------------
// Q‑table helpers
// --------------------------------------------------------------
void initQ(){ q_table.assign(WIDTH*HEIGHT*4, vector<float>(4,0.f)); }

inline int stateIndex(int r,int c,int dir){ return (r*WIDTH + c)*4 + dir; }

int chooseAction(int r,int c,int dir)
{
    // ε‑greedy
    if ((float)rand()/RAND_MAX < eps && episodes<MAX_EPISODES) return rand()%4;

    const auto& q = q_table[stateIndex(r,c,dir)];
    float best = *max_element(q.begin(), q.end());
    int bestIdx[4], n=0;
    for(int a=0;a<4;++a) if (fabs(q[a]-best)<1e-6f) bestIdx[n++]=a;
    return bestIdx[rand()%n];
}

void updateQ(int r,int c,int dir,int a,int nr,int nc,float reward)
{
    int s  = stateIndex(r,c,dir);
    int ns = stateIndex(nr,nc,a);
    float bestNext = *max_element(q_table[ns].begin(), q_table[ns].end());
    q_table[s][a] = (1-lr)*q_table[s][a] + lr*(reward + gamma*bestNext);
}

float rewardFn(int r,int c,bool gotFood,bool crash)
{
    if(crash) return -10.f;
    if(gotFood) return 10.f;
    float d = sqrtf((r-food_row)*(r-food_row) + (c-food_col)*(c-food_col));
    return 1.f/(d+1.f);
}

// --------------------------------------------------------------
// BFS helper when direct action invalid (optional fallback)
// --------------------------------------------------------------
vector<pair<int,int>> bfsToFood(int sr,int sc)
{
    queue<pair<int,int>> q;
    vector<vector<char>> vis(HEIGHT, vector<char>(WIDTH,0));
    vector<vector<pair<int,int>>> par(HEIGHT, vector<pair<int,int>>(WIDTH, {-1,-1}));
    q.push({sr,sc}); vis[sr][sc]=1;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    while(!q.empty()){
        auto cur=q.front(); q.pop();
        if(cur.first==food_row && cur.second==food_col){
            vector<pair<int,int>> path;
            auto p=cur;
            while(p.first!=sr || p.second!=sc){ path.push_back(p); p=par[p.first][p.second]; }
            reverse(path.begin(), path.end());
            return path;
        }
        for(int i=0;i<4;++i){int nr=cur.first+dr[i], nc=cur.second+dc[i];
            if(isValid(nr,nc)&&!vis[nr][nc]&&!isBody(nr,nc)){vis[nr][nc]=1; par[nr][nc]=cur; q.push({nr,nc});}}
    }
    return {};
}

// --------------------------------------------------------------
// Reset snake after crash or at start
// --------------------------------------------------------------
void resetSnake()
{
    ai_row = HEIGHT/2; ai_col = WIDTH/2; ai_length=2; ai_collision=false;
    body.clear(); logicBody.clear();
    body.push_back({ai_row,ai_col});
    logicBody.push_back({ai_row,ai_col});

    // place food
    generateAll();
    auto freeT = freeCells(body, allCells);
    auto pick  = freeT[rand()%freeT.size()];
    food_row = pick[0]; food_col = pick[1];
}

// --------------------------------------------------------------
// Main AI move – returns true if crash occurred
// --------------------------------------------------------------
bool aiMove(int& dir)
{
    static int frame=0; ++frame;

    // ε‑greedy every AI_UPDATE_INTERVAL frames
    if(frame%AI_UPDATE_INTERVAL==0 || episodes<MAX_EPISODES){
        int act = chooseAction(ai_row, ai_col, dir);
        int nr=ai_row, nc=ai_col;
        if(act==0) --nr; else if(act==1) ++nr; else if(act==2) --nc; else if(act==3) ++nc;

        bool valid=isValid(nr,nc)&&!isBody(nr,nc);
        bool gotFood=(nr==food_row&&nc==food_col);
        bool crash=!valid;
        float r=rewardFn(nr,nc,gotFood,crash);
        if(episodes<MAX_EPISODES) updateQ(ai_row,ai_col,dir,act,nr,nc,r);

        if(valid) dir=act; else {
            auto path=bfsToFood(ai_row,ai_col);
            if(!path.empty()){
                int dr=path[0].first-ai_row; int dc=path[0].second-ai_col;
                if(dr==-1) dir=0; else if(dr==1) dir=1; else if(dc==-1) dir=2; else if(dc==1) dir=3;
            }
        }
    }

    // execute dir
    if(dir==0) --ai_row; else if(dir==1) ++ai_row; else if(dir==2) --ai_col; else if(dir==3) ++ai_col;

    // collision check
    if(!isValid(ai_row,ai_col) || isBody(ai_row,ai_col)) return true;

    logicBody.insert(logicBody.begin(), {ai_row,ai_col});
    if((int)logicBody.size()>ai_length+1) logicBody.pop_back();
    body=logicBody;

    // food
    if(ai_row==food_row && ai_col==food_col){
        ++ai_score; ++ai_length;
        generateAll();
        auto freeT = freeCells(body, allCells);
        auto pick  = freeT[rand()%freeT.size()];
        food_row=pick[0]; food_col=pick[1];
    }

    return false;
}

// --------------------------------------------------------------
// Main loop wrapper
// --------------------------------------------------------------
void loop()
{
    static int dir = rand()%4;
    static int wait=0;

    if(wait){ if(--wait==0) resetSnake(); return; }

    bool crash = aiMove(dir);
    draw();

    if(episodes<MAX_EPISODES){ ++episodes; eps=max(0.1f, eps-0.001f); }
    if(crash) wait=10;
}

// --------------------------------------------------------------
// Entry point
// --------------------------------------------------------------
int main()
{
    srand((unsigned)time(nullptr));
    generateAll(); initQ(); initSDL(); resetSnake();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    bool running=true; SDL_Event e;
    while(running){
        while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) running=false;
        loop(); SDL_Delay(frame_delay_ms); }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
#endif
    return 0;
}
