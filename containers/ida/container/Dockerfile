FROM debian:buster-slim

ARG IDA_INSTALLER=ida_installer.run
ARG IDA_PASSWORD
RUN test -n "$IDA_PASSWORD"

################################################
# prepare environment and install dependencies #
################################################

RUN apt-get update &&  \
    apt-get install -y \
    binutils           \
    curl               \
    gcc                \
    libbz2-dev         \
    libdbus-1-3        \
    libfontconfig1     \
    libgdbm-dev        \
    libgl1-mesa-glx    \
    libglib2.0-0       \
    liblzma-dev        \
    libncurses5-dev    \
    libpython2.7       \
    libreadline-dev    \
    libsm6             \
    libsqlite3-dev     \
    libssl-dev         \
    libxi6             \
    libxrender1        \
    make               \
    python-pathlib     \
    python3            \
    tk-dev             \
    unzip              \
    wget               \
    xkb-data

# Python 3.4 is required for the idapython plugin
RUN cd /tmp && \
    wget https://www.python.org/ftp/python/3.4.9/Python-3.4.9.tar.xz && \
    tar xf Python-3.4.9.tar.xz && \
    cd Python-3.4.9 && \
    ./configure --enable-shared && \
    make -j16 && \
    make install

#############################
# install IDA 7.5           #
#---------------------------#
# will be installed to /ida #
#############################

COPY /installer/$IDA_INSTALLER /tmp/

RUN cd /tmp && chmod 755 $IDA_INSTALLER && ./$IDA_INSTALLER --mode unattended --prefix /ida --installpassword $IDA_PASSWORD

RUN touch /ida/license.displayed

# Patch running Qt applications in Docker
# RUN mv /ida/idaq /ida/idaq.orig
# COPY 6.8_specific/* /ida/
# RUN chmod 755 /ida/idaq && chmod 755 /ida/ida_entry

# Patch for Python 3.4
RUN mv /ida/ida /ida/ida.orig && mv /ida/ida64 /ida/ida64.orig
COPY 7.5_specific/* /ida/
RUN chmod 755 /ida/ida && chmod 755 /ida/ida64 && chmod 755 /ida/ida_entry && chmod 755 /ida/ida_entry64

COPY ida.reg /root/.idapro/

##############
# entrypoint #
##############

COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
