FROM debian:stretch-slim

ARG GRAP_VERSION=1.3.1

RUN apt-get update \
    && apt-get -y install build-essential cmake bison flex libboost-regex-dev libboost-system-dev libboost-filesystem-dev libseccomp-dev python3-dev python3-pefile python3-pyelftools python3-capstone swig wget \
    && apt-get clean

RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 10

RUN wget -U "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0" https://github.com/QuoSecGmbH/grap/archive/v$GRAP_VERSION.tar.gz

RUN tar xvf v$GRAP_VERSION.tar.gz \
    && mv grap-$GRAP_VERSION grap

RUN mkdir grap/build \
    && cd grap/build \
    && cmake ../src/ \
    && make \
    && make install

COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
