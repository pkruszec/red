set -xe

mkdir -p build
c++ -DDEBUG -ggdb -o build/red red_linux.cpp
