# Stage 1: Build with Emscripten
FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

# Install dependencies
RUN apt-get update && \
    apt-get install -y cmake ninja-build && \
    rm -rf /var/lib/apt/lists/*

# Build
RUN mkdir -p build && \
    cd build && \
    emcmake cmake .. -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
        -DCMAKE_CXX_FLAGS="-sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1" && \
    cmake --build . --verbose

# Stage 2: Serve with Nginx
FROM nginx:1.25-alpine

COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/index.html

EXPOSE 80
