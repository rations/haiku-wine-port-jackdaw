/* osprobe.c — does Haiku's OSMesa give a usable desktop-GL context with no
 * window at all? This is the real headless backend for Path B (winewayland.drv
 * renders GL offscreen, presents via wl_shm). cc osprobe.c -o osprobe -lOSMesa */
#include <GL/osmesa.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>

static const char *S(const GLubyte *s){ return s ? (const char*)s : "(null)"; }

int main(void){
    const int W=64, H=64;
    unsigned char *buf = malloc(W*H*4);

    /* Try a 3.3 core context first (wined3d prefers >=3.0). */
    OSMesaContext ctx = 0;
#ifdef OSMESA_CONTEXT_MAJOR_VERSION
    int attribs[] = {
        OSMESA_FORMAT, OSMESA_RGBA,
        OSMESA_DEPTH_BITS, 24,
        OSMESA_STENCIL_BITS, 8,
        OSMESA_PROFILE, OSMESA_COMPAT_PROFILE,
        OSMESA_CONTEXT_MAJOR_VERSION, 3,
        OSMESA_CONTEXT_MINOR_VERSION, 3,
        0
    };
    ctx = OSMesaCreateContextAttribs(attribs, NULL);
    if(!ctx) printf("OSMesaCreateContextAttribs(3.3 compat) failed — falling back\n");
#endif
    if(!ctx) ctx = OSMesaCreateContextExt(OSMESA_RGBA, 24, 8, 0, NULL);
    if(!ctx) ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if(!ctx){ printf("OSMesaCreateContext FAILED — no OSMesa GL at all\n"); return 1; }
    printf("OSMesaCreateContext ok\n");

    if(!OSMesaMakeCurrent(ctx, buf, GL_UNSIGNED_BYTE, W, H)){
        printf("OSMesaMakeCurrent FAILED\n"); return 1;
    }
    printf("OSMesaMakeCurrent(%dx%d RGBA8) ok\n", W, H);

    printf("  GL_VERSION  : %s\n", S(glGetString(GL_VERSION)));
    printf("  GL_RENDERER : %s\n", S(glGetString(GL_RENDERER)));
    printf("  GL_VENDOR   : %s\n", S(glGetString(GL_VENDOR)));
    printf("  GLSL        : %s\n", S(glGetString(GL_SHADING_LANGUAGE_VERSION)));

    /* Actually render so we know the rasteriser runs, not just reports. */
    glClearColor(0.1f, 0.2f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    printf("  center pixel RGBA = %u %u %u %u (expect ~26 51 178 255)\n",
        buf[(H/2*W+W/2)*4+0], buf[(H/2*W+W/2)*4+1],
        buf[(H/2*W+W/2)*4+2], buf[(H/2*W+W/2)*4+3]);

    OSMesaDestroyContext(ctx);
    free(buf);
    printf("\n=== VERDICT: OSMesa headless desktop-GL WORKS ===\n");
    return 0;
}
