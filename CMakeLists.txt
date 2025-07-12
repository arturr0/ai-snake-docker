cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXECUTABLE_SUFFIX ".html")

add_compile_options(
    -O3
    -sUSE_SDL=2
    -sUSE_SDL_IMAGE=2
    -sSDL2_IMAGE_FORMATS='["png"]'
)

add_link_options(
    -O3
    -sUSE_SDL=2
    -sUSE_SDL_IMAGE=2
    --shell-file ${CMAKE_SOURCE_DIR}/shell.html
    -sEXIT_RUNTIME=1
    -sALLOW_MEMORY_GROWTH=1
)

add_executable(aisnake_web src/main.cpp)