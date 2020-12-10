# Copyright 2020 Jetperch LLC
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

from libc.stdint cimport uint8_t, uint16_t, uint32_t

cdef extern from "embc/crc.h":

    uint8_t embc_crc_ccitt_8(uint8_t crc, const uint8_t  *data, uint32_t length)
    uint16_t embc_crc_ccitt_16(uint16_t crc, const uint8_t *data, uint32_t length)
    uint32_t embc_crc32(uint32_t crc, const uint8_t *data, uint32_t length)
