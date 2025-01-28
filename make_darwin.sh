#!/usr/bin/env sh
echo "In order to build the Tor Plus you should install next packages via Brew: gcc 10, json-c, asciidoc as on 2021.02.01."
echo "It is not possible to do via built-in compiler on macOS because of missing emplace_back method in vector type."
export CC=gcc-10
export CXX=c++-10
export INCLUDE_PATH=/usr/local/opt/json-c/include
export LIBRARY_PATH=/usr/local/opt/json-c/lib
./configure
make
echo "Done"

