cmake_minimum_required(VERSION 3.16)
project(amdump CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(amdump
    src/Main.cpp
    src/ProcessMap.cpp
    src/Dumper.cpp
)

target_include_directories(amdump PRIVATE include)
