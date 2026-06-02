#!/bin/sh -xe

[ "$ADDITIONAL_PACKAGES" ] || ADDITIONAL_PACKAGES='wine wine64 python3 python3-dev'

# Init package system
sudo -E apt update

echo
echo '--> Install the additional packages'
echo

sudo -E apt install -y --no-install-suggests --no-install-recommends $ADDITIONAL_PACKAGES

# Complete the cleaning

sudo -E apt -qq clean
sudo -E rm -rf /var/lib/apt/lists/* /tmp/*
