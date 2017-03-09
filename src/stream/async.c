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

#include "embc/stream/async.h"
#include <string.h> // memset

void embc_stream_close(struct embc_stream_consumer_s * self,
                       uint8_t file_id,
                       uint8_t status) {
    struct embc_stream_transaction_s transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.type = EMBC_STREAM_IOCTL_CLOSE;
    transaction.file_id = file_id;
    transaction.status = status;
    self->send(self, &transaction);
}
