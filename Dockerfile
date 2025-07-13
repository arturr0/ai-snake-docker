# Stage 1: Build with Emscripten
FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

# Install dependencies
RUN apt-get update && \
    apt-get install -y cmake ninja-build && \
    rm -rf /var/lib/apt/lists/*

# Verify files
RUN ls -la && test -f shell.html && test -f src/main.cpp

# Build
RUN mkdir -p build && \
    cd build && \
    emcmake cmake .. -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_VERBOSE_MAKEFILE=ON \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    ninja -v

# Stage 2: Serve with Nginx
FROM nginx:1.23-alpine

COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/index.html
COPY --from=builder /app/shell.html /usr/share/nginx/html/

EXPOSE 80
