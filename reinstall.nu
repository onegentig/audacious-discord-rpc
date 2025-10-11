#!/usr/bin/env nu
# This is a quick and dirty re-install script for myself
# to test new code change quickly, not intended for public
# use. – onegen

sudo rm -rf build
mkdir build
cd build
cmake ..
sudo make install
cd ..
