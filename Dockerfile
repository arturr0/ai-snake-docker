FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

RUN mkdir -p build && \
    cd build && \
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    emmake make -j$(nproc) VERBOSE=1

FROM nginx:1.23-alpine
COPY --from=builder /app/build/aisnake_web.* /usr/share/nginx/html/
COPY --from=builder /app/shell.html /usr/share/nginx/html/index.html
EXPOSE 80
