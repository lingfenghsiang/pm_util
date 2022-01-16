cmake_minimum_required(VERSION 3.0)
project(util_lib)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    # set(CMAKE_BUILD_TYPE Debug)
    # set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

add_definitions(-w)
set(src src/utils.cpp)

add_library(pm_util SHARED ${src})
