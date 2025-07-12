# Stage 1: Build with Emscripten
FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

# Install essential dependencies
RUN apt-get update && \
    apt-get install -y \
    cmake \
    ninja-build \
    git \
    && rm -rf /var/lib/apt/lists/*

# Build with detailed logging
RUN mkdir -p build && cd build && \
    emcmake cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    ninja -v

# Stage 2: Serve with Nginx
FROM nginx:1.25-alpine

COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/index.html

EXPOSE 80
