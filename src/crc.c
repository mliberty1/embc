/*
 * Copyright 2014-2017 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "embc/crc.h"

#define CRC8_POLYNOMIAL ((uint8_t) 0xE0)
#define CRC16_POLYNOMIAL ((uint16_t) 0x8408)
#define CRC32_POLYNOMIAL ((uint32_t) 0xEDB88320)

uint8_t crc_ccitt_8(uint8_t crc, uint8_t const * data, uint32_t length) {
    uint8_t byte;
    uint8_t mask;
    uint8_t const * data_end;
    int j;

    if ((0 == length) || (0 == data)) {
        return crc;
    }

    data_end = data + length;
    crc = ~crc;
    while (data < data_end) {
        byte = (*data++);
        crc = crc ^ byte;
        for (j = 7; j >= 0; --j) {  // Process each bit in byte
            mask = (uint8_t) -((int8_t) (crc & 1));
            crc = (crc >> 1) ^ (CRC8_POLYNOMIAL & mask);
        }
    }
    crc = ~crc;
    return crc;
}

uint16_t crc_ccitt_16(uint16_t crc, uint8_t const * data, uint32_t length) {
    uint8_t const * data_end = data + length;

    if ((0 == length) || (0 == data)) {
        return crc;
    }

    crc = ~crc;
    while (data < data_end) {
        crc  = (crc >> 8) | (crc << 8);
        crc ^= (*data++);
        crc ^= (crc & 0xff) >> 4;
        crc ^= crc << 12;
        crc ^= (crc & 0xff) << 5;
    }
    crc = ~crc;
    return crc;
}


uint32_t crc32(uint32_t crc, uint8_t const * data, uint32_t length) {
   uint32_t byte;
   uint32_t mask;
   uint8_t const * data_end;
   int j;

   if ((0 == length) || (0 == data)) {
      return crc;
   }

   data_end = data + length;
   crc = ~crc;
   while (data < data_end) {
      byte = (uint32_t) (*data++);
      crc = crc ^ byte;
      for (j = 7; j >= 0; --j) {  // Process each bit in byte
         mask = (uint32_t) -((int32_t) (crc & 1));
         crc = (crc >> 1) ^ (CRC32_POLYNOMIAL & mask);
      }
   }
   crc = ~crc;
   return crc;
}
