#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <queue>
#include <cmath>
#include <SDL/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace std;

// Game constants
const int WIDTH = 20;
const int HEIGHT = 20;
const int CELL_SIZE = 20;
const int AI_UPDATE_INTERVAL = 5;
const int LOG_INTERVAL = 100;
const int MAX_TRAINING_EPISODES = 5000000;
const float MIN_EXPLORATION = 0.01f;

// Game state
struct GameState {
    int head_x = HEIGHT / 2;
    int head_y = WIDTH / 2;
    int score = 0;
    int length = 2;
    int food_x = 0, food_y = 0;
    bool crashed = false;
    int speed = 200;
    vector<vector<int>> body;
    vector<vector<int>> trail;
};

// Q-learning parameters
struct QLearning {
    vector<vector<float>> table;
    float learning_rate = 0.1f;
    float discount_factor = 0.9f;
    float exploration_rate = 1.0f;
    int episodes = 0;
    const float exploration_decay = 0.9995f;
};

// Performance tracking
struct Performance {
    vector<int> scores;
    vector<float> avg_q_values;
    vector<int> lengths;
    vector<float> avg_rewards;
};

// SDL resources
struct SDLResources {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

// Global game objects
GameState game;
QLearning q_learning;
Performance performance;
SDLResources sdl;

#ifdef __EMSCRIPTEN__
EM_JS(void, initChartJS, (), {
    document.addEventListener('DOMContentLoaded', function() {
        var container = document.getElementById('chart-container');
        if (!container) {
            container = document.createElement('div');
            container.id = 'chart-container';
            container.style.width = '800px';
            container.style.margin = '20px auto';
            container.style.padding = '20px';
            container.style.backgroundColor = '#333';
            document.body.appendChild(container);
        }

        var scoreCanvas = document.createElement('canvas');
        scoreCanvas.id = 'scoreChart';
        container.appendChild(scoreCanvas);

        var qValueCanvas = document.createElement('canvas');
        qValueCanvas.id = 'qValueChart';
        container.appendChild(qValueCanvas);
    });
});

EM_JS(void, updateCharts, (int episode, int score, float avg_q, float exploration), {
    var statusElement = document.getElementById('status');
    if (statusElement) {
        statusElement.innerHTML = 
            `Episode: ${episode} | Score: ${score} | Avg Q: ${avg_q.toFixed(2)} | Exploration: ${exploration.toFixed(2)}`;
    }
});
#endif

// Helper functions
vector<vector<int>> generateAllPositions() {
    vector<vector<int>> positions(WIDTH * HEIGHT, vector<int>(2));
    int idx = 0;
    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j, ++idx) {
            positions[idx] = {i, j};
        }
    }
    return positions;
}

vector<vector<int>> getFreePositions(const vector<vector<int>>& occupied, const vector<vector<int>>& all) {
    vector<vector<int>> free = all;
    for (const auto& pos : occupied) {
        free.erase(remove(free.begin(), free.end(), pos), free.end());
    }
    return free;
}

bool isValidPosition(int x, int y) {
    return x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH;
}

bool isBodyPosition(int x, int y) {
    for (const auto& seg : game.body) {
        if (seg.size() == 2 && seg[0] == x && seg[1] == y) {
            return true;
        }
    }
    return false;
}

// Q-learning functions
void initQTable() {
    // State space: (x, y, direction, food_dir, danger)
    // Total states: WIDTH * HEIGHT * 4 * 16 * 16
    // We'll use a sparse representation
    q_learning.table.resize(WIDTH * HEIGHT * 1024); // Reduced state space
    for (auto& row : q_learning.table) {
        row.assign(4, 0.0f);
        // Small positive bias to encourage exploration
        for (float& val : row) {
            val = (rand() % 100) / 1000.0f - 0.01f;
        }
    }
}

int getStateIndex(int x, int y, int dir) {
    if (!isValidPosition(x, y)) return 0;
    
    // Calculate relative food position (4 directions)
    int food_dir = 0;
    if (game.food_x > x) food_dir = 1; // Food is below
    else if (game.food_x < x) food_dir = 2; // Food is above
    if (game.food_y > y) food_dir |= 4; // Food is right
    else if (game.food_y < y) food_dir |= 8; // Food is left
    
    // Calculate danger directions (4 directions)
    int danger = 0;
    if (!isValidPosition(x-1, y) || isBodyPosition(x-1, y)) danger |= 1; // Up
    if (!isValidPosition(x+1, y) || isBodyPosition(x+1, y)) danger |= 2; // Down
    if (!isValidPosition(x, y-1) || isBodyPosition(x, y-1)) danger |= 4; // Left
    if (!isValidPosition(x, y+1) || isBodyPosition(x, y+1)) danger |= 8; // Right
    
    // Combine into state index
    return (y * WIDTH + x) * 256 + dir * 64 + food_dir * 4 + danger;
}

int chooseAction(int x, int y, int current_dir) {
    // Exploration: random action
    if (static_cast<float>(rand()) / RAND_MAX < q_learning.exploration_rate) {
        return rand() % 4;
    }

    // Exploitation: best known action
    int state = getStateIndex(x, y, current_dir);
    if (state >= 0 && state < q_learning.table.size()) {
        return distance(q_learning.table[state].begin(),
                      max_element(q_learning.table[state].begin(), q_learning.table[state].end()));
    }
    return rand() % 4;
}

void updateQTable(int old_state, int action, int new_state, float reward) {
    if (old_state >= 0 && old_state < q_learning.table.size() && 
        new_state >= 0 && new_state < q_learning.table.size()) {
        float best_future = *max_element(q_learning.table[new_state].begin(), 
                                       q_learning.table[new_state].end());
        q_learning.table[old_state][action] = 
            (1 - q_learning.learning_rate) * q_learning.table[old_state][action] +
            q_learning.learning_rate * (reward + q_learning.discount_factor * best_future);
    }
}

float calculateReward(int prev_x, int prev_y, int x, int y, bool got_food, bool crashed) {
    if (crashed) return -100.0f;
    if (got_food) return 50.0f;
    
    // Reward for moving toward food
    float prev_dist = sqrtf((prev_x-game.food_x)*(prev_x-game.food_x) + 
                     (prev_y-game.food_y)*(prev_y-game.food_y));
    float new_dist = sqrtf((x-game.food_x)*(x-game.food_x) + 
                    (y-game.food_y)*(y-game.food_y));
    float dist_reward = (prev_dist - new_dist) * 5.0f;
    
    // Small positive reward for staying alive
    return dist_reward + 0.1f;
}

// Game functions
void resetGame() {
    game.head_x = HEIGHT / 2;
    game.head_y = WIDTH / 2;
    game.length = 2;
    game.body = {{game.head_x, game.head_y}};
    game.trail = {{game.head_x, game.head_y}};
    game.crashed = false;
    
    auto all_positions = generateAllPositions();
    auto free_positions = getFreePositions(game.trail, all_positions);
    if (!free_positions.empty()) {
        int k = rand() % free_positions.size();
        game.food_x = free_positions[k][0];
        game.food_y = free_positions[k][1];
    }
}

void spawnFood() {
    auto all_positions = generateAllPositions();
    auto free_positions = getFreePositions(game.trail, all_positions);
    if (!free_positions.empty()) {
        int k = rand() % free_positions.size();
        game.food_x = free_positions[k][0];
        game.food_y = free_positions[k][1];
    }
}

bool moveSnake(int& direction) {
    static int frame = 0;
    frame++;

    int prev_x = game.head_x;
    int prev_y = game.head_y;
    int prev_dir = direction;

    if (frame % AI_UPDATE_INTERVAL == 0 || q_learning.episodes < MAX_TRAINING_EPISODES) {
        int action = chooseAction(game.head_x, game.head_y, direction);
        
        int new_x = game.head_x, new_y = game.head_y;
        switch (action) {
            case 0: new_x--; break; // Up
            case 1: new_x++; break; // Down
            case 2: new_y--; break; // Left
            case 3: new_y++; break; // Right
        }

        bool valid = isValidPosition(new_x, new_y) && !isBodyPosition(new_x, new_y);
        bool got_food = (new_x == game.food_x && new_y == game.food_y);
        bool crashed = !valid;

        float reward = calculateReward(prev_x, prev_y, new_x, new_y, got_food, crashed);
        int old_state = getStateIndex(prev_x, prev_y, prev_dir);
        int new_state = getStateIndex(new_x, new_y, action);
        updateQTable(old_state, action, new_state, reward);

        if (valid) {
            direction = action;
        } else {
            vector<int> safe_actions;
            for (int i = 0; i < 4; i++) {
                int test_x = game.head_x, test_y = game.head_y;
                switch (i) {
                    case 0: test_x--; break;
                    case 1: test_x++; break;
                    case 2: test_y--; break;
                    case 3: test_y++; break;
                }
                if (isValidPosition(test_x, test_y) && !isBodyPosition(test_x, test_y)) {
                    safe_actions.push_back(i);
                }
            }
            if (!safe_actions.empty()) {
                direction = safe_actions[rand() % safe_actions.size()];
            } else {
                return true;
            }
        }
    }

    // Execute the move
    switch (direction) {
        case 0: game.head_x--; break;
        case 1: game.head_x++; break;
        case 2: game.head_y--; break;
        case 3: game.head_y++; break;
    }

    // Check collision
    if (!isValidPosition(game.head_x, game.head_y) || isBodyPosition(game.head_x, game.head_y)) {
        return true;
    }

    // Update snake body
    game.trail.insert(game.trail.begin(), {game.head_x, game.head_y});
    if (game.trail.size() > game.length + 2) {
        game.trail.resize(game.length + 2);
    }

    game.body.insert(game.body.begin(), {game.head_x, game.head_y});
    if (game.body.size() > game.length + 1) {
        game.body.resize(game.length + 1);
    }

    // Check food
    if (game.head_x == game.food_x && game.head_y == game.food_y) {
        game.score++;
        game.length++;
        spawnFood();
    }

    return false;
}

// SDL functions
void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        cerr << "SDL_Init Error: " << SDL_GetError() << endl;
        exit(1);
    }

    sdl.window = SDL_CreateWindow("AI Snake",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                WIDTH * CELL_SIZE,
                                HEIGHT * CELL_SIZE,
                                SDL_WINDOW_SHOWN);
    if (!sdl.window) {
        cerr << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
        SDL_Quit();
        exit(1);
    }

    sdl.renderer = SDL_CreateRenderer(sdl.window, -1, 
                                    SDL_RENDERER_ACCELERATED | 
                                    SDL_RENDERER_PRESENTVSYNC);
    if (!sdl.renderer) {
        cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(sdl.window);
        SDL_Quit();
        exit(1);
    }

    if (SDL_SetRenderDrawBlendMode(sdl.renderer, SDL_BLENDMODE_BLEND) != 0) {
        cerr << "SDL_SetRenderDrawBlendMode Error: " << SDL_GetError() << endl;
    }
}

void drawGame() {
    // Clear screen
    SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl.renderer);

    // Draw border
    SDL_SetRenderDrawColor(sdl.renderer, 50, 50, 50, 255);
    SDL_Rect border = {0, 0, WIDTH * CELL_SIZE, HEIGHT * CELL_SIZE};
    SDL_RenderDrawRect(sdl.renderer, &border);

    // Draw food
    SDL_SetRenderDrawColor(sdl.renderer, 255, 0, 0, 255);
    SDL_Rect food = {game.food_y * CELL_SIZE, game.food_x * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(sdl.renderer, &food);

    // Draw snake body
    SDL_SetRenderDrawColor(sdl.renderer, 0, 180, 0, 255);
    for (const auto& seg : game.body) {
        if (seg.size() == 2) {
            SDL_Rect body = {seg[1] * CELL_SIZE, seg[0] * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            SDL_RenderFillRect(sdl.renderer, &body);
        }
    }

    // Draw snake head
    SDL_SetRenderDrawColor(sdl.renderer, 0, 255, 0, 255);
    SDL_Rect head = {game.head_y * CELL_SIZE, game.head_x * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(sdl.renderer, &head);

    SDL_RenderPresent(sdl.renderer);
}

void logPerformance() {
    if (q_learning.episodes % LOG_INTERVAL == 0) {
        // Calculate average Q-value
        float total_q = 0;
        int count = 0;
        for (const auto& row : q_learning.table) {
            for (float val : row) {
                total_q += val;
                count++;
            }
        }
        float avg_q = count > 0 ? total_q / count : 0;

        performance.scores.push_back(game.score);
        performance.avg_q_values.push_back(avg_q);
        performance.lengths.push_back(game.length);

        #ifdef __EMSCRIPTEN__
        updateCharts(q_learning.episodes, game.score, avg_q, q_learning.exploration_rate);
        #else
        cout << "Episode: " << q_learning.episodes 
             << " | Score: " << game.score 
             << " | Avg Q: " << avg_q
             << " | Exploration: " << q_learning.exploration_rate << endl;
        #endif

        // Reset score for next episode
        game.score = 0;
    }
}

// Main game loop
void mainLoop() {
    static int direction = rand() % 4;
    static int reset_timer = 0;

    if (reset_timer > 0) {
        reset_timer--;
        if (reset_timer == 0) {
            resetGame();
        }
        return;
    }

    bool crashed = moveSnake(direction);
    drawGame();

    if (q_learning.episodes < MAX_TRAINING_EPISODES) {
        q_learning.episodes++;
        q_learning.exploration_rate = max(MIN_EXPLORATION, 
                                         q_learning.exploration_rate * q_learning.exploration_decay);
        logPerformance();
    }

    if (crashed) {
        reset_timer = 5;
    }
}

// Cleanup
void cleanup() {
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit();
}

int main() {
    srand((unsigned)time(nullptr));

    #ifdef __EMSCRIPTEN__
    export_functions();
    initChartJS();
    #endif

    // Initialize game state
    auto all_positions = generateAllPositions();
    game.body = {{HEIGHT/2, WIDTH/2}};
    game.trail = {{HEIGHT/2, WIDTH/2}};
    auto free_positions = getFreePositions(game.trail, all_positions);
    if (!free_positions.empty()) {
        game.food_x = free_positions[0][0];
        game.food_y = free_positions[0][1];
    }

    // Initialize Q-learning
    initQTable();
    
    // Initialize SDL
    initSDL();

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
    #else
    int direction = rand() % 4;
    int reset_timer = 0;
    
    bool running = true;
    while (running) {
        if (reset_timer > 0) {
            reset_timer--;
            if (reset_timer == 0) {
                resetGame();
            }
            SDL_Delay(game.speed);
            continue;
        }

        bool crashed = moveSnake(direction);
        drawGame();
        SDL_Delay(game.speed);

        if (q_learning.episodes < MAX_TRAINING_EPISODES) {
            q_learning.episodes++;
            q_learning.exploration_rate = max(MIN_EXPLORATION, 
                                            q_learning.exploration_rate * q_learning.exploration_decay);
            logPerformance();
        }

        if (crashed) {
            reset_timer = 5;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
    }
    #endif

    cleanup();
    return 0;
}

