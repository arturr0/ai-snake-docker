# Stage 1: Build with Emscripten
FROM trzeci/emscripten:latest as builder

WORKDIR /app
COPY . .

RUN mkdir build && \
    cd build && \
    emcmake cmake .. && \
    emmake make

# Stage 2: Serve with Nginx
FROM nginx:1.23-alpine

COPY --from=builder /app/build/aisnake_web.html /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.js /usr/share/nginx/html/
COPY --from=builder /app/build/aisnake_web.wasm /usr/share/nginx/html/
COPY --from=builder /app/shell.html /usr/share/nginx/html/index.html

EXPOSE 80