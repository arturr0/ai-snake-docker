# Stage 1: Build with Emscripten
FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

# Install basic build tools
RUN apt-get update && \
    apt-get install -y cmake ninja-build && \
    rm -rf /var/lib/apt/lists/*

# Build with verbose output
RUN mkdir -p build && \
    cd build && \
    emcmake cmake .. -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    cmake --build . --verbose

# Stage 2: Serve with Nginx
FROM nginx:1.25-alpine

# Proper MIME types for WASM
COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/index.html

# Add proper WASM MIME type
RUN echo "types { application/wasm wasm; }" > /etc/nginx/mime.types && \
    echo "include /etc/nginx/mime.types.default;" >> /etc/nginx/mime.types

EXPOSE 80
