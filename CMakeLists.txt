cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Compiler flags
    add_compile_options(
        -O3
        -sUSE_SDL=2
        -sUSE_WEBGL2=1
        -sFULL_ES3=1
        -sALLOW_MEMORY_GROWTH=1
        -sMAXIMUM_MEMORY=256MB
        -sNO_EXIT_RUNTIME=1
        -sASSERTIONS=1
    )
    
    # Linker flags
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_FUNCTIONS=['_main','_getExplorationRate']")
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sUSE_SDL=2")
else()
    find_package(SDL2 REQUIRED)
    add_compile_options(-O3)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()
