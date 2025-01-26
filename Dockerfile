# Multi-stage build for optimized performance

# Stage 1: Build stage
FROM ubuntu:24.04

# Set environment variables to prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update the package manager and install required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    libssh2-1-dev \
    zlib1g-dev \
    libpcre3-dev \
    git \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Set up the working directory
WORKDIR /app

# Copy source code and CMakeLists.txt into the container
COPY . /app

# Create a build directory and build the application
RUN mkdir -p /app/build && cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make

# Add corel to the PATH
ENV PATH=/app/build:$PATH

# Set the default command to run the built application
CMD ["tail", "-f", "/dev/null"]

