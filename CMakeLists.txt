cmake_minimum_required(VERSION 3.10)

project(simplex_example LANGUAGES CXX)

add_executable(example example.cpp)

# Strictly C++14 (no extensions)
set_target_properties(example PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)