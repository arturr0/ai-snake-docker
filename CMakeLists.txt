cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Emscripten compiler flags
    add_compile_options(
        -O3
        -sUSE_SDL=2
        -sUSE_SDL_IMAGE=2
        -sALLOW_MEMORY_GROWTH=1
        -sNO_EXIT_RUNTIME=1
        -sENVIRONMENT=web
        -sMIN_WEBGL_VERSION=1
        -sMAX_WEBGL_VERSION=1
        -sUSE_GLFW=3
        -sWASM=1
        -sASSERTIONS=1
    )
    
    # Linker flags
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_FUNCTIONS=['_main']")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sFORCE_FILESYSTEM=1")
    
    # Enable Emscripten-specific features
    add_definitions(-D__EMSCRIPTEN__)
else()
    # Native build settings
    find_package(SDL2 REQUIRED)
    add_compile_options(-O3 -Wall -Wextra)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()

# For both builds
target_compile_definitions(aisnake_web PRIVATE 
    WIDTH=20 
    HEIGHT=20 
    CELL_SIZE=20
)

# Installation (for native builds)
if(NOT EMSCRIPTEN)
    install(TARGETS aisnake_web DESTINATION bin)
endif()
