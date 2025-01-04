#!/usr/bin/env sh
echo "In order to build the Tor Plus you should install next packages via Brew: gcc 10, json-c, asciidoc as on 2021.02.01."
echo "It is not possible to do via built-in compiler on macOS because of missing emplace_back method in vector type."
INCLUDE_PATH=/usr/local/opt/json-c/include LIBRARY_PATH=/usr/local/opt/json-c/lib ./configure --disable-asciidoc
INCLUDE_PATH=/usr/local/opt/json-c/include LIBRARY_PATH=/usr/local/opt/json-c/lib make -j $(nproc)
#LIBRARY_PATH=/usr/local/opt/json-c/lib make test -j
make install
echo "Done"

