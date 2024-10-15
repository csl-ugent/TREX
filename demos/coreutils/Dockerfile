FROM debian:buster-slim

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    gcc                                                                 \
    make                                                                \
    wget                                                                \
    xz-utils                                                            \
    && rm -rf /var/lib/apt/lists/*

COPY build.sh /
ENV IS_RUNNING_IN_DOCKER=1
ENTRYPOINT ["/build.sh"]
