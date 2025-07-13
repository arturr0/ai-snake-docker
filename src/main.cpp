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

EM_JS(void, export_functions, (), {
    Module['getExplorationRate'] = Module.cwrap('getExplorationRate', 'number', []);
    Module['getStats'] = Module.cwrap('getStats', 'string', []);
});
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
const int MAX_MOVES_WITHOUT_FOOD = 100;

// Enhanced GameState with more statistics
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
    int lifetime_score = 0;
    int fruits_per_life = 0;
    int total_fruits = 0;
    int lives = 0;
    int moves_since_last_fruit = 0;
    float avg_fruits_per_life = 0;
    int longest_life_fruits = 0;
    int recent_fruits[10] = {0};
    int recent_index = 0;
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
    vector<float> fruits_per_life_history;
    vector<int> collision_counts;
};

// SDL resources
struct SDLResources {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Surface* screen = nullptr;
};

// Global game objects
GameState game;
QLearning q_learning;
Performance performance;
SDLResources sdl;

void spawnFood() {
    auto all_positions = generateAllPositions();
    auto free_positions = getFreePositions(game.trail, all_positions);
    if (!free_positions.empty()) {
        int k = rand() % free_positions.size();
        game.food_x = free_positions[k][0];
        game.food_y = free_positions[k][1];
    }
}

#ifdef __EMSCRIPTEN__
EM_JS(void, initChartJS, (), {
    if (typeof window.chartsInitialized === 'undefined') {
        console.log("Initializing charts...");
        
        var container = document.getElementById('chart-container');
        if (!container) {
            container = document.createElement('div');
            container.id = 'chart-container';
            container.style.width = '800px';
            container.style.margin = '0 auto';
            container.style.padding = '20px';
            container.style.backgroundColor = '#222';
            document.body.insertBefore(container, document.body.firstChild);
        }

        var scoreCanvas = document.createElement('canvas');
        scoreCanvas.id = 'scoreChart';
        container.appendChild(scoreCanvas);

        var lifetimeCanvas = document.createElement('canvas');
        lifetimeCanvas.id = 'lifetimeChart';
        container.appendChild(lifetimeCanvas);

        var fruitsCanvas = document.createElement('canvas');
        fruitsCanvas.id = 'fruitsChart';
        container.appendChild(fruitsCanvas);

        var collisionCanvas = document.createElement('canvas');
        collisionCanvas.id = 'collisionChart';
        container.appendChild(collisionCanvas);

        window.scoreChart = new Chart(scoreCanvas, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Episode Score',
                    data: [],
                    borderColor: 'rgba(75, 192, 192, 1)',
                    borderWidth: 1,
                    fill: false
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: { beginAtZero: true }
                }
            }
        });

        window.lifetimeChart = new Chart(lifetimeCanvas, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Lifetime Score',
                    data: [],
                    borderColor: 'rgba(255, 99, 132, 1)',
                    borderWidth: 1,
                    fill: false
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: { beginAtZero: true }
                }
            }
        });

        window.fruitsChart = new Chart(fruitsCanvas, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Fruits/Life (Avg 10)',
                    data: [],
                    borderColor: 'rgba(54, 162, 235, 1)',
                    borderWidth: 1,
                    fill: false
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: { beginAtZero: true }
                }
            }
        });

        window.collisionChart = new Chart(collisionCanvas, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Collision Rate',
                    data: [],
                    borderColor: 'rgba(255, 159, 64, 1)',
                    borderWidth: 1,
                    fill: false
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: { beginAtZero: true }
                }
            }
        });

        window.chartsInitialized = true;
    }
});

EM_JS(void, updateCharts, (int episode, int score, int lifetime, int fruits_per_life, int collision_rate), {
    try {
        var statusElement = document.getElementById('status');
        if (statusElement) {
            statusElement.innerHTML = 
                `Episode: ${episode} | Score: ${score} | Lifetime: ${lifetime} | Fruits/Life: ${fruits_per_life}`;
        }
        
        var statsElement = document.getElementById('stats');
        if (statsElement) {
            statsElement.innerHTML = 
                `Total Fruits: ${Module.ccall('getStats', 'string', [])}`;
        }
        
        if (window.scoreChart && window.lifetimeChart && window.fruitsChart && window.collisionChart) {
            // Score chart
            window.scoreChart.data.labels.push(episode);
            window.scoreChart.data.datasets[0].data.push(score);
            if (window.scoreChart.data.labels.length > 500) {
                window.scoreChart.data.labels.shift();
                window.scoreChart.data.datasets[0].data.shift();
            }
            window.scoreChart.update();
            
            // Lifetime chart
            window.lifetimeChart.data.labels.push(episode);
            window.lifetimeChart.data.datasets[0].data.push(lifetime);
            if (window.lifetimeChart.data.labels.length > 500) {
                window.lifetimeChart.data.labels.shift();
                window.lifetimeChart.data.datasets[0].data.shift();
            }
            window.lifetimeChart.update();
            
            // Fruits per life chart
            window.fruitsChart.data.labels.push(episode);
            window.fruitsChart.data.datasets[0].data.push(fruits_per_life);
            if (window.fruitsChart.data.labels.length > 500) {
                window.fruitsChart.data.labels.shift();
                window.fruitsChart.data.datasets[0].data.shift();
            }
            window.fruitsChart.update();
            
            // Collision rate chart
            window.collisionChart.data.labels.push(episode);
            window.collisionChart.data.datasets[0].data.push(collision_rate);
            if (window.collisionChart.data.labels.length > 500) {
                window.collisionChart.data.labels.shift();
                window.collisionChart.data.datasets[0].data.shift();
            }
            window.collisionChart.update();
        }
    } catch(e) {
        console.error('Chart update error:', e);
    }
});
#endif

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    float getExplorationRate() {
        return q_learning.exploration_rate;
    }
    
    EMSCRIPTEN_KEEPALIVE
    const char* getStats() {
        static string stats;
        stats = "Total: " + to_string(game.total_fruits) + 
                " | Avg: " + to_string(game.avg_fruits_per_life) + 
                " | Best: " + to_string(game.longest_life_fruits);
        return stats.c_str();
    }
}

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

bool isBodyPosition(int x, int y, bool include_head = true) {
    for (size_t i = include_head ? 0 : 1; i < game.body.size(); ++i) {
        const auto& seg = game.body[i];
        if (seg.size() == 2 && seg[0] == x && seg[1] == y) {
            return true;
        }
    }
    return false;
}

void initQTable() {
    q_learning.table.resize(WIDTH * HEIGHT * 1024);
    for (auto& row : q_learning.table) {
        row.assign(4, 0.0f);
        for (float& val : row) {
            val = (rand() % 100) / 1000.0f - 0.01f;
        }
    }
}

int getStateIndex(int x, int y, int dir) {
    if (!isValidPosition(x, y)) return 0;
    
    int food_dir = 0;
    if (game.food_x > x) food_dir = 1;
    else if (game.food_x < x) food_dir = 2;
    if (game.food_y > y) food_dir |= 4;
    else if (game.food_y < y) food_dir |= 8;
    
    int danger = 0;
    if (!isValidPosition(x-1, y) || isBodyPosition(x-1, y, false)) danger |= 1;
    if (!isValidPosition(x+1, y) || isBodyPosition(x+1, y, false)) danger |= 2;
    if (!isValidPosition(x, y-1) || isBodyPosition(x, y-1, false)) danger |= 4;
    if (!isValidPosition(x, y+1) || isBodyPosition(x, y+1, false)) danger |= 8;
    
    return (y * WIDTH + x) * 256 + dir * 64 + food_dir * 4 + danger;
}

int chooseAction(int x, int y, int current_dir) {
    if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < q_learning.exploration_rate) {
        return rand() % 4;
    }

    int state = getStateIndex(x, y, current_dir);
    if (state >= 0 && static_cast<size_t>(state) < q_learning.table.size()) {
        return distance(q_learning.table[state].begin(),
                      max_element(q_learning.table[state].begin(), q_learning.table[state].end()));
    }
    return rand() % 4;
}

void updateQTable(int old_state, int action, int new_state, float reward) {
    if (old_state >= 0 && static_cast<size_t>(old_state) < q_learning.table.size() && 
    new_state >= 0 && static_cast<size_t>(new_state) < q_learning.table.size()) {
        float best_future = *max_element(q_learning.table[new_state].begin(), 
                                       q_learning.table[new_state].end());
        q_learning.table[old_state][action] = 
            (1 - q_learning.learning_rate) * q_learning.table[old_state][action] +
            q_learning.learning_rate * (reward + q_learning.discount_factor * best_future);
    }
}

float calculateReward(int prev_x, int prev_y, int x, int y, bool got_food, bool crashed) {
    if (crashed) {
        // Strong penalty for crashing, scaled by snake length
        return -200.0f + (game.length * -5.0f);
    }
    
    if (got_food) {
        // Reward for getting food, scaled by snake length
        return 100.0f + (game.length * 1.0f);
    }
    
    float reward = 0;
    
    // Distance to food reward
    float prev_dist = abs(prev_x-game.food_x) + abs(prev_y-game.food_y);
    float new_dist = abs(x-game.food_x) + abs(y-game.food_y);
    reward += (prev_dist - new_dist) * 5.0f;
    
    // Body avoidance (more sophisticated)
    for (size_t i = 1; i < game.body.size(); i++) {
        float dist = sqrt(pow(x-game.body[i][0], 2) + pow(y-game.body[i][1], 2));
        if (dist < 3) reward -= (3-dist) * 10.0f;
    }
    
    // Small reward for surviving
    reward += 0.2f;
    
    // Penalty for not eating for a while
    if (game.moves_since_last_fruit > 50) {
        reward -= (game.moves_since_last_fruit - 50) * 0.5f;
    }
    
    return reward;
}

void resetGame() {
    game.head_x = HEIGHT / 2;
    game.head_y = WIDTH / 2;
    game.length = 2;
    game.score = 0;
    game.moves_since_last_fruit = 0;
    game.body = {{game.head_x, game.head_y}};
    game.trail = {{game.head_x, game.head_y}};
    game.crashed = false;
    game.lives++;
    
    // Update fruits statistics
    if (game.fruits_per_life > game.longest_life_fruits) {
        game.longest_life_fruits = game.fruits_per_life;
    }
    
    // Update recent fruits array
    game.recent_fruits[game.recent_index] = game.fruits_per_life;
    game.recent_index = (game.recent_index + 1) % 10;
    
    // Update average fruits per life
    if (game.lives > 1) {
        game.avg_fruits_per_life = (game.avg_fruits_per_life * (game.lives - 1) + game.fruits_per_life) / game.lives;
    } else {
        game.avg_fruits_per_life = game.fruits_per_life;
    }
    
    game.fruits_per_life = 0;
    
    spawnFood();
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
    
    game.moves_since_last_fruit++;

    int prev_x = game.head_x;
    int prev_y = game.head_y;
    int prev_dir = direction;

    if (frame % AI_UPDATE_INTERVAL == 0 || q_learning.episodes < MAX_TRAINING_EPISODES) {
        int action = chooseAction(game.head_x, game.head_y, direction);
        
        int new_x = game.head_x, new_y = game.head_y;
        switch (action) {
            case 0: new_x--; break;
            case 1: new_x++; break;
            case 2: new_y--; break;
            case 3: new_y++; break;
        }

        bool valid = isValidPosition(new_x, new_y) && !isBodyPosition(new_x, new_y, false);
        bool got_food = (new_x == game.food_x && new_y == game.food_y);
        bool crashed = !valid || (game.moves_since_last_fruit > MAX_MOVES_WITHOUT_FOOD);

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
                if (isValidPosition(test_x, test_y) && !isBodyPosition(test_x, test_y, false)) {
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

    switch (direction) {
        case 0: game.head_x--; break;
        case 1: game.head_x++; break;
        case 2: game.head_y--; break;
        case 3: game.head_y++; break;
    }

    if (!isValidPosition(game.head_x, game.head_y) || isBodyPosition(game.head_x, game.head_y, false) || 
        game.moves_since_last_fruit > MAX_MOVES_WITHOUT_FOOD) {
        performance.collision_counts.push_back(1);
        return true;
    }

    game.trail.insert(game.trail.begin(), {game.head_x, game.head_y});
    if (game.trail.size() > static_cast<size_t>(game.length + 2)) {
        game.trail.resize(game.length + 2);
    }

    game.body.insert(game.body.begin(), {game.head_x, game.head_y});
    if (game.body.size() > static_cast<size_t>(game.length)) {
        game.body.resize(game.length);
    }

    if (game.head_x == game.food_x && game.head_y == game.food_y) {
        game.score++;
        game.lifetime_score++;
        game.total_fruits++;
        game.fruits_per_life++;
        game.length++;
        game.moves_since_last_fruit = 0;
        spawnFood();
    }

    return false;
}

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
    SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl.renderer);

    SDL_SetRenderDrawColor(sdl.renderer, 50, 50, 50, 255);
    SDL_Rect border = {0, 0, WIDTH * CELL_SIZE, HEIGHT * CELL_SIZE};
    SDL_RenderDrawRect(sdl.renderer, &border);

    SDL_SetRenderDrawColor(sdl.renderer, 255, 0, 0, 255);
    SDL_Rect food = {game.food_y * CELL_SIZE, game.food_x * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(sdl.renderer, &food);

    SDL_SetRenderDrawColor(sdl.renderer, 0, 180, 0, 255);
    for (size_t i = 1; i < game.body.size(); i++) {
        const auto& seg = game.body[i];
        if (seg.size() == 2) {
            SDL_Rect body = {seg[1] * CELL_SIZE, seg[0] * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            SDL_RenderFillRect(sdl.renderer, &body);
        }
    }

    SDL_SetRenderDrawColor(sdl.renderer, 0, 255, 0, 255);
    SDL_Rect head = {game.head_y * CELL_SIZE, game.head_x * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(sdl.renderer, &head);

    SDL_RenderPresent(sdl.renderer);
}

void logPerformance() {
    if (q_learning.episodes % LOG_INTERVAL == 0) {
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
        performance.fruits_per_life_history.push_back(game.fruits_per_life);

        // Calculate collision rate
        float collision_rate = 0;
        if (!performance.collision_counts.empty()) {
            collision_rate = performance.collision_counts.back() * 100.0f / LOG_INTERVAL;
            performance.collision_counts.clear();
        }

        #ifdef __EMSCRIPTEN__
        updateCharts(q_learning.episodes, game.score, game.lifetime_score, game.fruits_per_life, collision_rate);
        #else
        cout << "Episode: " << q_learning.episodes 
             << " | Score: " << game.score 
             << " | Lifetime: " << game.lifetime_score
             << " | Fruits/Life: " << game.fruits_per_life
             << " | Avg Q: " << avg_q
             << " | Exploration: " << q_learning.exploration_rate 
             << " | Collision Rate: " << collision_rate << "%" << endl;
        #endif
    }
}

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
    
    // Create HTML status display
    EM_ASM(
        var statusDiv = document.createElement('div');
        statusDiv.id = 'status';
        statusDiv.style.cssText = 'position:fixed;top:10px;left:10px;color:white;font-family:Arial;z-index:1000;';
        document.body.appendChild(statusDiv);
        
        var statsDiv = document.createElement('div');
        statsDiv.id = 'stats';
        statsDiv.style.cssText = 'position:fixed;top:30px;left:10px;color:white;font-family:Arial;z-index:1000;';
        document.body.appendChild(statsDiv);
    );
    #endif

    auto all_positions = generateAllPositions();
    game.body = {{HEIGHT/2, WIDTH/2}};
    game.trail = {{HEIGHT/2, WIDTH/2}};
    auto free_positions = getFreePositions(game.trail, all_positions);
    if (!free_positions.empty()) {
        game.food_x = free_positions[0][0];
        game.food_y = free_positions[0][1];
    }

    initQTable();
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
