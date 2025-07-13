cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Explicitly set source files
set(SOURCE_FILES src/main.cpp)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    add_compile_options(
        -O3
        -sUSE_SDL=2
        -sALLOW_MEMORY_GROWTH=1
        -sNO_EXIT_RUNTIME=1
        -sENVIRONMENT=web
        -sEXPORTED_FUNCTIONS=['_main','_getExplorationRate','_getStats']
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']
    )
    
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} 
        -sUSE_SDL=2 
        --shell-file ${CMAKE_SOURCE_DIR}/shell_minimal.html")
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web ${SOURCE_FILES})

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()
