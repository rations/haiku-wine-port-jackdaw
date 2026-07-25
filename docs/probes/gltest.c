/* Minimal Win32 OpenGL smoke test: create a window, get a GL context via the
 * driver, print GL strings. Under the OSMesa backend this should report
 * llvmpipe 4.5 and no "suitable pixel format" failure. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

int main(void)
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "gltest";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("gltest", "gltest",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 240,
        NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) { printf("CreateWindow failed\n"); return 1; }

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pf = ChoosePixelFormat(hdc, &pfd);
    printf("ChoosePixelFormat -> %d\n", pf);
    if (!pf) { printf("no pixel format (GL unavailable)\n"); fflush(stdout); return 2; }
    if (!SetPixelFormat(hdc, pf, &pfd)) { printf("SetPixelFormat failed\n"); return 3; }

    HGLRC rc = wglCreateContext(hdc);
    printf("wglCreateContext -> %p\n", (void *)rc);
    if (!rc) { printf("wglCreateContext failed\n"); fflush(stdout); return 4; }
    if (!wglMakeCurrent(hdc, rc)) { printf("wglMakeCurrent failed\n"); return 5; }

    printf("GL_VERSION  = %s\n", (const char *)glGetString(GL_VERSION));
    printf("GL_RENDERER = %s\n", (const char *)glGetString(GL_RENDERER));
    printf("GL_VENDOR   = %s\n", (const char *)glGetString(GL_VENDOR));

    glClearColor(0.1f, 0.2f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    SwapBuffers(hdc);
    printf("rendered + swapped ok\n");
    fflush(stdout);
    return 0;
}
