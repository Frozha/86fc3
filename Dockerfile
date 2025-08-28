# Development environment for C++20 Matching Engine
FROM debian:bookworm

# Install compiler, build tools, debugging tools
RUN apt-get update && apt-get install -y \
    g++-13 \
    cmake \
    ninja-build \
    make \
    git \
    vim \
    gdb \
    valgrind \
    linux-perf \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Boost (
RUN apt-get update && apt-get install -y \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Catch2 for unit tests 
RUN git clone --branch v3.5.0 https://github.com/catchorg/Catch2.git /tmp/Catch2 && \
    cmake -S /tmp/Catch2 -B /tmp/Catch2/build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build /tmp/Catch2/build -j && \
    cmake --install /tmp/Catch2/build && \
    rm -rf /tmp/Catch2

# Set up working directory inside container
WORKDIR /app

# Default command: open shell
CMD ["/bin/bash"]
