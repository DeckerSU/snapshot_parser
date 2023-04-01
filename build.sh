#!/usr/bin/env bash
git submodule update --init
#g++ parser.cpp utilstrencodings.cpp -I./rapidjson/include -std=c++11 -o parser
g++ -g0 -O3 parser.cpp utilstrencodings.cpp -I./rapidjson/include \
-I./depends_build/include -std=c++11 -L./depends_build/lib \
-Wl,-Bstatic -lbitcoin-system -lsecp256k1 -Wl,-Bdynamic \
-lboost_system -lboost_thread -lboost_program_options -lboost_regex \
-lgmp -lpthread -o bruter

