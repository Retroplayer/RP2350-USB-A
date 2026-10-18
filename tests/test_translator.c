#include "translator/boot_keyboard.h"
#include "videowriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static unsigned tests;
#define RUN(f) do { f(); ++tests; printf("PASS %s\n",#f); } while(0)
typedef struct { uint8_t data[256]; unsigned count; bool busy; } fake_uart;
typedef struct { vw_keyboard vw; kb_pipeline p; kb_boot_keyboard boot[KB_SOURCES]; fake_uart uart; } fixture;
static bool tx(void *ctx, uint8_t byte) {
    fake_uart *u = ctx;
    if (u->busy) return false;
    CHECK(u->count < sizeof(u->data));
    u->data[u->count++] = byte;
    return true;
}
static void init(fixture *f, vw_enable_mode mode) {
    memset(f,0,sizeof(*f));
    vw_config c = vw_default_config(); c.mode = mode;
    CHECK(vw_init(&f->vw,&c,tx,&f->uart));
    kb_pipeline_init(&f->p,vw_target(&f->vw));
}
static void key(fixture *f, uint8_t source, uint8_t usage, bool down, uint64_t now) {
    kb_event e = {now,source,usage,down}; kb_pipeline_event(&f->p,&e);
}
static void sink(void *ctx, const kb_event *e) { kb_pipeline_event(ctx,e); }
static bool report(fixture *f, uint8_t source, uint8_t mods, uint8_t k, uint64_t now) {
    const uint8_t r[8] = {mods,0,k,0,0,0,0,0};
    return kb_boot_report(&f->boot[source],source,r,8,now,sink,&f->p);
}
static void poll(fixture *f, uint64_t now) {
    vw_enable(&f->vw,false,true,now); kb_pipeline_tick(&f->p,now+10);
}
static void boot_to_target(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    CHECK(report(&f,0,0,4,100));
    CHECK(f.vw.count==1 && f.uart.count==0);
    CHECK(report(&f,0,0,4,101)); CHECK(f.vw.count==1);
    poll(&f,110); CHECK(f.uart.count==1 && f.uart.data[0]=='a');
    CHECK(report(&f,0,0,0,120)); CHECK(!kb_down(&f.p.state,4));
    kb_pipeline_tick(&f.p,1000000); CHECK(f.vw.count==0);
}
static void modifiers_and_caps(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    CHECK(report(&f,0,2,4,0)); poll(&f,10); CHECK(f.uart.data[0]=='A');
    CHECK(report(&f,0,0,0,30)); key(&f,0,0x39,true,40); key(&f,0,0x39,false,41);
    poll(&f,50); CHECK(f.uart.data[1]==0x90);
    CHECK(report(&f,0,2,4,70)); poll(&f,80); CHECK(f.uart.data[2]=='A');
    CHECK(report(&f,0,0,0,100)); key(&f,0,4,true,110); poll(&f,120);
    CHECK(f.uart.data[3]=='A');
}
static void special_mapping(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    CHECK(report(&f,0,2,0x28,0)); poll(&f,10); CHECK(f.uart.data[0]==0xc7);
    CHECK(report(&f,0,0,0x52,30)); poll(&f,40); CHECK(f.uart.data[1]==0xb0);
    CHECK(report(&f,0,0,0,60)); CHECK(report(&f,0,0,0x40,70));
    poll(&f,80); CHECK(f.uart.data[2]==0xb8);
}
static void remap_and_held_change(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    kb_pipeline_map(&f.p,4,5); key(&f,0,4,true,0);
    kb_pipeline_map(&f.p,4,6); key(&f,0,4,false,1);
    CHECK(!kb_down(&f.p.state,5) && !kb_down(&f.p.state,6));
    poll(&f,10); CHECK(f.uart.data[0]=='b');
    key(&f,0,4,true,30); poll(&f,40); CHECK(f.uart.data[1]=='c');
    key(&f,0,4,false,60); kb_pipeline_map(&f.p,4,0); key(&f,0,4,true,70);
    CHECK(f.vw.count==0);
    kb_pipeline_map(&f.p,4,5); key(&f,0,4,true,71); /* still physically held */
    CHECK(f.vw.count==0);
    key(&f,0,4,false,72); key(&f,0,4,true,73);
    CHECK(f.vw.count==1);
}
static void shared_mapping_and_sources(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    kb_pipeline_map(&f.p,5,4);
    key(&f,0,4,true,0); key(&f,0,5,true,1); key(&f,1,4,true,2);
    CHECK(f.p.state.held[4]==3 && f.vw.count==1);
    kb_pipeline_release_source(&f.p,0,3); CHECK(f.p.state.held[4]==1);
    CHECK(f.vw.repeat_key==4);
    kb_pipeline_release_source(&f.p,1,4); CHECK(!kb_down(&f.p.state,4));
    CHECK(!f.vw.repeat_key);
}
static void malformed_and_rollover(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    CHECK(report(&f,0,0,4,0));
    CHECK(!report(&f,0,2,1,1)); CHECK(kb_down(&f.p.state,4)); CHECK(!kb_shift(&f.p.state));
    uint8_t bad[9]={0};
    CHECK(!kb_boot_report(&f.boot[0],0,bad,7,0,sink,&f.p));
    CHECK(!kb_boot_report(&f.boot[0],0,bad,9,0,sink,&f.p));
    CHECK(!kb_boot_report(&f.boot[0],0,NULL,8,0,sink,&f.p));
    CHECK(!report(&f,0,0,0xe1,2));
    CHECK(report(&f,0,0,0,3)); CHECK(!kb_down(&f.p.state,4));
}
static void boot_disconnect(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    CHECK(report(&f,0,2,4,0)); CHECK(report(&f,1,0,4,1));
    kb_boot_disconnect(&f.boot[0],0,2,sink,&f.p);
    CHECK(!kb_shift(&f.p.state)); CHECK(kb_down(&f.p.state,4));
    kb_boot_disconnect(&f.boot[1],1,3,sink,&f.p);
    CHECK(!kb_down(&f.p.state,4)); CHECK(!f.vw.repeat_key);
}
static void request_timing_and_no_credit(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    vw_enable(&f.vw,false,true,0); key(&f,0,4,true,1);
    kb_pipeline_tick(&f.p,10); CHECK(!f.uart.count);
    vw_enable(&f.vw,false,true,20);
    kb_pipeline_tick(&f.p,29); CHECK(!f.uart.count);
    kb_pipeline_tick(&f.p,30); CHECK(f.uart.count==1);
    key(&f,0,5,true,40); kb_pipeline_tick(&f.p,50); CHECK(f.uart.count==1);
    vw_enable(&f.vw,true,true,60); kb_pipeline_tick(&f.p,70);
    CHECK(f.uart.count==2);
    key(&f,0,6,true,80); kb_pipeline_tick(&f.p,90); CHECK(f.uart.count==2);
}
static void busy_and_late_request(void) {
    fixture f; init(&f,VW_REQUEST_EDGE); key(&f,0,4,true,0);
    f.uart.busy=true; vw_enable(&f.vw,false,true,0); kb_pipeline_tick(&f.p,10);
    CHECK(f.vw.count==1 && !f.uart.count);
    f.uart.busy=false; kb_pipeline_tick(&f.p,100); CHECK(f.uart.count==1);
    key(&f,0,5,true,200); vw_enable(&f.vw,false,true,210);
    kb_pipeline_tick(&f.p,711); CHECK(f.uart.count==1 && f.vw.count==1 && f.vw.missed==1);
    poll(&f,800); CHECK(f.uart.count==2 && f.uart.data[1]=='b');
}
static void continuous_and_pulse(void) {
    fixture f; init(&f,VW_ENABLE_LEVEL);
    key(&f,0,4,true,0); key(&f,0,5,true,1);
    vw_enable(&f.vw,true,true,10); kb_pipeline_tick(&f.p,19); CHECK(!f.uart.count);
    kb_pipeline_tick(&f.p,20); kb_pipeline_tick(&f.p,21); CHECK(f.uart.count==2);
    vw_enable(&f.vw,false,false,30); key(&f,0,6,true,31);
    kb_pipeline_tick(&f.p,40); CHECK(f.uart.count==2);
    /* A brief pulse still delivers one byte even if high before service. */
    vw_enable(&f.vw,false,true,50); kb_pipeline_tick(&f.p,60);
    CHECK(f.uart.count==3);
}
static void queue_full_and_wrap(void) {
    fixture f; init(&f,VW_REQUEST_EDGE); f.vw.config.fifo_capacity=2;
    key(&f,0,4,true,0); key(&f,0,5,true,1); key(&f,0,6,true,2);
    CHECK(f.vw.count==2 && f.vw.dropped==1);
    poll(&f,10); key(&f,0,7,true,30); poll(&f,40); poll(&f,60);
    CHECK(f.uart.count==3 && f.uart.data[0]=='a' && f.uart.data[1]=='b' && f.uart.data[2]=='d');
}
static void repeat_and_release(void) {
    fixture f; init(&f,VW_REQUEST_EDGE); key(&f,0,4,true,0);
    kb_pipeline_tick(&f.p,499999); CHECK(f.vw.count==1);
    kb_pipeline_tick(&f.p,500000); CHECK(f.vw.count==2);
    key(&f,0,0xe1,true,500001);
    kb_pipeline_tick(&f.p,600000); CHECK(f.vw.count==3);
    poll(&f,600010); poll(&f,600030); poll(&f,600050);
    CHECK(f.uart.data[0]=='a' && f.uart.data[1]=='a' && f.uart.data[2]=='A');
    key(&f,0,4,false,600070); kb_pipeline_tick(&f.p,2000000); CHECK(f.vw.count==0);
}
static void repeat_no_burst_and_ctrl(void) {
    fixture f; init(&f,VW_REQUEST_EDGE); key(&f,0,4,true,0);
    kb_pipeline_tick(&f.p,10000000); CHECK(f.vw.count==2);
    kb_pipeline_tick(&f.p,10000000); CHECK(f.vw.count==2);
    key(&f,0,0xe0,true,10000001); kb_pipeline_tick(&f.p,10100000);
    CHECK(f.vw.count==2); key(&f,0,5,true,10100001); CHECK(f.vw.count==2);
}
static void reset_retains_remap(void) {
    fixture f; init(&f,VW_REQUEST_EDGE); kb_pipeline_map(&f.p,4,5);
    key(&f,0,4,true,0); vw_enable(&f.vw,true,true,1);
    kb_pipeline_reset(&f.p);
    CHECK(!f.vw.count && !f.vw.pending && !f.vw.repeat_key && !kb_down(&f.p.state,5));
    CHECK(f.p.map[4]==5); kb_pipeline_tick(&f.p,1000000); CHECK(!f.uart.count);
}
static void configuration_and_custom_map(void) {
    fixture f; init(&f,VW_REQUEST_EDGE);
    vw_config c=vw_default_config(); c.fifo_capacity=0; CHECK(!vw_init(&f.vw,&c,tx,&f.uart));
    c=vw_default_config(); c.response_timeout_us=1; CHECK(!vw_init(&f.vw,&c,tx,&f.uart));
    c=vw_default_config(); c.mapping_count=1; c.mappings=NULL; CHECK(!vw_init(&f.vw,&c,tx,&f.uart));
    const vw_mapping m[]={ {0x29,0xac,0xad,false} };
    c=vw_default_config(); c.mappings=m; c.mapping_count=1;
    CHECK(vw_init(&f.vw,&c,tx,&f.uart)); key(&f,0,0x29,true,0); poll(&f,10);
    CHECK(f.uart.data[0]==0xac); kb_pipeline_tick(&f.p,1000000); CHECK(f.vw.count==0);
}
/* A different API consumer proves the core has no VideoWriter dependency. */
static void count_event(void *ctx,const kb_event *e,const kb_state *s) {
    unsigned *n=ctx; CHECK(kb_down(s,e->key)==e->pressed); ++*n;
}
static void independent_target(void) {
    unsigned n=0; static const kb_target_api api={count_event,NULL,NULL};
    kb_pipeline p; kb_pipeline_init(&p,(kb_target){&api,&n});
    kb_event e={0,0,4,true}; kb_pipeline_event(&p,&e);
    e.pressed=false; kb_pipeline_event(&p,&e); CHECK(n==2);
}
int main(void) {
    RUN(boot_to_target); RUN(modifiers_and_caps); RUN(special_mapping);
    RUN(remap_and_held_change); RUN(shared_mapping_and_sources);
    RUN(malformed_and_rollover); RUN(boot_disconnect);
    RUN(request_timing_and_no_credit); RUN(busy_and_late_request); RUN(continuous_and_pulse);
    RUN(queue_full_and_wrap); RUN(repeat_and_release); RUN(repeat_no_burst_and_ctrl);
    RUN(reset_retains_remap); RUN(configuration_and_custom_map); RUN(independent_target);
    printf("%u tests passed\n",tests); return 0;
}
