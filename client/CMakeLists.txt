cmake_minimum_required(VERSION 2.8)
project(ITA_BPM-software)
set(ITA-BPM-software_VERSION_MAJOR 1)
set(ITA-BPM-software_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

include(ExternalProject)
ExternalProject_Add(project_yaml_cpp
        GIT_REPOSITORY "https://github.com/jbeder/yaml-cpp"
        GIT_TAG "yaml-cpp-0.7.0"
        INSTALL_COMMAND ""
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp)
ExternalProject_Get_Property(project_yaml_cpp source_dir)
ExternalProject_Get_Property(project_yaml_cpp binary_dir)

add_library(yaml-cpp SHARED IMPORTED)
add_dependencies(yaml-cpp project_yaml_cpp)
set_property(TARGET yaml-cpp PROPERTY IMPORTED_LOCATION ${binary_dir}/libyaml-cpp.a)
include_directories(${source_dir}/include)

ExternalProject_Add(project_zmq
        GIT_REPOSITORY "https://github.com/zeromq/libzmq.git"
        GIT_TAG "v4.3.4"
        INSTALL_COMMAND ""
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zmq)
ExternalProject_Get_Property(project_zmq source_dir)
ExternalProject_Get_Property(project_zmq binary_dir)

add_library(zmq SHARED IMPORTED)
add_dependencies(zmq project_zmq)
set_property(TARGET zmq PROPERTY IMPORTED_LOCATION ${binary_dir}/lib/libzmq.a)
include_directories(${source_dir}/include)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-psabi -fstack-protector -Wno-unused-variable) 

add_executable(client ${SOURCE_DIR}/client.cpp)
target_link_libraries(client PUBLIC zmq pthread yaml-cpp)

