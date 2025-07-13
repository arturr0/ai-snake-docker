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
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    cmake --build . --verbose && \
    cp aisnake_web.* /app/dist/

# Stage 2: Serve with Nginx
FROM nginx:1.25-alpine

# Create working directory
WORKDIR /usr/share/nginx/html

# Copy built files from builder
COPY --from=builder /app/dist/ .
COPY shell.html .

# Configure nginx
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 80
