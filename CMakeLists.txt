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

cmake_minimum_required (VERSION 3.1)
set(PARENT_PROJECT_DIR ${PROJECT_SOURCE_DIR})
project(EMBC
        VERSION 0.2.0
        LANGUAGES C)
SET(PROJECT_PREFIX EMBC)
SET(VERSION_STRING "${PROJECT_VERSION}")
SET(VERSION_UNDERSCORE "${PROJECT_VERSION_MAJOR}_${PROJECT_VERSION_MINOR}_${PROJECT_VERSION_PATCH}")

if (NOT PARENT_PROJECT_DIR)
    set(EMBC_TOPLEVEL 1)
else()
    set(EMBC_TOPLEVEL 0)
endif()

option(EMBC_DOCS "Use Doxygen to create the HTML based Host API documentation" OFF)
option(EMBC_UNIT_TEST "Build the embc unit tests" ON)
option(EMBC_EXAMPLES "Build the embc examples" ON)

function (SET_FILENAME _filename)
    get_filename_component(b ${_filename} NAME)
    set_source_files_properties(${_filename} PROPERTIES
        COMPILE_DEFINITIONS "__FILENAME__=\"${b}\"")
endfunction (SET_FILENAME)


if (EMBC_TOPLEVEL AND CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall -Werror -Wpedantic -Wextra -fPIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()
remove_definitions(-D__cplusplus)

add_subdirectory(third-party)

set(EMBC_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
    CACHE INTERNAL "embc source path" FORCE
)
set(EMBC_INCLUDE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${THIRD_PARTY_INCLUDES}
        CACHE INTERNAL "embc include paths" FORCE
        )

set(EMBC_LIBS embc ${THIRD_PARTY_LIBS} CACHE INTERNAL "embc libraries" FORCE)
set(EMBC_DEPENDS embc CACHE INTERNAL "embc dependencies" FORCE)

include_directories(${EMBC_INCLUDE})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/include)
add_subdirectory(src)

if(EMBC_UNIT_TEST AND EMBC_TOPLEVEL AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(test)
endif()

if(EMBC_DOCS AND EMBC_TOPLEVEL)
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
