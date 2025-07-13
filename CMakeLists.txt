cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common compiler options
add_compile_options(-Wall -Wextra)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Combined linker flags for better readability
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} \
        -s USE_SDL=2 \
        -s WASM=1 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s EXPORTED_FUNCTIONS=['_main','_getExplorationRate'] \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
        --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
    
    # Optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
        add_link_options(-Oz)
    else()
        add_compile_options(-g4)
        add_link_options(-s ASSERTIONS=1)
    endif()
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common compiler options
add_compile_options(-Wall -Wextra)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Combined linker flags for better readability
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} \
        -s USE_SDL=2 \
        -s WASM=1 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s EXPORTED_FUNCTIONS=['_main','_getExplorationRate'] \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
        --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
    
    # Optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
        add_link_options(-Oz)
    else()
        add_compile_options(-g4)
        add_link_options(-s ASSERTIONS=1)
    endif()
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()
