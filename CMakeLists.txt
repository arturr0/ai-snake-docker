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
        -sFORCE_FILESYSTEM=1
    )
    
    # Linker flags
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_FUNCTIONS=['_main','_emscripten_set_main_loop','_emscripten_cancel_main_loop']")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']")
    
    # SDL2 linking
    set(SDL2_LIBRARIES "")
    set(SDL2_INCLUDE_DIRS "")
else()
    find_package(SDL2 REQUIRED)
    add_compile_options(-O3 -Wall -Wextra)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    add_compile_options(
        -O3
        -sUSE_SDL=2
        -sALLOW_MEMORY_GROWTH=1
        -sMAXIMUM_MEMORY=256MB
        -sINITIAL_MEMORY=64MB
        -sNO_EXIT_RUNTIME=1
        -sENVIRONMENT=web
        -sASSERTIONS=1
    )
    
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_FUNCTIONS=['_main','_emscripten_set_main_loop','_emscripten_cancel_main_loop']")
endif()
