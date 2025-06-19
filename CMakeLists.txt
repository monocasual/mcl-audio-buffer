cmake_minimum_required(VERSION 3.25)

project(geompp LANGUAGES CXX)

add_executable(tests 
    src/audioBuffer.hpp 
    tests/audioBuffer.cpp)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(tests PRIVATE cxx_std_23)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(tests PRIVATE /W4 /bigobj)
else()
	target_compile_options(tests PRIVATE
		-Wall -Wextra -Wpedantic -Wunreachable-code -Wcast-align -Wredundant-decls
        -Woverloaded-virtual -Wreorder)
endif()

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.5.3)

if(catch2_ADDED)	 
    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
endif()