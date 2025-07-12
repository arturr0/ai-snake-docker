#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <queue>
#include <cmath>
#include <SDL2/SDL.h>

using namespace std;

// Game constants
const int WIDTH = 20;
const int HEIGHT = 20;
const int CELL_SIZE = 20;
const int AI_UPDATE_INTERVAL = 5;

// Game state
int ai_pion = HEIGHT/2, ai_poz = WIDTH/2, ai_punkty = 0, ai_longer = 2;
int food_x, food_y;
bool ai_kolizja = false;
int czas = 200;

// AI snake data
vector<vector<int>> ai_pozycja = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
vector<vector<int>> ai_pozycjac = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

// Game board
vector<vector<int>> v1(WIDTH * HEIGHT, vector<int>(2, 0));

// AI learning
vector<vector<float>> q_table;
float learning_rate = 0.1f;
float discount_factor = 0.9f;
float exploration_rate = 0.3f;
int training_episodes = 0;
const int MAX_TRAINING_EPISODES = 1000;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        cerr << "SDL_Init Error: " << SDL_GetError() << endl;
        exit(1);
    }

    window = SDL_CreateWindow("AI Snake", 
                            SDL_WINDOWPOS_CENTERED, 
                            SDL_WINDOWPOS_CENTERED,
                            WIDTH * CELL_SIZE, 
                            HEIGHT * CELL_SIZE + 60,
                            SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
        SDL_Quit();
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }
}

void draw() {
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
    
    // Draw snake
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect head = {ai_poz * CELL_SIZE, ai_pion * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(renderer, &head);
    
    for (const auto& segment : ai_pozycja) {
        SDL_Rect body = {segment[1] * CELL_SIZE, segment[0] * CELL_SIZE, CELL_SIZE, CELL_SIZE};
        SDL_RenderFillRect(renderer, &body);
    }
    
    // Draw score
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = SDL_CreateRGBSurface(0, WIDTH * CELL_SIZE, 60, 32, 0, 0, 0, 0);
    string scoreText = "AI Score: " + to_string(ai_punkty) + " | Training: " + 
                      to_string(training_episodes) + "/" + to_string(MAX_TRAINING_EPISODES);
    SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
    
    SDL_RenderPresent(renderer);
}

vector<vector<int>> generuj() {
    int idx = 0;
    for (int i = 0; i < WIDTH; ++i)
        for (int j = 0; j < HEIGHT; ++j, ++idx)
            v1[idx] = {i, j};
    return v1;
}

vector<vector<int>> wolne(const vector<vector<int>>& snake, vector<vector<int>> all) {
    for (const auto& segment : snake)
        all.erase(remove(all.begin(), all.end(), segment), all.end());
    return all;
}

bool ai_urobos() {
    for (size_t i = 2; i < ai_pozycja.size(); i++) {
        if (ai_pion == ai_pozycja[i][0] && ai_poz == ai_pozycja[i][1])
            return true;
    }
    return false;
}

bool isValidPosition(int x, int y) {
    return x > 0 && x < HEIGHT-1 && y > 0 && y < WIDTH-1;
}

bool isAIBody(int x, int y) {
    for (const auto& segment : ai_pozycja) {
        if (segment[0] == x && segment[1] == y)
            return true;
    }
    return false;
}

void initQTable() {
    q_table.resize(WIDTH * HEIGHT * 4);
    for (auto& row : q_table) {
        row.resize(4, 0.0f);
    }
}

int getStateIndex(int x, int y, int dir) {
    return (y * WIDTH + x) * 4 + dir;
}

int chooseAction(int x, int y, int current_dir) {
    if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < exploration_rate && training_episodes < MAX_TRAINING_EPISODES) {
        return rand() % 4;
    } else {
        int state = getStateIndex(x, y, current_dir);
        return distance(q_table[state].begin(), 
                       max_element(q_table[state].begin(), q_table[state].end()));
    }
}

void updateQTable(int old_x, int old_y, int old_dir, int action, 
                 int new_x, int new_y, float reward) {
    int old_state = getStateIndex(old_x, old_y, old_dir);
    int new_state = getStateIndex(new_x, new_y, action);
    
    float best_future_value = *max_element(q_table[new_state].begin(), 
                                         q_table[new_state].end());
    
    q_table[old_state][action] = (1 - learning_rate) * q_table[old_state][action] + 
                                learning_rate * (reward + discount_factor * best_future_value);
}

float calculateReward(int x, int y, bool got_food, bool crashed) {
    if (crashed) return -10.0f;
    if (got_food) return 10.0f;
    
    float dist_to_food = sqrt(pow(x - food_x, 2) + pow(y - food_y, 2));
    return 1.0f / (dist_to_food + 1);
}

vector<pair<int, int>> findPathToFood(int start_x, int start_y) {
    queue<pair<int, int>> q;
    vector<vector<pair<int, int>>> parent(WIDTH, vector<pair<int, int>>(HEIGHT, {-1, -1}));
    vector<vector<bool>> visited(WIDTH, vector<bool>(HEIGHT, false));
    
    q.push({start_x, start_y});
    visited[start_x][start_y] = true;
    
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    while (!q.empty()) {
        auto current = q.front();
        q.pop();
        
        if (current.first == food_x && current.second == food_y) {
            vector<pair<int, int>> path;
            pair<int, int> node = current;
            while (node.first != start_x || node.second != start_y) {
                path.push_back(node);
                node = parent[node.first][node.second];
            }
            reverse(path.begin(), path.end());
            return path;
        }
        
        for (int i = 0; i < 4; i++) {
            int nx = current.first + dx[i];
            int ny = current.second + dy[i];
            
            if (isValidPosition(nx, ny) && !visited[nx][ny] && !isAIBody(nx, ny)) {
                visited[nx][ny] = true;
                parent[nx][ny] = current;
                q.push({nx, ny});
            }
        }
    }
    
    return {};
}

void aiMove(int& dir) {
    static int ai_update_counter = 0;
    ai_update_counter++;
    
    if (ai_update_counter % AI_UPDATE_INTERVAL == 0 || training_episodes < MAX_TRAINING_EPISODES) {
        int action = chooseAction(ai_pion, ai_poz, dir);
        
        int new_x = ai_pion, new_y = ai_poz;
        switch (action) {
            case 0: new_x--; break;
            case 1: new_x++; break; 
            case 2: new_y--; break;
            case 3: new_y++; break;
        }
        
        bool valid_move = isValidPosition(new_x, new_y) && !isAIBody(new_x, new_y);
        bool got_food = (new_x == food_x && new_y == food_y);
        bool crashed = !valid_move;
        
        float reward = calculateReward(new_x, new_y, got_food, crashed);
        
        if (training_episodes < MAX_TRAINING_EPISODES) {
            updateQTable(ai_pion, ai_poz, dir, action, new_x, new_y, reward);
        }
        
        if (valid_move) {
            dir = action;
        } else {
            auto path = findPathToFood(ai_pion, ai_poz);
            if (!path.empty()) {
                int dx = path[0].first - ai_pion;
                int dy = path[0].second - ai_poz;
                
                if (dx == -1) dir = 0;
                else if (dx == 1) dir = 1;
                else if (dy == -1) dir = 2;
                else if (dy == 1) dir = 3;
            }
        }
    }
    
    switch (dir) {
        case 0: ai_pion--; break;
        case 1: ai_pion++; break;
        case 2: ai_poz--; break;
        case 3: ai_poz++; break;
    }
    
    ai_pozycjac.insert(ai_pozycjac.begin(), {ai_pion, ai_poz});
    if (ai_pozycjac.size() > ai_longer + 2) {
        ai_pozycjac.resize(ai_longer + 2);
    }
    
    if (ai_pion == food_x && ai_poz == food_y) {
        ai_punkty++;
        v1 = generuj();
        auto freeTiles = wolne(ai_pozycjac, v1);
        if (!freeTiles.empty()) {
            int los = rand() % freeTiles.size();
            food_x = freeTiles[los][0];
            food_y = freeTiles[los][1];
            ai_longer++;
        }
    }
    
    ai_pozycja.insert(ai_pozycja.begin(), {ai_pion, ai_poz});
    if (ai_pozycja.size() > ai_longer + 1) {
        ai_pozycja.resize(ai_longer + 1);
    }
    
    ai_kolizja = ai_urobos();
}

void main_loop() {
    static int ai_dir = rand() % 4;
    
    if (!ai_kolizja && 
        ai_poz > 0 && ai_poz < WIDTH - 1 && 
        ai_pion > 0 && ai_pion < HEIGHT - 1) {
        
        aiMove(ai_dir);
        draw();
        
        if (training_episodes < MAX_TRAINING_EPISODES) {
            training_episodes++;
            exploration_rate = max(0.1f, exploration_rate - 0.001f);
        }
    } else {
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        cout << "AI Crashed! Final Score: " << ai_punkty << endl;
        #endif
    }
}

int main() {
    srand((unsigned int)time(NULL));
    v1 = generuj();
    auto freeTiles = wolne(ai_pozycjac, v1);
    
    if (!freeTiles.empty()) {
        int los = rand() % freeTiles.size();
        food_x = freeTiles[los][0];
        food_y = freeTiles[los][1];
    }
    
    initQTable();
    initSDL();
    
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
    #else
    while (!ai_kolizja && 
           ai_poz > 0 && ai_poz < WIDTH - 1 && 
           ai_pion > 0 && ai_pion < HEIGHT - 1) {
        
        static int ai_dir = rand() % 4;
        aiMove(ai_dir);
        draw();
        SDL_Delay(czas);
        
        if (training_episodes < MAX_TRAINING_EPISODES) {
            training_episodes++;
            exploration_rate = max(0.1f, exploration_rate - 0.001f);
        }
    }
    
    cout << "AI Crashed! Final Score: " << ai_punkty << endl;
    #endif
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
