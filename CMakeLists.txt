# Copyright 2014-2017 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required (VERSION 2.8)
project(EMBC C)
include(version.cmake)

SET(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
SET(VERSION_UNDERSCORE "${VERSION_MAJOR}_${VERSION_MINOR}_${VERSION_PATCH}")

option(EMBC_DOCS "Use Doxygen to create the HTML based Host API documentation" OFF)
option(EMBC_UNIT_TEST "Build the embc unit tests" ON)
option(EMBC_EXAMPLES "Build the embc examples" ON)

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall -Werror -Wpedantic -Wextra -fPIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
remove_definitions(-D__cplusplus)

add_subdirectory(src)

if(EMBC_UNIT_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

if(EMBC_DOCS)
    FIND_PACKAGE(Doxygen)
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR 
                "Doxygen is needed to build the documentation. Please install it")
    endif()
    #-- Configure the Template Doxyfile for our specific project
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in 
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile  @ONLY IMMEDIATE)
    #-- Add a custom target to run Doxygen when ever the project is built
    add_custom_target(docs ALL
                      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                      SOURCES ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                      DEPENDS embc)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/ DESTINATION doc)
endif()
