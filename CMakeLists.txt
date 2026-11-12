cmake_minimum_required(VERSION 3.10)

project(JOINT_TNTA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ROOT REQUIRED COMPONENTS Tree RIO Hist Graf Gpad)

# The two detector codebases, checked out beside this one by default. Override with
# -DNULIB_DIR=... / -DTACC_DIR=... to build against other checkouts.
set(NULIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../NULIB CACHE PATH "NULIB source folder")
set(TACC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../TAcc CACHE PATH "TAcc source folder")
# Only their libraries are used: tncore (TexNeut) and tacore (TexAT). Each provides its
# own include folder, so headers are included by name; their apps are left out of this build.
add_subdirectory(${NULIB_DIR} ${CMAKE_BINARY_DIR}/NULIB EXCLUDE_FROM_ALL)
add_subdirectory(${TACC_DIR} ${CMAKE_BINARY_DIR}/TAcc EXCLUDE_FROM_ALL)

# run pairing has no ROOT or detector dependencies, so its test builds on its own
add_library(runpairing src/runpairing.cpp)
target_include_directories(runpairing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(joint
    src/beamtiming.cpp
    src/bridges.cpp
    src/jointconfig.cpp
    src/output.cpp
    src/reactions.cpp
)

target_include_directories(joint PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# public: joint's headers use both detectors' types
target_link_libraries(joint PUBLIC
    tncore
    tacore
    runpairing
)

###########################################
add_executable(jointanalysis
    apps/analysis.cc
)
target_link_libraries(jointanalysis PRIVATE joint)

###########################################
# ctest --test-dir build   (checks the real pair lists in info/ too)
enable_testing()
add_executable(runpairing_test
    tests/runpairing.cc
)
target_link_libraries(runpairing_test PRIVATE runpairing)
add_test(NAME runpairing COMMAND runpairing_test ${CMAKE_CURRENT_SOURCE_DIR}/info)
