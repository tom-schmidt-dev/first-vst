cmake_minimum_required(VERSION 3.15)
project(DDSP_DSP_Tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(test_dsp tests/test_dsp.cpp)
