#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <queue>
#include <cmath>
#include <SDL/SDL.h>
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
const int AI_UPDATE_INTERVAL = 5;     // frames between Q‑learning updates

// --------------------------------------------------------------
// Game state
// --------------------------------------------------------------
int ai_pion   = HEIGHT / 2;           // head row
int ai_poz    = WIDTH  / 2;           // head col
int ai_punkty = 0;                    // score
int ai_longer = 2;                    // snake length – grows with food

int food_x, food_y;                   // food position
bool ai_kolizja = false;              // collision flag
int czas = 200;                       // delay (ms) for native build

// --------------------------------------------------------------
// AI snake data structures
// --------------------------------------------------------------
vector<vector<int>> ai_pozycja  = { {0,0}, {0,0}, {0,0}, {0,0} }; // cells actually drawn
vector<vector<int>> ai_pozycjac = { {0,0}, {0,0}, {0,0}, {0,0} }; // helper for logic only

// --------------------------------------------------------------
// Board helper – pre‑filled with every (row,col) once at start
// --------------------------------------------------------------
vector<vector<int>> v1(WIDTH * HEIGHT, vector<int>(2, 0));

// --------------------------------------------------------------
// Q‑learning tables & params (simple tabular RL)
// --------------------------------------------------------------
vector<vector<float>> q_table;      // [state] -> 4 action‑values
float learning_rate   = 0.1f;
float discount_factor = 0.9f;
float exploration_rate = 0.3f;      // ε‑greedy, decays during training
int   training_episodes = 0;
const int MAX_TRAINING_EPISODES = 1000;

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
// Draw everything for one frame – *no dynamic leaks!*
// --------------------------------------------------------------
void draw()
{
    // clear background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // border
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect border = { 0, 0, WIDTH * CELL_SIZE, HEIGHT * CELL_SIZE };
    SDL_RenderDrawRect(renderer, &border);

    // food
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect food = { food_y * CELL_SIZE, food_x * CELL_SIZE, CELL_SIZE, CELL_SIZE };
    SDL_RenderFillRect(renderer, &food);

    // snake head
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect head = { ai_poz * CELL_SIZE, ai_pion * CELL_SIZE, CELL_SIZE, CELL_SIZE };
    SDL_RenderFillRect(renderer, &head);

    // snake body
    for (const auto& seg : ai_pozycja) {
        SDL_Rect body = { seg[1] * CELL_SIZE, seg[0] * CELL_SIZE, CELL_SIZE, CELL_SIZE };
        SDL_RenderFillRect(renderer, &body);
    }

    SDL_RenderPresent(renderer);
}

// --------------------------------------------------------------
// Helper: generate complete list of free cells once
// --------------------------------------------------------------
vector<vector<int>> generuj()
{
    int idx = 0;
    for (int i = 0; i < HEIGHT; ++i)
        for (int j = 0; j < WIDTH; ++j, ++idx)
            v1[idx] = { i, j };
    return v1;
}

// --------------------------------------------------------------
// Remove cells occupied by snake from list of all cells
// --------------------------------------------------------------
vector<vector<int>> wolne(const vector<vector<int>>& snake, vector<vector<int>> all)
{
    for (const auto& seg : snake)
        all.erase(remove(all.begin(), all.end(), seg), all.end());
    return all;
}

bool ai_urobos()
{
    for (size_t i = 2; i < ai_pozycja.size(); ++i) {
        if (ai_pion == ai_pozycja[i][0] && ai_poz == ai_pozycja[i][1])
            return true;
    }
    return false;
}

inline bool isValidPosition(int x, int y)
{
    return x > 0 && x < HEIGHT - 1 && y > 0 && y < WIDTH - 1;
}

bool isAIBody(int x, int y)
{
    for (const auto& seg : ai_pozycja)
        if (seg[0] == x && seg[1] == y) return true;
    return false;
}

// --------------------------------------------------------------
// Q‑table initialisation (state = (row,col,dir) → action values)
// --------------------------------------------------------------
void initQTable()
{
    q_table.resize(WIDTH * HEIGHT * 4);    // 4 possible previous dirs
    for (auto& row : q_table) row.assign(4, 0.0f);
}

inline int getStateIndex(int x, int y, int dir)
{
    return (y * WIDTH + x) * 4 + dir;
}

int chooseAction(int x, int y, int current_dir)
{
    if ((float)rand() / RAND_MAX < exploration_rate &&
        training_episodes < MAX_TRAINING_EPISODES)
        return rand() % 4;                       // explore

    int state = getStateIndex(x, y, current_dir);
    return (int)distance(q_table[state].begin(),
                         max_element(q_table[state].begin(), q_table[state].end()));
}

void updateQTable(int ox,int oy,int odir,int action,int nx,int ny,float reward)
{
    int old_state = getStateIndex(ox, oy, odir);
    int new_state = getStateIndex(nx, ny, action);

    float best_future = *max_element(q_table[new_state].begin(), q_table[new_state].end());

    q_table[old_state][action] = (1 - learning_rate) * q_table[old_state][action] +
                                 learning_rate * (reward + discount_factor * best_future);
}

float calculateReward(int x,int y,bool got_food,bool crashed)
{
    if (crashed)  return -10.0f;
    if (got_food) return  10.0f;

    float d = sqrtf((float)((x - food_x)*(x - food_x) + (y - food_y)*(y - food_y)));
    return 1.0f / (d + 1.0f);
}

// --------------------------------------------------------------
// Simple BFS to avoid dead‑ends when ε‑greedy crashes into body
// --------------------------------------------------------------
vector<pair<int,int>> findPathToFood(int sx,int sy)
{
    queue<pair<int,int>> q;
    vector<vector<pair<int,int>>> parent(HEIGHT, vector<pair<int,int>>(WIDTH, {-1,-1}));
    vector<vector<char>> visited(HEIGHT, vector<char>(WIDTH, 0));

    q.push({sx, sy});
    visited[sx][sy] = 1;

    const int dx[4] = {-1,1,0,0};
    const int dy[4] = {0,0,-1,1};

    while (!q.empty())
    {
        auto cur = q.front(); q.pop();
        if (cur.first == food_x && cur.second == food_y) {
            vector<pair<int,int>> path;
            for (auto p = cur; p.first!=sx || p.second!=sy; p = parent[p.first][p.second])
                path.push_back(p);
            reverse(path.begin(), path.end());
            return path;
        }
        for (int i=0;i<4;++i) {
            int nx = cur.first + dx[i];
            int ny = cur.second + dy[i];
            if (isValidPosition(nx,ny) && !visited[nx][ny] && !isAIBody(nx,ny)) {
                visited[nx][ny] = 1;
                parent[nx][ny] = cur;
                q.push({nx,ny});
            }
        }
    }
    return {};           // no path
}

// --------------------------------------------------------------
// Reset snake to initial state after crash
// --------------------------------------------------------------
void resetSnake() {
    ai_pion = HEIGHT / 2;
    ai_poz = WIDTH / 2;
    ai_longer = 2;
    ai_pozycja = { {0,0}, {0,0}, {0,0}, {0,0} };
    ai_pozycjac = { {0,0}, {0,0}, {0,0}, {0,0} };
    ai_kolizja = false;
    
    // Reposition food
    v1 = generuj();
    auto freeTiles = wolne(ai_pozycjac, v1);
    if (!freeTiles.empty()) {
        int k = rand() % freeTiles.size();
        food_x = freeTiles[k][0];
        food_y = freeTiles[k][1];
    }
}

// --------------------------------------------------------------
// Main AI move – chooses action, updates Q‑table & state
// --------------------------------------------------------------
bool aiMove(int& dir)
{
    static int frame = 0;
    ++frame;

    if (frame % AI_UPDATE_INTERVAL == 0 || training_episodes < MAX_TRAINING_EPISODES)
    {
        int action = chooseAction(ai_pion, ai_poz, dir);
        int nx = ai_pion, ny = ai_poz;
        switch (action) {
            case 0: --nx; break; // up
            case 1: ++nx; break; // down
            case 2: --ny; break; // left
            case 3: ++ny; break; // right
        }

        bool valid   = isValidPosition(nx,ny) && !isAIBody(nx,ny);
        bool gotFood = (nx==food_x && ny==food_y);
        bool crash   = !valid;

        float reward = calculateReward(nx,ny,gotFood,crash);
        if (training_episodes < MAX_TRAINING_EPISODES)
            updateQTable(ai_pion,ai_poz,dir,action,nx,ny,reward);

        if (valid)
            dir = action;              // follow chosen action
        else {
            auto path = findPathToFood(ai_pion, ai_poz);
            if (!path.empty()) {
                int dx = path[0].first  - ai_pion;
                int dy = path[0].second - ai_poz;
                if (dx==-1) dir=0;
                else if (dx==1) dir=1;
                else if (dy==-1) dir=2;
                else if (dy==1) dir=3;
            }
        }
    }

    // actually move
    switch (dir) {
        case 0: --ai_pion; break;
        case 1: ++ai_pion; break;
        case 2: --ai_poz;  break;
        case 3: ++ai_poz;  break;
    }

    // Check for collision
    ai_kolizja = !isValidPosition(ai_pion, ai_poz) || isAIBody(ai_pion, ai_poz);
    if (ai_kolizja) {
        return true; // signal that we crashed
    }

    // helper list for logic (without head duplicate at slot 0)
    ai_pozycjac.insert(ai_pozycjac.begin(), {ai_pion, ai_poz});
    if (ai_pozycjac.size() > ai_longer + 2)
        ai_pozycjac.resize(ai_longer + 2);

    // food collision ⇒ grow and reposition food
    if (ai_pion==food_x && ai_poz==food_y)
    {
        ++ai_punkty;
        v1 = generuj();
        auto freeTiles = wolne(ai_pozycjac, v1);
        if (!freeTiles.empty()) {
            int k = rand() % freeTiles.size();
            food_x = freeTiles[k][0];
            food_y = freeTiles[k][1];
            ++ai_longer;
        }
    }

    // visible body list (includes head)
    ai_pozycja.insert(ai_pozycja.begin(), {ai_pion, ai_poz});
    if (ai_pozycja.size() > ai_longer + 1)
        ai_pozycja.resize(ai_longer + 1);

    return false; // no crash
}

// --------------------------------------------------------------
// Main game loop wrapper for Emscripten / native
// --------------------------------------------------------------
void main_loop()
{
    static int ai_dir = rand() % 4;
    static int reset_timer = 0;

    if (reset_timer > 0) {
        reset_timer--;
        if (reset_timer == 0) {
            resetSnake();
        }
        return;
    }

    bool crashed = aiMove(ai_dir);
    draw();

    if (training_episodes < MAX_TRAINING_EPISODES) {
        ++training_episodes;
        exploration_rate = max(0.1f, exploration_rate - 0.001f);
    }

    if (crashed) {
        reset_timer = 10; // Wait 10 frames before restarting
    }
}

// --------------------------------------------------------------
// Program entry
// --------------------------------------------------------------
int main()
{
    srand((unsigned)time(nullptr));

    v1 = generuj();                    // fill full board list once
    auto freeTiles = wolne(ai_pozycjac, v1);
    if (!freeTiles.empty()) {
        int k = rand() % freeTiles.size();
        food_x = freeTiles[k][0];
        food_y = freeTiles[k][1];
    }

    initQTable();
    initSDL();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    static int ai_dir = rand() % 4;
    static int reset_timer = 0;
    
    bool running = true;
    while (running) {
        if (reset_timer > 0) {
            reset_timer--;
            if (reset_timer == 0) {
                resetSnake();
            }
            SDL_Delay(czas);
            continue;
        }

        bool crashed = aiMove(ai_dir);
        draw();
        SDL_Delay(czas);

        if (training_episodes < MAX_TRAINING_EPISODES) {
            ++training_episodes;
            exploration_rate = max(0.1f, exploration_rate - 0.001f);
        }

        if (crashed) {
            reset_timer = 10;
        }

        // Check for quit event
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
    }
    cout << "Training complete. Final exploration rate: " << exploration_rate << endl;
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
