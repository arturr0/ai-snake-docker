cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Explicitly set source files
file(GLOB SOURCE_FILES "src/*.cpp")

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # These flags go to both compiler and linker
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1")
    
    # Linker-specific flags
    set(CMAKE_EXE_LINKER_FLAGS 
        "${CMAKE_EXE_LINKER_FLAGS} "
        "--shell-file ${CMAKE_SOURCE_DIR}/shell.html "
        "-sEXPORTED_FUNCTIONS=['_main','_getExplorationRate','_getStats'] "
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] "
        "-sMODULARIZE=1 "
        "-sALLOW_MEMORY_GROWTH=1"
    )
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web ${SOURCE_FILES})

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()
