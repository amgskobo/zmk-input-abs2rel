/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <zmk-input-abs2rel/absolute_to_relative_core.h>
#include "../../drivers/input/input_processor_absolute_to_relative.c"
static void send(struct input_event *event,unsigned source) {
    struct zmk_input_processor_state state={source};
    assert(absolute_to_relative_handle_event(&device,event,0,0,&state)==0);
}
int main(void) {
    struct absolute_to_relative_data *data=device.data;
    assert(absolute_to_relative_init(&device)==0);
    assert(stream_for_event(data,NULL)==&data->streams[0]);
    struct zmk_input_processor_state invalid={2};
    assert(stream_for_event(data,&invalid)==NULL);
    struct input_event event={INPUT_EV_ABS,INPUT_ABS_X,50,true};
    send(&event,2); assert(event.type==INPUT_EV_ABS && event.value==50 && event.sync);
    for(unsigned source=0;source<2;source++) {
        for(unsigned axis=0;axis<2;axis++) {
            int32_t base=(int32_t)(100*source+50);
            event=(struct input_event){INPUT_EV_ABS,(uint16_t)axis,base,axis!=0};
            send(&event,source);
            assert(event.type==INPUT_EV_MSC && event.code==INPUT_MSC_SCAN && event.value==0);
            assert(event.sync==(axis!=0));
            event=(struct input_event){INPUT_EV_ABS,(uint16_t)axis,base+10,true};
            send(&event,source);
            assert(event.type==INPUT_EV_REL && event.code==axis && event.value==5 && event.sync);
        }
    }
    /* All button edges and values are transparent, before any reset checks
     * could turn an edge into a discarded stale coordinate. */
    const uint16_t codes[]={INPUT_BTN_TOUCH,INPUT_BTN_0,INPUT_BTN_0+1,42};
    const int32_t values[]={1,2,INT32_MAX,-1,0};
    for(unsigned source=0;source<2;source++) for(unsigned c=0;c<4;c++) for(unsigned v=0;v<5;v++) {
        event=(struct input_event){INPUT_EV_KEY,codes[c],values[v],v%2!=0};
        watched=&data->reset_generation; reads=0; inject_read=1;
        atomic_val_t before=data->reset_generation;
        send(&event,source); watched=NULL;
        assert(data->reset_generation==before+1);
        assert(event.type==INPUT_EV_KEY && event.code==codes[c] && event.value==values[v]);
        assert(event.sync==(v%2!=0));
    }
    assert(!data->streams[0].x.initialized && !data->streams[1].y.initialized);
    event=(struct input_event){INPUT_EV_ABS,42,7,true}; send(&event,0);
    assert(event.type==INPUT_EV_ABS && event.code==42 && event.value==7 && event.sync);
    assert(absolute_to_relative_layer_listener(NULL)==0);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,900,true};
    send(&event,0); assert(event.type==INPUT_EV_MSC && event.sync);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,920,true};
    send(&event,0); assert(event.type==INPUT_EV_REL && event.value==10);
    watched=&data->reset_generation; reads=0; inject_read=1;
    event=(struct input_event){INPUT_EV_REL,INPUT_REL_X,4,true};
    send(&event,0); watched=NULL;
    assert(event.type==INPUT_EV_MSC && event.sync && event.value==0);
    event=(struct input_event){INPUT_EV_MSC,42,5,true}; send(&event,0);
    assert(event.type==INPUT_EV_MSC && event.code==42 && event.value==5);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,10,true};
    assert(absolute_to_relative_handle_event(&device,&event,0,0,NULL)==0);
    assert(event.type==INPUT_EV_MSC && event.sync);
    /* A touch reset must not discard the other source's established reference. */
    assert(absolute_to_relative_init(&device)==0);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,100,false}; send(&event,0);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,900,true}; send(&event,1);
    event=(struct input_event){INPUT_EV_KEY,INPUT_BTN_TOUCH,2,true}; send(&event,0);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,880,true}; send(&event,1);
    assert(event.type==INPUT_EV_REL && event.value==-10 && event.sync);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,600,true}; send(&event,0);
    assert(event.type==INPUT_EV_MSC && event.value==0 && event.sync);
    /* A reset after the final snapshot is handled on the next input. */
    watched=&data->reset_generation; reads=0; inject_read=2;
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,620,true}; send(&event,0); watched=NULL;
    assert(event.type==INPUT_EV_REL && event.value==10 && event.sync);
    event=(struct input_event){INPUT_EV_ABS,INPUT_ABS_X,800,true}; send(&event,0);
    assert(event.type==INPUT_EV_MSC && event.value==0 && event.sync);
    assert(absolute_to_relative_init(&device)==0);
    assert(data->reset_generation==0 && data->streams[0].applied_generation==0);
    assert(!data->streams[0].x.initialized && !data->streams[1].y.initialized);
    puts("conversion/reference/streams/generation/source-edge transparency: PASS");
    return 0;
}
