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

const int WIDTH = 20;
const int HEIGHT = 20;
const int CELL_SIZE = 20;
const int AI_UPDATE_INTERVAL = 5;
const int LOG_INTERVAL = 100; // Log every 100 episodes

// Game state
int ai_pion = HEIGHT / 2;
int ai_poz = WIDTH / 2;
int ai_punkty = 0;
int ai_longer = 2;
int food_x = 0, food_y = 0;
bool ai_kolizja = false;
int czas = 200;

// AI snake data
vector<vector<int>> ai_pozycja;
vector<vector<int>> ai_pozycjac;
vector<vector<int>> v1;

// Q-learning
vector<vector<float>> q_table;
float learning_rate = 0.3f;
float discount_factor = 0.95f;
float exploration_rate = 0.5f;
int training_episodes = 0;
const int MAX_TRAINING_EPISODES = 20000;
const float MIN_EXPLORATION = 0.01f;

// Performance tracking
vector<int> episode_scores;
vector<float> avg_q_values;
vector<int> episode_lengths;

// SDL
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

#ifdef __EMSCRIPTEN__
// Update the EM_JS initChartJS function to be more robust
EM_JS(void, initChartJS, (), {
    // Wait until DOM is fully loaded
    document.addEventListener('DOMContentLoaded', function() {
        // Create container if it doesn't exist
        let container = document.getElementById('chart-container');
        if (!container) {
            container = document.createElement('div');
            container.id = 'chart-container';
            container.style.width = '800px';
            container.style.margin = '0 auto';
            container.style.backgroundColor = '#222';
            container.style.padding = '20px';
            container.style.borderRadius = '10px';
            document.body.appendChild(container);
        }

        // Create status element if it doesn't exist
        let statusElement = document.getElementById('status');
        if (!statusElement) {
            statusElement = document.createElement('div');
            statusElement.id = 'status';
            statusElement.style.textAlign = 'center';
            statusElement.style.marginBottom = '20px';
            statusElement.style.fontSize = '18px';
            statusElement.style.padding = '10px';
            statusElement.style.backgroundColor = '#333';
            statusElement.style.borderRadius = '5px';
            container.insertBefore(statusElement, container.firstChild);
        }

        // Initialize charts only if they don't exist
        if (typeof window.scoreChart === 'undefined') {
            const scoreCanvas = document.createElement('canvas');
            scoreCanvas.id = 'scoreChart';
            scoreCanvas.width = 800;
            scoreCanvas.height = 400;
            container.appendChild(scoreCanvas);

            const qValueCanvas = document.createElement('canvas');
            qValueCanvas.id = 'qValueChart';
            qValueCanvas.width = 800;
            qValueCanvas.height = 400;
            container.appendChild(qValueCanvas);

            window.scoreChart = new Chart(scoreCanvas, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Score',
                        data: [],
                        borderColor: 'rgb(75, 192, 192)',
                        tension: 0.1,
                        fill: false
                    }]
                },
                options:
            responsive: false,
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });

    window.qValueChart = new Chart(qValueCanvas, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Average Q-value',
                data: [],
                borderColor: 'rgb(255, 99, 132)',
                tension: 0.1,
                fill: false
            }]
        },
        options: {
            responsive: false
        }
    });
});

EM_JS(void, updateCharts, (int episode, int score, float avg_q), {
    // Safe element access
    var statusElement = document.getElementById('status');
    if (statusElement) {
        statusElement.innerHTML = 
            `Episode: ${episode} | Score: ${score} | Exploration: ${Module.getExplorationRate().toFixed(2)}`;
    }
    
    // Initialize charts if they don't exist
    if (typeof window.scoreChart === 'undefined') {
        // Chart initialization code here...
    }
    
    // Update charts if they exist
    if (window.scoreChart && window.qValueChart) {
        window.scoreChart.data.labels.push(episode);
        window.scoreChart.data.datasets[0].data.push(score);
        window.scoreChart.update();
        
        window.qValueChart.data.labels.push(episode);
        window.qValueChart.data.datasets[0].data.push(avg_q);
        window.qValueChart.update();
    }
});

EM_JS(float, getExplorationRate, (), {
    return Module.getExplorationRate();
});
#endif

void logPerformance() {
    if (training_episodes % LOG_INTERVAL == 0) {
        // Calculate average Q-value
        float total_q = 0;
        int count = 0;
        for (const auto& row : q_table) {
            for (float val : row) {
                total_q += val;
                count++;
            }
        }
        float avg_q = count > 0 ? total_q / count : 0;

        episode_scores.push_back(ai_punkty);
        avg_q_values.push_back(avg_q);
        episode_lengths.push_back(ai_longer);

        #ifdef __EMSCRIPTEN__
        updateCharts(training_episodes, ai_punkty, avg_q);
        #else
        cout << "Episode: " << training_episodes 
             << " | Score: " << ai_punkty 
             << " | Avg Q: " << avg_q
             << " | Exploration: " << exploration_rate << endl;
        #endif

        // Reset score for next episode
        ai_punkty = 0;
    }
}

void initSDL() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Create window
    window = SDL_CreateWindow("AI Snake",
                            SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED,
                            WIDTH * CELL_SIZE,
                            HEIGHT * CELL_SIZE,
                            SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        exit(1);
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, 
                                SDL_RENDERER_ACCELERATED | 
                                SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }

    // Set blend mode (remove logical size as it's not supported in Emscripten)
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) != 0) {
        std::cerr << "SDL_SetRenderDrawBlendMode Error: " << SDL_GetError() << std::endl;
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

    // Draw snake body
    SDL_SetRenderDrawColor(renderer, 0, 180, 0, 255);
    for (const auto& seg : ai_pozycja) {
        if (seg.size() == 2) {
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
    vector<vector<int>> new_v1(WIDTH * HEIGHT, vector<int>(2, 0));
    int idx = 0;
    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j, ++idx) {
            new_v1[idx] = {i, j};
        }
    }
    return new_v1;
}

vector<vector<int>> wolne(const vector<vector<int>>& snake, vector<vector<int>> all) {
    vector<vector<int>> result = all;
    for (const auto& seg : snake) {
        result.erase(remove(result.begin(), result.end(), seg), result.end());
    }
    return result;
}

bool isValidPosition(int x, int y) {
    return x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH;
}

bool isAIBody(int x, int y) {
    for (const auto& seg : ai_pozycja) {
        if (seg.size() == 2 && seg[0] == x && seg[1] == y) {
            return true;
        }
    }
    return false;
}

void initQTable() {
    q_table.resize(WIDTH * HEIGHT * 4);
    for (auto& row : q_table) {
        row.assign(4, 0.0f);
        for (float& val : row) {
            val = (rand() % 100) / 1000.0f - 0.05f;
        }
    }
}

int getStateIndex(int x, int y, int dir) {
    if (!isValidPosition(x, y)) return 0;
    return (y * WIDTH + x) * 4 + dir;
}

int chooseAction(int x, int y, int current_dir) {
    if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < exploration_rate) {
        return rand() % 4;
    }

    int state = getStateIndex(x, y, current_dir);
    if (state >= 0 && state < q_table.size()) {
        return (int)distance(q_table[state].begin(),
            max_element(q_table[state].begin(), q_table[state].end()));
    }
    return rand() % 4;
}

void updateQTable(int ox, int oy, int odir, int action, int nx, int ny, float reward) {
    int old_state = getStateIndex(ox, oy, odir);
    int new_state = getStateIndex(nx, ny, action);

    if (old_state >= 0 && old_state < q_table.size() && 
        new_state >= 0 && new_state < q_table.size()) {
        float best_future = *max_element(q_table[new_state].begin(), q_table[new_state].end());
        q_table[old_state][action] = (1 - learning_rate) * q_table[old_state][action] +
            learning_rate * (reward + discount_factor * best_future);
    }
}

float calculateReward(int x, int y, bool got_food, bool crashed) {
    if (crashed) return -200.0f;
    if (got_food) return 100.0f;
    
    int min_wall_dist = min(min(x, HEIGHT-1-x), min(y, WIDTH-1-y));
    if (min_wall_dist == 0) return -50.0f;
    if (min_wall_dist == 1) return -10.0f;
    
    float dist = sqrtf((x-food_x)*(x-food_x) + (y-food_y)*(y-food_y));
    float max_dist = sqrtf(WIDTH*WIDTH + HEIGHT*HEIGHT);
    return 5.0f * (1.0f - (dist / max_dist));
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
        updateQTable(prev_x, prev_y, prev_dir, action, nx, ny, reward);

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
            } else {
                return true;
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

    ai_pozycja.insert(ai_pozycja.begin(), {ai_pion, ai_poz});
    if (ai_pozycja.size() > ai_longer + 1) {
        ai_pozycja.resize(ai_longer + 1);
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
        exploration_rate = max(MIN_EXPLORATION, exploration_rate * 0.9999f);
        logPerformance();
    }

    if (crashed) {
        reset_timer = 5;
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    float getExplorationRate() {
        return exploration_rate;
    }
}

int main() {
    srand((unsigned)time(nullptr));

    v1 = generuj();
    ai_pozycja = {{HEIGHT/2, WIDTH/2}};
    ai_pozycjac = {{HEIGHT/2, WIDTH/2}};
    
    auto freeTiles = wolne(ai_pozycjac, v1);
    if (!freeTiles.empty()) {
        food_x = freeTiles[0][0];
        food_y = freeTiles[0][1];
    }

    initQTable();
    initSDL();

    #ifdef __EMSCRIPTEN__
    initChartJS();
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
            exploration_rate = max(MIN_EXPLORATION, exploration_rate * 0.9999f);
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

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
