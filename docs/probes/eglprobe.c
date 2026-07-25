/* eglprobe.c — can Haiku's EGL give a *desktop-GL* context with no window?
 *
 * Path B for winewayland.drv D3D editors hinges on: (1) a surfaceless EGL
 * display, (2) binding EGL_OPENGL_API (desktop GL — wined3d needs it, not
 * GLES), (3) a high-enough GL version, (4) actually making a context current
 * with no on-screen surface (EGL_KHR_surfaceless_context or a pbuffer).
 *
 * cc eglprobe.c -o eglprobe -lEGL -lGL
 */
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

static const char *S(const GLubyte *s){ return s ? (const char*)s : "(null)"; }

static int has(const char *exts, const char *e){
    if(!exts) return 0;
    return strstr(exts, e) != NULL;
}

static int try_api(EGLDisplay dpy, const char *client_exts, EGLenum api, const char *apiname){
    printf("\n--- bind %s ---\n", apiname);
    if(!eglBindAPI(api)){
        printf("  eglBindAPI(%s) FAILED (0x%x) — not supported\n", apiname, eglGetError());
        return 0;
    }
    printf("  eglBindAPI(%s) ok\n", apiname);

    EGLint cfgattr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, (api==EGL_OPENGL_API)?EGL_OPENGL_BIT:EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg; EGLint ncfg=0;
    if(!eglChooseConfig(dpy, cfgattr, &cfg, 1, &ncfg) || ncfg<1){
        printf("  eglChooseConfig: no pbuffer/renderable config (n=%d err=0x%x)\n", ncfg, eglGetError());
        /* retry with no surface-type restriction */
        EGLint cfgattr2[] = { EGL_RENDERABLE_TYPE,
            (api==EGL_OPENGL_API)?EGL_OPENGL_BIT:EGL_OPENGL_ES2_BIT, EGL_NONE };
        if(!eglChooseConfig(dpy, cfgattr2, &cfg, 1, &ncfg) || ncfg<1){
            printf("  eglChooseConfig(relaxed): still none (n=%d err=0x%x)\n", ncfg, eglGetError());
            return 0;
        }
        printf("  eglChooseConfig(relaxed): got a config (no PBUFFER_BIT)\n");
    } else {
        printf("  eglChooseConfig: got pbuffer-capable config\n");
    }

    EGLint ctxattr_core[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT,
        (api==EGL_OPENGL_API)?ctxattr_core:NULL);
    if(ctx==EGL_NO_CONTEXT && api==EGL_OPENGL_API){
        printf("  eglCreateContext(GL 3.3) failed (0x%x) — retry default version\n", eglGetError());
        ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
    }
    if(ctx==EGL_NO_CONTEXT){
        printf("  eglCreateContext FAILED (0x%x)\n", eglGetError());
        return 0;
    }
    printf("  eglCreateContext ok\n");

    int made=0;
    if(has(client_exts, "EGL_KHR_surfaceless_context")){
        if(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)){
            printf("  eglMakeCurrent(surfaceless) ok\n"); made=1;
        } else printf("  eglMakeCurrent(surfaceless) failed (0x%x)\n", eglGetError());
    }
    if(!made){
        EGLint pb[] = { EGL_WIDTH,16, EGL_HEIGHT,16, EGL_NONE };
        EGLSurface s = eglCreatePbufferSurface(dpy, cfg, pb);
        if(s==EGL_NO_SURFACE) printf("  eglCreatePbufferSurface failed (0x%x)\n", eglGetError());
        else if(eglMakeCurrent(dpy, s, s, ctx)){ printf("  eglMakeCurrent(pbuffer 16x16) ok\n"); made=1; }
        else printf("  eglMakeCurrent(pbuffer) failed (0x%x)\n", eglGetError());
    }
    if(!made){ eglDestroyContext(dpy, ctx); return 0; }

    printf("  GL_VERSION  : %s\n", S(glGetString(GL_VERSION)));
    printf("  GL_RENDERER : %s\n", S(glGetString(GL_RENDERER)));
    printf("  GL_VENDOR   : %s\n", S(glGetString(GL_VENDOR)));
    printf("  GLSL        : %s\n", S(glGetString(GL_SHADING_LANGUAGE_VERSION)));

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    return 1;
}

int main(void){
    const char *client_exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("=== EGL client (no-display) extensions ===\n%s\n", client_exts?client_exts:"(none)");
    printf("  surfaceless platform : %s\n", has(client_exts,"EGL_MESA_platform_surfaceless")?"YES":"no");
    printf("  device platform      : %s\n", has(client_exts,"EGL_EXT_platform_device")?"YES":"no");
    printf("  wayland platform     : %s\n", has(client_exts,"EGL_EXT_platform_wayland")?"YES":"no");

    EGLDisplay dpy = EGL_NO_DISPLAY;
    /* Prefer the extension entrypoint for surfaceless. */
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if(getPlatformDisplay && has(client_exts,"EGL_MESA_platform_surfaceless")){
        dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
        printf("\neglGetPlatformDisplayEXT(SURFACELESS_MESA) -> %p\n", (void*)dpy);
    }
    if(dpy==EGL_NO_DISPLAY){
        dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        printf("fallback eglGetDisplay(DEFAULT) -> %p\n", (void*)dpy);
    }
    if(dpy==EGL_NO_DISPLAY){ printf("no EGL display\n"); return 1; }

    EGLint major=0, minor=0;
    if(!eglInitialize(dpy, &major, &minor)){
        printf("eglInitialize FAILED (0x%x)\n", eglGetError());
        return 1;
    }
    printf("EGL initialized %d.%d\n", major, minor);
    printf("  EGL_VENDOR  : %s\n", eglQueryString(dpy, EGL_VENDOR));
    printf("  EGL_VERSION : %s\n", eglQueryString(dpy, EGL_VERSION));
    printf("  EGL_CLIENT_APIS : %s\n", eglQueryString(dpy, EGL_CLIENT_APIS));
    const char *dpy_exts = eglQueryString(dpy, EGL_EXTENSIONS);
    printf("  display extensions:\n%s\n", dpy_exts?dpy_exts:"(none)");
    printf("  surfaceless_context : %s\n", has(dpy_exts,"EGL_KHR_surfaceless_context")?"YES":"no");

    int gl  = try_api(dpy, dpy_exts, EGL_OPENGL_API, "EGL_OPENGL_API (desktop GL)");
    int gles= try_api(dpy, dpy_exts, EGL_OPENGL_ES_API, "EGL_OPENGL_ES_API");

    printf("\n=== VERDICT ===\n");
    printf("  desktop GL context (wined3d needs this): %s\n", gl?"AVAILABLE":"NOT available");
    printf("  GLES context                           : %s\n", gles?"available":"not available");
    eglTerminate(dpy);
    return 0;
}
