FROM ubuntu:24.04

ARG SDE_VERSION=sde-external-9.38.0-2024-04-18-lin
ARG LLVM_VERSION=18

RUN dpkg --add-architecture i386 &&                                     \
    apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    binutils-multiarch                                                  \
    build-essential                                                     \
    cmake                                                               \
    elfutils                                                            \
    g++-multilib                                                        \
    gcc-multilib                                                        \
    libc++-dev                                                          \
    libc-dbg                                                            \
    libc-dbg:i386                                                       \
    llvm-$LLVM_VERSION-tools                                            \
    ninja-build                                                         \
    pipx                                                                \
    python3                                                             \
    python3-intervaltree                                                \
    python3-pip                                                         \
    python3-pyelftools                                                  \
    strace                                                              \
    wget                                                                \
    && rm -rf /var/lib/apt/lists/*

RUN pipx install lit

COPY $SDE_VERSION.tar.xz /

RUN tar xvf $SDE_VERSION.tar.xz && mv $SDE_VERSION sde
ENV PATH "$PATH:/sde"
ENV PATH "$PATH:/root/.local/bin"
ENV PATH "/usr/lib/llvm-$LLVM_VERSION/bin:$PATH"

COPY *.sh /
COPY *.py /
RUN chmod +x /*.sh
ENTRYPOINT ["/entrypoint.sh"]
