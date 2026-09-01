cmake_minimum_required(VERSION 3.22.1)
project("fireball_core")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    fireball_core
    SHARED
    fireball_jni.cc
)

target_include_directories(
    fireball_core
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(log-lib log)

target_link_libraries(
    fireball_core
    ${log-lib}
)
