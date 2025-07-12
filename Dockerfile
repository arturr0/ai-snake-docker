# Stage 1: Build with Emscripten
FROM trzeci/emscripten:latest as builder

WORKDIR /app
COPY . .

# Set up build environment
RUN . /emsdk_portable/emsdk_env.sh && \
    mkdir build && \
    cd build && \
    emcmake cmake .. -DCMAKE_BUILD_TYPE=Release && \
    emmake make -j4
