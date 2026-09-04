cmake_minimum_required(VERSION 3.16)
project(amdump CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(amdump
        core/src/Main.cpp
        core/src/ProcessFinder.cpp
        core/src/ProcessMap.cpp
        core/src/Dumper.cpp
)

target_include_directories(amdump PRIVATE core/include)

if(MSVC)
    target_compile_options(amdump PRIVATE
            /W4 /permissive- /WX
    )
    target_compile_definitions(amdump PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_property(TARGET amdump PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_link_options(amdump PRIVATE -static)
    target_compile_options(amdump PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wcast-qual -Wcast-align -Wold-style-cast
            -Woverloaded-virtual -Wnull-dereference
            -Wdouble-promotion -Wformat=2 -Werror
    )
endif()
