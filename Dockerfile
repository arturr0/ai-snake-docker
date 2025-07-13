# Stage 1: Build with Emscripten
FROM emscripten/emsdk:3.1.45 as builder

WORKDIR /app
COPY . .

# Install dependencies
RUN apt-get update && \
    apt-get install -y cmake ninja-build && \
    rm -rf /var/lib/apt/lists/*

# Create build directory
RUN mkdir -p build && mkdir -p dist

# Build
WORKDIR /app/build
RUN emcmake cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --verbose

# Stage 2: Serve with Nginx
FROM nginx:1.25-alpine

# Copy built files
COPY --from=builder /app/dist/ /usr/share/nginx/html/

EXPOSE 80
