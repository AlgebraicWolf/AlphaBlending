cmake_minimum_required(VERSION 3.17)
project(AlphaBlending)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O1")


add_executable(AlphaBlending main.cpp)