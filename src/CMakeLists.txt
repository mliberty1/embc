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

if (EMBC_TOPLEVEL AND CMAKE_HOST_UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__=\"\\\"$(basename $(subst  ${CMAKE_CURRENT_SOURCE_DIR}/,embc/,$(abspath $<)))\\\"\"")
endif()


include_directories(${EMBC_INCLUDE})

set(SOURCES
        collections/bbuf.c
        collections/hashmap.c
        collections/intmap.c
        cli.c
        crc.c
        ec.c
        fsm.c
        lfsr.c
        log.c
        memory/block.c
        memory/object_pool.c
        memory/pool.c
        stream/async.c
        stream/async_sink.c
        stream/async_source.c
        stream/framer.c
        ${PLATFORM_SRC}
)

add_library(embc_objlib OBJECT ${SOURCES})

if(${EMBC_TOPLEVEL})
    add_library(embc SHARED $<TARGET_OBJECTS:embc_objlib> lib.c)
else()
    add_library(embc STATIC $<TARGET_OBJECTS:embc_objlib>)
    add_library(embc_lib OBJECT lib.c)
endif()
add_dependencies(embc embc_objlib)
