/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
void kosync_state_begin(const char* path, uint32_t size, uint16_t chapter, uint32_t offset);
void kosync_state_touch(const char* path, uint32_t size, uint16_t chapter, uint32_t offset);
void kosync_state_accept(const char* path, uint16_t chapter, uint32_t offset, int64_t timestamp);
void kosync_state_commit(const char* path);
int64_t kosync_state_timestamp(const char* path, uint16_t chapter, uint32_t offset);
bool kosync_state_remote_newer(int64_t local, int64_t remote, bool* known);
