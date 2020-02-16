cmake_minimum_required(VERSION 3.2)
project(osiwave)
set(CMAKE_CXX_STANDARD 14)

add_executable(osiwave osiwave.cpp wave.cpp dcfilter.cpp xcross.cpp)
