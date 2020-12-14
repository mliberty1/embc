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

if ((${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") AND ${CMAKE_CROSSCOMPILING})
    SET(EMBC_PLATFORM "ARM")
else()
    SET(EMBC_PLATFORM "STDLIB")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/include/embc/config.h)

set(SOURCES
        collections/hashmap.c
        collections/intmap.c
        collections/list.c
        collections/strmap.c
        cli.c
        crc.c
        cstr.c
        ec.c
        event_manager.c
        fsm.c
        lfsr.c
        log.c
        pattern_32a.c
        platform.c
        memory/block.c
        memory/buffer.c
        memory/object_pool.c
        memory/pool.c
        stream/async.c
        stream/async_sink.c
        stream/async_source.c
        stream/data_link.c
        stream/framer.c
        stream/ring_buffer_u8.c
        ${PLATFORM_SRC}
)

foreach(f IN LISTS SOURCES)
    SET_FILENAME("${f}")
endforeach()

add_library(embc_objlib OBJECT ${SOURCES})

if(${EMBC_TOPLEVEL})
    add_library(embc SHARED $<TARGET_OBJECTS:embc_objlib> lib.c)
else()
    add_library(embc STATIC $<TARGET_OBJECTS:embc_objlib>)
    add_library(embc_lib OBJECT lib.c)
endif()
add_dependencies(embc embc_objlib)
