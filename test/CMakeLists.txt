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

#add_definitions(-Wno-format)

SET(cmocka_dir ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/cmocka)
include(CheckCCompilerFlag)
include(${cmocka_dir}/ConfigureChecks.cmake)
configure_file(${cmocka_dir}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${cmocka_dir}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../third-party/uthash/src)
add_library(cmocka STATIC
        ${cmocka_dir}/src/cmocka.c)
target_compile_definitions(cmocka PRIVATE -DHAVE_CONFIG_H=1)

set(TEST_FILES  hal.c)
add_library(test_objlib OBJECT ${TEST_FILES})

set(objects
        $<TARGET_OBJECTS:embc_objlib>
        $<TARGET_OBJECTS:test_objlib>)

set(dependencies
        embc_objlib
        test_objlib
        cmocka)

function (ADD_CMOCKA_TEST _testName _path)
    set(TARGET ${_testName})
    add_executable(${_testName} "${_path}/${_testName}.c" ${objects})
    add_dependencies(${TARGET} embc_objlib test_objlib cmocka)
    target_link_libraries(${_testName} cmocka)
    add_test(${_testName} ${CMAKE_CURRENT_BINARY_DIR}/${_testName})
endfunction (ADD_CMOCKA_TEST)

ADD_CMOCKA_TEST(argchk_test .)
ADD_CMOCKA_TEST(bbuf_test collections)
ADD_CMOCKA_TEST(hashmap_test collections)
ADD_CMOCKA_TEST(intmap_test collections)
ADD_CMOCKA_TEST(cdef_test .)
ADD_CMOCKA_TEST(cli_test .)
ADD_CMOCKA_TEST(crc_test .)
ADD_CMOCKA_TEST(dbc_test .)
ADD_CMOCKA_TEST(ec_test .)
ADD_CMOCKA_TEST(fsm_test .)
ADD_CMOCKA_TEST(lfsr_test .)
ADD_CMOCKA_TEST(pool_test memory)

# test static assert compile failures
# http://stackoverflow.com/questions/30155619/expected-build-failure-tests-in-cmake
add_executable(cdef_static_assert_test cdef_static_assert_test.c)
set_target_properties(cdef_static_assert_test PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE)
add_test(NAME cdef_static_assert_test
        COMMAND ${CMAKE_COMMAND} --build . --target cdef_static_assert_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(cdef_static_assert_test PROPERTIES WILL_FAIL TRUE)
