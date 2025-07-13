cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCE_FILES "src/*.cpp")

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Compile options
    add_compile_options(
        -O3
        -sUSE_SDL=2
        -sALLOW_MEMORY_GROWTH=1
    )
    
    # Linker options - use target_link_options instead
    add_executable(aisnake_web ${SOURCE_FILES})
    
    target_link_options(aisnake_web PRIVATE
        --shell-file=${CMAKE_SOURCE_DIR}/shell.html
        -sEXPORTED_FUNCTIONS=_main,_getExplorationRate,_getStats
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        -sMODULARIZE=1
        -sALLOW_MEMORY_GROWTH=1
        -sUSE_SDL=2
    )
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    add_executable(aisnake_web ${SOURCE_FILES})
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()
