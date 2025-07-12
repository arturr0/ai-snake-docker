# Stage 1: Build with Emscripten
FROM trzeci/emscripten:sdk-tag-1.39.4-64bit as builder

WORKDIR /app
COPY . .

# Set up build environment
RUN mkdir -p build && \
    cd build && \
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk_portable/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    emmake make VERBOSE=1

# Stage 2: Serve with Nginx
FROM nginx:1.23-alpine

COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/shell.html /usr/share/nginx/html/index.html

EXPOSE 80
