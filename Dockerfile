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

# Create custom mime.types file with WASM support
RUN echo "types {" > /etc/nginx/mime.types && \
    echo "    application/wasm wasm;" >> /etc/nginx/mime.types && \
    echo "    text/html html;" >> /etc/nginx/mime.types && \
    echo "    application/javascript js;" >> /etc/nginx/mime.types && \
    echo "}" >> /etc/nginx/mime.types

# Copy all necessary files
COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/index.html

EXPOSE 80
