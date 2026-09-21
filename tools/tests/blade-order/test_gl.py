from pathlib import Path
import struct,subprocess,tempfile,sys
repo=Path(__file__).resolve().parents[3]
work=tempfile.TemporaryDirectory(prefix="dethrace-blade-gl-")
root=Path(work.name)
d=Path(sys.argv[1]).read_bytes()
o=0;name=''
while o+8<=len(d):
 k,n=struct.unpack_from('>II',d,o);b=d[o+8:o+8+n];o+=8+n
 if k==3:name=b[11:].rstrip(b'\0').decode()
 if k==33 and name=='bglspike.pix':
  pix=b[8:];assert len(pix)==4096;break
else:raise AssertionError('Missing blade texture')
rgba=bytes(v for p in pix for v in (255,255,255,0 if p==0 else 255))
(root/'blade.rgba').write_bytes(rgba)
print('Actual BGLSPIKE asset: INDEX8 64x64; transparent pixels',pix.count(0),'/',len(pix),flush=True)
src=(repo/'lib/BRender-v1.3.2/drivers/3dfx_dos/match.c').read_text();start=src.index('typedef struct fxa_blade_triangle');end=src.index('\n#endif',start)
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#define BR_ASM_CALL
#define BR_FALSE 0
#define BR_TRUE 1
#define C_Q 0
typedef int br_boolean;
typedef unsigned br_uint_32;
typedef struct {float comp_f[16];} brp_vertex;
typedef struct {int unused;} brp_block;
typedef struct br_primitive_state {int selected; float r,g,b,z;} br_primitive_state;
typedef void fxa_blade_render_fn(brp_block *,brp_vertex *,brp_vertex *,brp_vertex *);
'''
tail=r'''
static GLuint tex;
static int immediate;
void Set3DfxState(br_primitive_state *s, br_uint_32 flags) {
 blade_current_state=s;blade_current_flags=flags;fxa_blade_selected=s->selected;
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);
 glDepthMask(s->selected ? GL_FALSE : GL_TRUE);
 glColor3f(s->r,s->g,s->b);
 if(s->selected){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,tex);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
 else {glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);}
}
static void draw(brp_block *b,brp_vertex *a,brp_vertex *c,brp_vertex *d){
 if(!immediate && fxa_blade_selected && FXA_QueueBlade(draw,b,a,c,d))return;
 brp_vertex *v[3]={a,c,d};
 glBegin(GL_TRIANGLES);
 for(int i=0;i<3;i++){glTexCoord2f(v[i]->comp_f[4],v[i]->comp_f[5]);glVertex3f(v[i]->comp_f[1],v[i]->comp_f[2],v[i]->comp_f[3]);}
 glEnd();
}
static void quad(br_primitive_state *s,float x0,float y0,float x1,float y1){
 brp_block b={0};float z=s->z;
 brp_vertex v[4]={{{1,x0,y0,z,0,0}},{{1,x1,y0,z,1,0}},{{1,x1,y1,z,1,1}},{{1,x0,y1,z,0,1}}};
 Set3DfxState(s,0);draw(&b,&v[0],&v[1],&v[2]);draw(&b,&v[0],&v[2],&v[3]);
}
static void scene(int old,int occluder,unsigned char *out){
 br_primitive_state blade={1,1,1,1,0.4f},body={0,1,0,0,0.2f},front={0,0,1,0,0.8f};
 immediate=old;glDepthMask(GL_TRUE);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 quad(&blade,0,0,1,1);quad(&body,0,0,1,1);
 if(occluder)quad(&front,0,0,0.5f,1);
 FXA_FlushBlades();glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,out);assert(glGetError()==GL_NO_ERROR);
}
int main(int argc,char **argv){
 PFNEGLGETPLATFORMDISPLAYEXTPROC getdisplay=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");assert(getdisplay);
 EGLDisplay d=getdisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);assert(d!=EGL_NO_DISPLAY);assert(eglInitialize(d,NULL,NULL));assert(eglBindAPI(EGL_OPENGL_API));
 EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
 EGLConfig cfg;EGLint n;assert(eglChooseConfig(d,ca,&cfg,1,&n)&&n);
 EGLint sa[]={EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE};
 EGLSurface s=eglCreatePbufferSurface(d,cfg,sa);EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,NULL);assert(c!=EGL_NO_CONTEXT);assert(eglMakeCurrent(d,s,s,c));
 printf("Host GL renderer: %s\n",glGetString(GL_RENDERER));
 glViewport(0,0,64,64);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 unsigned char texture[16384],old[16384],fixed[16384],blocked[16384];FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(texture,1,sizeof(texture),f)==sizeof(texture));fclose(f);
 glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,texture);
 scene(1,0,old);scene(0,0,fixed);scene(0,1,blocked);
 int visible=0;
 for(int i=0;i<4096;i++){
  assert(old[i*4]==255 && old[i*4+1]==0 && old[i*4+2]==0);
  int opaque=texture[i*4+3]!=0;
  assert(fixed[i*4]==255 && fixed[i*4+1]==(opaque?255:0) && fixed[i*4+2]==(opaque?255:0));
  if(opaque)visible++;
  if(i%64<32)assert(blocked[i*4]==0 && blocked[i*4+1]==255 && blocked[i*4+2]==0);
  else for(int j=0;j<3;j++)assert(blocked[i*4+j]==fixed[i*4+j]);
 }
 printf("PASS: all 4096 pixels match real blade mask; old order hides blade, deferred order preserves %d opaque texels, transparent texels reveal body, nearer object still occludes\n",visible);
 FXA_FreeBlades();glDeleteTextures(1,&tex);eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);
}
'''
p=root/'queue-gl-test.c';p.write_text(prefix+src[start:end]+tail)
subprocess.run(['cc','-O2',str(p),'-o',str(p.with_suffix('')),'-lEGL','-lGL'],check=True)
subprocess.run([str(p.with_suffix('')),str(root/'blade.rgba')],check=True)
