#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <queue>
#include <cmath>
#include <SDL2/SDL.h>
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
const int MAX_MOVES_WITHOUT_FOOD = WIDTH * HEIGHT * 2; // Prevent infinite loops

// Enhanced GameState with more statistics
struct GameState {
    int head_x = HEIGHT / 2;
    int head_y = WIDTH / 2;
    int score = 0;
    int length = 3; // Start with longer body to encourage movement
    int food_x = 0, food_y = 0;
    bool crashed = false;
    int speed = 200;
    vector<vector<int>> body;
    vector<vector<int>> trail;
    int lifetime_score = 0;
    int moves_since_last_fruit = 0;
    int total_fruits = 0;
    int crashes = 0;
};

// Enhanced Q-learning with adaptive learning
struct QLearning {
    vector<vector<float>> table;
    float learning_rate = 0.2f; // Increased learning rate
    float discount_factor = 0.95f;
    float exploration_rate = 1.0f;
    int episodes = 0;
    const float exploration_decay = 0.9997f; // Slower decay
};

// Performance tracking
struct Performance {
    vector<int> scores;
    vector<float> avg_q_values;
    vector<int> lengths;
    vector<float> exploration_rates;
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

        var explorationCanvas = document.createElement('canvas');
        explorationCanvas.id = 'explorationChart';
        container.appendChild(explorationCanvas);

        window.scoreChart = new Chart(scoreCanvas, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Score',
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

        window.explorationChart = new Chart(explorationCanvas, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Exploration Rate',
                    data: [],
                    borderColor: 'rgba(255, 99, 132, 1)',
                    borderWidth: 1,
                    fill: false
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: { 
                        beginAtZero: true,
                        max: 1.0
                    }
                }
            }
        });

        window.chartsInitialized = true;
    }
});

EM_JS(void, updateCharts, (int episode, int score, float exploration), {
    try {
        var statusElement = document.getElementById('status');
        if (statusElement) {
            statusElement.innerHTML = 
                `Episode: ${episode} | Score: ${score} | Exploration: ${exploration.toFixed(3)}`;
        }
        
        if (window.scoreChart && window.explorationChart) {
            // Score chart
            window.scoreChart.data.labels.push(episode);
            window.scoreChart.data.datasets[0].data.push(score);
            if (window.scoreChart.data.labels.length > 500) {
                window.scoreChart.data.labels.shift();
                window.scoreChart.data.datasets[0].data.shift();
            }
            window.scoreChart.update();
            
            // Exploration chart
            window.explorationChart.data.labels.push(episode);
            window.explorationChart.data.datasets[0].data.push(exploration);
            if (window.explorationChart.data.labels.length > 500) {
                window.explorationChart.data.labels.shift();
                window.explorationChart.data.datasets[0].data.shift();
            }
            window.explorationChart.update();
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
        stats = "Fruits: " + to_string(game.total_fruits) + 
                " | Crashes: " + to_string(game.crashes) + 
                " | Length: " + to_string(game.length);
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
        if (game.body[i][0] == x && game.body[i][1] == y) {
            return true;
        }
    }
    return false;
}

void initQTable() {
    // More compact state representation
    int state_space_size = WIDTH * HEIGHT * 4 * 16; // x, y, dir, danger
    q_learning.table.resize(state_space_size);
    for (auto& row : q_learning.table) {
        row.assign(4, 0.0f);
        // Small random initial values to encourage exploration
        for (float& val : row) {
            val = (rand() % 100) / 1000.0f - 0.05f;
        }
    }
}

int getStateIndex(int x, int y, int dir) {
    if (!isValidPosition(x, y)) return 0;
    
    // Food direction (4 possible combinations)
    int food_dir = 0;
    if (game.food_x > x) food_dir |= 1;
    else if (game.food_x < x) food_dir |= 2;
    if (game.food_y > y) food_dir |= 4;
    else if (game.food_y < y) food_dir |= 8;
    
    // Danger detection (immediate surroundings)
    int danger = 0;
    if (!isValidPosition(x-1, y) || isBodyPosition(x-1, y, false)) danger |= 1;
    if (!isValidPosition(x+1, y) || isBodyPosition(x+1, y, false)) danger |= 2;
    if (!isValidPosition(x, y-1) || isBodyPosition(x, y-1, false)) danger |= 4;
    if (!isValidPosition(x, y+1) || isBodyPosition(x, y+1, false)) danger |= 8;
    
    // Compact state representation
    return (y * WIDTH + x) * 64 + dir * 16 + food_dir * 4 + danger;
}

int chooseAction(int x, int y, int current_dir) {
    // Epsilon-greedy policy
    if ((float)rand() / RAND_MAX < q_learning.exploration_rate) {
        return rand() % 4;
    }

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
    if (crashed) {
        game.crashes++;
        return -100.0f - (game.length * 2.0f); // Stronger penalty for longer snakes
    }
    
    if (got_food) {
        return 50.0f + (game.length * 0.5f); // Reward increases with length
    }
    
    float reward = 0.0f;
    
    // Distance to food reward (Manhattan distance)
    float prev_dist = abs(prev_x - game.food_x) + abs(prev_y - game.food_y);
    float new_dist = abs(x - game.food_x) + abs(y - game.food_y);
    reward += (prev_dist - new_dist) * 2.0f;
    
    // Body avoidance (more sophisticated)
    for (int i = 1; i < game.body.size(); i++) {
        float dist = sqrt(pow(x - game.body[i][0], 2) + pow(y - game.body[i][1], 2);
        if (dist < 3) reward -= (3 - dist) * 5.0f; // Strong penalty for getting close to body
    }
    
    // Small reward for surviving
    reward += 0.1f;
    
    // Penalty for not eating for a while
    if (game.moves_since_last_fruit > WIDTH) {
        reward -= (game.moves_since_last_fruit - WIDTH) * 0.1f;
    }
    
    return reward;
}

void spawnFood() {
    auto all_positions = generateAllPositions();
    auto free_positions = getFreePositions(game.body, all_positions);
    if (!free_positions.empty()) {
        int k = rand() % free_positions.size();
        game.food_x = free_positions[k][0];
        game.food_y = free_positions[k][1];
    }
}

void resetGame() {
    game.head_x = HEIGHT / 2;
    game.head_y = WIDTH / 2;
    game.score = 0;
    game.length = 3; // Start with longer body
    game.body = {{game.head_x, game.head_y}};
    game.crashed = false;
    game.moves_since_last_fruit = 0;
    
    spawnFood();
}

bool moveSnake(int& direction) {
    game.moves_since_last_fruit++;
    
    // Check for starvation
    if (game.moves_since_last_fruit > MAX_MOVES_WITHOUT_FOOD) {
        return true;
    }

    int prev_x = game.head_x;
    int prev_y = game.head_y;
    int prev_dir = direction;

    // AI decision
    if (q_learning.episodes < MAX_TRAINING_EPISODES) {
        int action = chooseAction(game.head_x, game.head_y, direction);
        
        // Simulate move
        int new_x = game.head_x, new_y = game.head_y;
        switch (action) {
            case 0: new_x--; break;
            case 1: new_x++; break;
            case 2: new_y--; break;
            case 3: new_y++; break;
        }

        bool valid = isValidPosition(new_x, new_y) && !isBodyPosition(new_x, new_y, false);
        bool got_food = (new_x == game.food_x && new_y == game.food_y);
        bool crashed = !valid;

        float reward = calculateReward(prev_x, prev_y, new_x, new_y, got_food, crashed);
        int old_state = getStateIndex(prev_x, prev_y, prev_dir);
        int new_state = getStateIndex(new_x, new_y, action);
        updateQTable(old_state, action, new_state, reward);

        if (valid) {
            direction = action;
        } else {
            // Find safe alternative moves
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
            direction = !safe_actions.empty() ? safe_actions[rand() % safe_actions.size()] : direction;
        }
    }

    // Execute move
    switch (direction) {
        case 0: game.head_x--; break;
        case 1: game.head_x++; break;
        case 2: game.head_y--; break;
        case 3: game.head_y++; break;
    }

    // Check collisions
    if (!isValidPosition(game.head_x, game.head_y) || isBodyPosition(game.head_x, game.head_y, false)) {
        return true;
    }

    // Update body
    game.body.insert(game.body.begin(), {game.head_x, game.head_y});
    if (game.body.size() > game.length) {
        game.body.pop_back();
    }

    // Check food
    if (game.head_x == game.food_x && game.head_y == game.food_y) {
        game.score++;
        game.total_fruits++;
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

    #ifdef __EMSCRIPTEN__
    EM_ASM(
        Module.canvas = document.getElementById('canvas');
        Module.canvas.width = 400;
        Module.canvas.height = 400;
    );
    #endif

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
}

void drawGame() {
    SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl.renderer);

    // Draw grid
    SDL_SetRenderDrawColor(sdl.renderer, 30, 30, 30, 255);
    for (int i = 0; i <= WIDTH; i++) {
        SDL_RenderDrawLine(sdl.renderer, i * CELL_SIZE, 0, i * CELL_SIZE, HEIGHT * CELL_SIZE);
    }
    for (int j = 0; j <= HEIGHT; j++) {
        SDL_RenderDrawLine(sdl.renderer, 0, j * CELL_SIZE, WIDTH * CELL_SIZE, j * CELL_SIZE);
    }

    // Draw food
    SDL_SetRenderDrawColor(sdl.renderer, 255, 50, 50, 255);
    SDL_Rect food = {game.food_y * CELL_SIZE, game.food_x * CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_RenderFillRect(sdl.renderer, &food);

    // Draw body (gradient from head to tail)
    for (size_t i = 0; i < game.body.size(); i++) {
        float ratio = (float)i / game.body.size();
        SDL_SetRenderDrawColor(sdl.renderer, 
                              (int)(100 * ratio), 
                              (int)(200 - 100 * ratio), 
                              (int)(100 * ratio), 
                              255);
        SDL_Rect segment = {
            game.body[i][1] * CELL_SIZE,
            game.body[i][0] * CELL_SIZE,
            CELL_SIZE,
            CELL_SIZE
        };
        SDL_RenderFillRect(sdl.renderer, &segment);
    }

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
        performance.exploration_rates.push_back(q_learning.exploration_rate);

        #ifdef __EMSCRIPTEN__
        updateCharts(q_learning.episodes, game.score, q_learning.exploration_rate);
        #else
        cout << "Episode: " << q_learning.episodes 
             << " | Score: " << game.score 
             << " | Length: " << game.length
             << " | Avg Q: " << avg_q
             << " | Exploration: " << q_learning.exploration_rate << endl;
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
        reset_timer = 10; // Longer reset delay for visibility
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
    #endif

    resetGame();
    initQTable();
    initSDL();

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
    #else
    while (true) {
        mainLoop();
        SDL_Delay(game.speed);
    }
    #endif

    cleanup();
    return 0;
}
