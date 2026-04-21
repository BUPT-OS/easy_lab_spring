FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04

# Install additional tools for memory allocator development
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    gcc \
    g++ \
    gdb \
    make \
    valgrind \
    man-db \
    manpages-dev \
    manpages-posix \
    manpages-posix-dev \
    strace \
    ltrace \
    gawk bison \
    && apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

# Set default shell to bash
ENV SHELL=/bin/bash
