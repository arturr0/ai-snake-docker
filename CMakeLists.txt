cmake_minimum_required(VERSION 3.15)
project(AISnake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common compiler flags for all builds
add_compile_options(-O3)

if(EMSCRIPTEN)
    # Emscripten-specific settings
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Combine all linker flags in one clean block
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} \
        -s USE_SDL=2 \
        -s USE_WEBGL2=1 \
        -s ALLOW_MEMORY_GROWTH=1 \
        --shell-file ${CMAKE_SOURCE_DIR}/shell.html \
        -s EXPORTED_FUNCTIONS=['_main','_getExplorationRate','_malloc','_free'] \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString'] \
        -s MODULARIZE=1 \
        -s EXPORT_ES6=1 \
        -s SINGLE_FILE=1")
    
    # Enable WASM and optimize for size
    add_link_options(-s WASM=1 -Oz)
    
    # For better debugging
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g4)
        add_link_options(-s ASSERTIONS=1 -s DEMANGLE_SUPPORT=1)
    endif()
else()
    # Native build settings
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

add_executable(aisnake_web src/main.cpp)

if(NOT EMSCRIPTEN)
    target_link_libraries(aisnake_web ${SDL2_LIBRARIES})
endif()

# Installation (optional)
install(TARGETS aisnake_web
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bin)
