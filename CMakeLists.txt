cmake_minimum_required(VERSION 3.10)

project(fuse_demo)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(fuse_demo main.cpp)

target_link_libraries(fuse_demo
        ${FUSE3_LDFLAGS}
        stdc++fs
        )

target_include_directories(fuse_demo PRIVATE ${FUSE3_INCLUDE_DIRS})