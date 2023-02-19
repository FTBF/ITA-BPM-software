cmake_minimum_required(VERSION 2.8)
project(ITA_BPM-software)
set(ITA-BPM-software_VERSION_MAJOR 1)
set(ITA-BPM-software_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

include(ExternalProject)
ExternalProject_Add(project_yaml_cpp
  #        GIT_REPOSITORY "https://github.com/jbeder/yaml-cpp"
        GIT_REPOSITORY "${CMAKE_SOURCE_DIR}/../yaml-cpp.git"
        GIT_TAG "yaml-cpp-0.7.0"
        INSTALL_COMMAND ""
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp)
ExternalProject_Get_Property(project_yaml_cpp source_dir)
ExternalProject_Get_Property(project_yaml_cpp binary_dir)

add_library(yaml-cpp SHARED IMPORTED)
add_dependencies(yaml-cpp project_yaml_cpp)
set_property(TARGET yaml-cpp PROPERTY IMPORTED_LOCATION ${binary_dir}/libyaml-cpp.a)


include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${source_dir}/include)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-psabi -fstack-protector -Wno-unused-variable) 

add_library(UIO  ${SOURCE_DIR}/UIO.cpp)
add_library(LTC2333  ${SOURCE_DIR}/LTC2333.cpp)

#target_link_libraries(pthread)

add_executable(test ${SOURCE_DIR}/test.cpp)

#target_link_libraries(test PUBLIC yaml-cpp UIO LTC2333)
target_link_libraries(test PUBLIC UIO LTC2333)

add_executable(server ${SOURCE_DIR}/server.cpp)

#target_link_libraries(test PUBLIC yaml-cpp UIO LTC2333)
target_link_libraries(server PUBLIC UIO LTC2333 zmq pthread yaml-cpp)
