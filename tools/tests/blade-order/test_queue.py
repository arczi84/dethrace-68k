from pathlib import Path
import subprocess, tempfile
repo=Path(__file__).resolve().parents[3]
work=tempfile.TemporaryDirectory(prefix="dethrace-blade-queue-")
src=(repo/'lib/BRender-v1.3.2/drivers/3dfx_dos/match.c').read_text()
start=src.index('typedef struct fxa_blade_triangle')
end=src.index('\n#endif',start)
queue=src[start:end]
header=r'''
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define BR_ASM_CALL
#define BR_FALSE 0
#define BR_TRUE 1
#define C_Q 0
typedef int br_boolean;
typedef unsigned int br_uint_32;
typedef struct { float comp_f[16]; } brp_vertex;
typedef struct { int unused; } brp_block;
typedef struct br_primitive_state { int id, selected; float depth; int alpha; } br_primitive_state;
typedef void fxa_blade_render_fn(brp_block *,brp_vertex *,brp_vertex *,brp_vertex *);
static int fail_alloc;
static void *test_realloc(void *p, size_t n) { return fail_alloc ? NULL : realloc(p,n); }
#define realloc test_realloc
'''
tail=r'''
static int trace[256], ntrace, pixel, current_colour, current_alpha;
static float depth_buffer, current_depth;
void Set3DfxState(br_primitive_state *s, br_uint_32 flags) {
    blade_current_state=s; blade_current_flags=flags;
    fxa_blade_selected=s->selected;
    current_colour=s->id; current_depth=s->depth; current_alpha=s->alpha;
}
static void draw(brp_block *b, brp_vertex *a, brp_vertex *c, brp_vertex *d) {
    if(fxa_blade_selected && FXA_QueueBlade(draw,b,a,c,d)) return;
    assert(ntrace<256); trace[ntrace++]=current_colour;
    if(current_depth < depth_buffer && current_alpha) {
        pixel=current_colour;
        if(!fxa_blade_selected) depth_buffer=current_depth;
    }
}
int main(void) {
    brp_vertex v={{0.5f}}, farv={{0.1f}};
    brp_block b={0};
    br_primitive_state blade={10,1,2,255}, body={20,0,3,255}, road={30,0,4,255};
    // Original immediate sequence: the body overwrites a nearer blade whose
    // depth writes are off. This is the game path without a primitive heap.
    depth_buffer=100; pixel=0;
    Set3DfxState(&blade,3); fxa_blade_selected=0; current_alpha=255;
    // Explicit old depth-write-off behaviour.
    pixel=blade.id;
    Set3DfxState(&body,7); draw(&b,&v,&v,&v);
    assert(pixel==20);
    // New sequence: the same body and road, deferred blade survives.
    depth_buffer=100; pixel=0; ntrace=0;
    Set3DfxState(&blade,3); draw(&b,&v,&v,&v);
    assert(ntrace==0 && blade_count==1);
    // Input/state snapshots must survive source storage being reused.
    v.comp_f[0]=99; blade.id=99;
    assert(blade_triangles[0].v[0].comp_f[0]==0.5f);
    Set3DfxState(&body,7); draw(&b,&v,&v,&v);
    Set3DfxState(&road,9); draw(&b,&v,&v,&v);
    FXA_FlushBlades();
    assert(pixel==10 && depth_buffer==3 && ntrace==3);
    assert(trace[0]==20 && trace[1]==30 && trace[2]==10);
    assert(blade_current_state==&road && blade_current_flags==9 && !fxa_blade_selected);
    assert(current_colour==30 && blade_count==0 && !blade_replaying);
    FXA_FlushBlades(); assert(ntrace==3);
    // Transparent blade texel must leave the body visible and depth unchanged.
    blade.id=10; blade.alpha=0;
    Set3DfxState(&blade,3); draw(&b,&v,&v,&v);
    Set3DfxState(&body,7); pixel=20; depth_buffer=3;
    FXA_FlushBlades(); assert(pixel==20 && depth_buffer==3);
    // A genuinely nearer surface must occlude the blade.
    blade.alpha=255;
    Set3DfxState(&blade,3); draw(&b,&v,&v,&v);
    Set3DfxState(&body,7); pixel=20; depth_buffer=1;
    FXA_FlushBlades(); assert(pixel==20 && depth_buffer==1);
    // Multiple cutouts render far to near; realloc growth keeps old records.
    FXA_FreeBlades(); ntrace=0; depth_buffer=100;
    for(int i=0;i<70;i++) {
        blade.id=i; v.comp_f[0]=(70-i)*0.01f;
        Set3DfxState(&blade,3); draw(&b,&v,&v,&v);
    }
    assert(blade_count==70 && blade_capacity>=70);
    Set3DfxState(&road,9); FXA_FlushBlades();
    for(int i=0;i<70;i++) assert(trace[i]==69-i);
    size_t cap=blade_capacity; void *mem=blade_triangles;
    Set3DfxState(&blade,3); draw(&b,&farv,&farv,&farv);
    FXA_FlushBlades(); assert(blade_capacity==cap && blade_triangles==mem);
    FXA_FreeBlades(); assert(!blade_triangles && !blade_count && !fxa_blade_selected);
    // OOM uses the original immediate path; no recursive replay or dropped draw.
    fail_alloc=1; ntrace=0;
    Set3DfxState(&blade,3); draw(&b,&v,&v,&v);
    assert(ntrace==1 && blade_count==0);
    FXA_FreeBlades();
    puts("PASS: old order reproduces occlusion; deferred cutout, alpha, near occluder, state restoration, vertex snapshots, ordering, storage reuse, empty flush and OOM");
}
'''
p=(Path(work.name)/'queue-test.c');p.write_text(header+queue+tail)
subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(p.with_suffix(''))],check=True)
subprocess.run([str(p.with_suffix(''))],check=True)
