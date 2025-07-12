cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
        -O3 \
        -s USE_SDL=2 \
        -s USE_SDL_IMAGE=2 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s NO_EXIT_RUNTIME=1 \
        -s ENVIRONMENT=web \
        -s MODULARIZE=1 \
        -s EXPORT_NAME=AISnakeModule \
        -s EXPORTED_FUNCTIONS=['_main','_start_game'] \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
        --shell-file ${CMAKE_SOURCE_DIR}/shell.html \
    ")

else()
    find_package(SDL2 REQUIRED)
    add_compile_options(-O3)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()
