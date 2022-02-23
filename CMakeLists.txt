cmake_minimum_required(VERSION 3.10)

project(fuse_demo)

set(CMAKE_CXX_STANDARD 17)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O3 -g -std=c++17")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(PkgConfig REQUIRED)

pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(fuse_demo main.cpp)

target_link_libraries(fuse_demo
        ${FUSE3_LDFLAGS}
        stdc++fs
        )

target_include_directories(fuse_demo PRIVATE ${FUSE3_INCLUDE_DIRS})