cmake_minimum_required(VERSION 3.25)

project(geompp LANGUAGES CXX)

add_executable(tests 
    src/audioBuffer.hpp 
    tests/audioBuffer.cpp)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(tests PRIVATE cxx_std_23)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.5.3)

if(catch2_ADDED)	 
    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
endif()