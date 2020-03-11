#!/usr/bin/env bash
git submodule update --init
g++ parser.cpp utilstrencodings.cpp -I./rapidjson/include -std=c++11 -o parser
