FROM ubuntu:20.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    binutils-multiarch                                                  \
    build-essential                                                     \
    g++-multilib                                                        \
    gcc-multilib                                                        \
    libc++-dev                                                          \
    lldb-11                                                             \
    && rm -rf /var/lib/apt/lists/*

CMD ["lldb-server-11", "platform", "--listen", "*:1234", "--server", "--min-gdbserver-port", "31200", "--max-gdbserver-port", "31300"]

EXPOSE 1234
EXPOSE 31200-31300

# Set working directory to /tmp.
# This directory is used to upload the binary executables to.
WORKDIR "/tmp"
