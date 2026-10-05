# syntax=docker/dockerfile:1

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang-20 \
    clang++-20 \
    cmake \
    git \
    libffi-dev \
    libgit2-dev \
    libssl-dev \
    libzstd-dev \
    lld-20 \
    llvm-20 \
    ninja-build \
    pkg-config \
    python3 \
    zsh \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 20 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 20 \
    && chsh -s /bin/zsh root

WORKDIR /workspace
COPY . /workspace

RUN cat <<'EOF' > /root/.zshrc
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
PROMPT="%n@%m:%~ %# "
autoload -Uz compinit && compinit
EOF

# This image only sets up the Ubuntu development environment.
# It does not build the project or run any command by default.
# Use an explicit command when running the container, e.g.:
#   docker run --rm -it verona-bc /bin/bash
