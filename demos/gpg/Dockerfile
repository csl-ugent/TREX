FROM ubuntu:20.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential                                                     \
    file                                                                \
    git                                                                 \
    wget                                                                \
    && rm -rf /var/lib/apt/lists/*

COPY build.sh /
ENV IS_RUNNING_IN_DOCKER=1
ENTRYPOINT ["/build.sh"]
