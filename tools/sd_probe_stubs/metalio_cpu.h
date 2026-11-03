/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
typedef struct {bool held;int error;}metalio_cpu_guard_t;
static inline metalio_cpu_guard_t metalio_cpu_guard_begin(void){return (metalio_cpu_guard_t){0};}
static inline void metalio_cpu_guard_end(metalio_cpu_guard_t*p){(void)p;}
static inline int metalio_cpu_init(void){return 0;}
static inline int metalio_cpu_wake(void){return 0;}
static inline bool metalio_cpu_idle_begin(bool eligible,unsigned ms){(void)eligible;(void)ms;return false;}
#define METALIO_CPU_GUARD_OR_RETURN(...) ((void)0)
