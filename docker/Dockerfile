# Image: stateoftheartio/qt6:6.7-mingw-aqt

FROM ubuntu:24.04

# Just version "2.0.6" for example
ARG AQT_VERSION=3.3.0
ARG AQT_SHA256=4f74d4c95c464d238d7e17ec2d9b7f22a7c333f0f5270a62584e2b47fc765150

ARG QT_VERSION=6.8.0
ARG QT_PATH=C:\\Qt

ARG ADDITIONAL_PACKAGES="git openssh-client curl ca-certificates wine wine64 python3 python3-dev"

ENV DEBIAN_FRONTEND=noninteractive \
    DEBCONF_NONINTERACTIVE_SEEN=true \
    QT_VERSION=${QT_VERSION} \
    AQT_VERSION=${AQT_VERSION} \
    AQT_SHA256=${AQT_SHA256} \
    QT_PATH=${QT_PATH} \
    WINEDEBUG=-all

COPY get_qt.sh install_packages.sh /tmp/

# Install required packages, add group & user + sudo
RUN apt update && apt install -y --no-install-suggests --no-install-recommends locales sudo && \
    sudo apt -qq clean && rm -rf /var/lib/apt/lists/* && \
    groupadd -r user && useradd -m -g user user && \
    echo 'user ALL=NOPASSWD: ALL' > /etc/sudoers.d/user

# Reconfigure locale
RUN locale-gen en_US.UTF-8 && dpkg-reconfigure locales

USER user
WORKDIR /home/user
ENV HOME /home/user

# Get Qt binaries with aqt
RUN /tmp/get_qt.sh

# Install the required packages
RUN /tmp/install_packages.sh
