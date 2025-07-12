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

// Game constants
const int WIDTH = 20;
const int HEIGHT = 20;
const int CELL_SIZE = 20;
const int AI_UPDATE_INTERVAL = 5;

// Game state
int ai_pion = HEIGHT / 2;
int ai_poz = WIDTH / 2;
int ai_punkty = 0;
int ai_longer = 2;
int food_x, food_y;
bool ai_kolizja = false;
int czas = 200;

// AI snake data
vector<vector<int>> ai_pozycja = {{0,0}, {0,0}, {0,0}, {0,0}};
vector<vector<int>> ai_pozycjac = {{0,0}, {0,0}, {0,0}, {0,0}};
vector<vector<int>> v1(WIDTH * HEIGHT, vector<int>(2, 0));

// Q-learning
vector<vector<float>> q_table;
float learning_rate = 0.1f;
float discount_factor = 0.9f;
float exploration_rate = 0.3f;
int training_episodes = 0;
const int MAX_TRAINING_EPISODES = 1000;

// SDL
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

void initSDL() {
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

void draw() {
    // Clear background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw border
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect border = {0, 0, WIDTH * CELL_SIZE, HEIGHT * CELL_SIZE};
    SDL_RenderDrawRect(renderer, &border);

    // Draw food
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect food = {food_y * CELL_SIZE, food_x * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(renderer, &food);

    // Draw snake body
    SDL_SetRenderDrawColor(renderer, 0, 180, 0, 255);
    for (const auto& seg : ai_pozycja) {
        if (seg[0] >= 0 && seg[1] >= 0) {
            SDL_Rect body = {seg[1] * CELL_SIZE, seg[0] * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            SDL_RenderFillRect(renderer, &body);
        }
    }

    // Draw snake head
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect head = {ai_poz * CELL_SIZE, ai_pion * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(renderer, &head);

    SDL_RenderPresent(renderer);
}

vector<vector<int>> generuj() {
    int idx = 0;
    for (int i = 0; i < HEIGHT; ++i)
        for (int j = 0; j < WIDTH; ++j, ++idx)
            v1[idx] = {i, j};
    return v1;
}

vector<vector<int>> wolne(const vector<vector<int>>& snake, vector<vector<int>> all) {
    for (const auto& seg : snake)
        all.erase(remove(all.begin(), all.end(), seg), all.end());
    return all;
}

bool ai_urobos() {
    for (size_t i = 1; i < ai_pozycja.size(); ++i) {
        if (ai_pion == ai_pozycja[i][0] && ai_poz == ai_pozycja[i][1])
            return true;
    }
    return false;
}

inline bool isValidPosition(int x, int y) {
    return x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH;
}

bool isAIBody(int x, int y) {
    for (const auto& seg : ai_pozycja)
        if (seg[0] == x && seg[1] == y) return true;
    return false;
}

void initQTable() {
    q_table.resize(WIDTH * HEIGHT * 4);
    for (auto& row : q_table) row.assign(4, 0.0f);
}

inline int getStateIndex(int x, int y, int dir) {
    return (y * WIDTH + x) * 4 + dir;
}

int chooseAction(int x, int y, int current_dir) {
    if ((float)rand() / RAND_MAX < exploration_rate &&
        training_episodes < MAX_TRAINING_EPISODES)
        return rand() % 4;

    int state = getStateIndex(x, y, current_dir);
    return (int)distance(q_table[state].begin(),
        max_element(q_table[state].begin(), q_table[state].end()));
}

void updateQTable(int ox, int oy, int odir, int action, int nx, int ny, float reward) {
    int old_state = getStateIndex(ox, oy, odir);
    int new_state = getStateIndex(nx, ny, action);

    float best_future = *max_element(q_table[new_state].begin(), q_table[new_state].end());

    q_table[old_state][action] = (1 - learning_rate) * q_table[old_state][action] +
        learning_rate * (reward + discount_factor * best_future);
}

float calculateReward(int x, int y, bool got_food, bool crashed) {
    if (crashed) return -50.0f;
    if (got_food) return 100.0f;
    
    float prev_dist = sqrtf((ai_pion-food_x)*(ai_pion-food_x) + 
                         (ai_poz-food_y)*(ai_poz-food_y));
    float new_dist = sqrtf((x-food_x)*(x-food_x) + 
                        (y-food_y)*(y-food_y));
    
    if (new_dist < prev_dist) return 1.0f;
    if (new_dist > prev_dist) return -1.0f;
    return 0.0f;
}

vector<pair<int,int>> findPathToFood(int sx, int sy) {
    queue<pair<int,int>> q;
    vector<vector<pair<int,int>>> parent(HEIGHT, vector<pair<int,int>>(WIDTH, {-1,-1}));
    vector<vector<char>> visited(HEIGHT, vector<char>(WIDTH, 0));

    q.push({sx, sy});
    visited[sx][sy] = 1;

    const int dx[4] = {-1,1,0,0};
    const int dy[4] = {0,0,-1,1};

    while (!q.empty()) {
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
    return {};
}

void resetSnake() {
    ai_pion = HEIGHT / 2;
    ai_poz = WIDTH / 2;
    ai_longer = 2;
    ai_pozycja = {{ai_pion, ai_poz}};
    ai_pozycjac = {{ai_pion, ai_poz}};
    ai_kolizja = false;
    
    v1 = generuj();
    auto freeTiles = wolne(ai_pozycjac, v1);
    if (!freeTiles.empty()) {
        int k = rand() % freeTiles.size();
        food_x = freeTiles[k][0];
        food_y = freeTiles[k][1];
    }
}

bool aiMove(int& dir) {
    static int frame = 0;
    ++frame;

    int prev_x = ai_pion;
    int prev_y = ai_poz;
    int prev_dir = dir;

    if (frame % AI_UPDATE_INTERVAL == 0 || training_episodes < MAX_TRAINING_EPISODES) {
        int action = chooseAction(ai_pion, ai_poz, dir);
        
        int nx = ai_pion, ny = ai_poz;
        switch (action) {
            case 0: --nx; break;
            case 1: ++nx; break;
            case 2: --ny; break;
            case 3: ++ny; break;
        }

        bool valid = isValidPosition(nx, ny) && !isAIBody(nx, ny);
        bool gotFood = (nx == food_x && ny == food_y);
        bool crash = !valid;

        float reward = calculateReward(nx, ny, gotFood, crash);
        if (training_episodes < MAX_TRAINING_EPISODES) {
            updateQTable(prev_x, prev_y, prev_dir, action, nx, ny, reward);
        }

        if (valid) {
            dir = action;
        } else {
            vector<int> safe_actions;
            for (int i = 0; i < 4; i++) {
                int tx = ai_pion, ty = ai_poz;
                switch (i) {
                    case 0: --tx; break;
                    case 1: ++tx; break;
                    case 2: --ty; break;
                    case 3: ++ty; break;
                }
                if (isValidPosition(tx, ty) && !isAIBody(tx, ty)) {
                    safe_actions.push_back(i);
                }
            }
            if (!safe_actions.empty()) {
                dir = safe_actions[rand() % safe_actions.size()];
            }
        }
    }

    switch (dir) {
        case 0: --ai_pion; break;
        case 1: ++ai_pion; break;
        case 2: --ai_poz; break;
        case 3: ++ai_poz; break;
    }

    if (!isValidPosition(ai_pion, ai_poz) || isAIBody(ai_pion, ai_poz)) {
        return true;
    }

    ai_pozycjac.insert(ai_pozycjac.begin(), {ai_pion, ai_poz});
    if (ai_pozycjac.size() > ai_longer + 2) {
        ai_pozycjac.resize(ai_longer + 2);
    }

    if (ai_pion == food_x && ai_poz == food_y) {
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

    ai_pozycja.insert(ai_pozycja.begin(), {ai_pion, ai_poz});
    if (ai_pozycja.size() > ai_longer + 1) {
        ai_pozycja.resize(ai_longer + 1);
    }

    return false;
}

void main_loop() {
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
        reset_timer = 10;
    }
}

int main() {
    srand((unsigned)time(nullptr));

    v1 = generuj();
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
