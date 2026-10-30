/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>
static const struct device *converter=DEVICE_DT_GET(DT_NODELABEL(zip_absolute_to_relative));
static const struct device *legacy=DEVICE_DT_GET(DT_NODELABEL(zip_absolute_to_relative_scroll));
static unsigned order;
static void sample(unsigned source,uint8_t type,uint16_t code,int32_t value,bool sync,
                   uint8_t expected_type,uint16_t expected_code,int32_t expected_value) {
 struct input_event event={.type=type,.code=code,.value=value,.sync=sync};
 struct zmk_input_processor_state state={.input_device_index=source};
 __ASSERT_NO_MSG(zmk_input_processor_handle_event(converter,&event,0,0,&state)==ZMK_INPUT_PROC_CONTINUE);
 printk("CONVERTER order=%u source=%u in=%u/%u/%d out=%u/%u/%d sync=%u time=%lld\n",
        ++order,source,type,code,value,event.type,event.code,event.value,event.sync,k_uptime_get());
 __ASSERT_NO_MSG(event.type==expected_type && event.code==expected_code &&
                 event.value==expected_value && event.sync==sync);
}
static void axis(unsigned source,unsigned axis,int32_t value,bool first,int32_t expected) {
 sample(source,INPUT_EV_ABS,axis,value,axis!=0,
        first?INPUT_EV_MSC:INPUT_EV_REL,first?INPUT_MSC_SCAN:axis,expected);
}
static void run(void *a,void *b,void *c) {
 ARG_UNUSED(a);ARG_UNUSED(b);ARG_UNUSED(c);
 __ASSERT_NO_MSG(device_is_ready(converter));
 axis(0,INPUT_ABS_X,100,true,0); axis(1,INPUT_ABS_X,900,true,0);
 axis(0,INPUT_ABS_X,110,false,5); axis(1,INPUT_ABS_X,880,false,-10);
 axis(0,INPUT_ABS_Y,200,true,0); axis(0,INPUT_ABS_Y,190,false,-5);
 sample(0,INPUT_EV_KEY,INPUT_BTN_TOUCH,2,true,INPUT_EV_KEY,INPUT_BTN_TOUCH,2);
 axis(0,INPUT_ABS_X,500,true,0); axis(1,INPUT_ABS_X,860,false,-20);
 __ASSERT_NO_MSG(zmk_keymap_layer_activate(1,false)>=0);
 axis(0,INPUT_ABS_X,800,true,0); axis(1,INPUT_ABS_X,200,true,0);
 axis(0,INPUT_ABS_X,790,false,-5); axis(1,INPUT_ABS_X,220,false,10);
 for(unsigned source=0;source<2;source++) {
  const int values[]={1,2,-1,0};
  for(unsigned i=0;i<ARRAY_SIZE(values);i++)
   sample(source,INPUT_EV_KEY,INPUT_BTN_0,values[i],i%2,INPUT_EV_KEY,INPUT_BTN_0,values[i]);
 }
 /* Opt-in instance must retain independent history for the same source. */
 __ASSERT_NO_MSG(device_is_ready(legacy));
 struct input_event event={.type=INPUT_EV_ABS,.code=INPUT_ABS_X,.value=700,.sync=true};
 struct zmk_input_processor_state state={.input_device_index=0};
 __ASSERT_NO_MSG(zmk_input_processor_handle_event(legacy,&event,0,0,&state)==ZMK_INPUT_PROC_CONTINUE);
 __ASSERT_NO_MSG(event.type==INPUT_EV_MSC && event.sync);
 event=(struct input_event){.type=INPUT_EV_ABS,.code=INPUT_ABS_X,.value=720,.sync=true};
 __ASSERT_NO_MSG(zmk_input_processor_handle_event(legacy,&event,0,0,&state)==ZMK_INPUT_PROC_CONTINUE);
 __ASSERT_NO_MSG(event.type==INPUT_EV_REL && event.value==10 && event.sync);
 axis(0,INPUT_ABS_X,780,false,-10);
 sample(255,INPUT_EV_ABS,INPUT_ABS_X,123,true,INPUT_EV_ABS,INPUT_ABS_X,123);
 sample(0,INPUT_EV_REL,INPUT_REL_Y,-7,true,INPUT_EV_REL,INPUT_REL_Y,-7);
 printk("abs2rel native contract: PASS\n"); exit(0);
}
K_THREAD_DEFINE(abs2rel_native_test,4096,run,NULL,NULL,NULL,K_LOWEST_APPLICATION_THREAD_PRIO,0,100);
