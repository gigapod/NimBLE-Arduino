/*
 * NimBLE Porting Layer (NPL) type definitions for Teensy 4.x.
 *
 * Teensyduino ships no RTOS, so unlike the FreeRTOS-based npl used for ESP32
 * and Nordic nRF52 (see ../../../porting/npl/freertos), this is a cooperative,
 * single-threaded implementation: everything (host stack processing, UART
 * pumping, software timers) runs on the sketch's single call stack, driven by
 * repeated calls into nimble_port_teensy_pump() -- see
 * nimble/teensy_port/npl/include/nimble/nimble_port_teensy.h. There is no
 * preemption and no real inter-task contention, so mutexes/semaphores below
 * are intentionally simple: a mutex never actually blocks (there is only ever
 * one logical caller), and a semaphore's "pend" loop just keeps calling the
 * pump function until its count is nonzero or it times out.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _NIMBLE_NPL_OS_H_
#define _NIMBLE_NPL_OS_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

#define BLE_NPL_OS_ALIGNMENT (4)

/* ble_npl_time_t ticks are milliseconds here -- see ble_npl_time_get(). */
#define BLE_NPL_TIME_FOREVER (0xFFFFFFFFu)

typedef uint32_t ble_npl_time_t;
typedef int32_t ble_npl_stime_t;

struct ble_npl_event {
    bool queued;
    ble_npl_event_fn *fn;
    void *arg;
    struct ble_npl_event *next; /* intrusive singly-linked list */
};

struct ble_npl_eventq {
    struct ble_npl_event *head;
    struct ble_npl_event *tail;
};

struct ble_npl_callout {
    struct ble_npl_event ev;
    struct ble_npl_eventq *evq;
    uint32_t expiry_ms;
    bool active;
    struct ble_npl_callout *next; /* intrusive singly-linked "armed" list */
};

struct ble_npl_mutex {
    uint16_t depth; /* always succeeds immediately -- see file header comment */
};

struct ble_npl_sem {
    uint16_t count;
};

/*
 * These four are declared here (rather than in nimble_npl.h, which every
 * platform shares) because upstream only ever defines them inside each
 * platform's own os-specific header -- see npl/freertos's nimble_npl_os.h.
 * Implemented in npl_os_teensy.cpp.
 */
void ble_npl_event_deinit(struct ble_npl_event *ev);
ble_npl_error_t ble_npl_mutex_deinit(struct ble_npl_mutex *mu);
ble_npl_error_t ble_npl_sem_deinit(struct ble_npl_sem *sem);
void ble_npl_callout_deinit(struct ble_npl_callout *co);

#ifdef __cplusplus
}
#endif

#endif /* _NIMBLE_NPL_OS_H_ */
