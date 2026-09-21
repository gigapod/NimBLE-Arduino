// NimBLE Porting Layer (NPL) for Teensy 4.x -- see nimble_npl_os.h for the
// type definitions and the cooperative single-threaded design this
// implements. Every ble_npl_* function here is plain C linkage: they satisfy
// the extern prototypes nimble/nimble/include/nimble/nimble_npl.h declares
// after including our nimble_npl_os.h.

#ifdef ARDUINO_TEENSY41

#include <Arduino.h>
#include <string.h>

extern "C" {
#include "nimble/nimble/include/nimble/nimble_npl.h"
#include "nimble/porting/nimble/include/nimble/nimble_port.h"
}

#include "nimble/teensy_port/npl/include/nimble/nimble_port_teensy.h"
#include "nimble/teensy_port/transport/include/nimble/teensy_hci_uart.h"

namespace {

struct ble_npl_callout *s_active_callouts = nullptr;

void callout_list_add(struct ble_npl_callout *co) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    bool present = false;
    for (struct ble_npl_callout *p = s_active_callouts; p != nullptr; p = p->next) {
        if (p == co) {
            present = true;
            break;
        }
    }
    if (!present) {
        co->next = s_active_callouts;
        s_active_callouts = co;
    }
    ble_npl_hw_exit_critical(ctx);
}

void callout_list_remove(struct ble_npl_callout *co) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    struct ble_npl_callout **pp = &s_active_callouts;
    while (*pp != nullptr && *pp != co) {
        pp = &(*pp)->next;
    }
    if (*pp == co) {
        *pp = co->next;
    }
    co->next = nullptr;
    ble_npl_hw_exit_critical(ctx);
}

void run_expired_callouts() {
    uint32_t now = millis();
    for (;;) {
        struct ble_npl_callout *fire = nullptr;

        uint32_t ctx = ble_npl_hw_enter_critical();
        struct ble_npl_callout **pp = &s_active_callouts;
        while (*pp != nullptr) {
            if ((int32_t)(now - (*pp)->expiry_ms) >= 0) {
                fire = *pp;
                *pp = fire->next;
                fire->next = nullptr;
                fire->active = false;
                break;
            }
            pp = &(*pp)->next;
        }
        ble_npl_hw_exit_critical(ctx);

        if (fire == nullptr) {
            break;
        }
        ble_npl_eventq_put(fire->evq, &fire->ev);
    }
}

} // namespace

extern "C" void nimble_port_teensy_pump(void) {
    teensy_hci_uart_poll_rx();
    run_expired_callouts();

    struct ble_npl_eventq *dflt = nimble_port_get_dflt_eventq();
    if (dflt != nullptr) {
        struct ble_npl_event *ev;
        while ((ev = ble_npl_eventq_get(dflt, 0)) != nullptr) {
            ble_npl_event_run(ev);
        }
    }
}

extern "C" {

/* ---- Generic ---- */

bool ble_npl_os_started(void) {
    return true;
}

void *ble_npl_get_current_task_id(void) {
    return (void *)1;
}

/* ---- Event queue ---- */

void ble_npl_eventq_init(struct ble_npl_eventq *evq) {
    evq->head = nullptr;
    evq->tail = nullptr;
}

void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    if (!ev->queued) {
        ev->next = nullptr;
        ev->queued = true;
        if (evq->tail != nullptr) {
            evq->tail->next = ev;
        } else {
            evq->head = ev;
        }
        evq->tail = ev;
    }
    ble_npl_hw_exit_critical(ctx);
}

void ble_npl_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    if (ev->queued) {
        struct ble_npl_event **pp = &evq->head;
        struct ble_npl_event *prev = nullptr;
        while (*pp != nullptr && *pp != ev) {
            prev = *pp;
            pp = &(*pp)->next;
        }
        if (*pp == ev) {
            *pp = ev->next;
            if (evq->tail == ev) {
                evq->tail = prev;
            }
        }
        ev->queued = false;
        ev->next = nullptr;
    }
    ble_npl_hw_exit_critical(ctx);
}

struct ble_npl_event *ble_npl_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo) {
    uint32_t start = millis();
    for (;;) {
        uint32_t ctx = ble_npl_hw_enter_critical();
        struct ble_npl_event *ev = evq->head;
        if (ev != nullptr) {
            evq->head = ev->next;
            if (evq->head == nullptr) {
                evq->tail = nullptr;
            }
            ev->next = nullptr;
            ev->queued = false;
        }
        ble_npl_hw_exit_critical(ctx);

        if (ev != nullptr) {
            return ev;
        }
        if (tmo == 0) {
            return nullptr;
        }

        // Reentrant on purpose: lets a blocking wait made from outside our
        // own top-level pump (e.g. from ble_npl_sem_pend, or a caller
        // pending directly on this queue) keep the host alive. Bounded to
        // one extra level, since the pump's own eventq_get call uses tmo=0.
        nimble_port_teensy_pump();

        if (tmo != BLE_NPL_TIME_FOREVER && (uint32_t)(millis() - start) >= tmo) {
            return nullptr;
        }
    }
}

bool ble_npl_eventq_is_empty(struct ble_npl_eventq *evq) {
    return evq->head == nullptr;
}

void ble_npl_event_run(struct ble_npl_event *ev) {
    ev->fn(ev);
}

void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn, void *arg) {
    memset(ev, 0, sizeof(*ev));
    ev->fn = fn;
    ev->arg = arg;
}

bool ble_npl_event_is_queued(struct ble_npl_event *ev) {
    return ev->queued;
}

void *ble_npl_event_get_arg(struct ble_npl_event *ev) {
    return ev->arg;
}

void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg) {
    ev->arg = arg;
}

void ble_npl_event_deinit(struct ble_npl_event *ev) {
    (void)ev;
}

/* ---- Mutexes ----
 *
 * There is only ever one logical caller in this cooperative model (nested
 * pump calls all run on the same stack), so a mutex can never actually be
 * contended -- it just needs to tolerate being taken recursively, which a
 * plain depth counter does for free. */

ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu) {
    mu->depth = 0;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_deinit(struct ble_npl_mutex *mu) {
    mu->depth = 0;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout) {
    (void)timeout;
    mu->depth++;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu) {
    if (mu->depth == 0) {
        return BLE_NPL_BAD_MUTEX;
    }
    mu->depth--;
    return BLE_NPL_OK;
}

/* ---- Semaphores ---- */

ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
    sem->count = tokens;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_deinit(struct ble_npl_sem *sem) {
    sem->count = 0;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout) {
    uint32_t start = millis();
    for (;;) {
        uint32_t ctx = ble_npl_hw_enter_critical();
        if (sem->count > 0) {
            sem->count--;
            ble_npl_hw_exit_critical(ctx);
            return BLE_NPL_OK;
        }
        ble_npl_hw_exit_critical(ctx);

        if (timeout == 0) {
            return BLE_NPL_TIMEOUT;
        }

        nimble_port_teensy_pump();

        if (timeout != BLE_NPL_TIME_FOREVER && (uint32_t)(millis() - start) >= timeout) {
            return BLE_NPL_TIMEOUT;
        }
    }
}

ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem) {
    uint32_t ctx = ble_npl_hw_enter_critical();
    if (sem->count < UINT16_MAX) {
        sem->count++;
    }
    ble_npl_hw_exit_critical(ctx);
    return BLE_NPL_OK;
}

uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem) {
    return sem->count;
}

/* ---- Callouts (software timers) ---- */

int ble_npl_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                          ble_npl_event_fn *ev_cb, void *ev_arg) {
    memset(co, 0, sizeof(*co));
    ble_npl_event_init(&co->ev, ev_cb, ev_arg);
    co->evq = evq;
    return 0;
}

void ble_npl_callout_deinit(struct ble_npl_callout *co) {
    ble_npl_callout_stop(co);
}

ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks) {
    co->expiry_ms = millis() + ticks;
    co->active = true;
    callout_list_add(co);
    return BLE_NPL_OK;
}

void ble_npl_callout_stop(struct ble_npl_callout *co) {
    co->active = false;
    callout_list_remove(co);
}

bool ble_npl_callout_is_active(struct ble_npl_callout *co) {
    return co->active;
}

ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *co) {
    return co->expiry_ms;
}

ble_npl_time_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t time) {
    int32_t remaining = (int32_t)(co->expiry_ms - time);
    return remaining > 0 ? (ble_npl_time_t)remaining : 0;
}

void ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg) {
    co->ev.arg = arg;
}

/* ---- Time ----
 *
 * ble_npl_time_t ticks are milliseconds (Arduino's millis()), so the
 * ms<->ticks conversions below are trivial. */

ble_npl_time_t ble_npl_time_get(void) {
    return (ble_npl_time_t)millis();
}

ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
    *out_ticks = ms;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms) {
    *out_ms = ticks;
    return BLE_NPL_OK;
}

ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms) {
    return ms;
}

uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks) {
    return ticks;
}

void ble_npl_time_delay(ble_npl_time_t ticks) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ticks) {
        nimble_port_teensy_pump();
    }
}

/* ---- Critical sections ----
 *
 * Teensyduino's core doesn't pull in CMSIS's __get_PRIMASK()/__set_PRIMASK()
 * (it only ever uses the unconditional __disable_irq()/__enable_irq() GCC
 * builtins -- see e.g. DMAChannel.cpp), so read/write PRIMASK directly. */

namespace {

inline uint32_t read_primask(void) {
    uint32_t primask;
    __asm__ volatile("MRS %0, primask" : "=r"(primask));
    return primask;
}

inline void write_primask(uint32_t primask) {
    __asm__ volatile("MSR primask, %0" ::"r"(primask) : "memory");
}

} // namespace

uint32_t ble_npl_hw_enter_critical(void) {
    uint32_t ctx = read_primask();
    __disable_irq();
    return ctx;
}

void ble_npl_hw_exit_critical(uint32_t ctx) {
    write_primask(ctx);
}

bool ble_npl_hw_is_in_critical(void) {
    return (read_primask() & 1u) != 0;
}

} // extern "C"

#endif // ARDUINO_TEENSY41
