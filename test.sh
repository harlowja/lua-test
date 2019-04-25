#!/bin/bash

set -e

set -x

rm -f test

g++ -std=c++11 -Wall -L/usr/lib/x86_64-linux-gnu/ -I/usr/include/lua5.3 \
               test.cpp -ldl -llua5.3 -lboost_filesystem -lboost_system -o test

./test test.lua
