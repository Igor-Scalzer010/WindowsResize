cmake_minimum_required(VERSION 3.21)

project(WindowsResize LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(WindowsResize WIN32
    src/main.cpp
    src/windows_resize_app.cpp
    src/windows_resize_app.hpp
    src/resource.rc
)

target_compile_definitions(WindowsResize PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
