/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int atomic_t;
typedef int atomic_val_t;
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ARG_UNUSED(x) ((void)(x))
#define DT_NUM_INST_STATUS_OKAY(c) 2
#define DT_INST_FOREACH_STATUS_OKAY(fn) fn(0)
#define DEVICE_DT_INST_GET(n) (&device)
#define DEVICE_DT_INST_DEFINE(n,init,pm,storage,config,level,priority,driver) \
    static const struct device device={.data=storage,.api=driver};
#define INPUT_EV_KEY 1
#define INPUT_EV_REL 2
#define INPUT_EV_ABS 3
#define INPUT_EV_MSC 4
#define INPUT_MSC_SCAN 4
#define INPUT_ABS_X 0
#define INPUT_ABS_Y 1
#define INPUT_REL_X 0
#define INPUT_REL_Y 1
#define INPUT_BTN_TOUCH 330
#define INPUT_BTN_0 256
#define ZMK_INPUT_PROC_CONTINUE 0
#define ZMK_EV_EVENT_BUBBLE 0
struct device { void *data; const void *api; };
struct zmk_input_processor_state { unsigned input_device_index; };
struct input_event { uint8_t type; uint16_t code; int32_t value; bool sync; };
struct zmk_input_processor_driver_api {
 int (*handle_event)(const struct device *,struct input_event *,uint32_t,uint32_t,
                     struct zmk_input_processor_state *);
};
typedef int zmk_event_t;
#define ZMK_LISTENER(name,callback) static int (*name)(const zmk_event_t *) __attribute__((used))=callback
#define ZMK_SUBSCRIPTION(...)
static atomic_t *watched;
static unsigned reads, inject_read;
static int atomic_get(const atomic_t *p) {
 int snapshot=*p;
 if(p==watched && ++reads==inject_read) { ++*watched; }
 return snapshot;
}
static void atomic_set(atomic_t *p,int v) { *p=v; }
static void atomic_inc(atomic_t *p) { ++*p; }
