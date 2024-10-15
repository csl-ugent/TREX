FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential                                                     \
    bzip2                                                               \
    file                                                                \
    g++-multilib                                                        \
    gcc-multilib                                                        \
    git                                                                 \
    nasm                                                                \
    wget                                                                \
    && rm -rf /var/lib/apt/lists/*

COPY build.sh /
ENV IS_RUNNING_IN_DOCKER=1
ENTRYPOINT ["/build.sh"]
