# Stage 1: Build with Emscripten
FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

# Set up build environment
RUN mkdir -p build && \
    cd build && \
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    emmake make -j$(nproc) VERBOSE=1

# Stage 2: Serve with Nginx
FROM nginx:1.23-alpine

# Create directory for MIME types and add WASM support
RUN mkdir -p /etc/nginx/conf.d && \
    echo "types { application/wasm wasm; }" > /etc/nginx/conf.d/wasm.conf && \
    echo "include /etc/nginx/conf.d/wasm.conf;" >> /etc/nginx/nginx.conf

# Copy all necessary files
COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/shell.html /usr/share/nginx/html/index.html

EXPOSE 80
