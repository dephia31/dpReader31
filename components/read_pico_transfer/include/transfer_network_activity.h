/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
/* Reentrant on the owner task. Another job must retry, never preempt active I/O.
 * No task, heap or persisted preference is created by this guard. */
bool read_pico_transfer_network_begin(void);
void read_pico_transfer_network_end(void);
bool read_pico_transfer_network_busy(void);
/// Activity owned by another task, for UI status while this task owns an idle/sleep boundary.
bool read_pico_transfer_network_background_busy(void);
void read_pico_transfer_network_request_stop(void);
typedef struct {bool held;} transfer_network_guard_t;
static inline void transfer_network_guard_end(transfer_network_guard_t *guard) {
    if(guard->held){read_pico_transfer_network_end();guard->held=false;}
}
#define TRANSFER_NETWORK_GUARD_OR_RETURN(result) \
    transfer_network_guard_t network_scope __attribute__((cleanup(transfer_network_guard_end)))={read_pico_transfer_network_begin()}; \
    if(!network_scope.held)return result
