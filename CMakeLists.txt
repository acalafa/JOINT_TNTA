cmake_minimum_required(VERSION 3.10)

project(JOINT_TNTA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ROOT REQUIRED COMPONENTS Tree RIO Hist Graf Gpad)

# NULIB's tncore library; its own apps are left out of this build
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../NULIB ${CMAKE_BINARY_DIR}/NULIB EXCLUDE_FROM_ALL)
# TAcc's tacore library, for run pairing and TexAT file names; its apps are left out too
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../TAcc ${CMAKE_BINARY_DIR}/TAcc EXCLUDE_FROM_ALL)

add_library(joint
    src/bridges.cpp
)

target_include_directories(joint PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(joint PUBLIC
    tncore
)
# private: TAcc's utils.h colour macros clash with NULIB's tformat names
target_link_libraries(joint PRIVATE
    tacore
)

###########################################
add_executable(jointanalysis
    apps/analysis.cc
)
target_link_libraries(jointanalysis PRIVATE joint)
