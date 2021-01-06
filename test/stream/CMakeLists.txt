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

ADD_CMOCKA_TEST(async_test)
ADD_CMOCKA_TEST(async_sink_test)
ADD_CMOCKA_TEST(async_source_test)
ADD_CMOCKA_TEST(data_link_test)
ADD_CMOCKA_TEST(framer_test)
ADD_CMOCKA_TEST(msg_ring_buffer_test)
ADD_CMOCKA_TEST(pubsub_test)
ADD_CMOCKA_TEST(ring_buffer_u64_test)

# transport_test special build to break data_link dependency
SET_FILENAME("transport_test.c")
add_executable(transport_test transport_test.c ../../src/stream/transport.c ../../src/platform.c ../../src/log.c $<TARGET_OBJECTS:test_objlib>)
add_dependencies(${TARGET} test_objlib cmocka)
target_link_libraries(transport_test cmocka)
add_test(transport_test ${CMAKE_CURRENT_BINARY_DIR}/transport_test)


add_executable(stream_tester stream_tester.c $<TARGET_OBJECTS:embc_objlib>)
add_dependencies(stream_tester embc_objlib test_objlib)

include_directories(../../host/include)

SET_FILENAME("host.c")
add_executable(host host.c $<TARGET_OBJECTS:embc_objlib>)
add_dependencies(host embc_objlib test_objlib)
