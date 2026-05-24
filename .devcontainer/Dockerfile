# 使用基础镜像为 Ubuntu 22.04
FROM ubuntu:22.04
COPY sources.list /etc/apt/sources.list

# 安装必要的软件包和工具链
RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    vim \
    python3 \
    gdb bsdmainutils dosfstools libext2fs-dev && \
    rm -rf /var/lib/apt/lists/*

# 默认的工作目录
WORKDIR /workspace