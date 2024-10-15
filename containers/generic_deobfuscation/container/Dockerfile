FROM debian:bullseye-slim

ARG cmake_version=3.24.2

ARG wget_user_agent="Mozilla/5.0 (X11; Linux x86_64; rv:104.0) Gecko/20100101 Firefox/104.0"

ARG deobfuscator_archive="generic_deobfuscator.tar.gz"

# If this is changed, also change it in the entrypoint.sh file!
ARG deobfuscator_folder="generic_deobfuscator"


RUN apt-get update \
    && apt-get -y install  \
    build-essential \
    git \
    wget \
    python3 \
    && apt-get clean

# Build recent version of cmake, the one shipped by Debian is too old.
RUN wget -U "$wget_user_agent" https://github.com/Kitware/CMake/releases/download/v$cmake_version/cmake-$cmake_version-Linux-x86_64.tar.gz
#RUN chmod +x cmake-$cmake_version-Linux-x86_64.sh && ./cmake-$cmake_version-Linux-x86_64.sh --skip-license
RUN tar xvf cmake-$cmake_version-Linux-x86_64.tar.gz && ln -s /cmake-$cmake_version-linux-x86_64/bin/cmake /usr/local/bin

COPY $deobfuscator_archive /
RUN tar -xvf /$deobfuscator_archive
#RUN tar -xvf /$deobfuscator_archive \
#    && mv /$deobfuscator_folder-master /$deobfuscator_folder
#    && mv /$deobfuscator_folder /$deobfuscator_folder

# Old make style with udis 1.7 included in src
#RUN cd /$deobfuscator_folder/udis86-1.7/  \
#    && make \
#    && cd /$deobfuscator_folder \
#    && make

# New cmake style with git submodules
RUN cd /$deobfuscator_folder  \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=VerboseRelease .. \
    && make \
    && mv deobf ..

COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
