#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <windows.h>
#include <GL/gl.h>

static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

int wmain(int argc, wchar_t **argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int seconds = argc > 1 ? _wtoi(argv[1]) : 5;
    int       width = argc > 2 ? _wtoi(argv[2]) : 640;
    int       height = argc > 3 ? _wtoi(argv[3]) : 360;
    const bool resize = argc > 4 && wcscmp(argv[4], L"resize") == 0;
    bool       resized = false;
    WNDCLASSW type = {};
    type.style = CS_OWNDC;
    type.lpfnWndProc = procedure;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"gl_window";
    RegisterClassW(&type);
    RECT area = {0, 0, width, height};
    AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowW(L"gl_window", L"gl_window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100,
                                area.right - area.left, area.bottom - area.top, nullptr, nullptr, type.hInstance,
                                nullptr);
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR wanted = {};
    wanted.nSize = sizeof wanted;
    wanted.nVersion = 1;
    wanted.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    wanted.iPixelType = PFD_TYPE_RGBA;
    wanted.cColorBits = 32;
    wanted.cDepthBits = 24;
    const int format = ChoosePixelFormat(dc, &wanted);
    if (format == 0 || !SetPixelFormat(dc, format, &wanted)) {
        wprintf(L"no pixel format\n");
        return 1;
    }
    HGLRC context = wglCreateContext(dc);
    if (context == nullptr || !wglMakeCurrent(dc, context)) {
        wprintf(L"no context\n");
        return 2;
    }
    wprintf(L"renderer %hs\n", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    LARGE_INTEGER frequency = {}, start = {}, now = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    int frames = 0;
    int errors = 0;
    for (;;) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        QueryPerformanceCounter(&now);
        const double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (elapsed >= seconds) break;
        if (resize && !resized && elapsed >= 2.0) {
            resized = true;
            RECT grown = {0, 0, width * 4 / 3, height * 4 / 3};
            AdjustWindowRect(&grown, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(window, nullptr, 0, 0, grown.right - grown.left, grown.bottom - grown.top,
                         SWP_NOMOVE | SWP_NOZORDER);
            RECT client = {};
            GetClientRect(window, &client);
            width = client.right;
            height = client.bottom;
            glViewport(0, 0, width, height);
            wprintf(L"resized to %dx%d after %d swaps\n", width, height, frames);
        }
        glDisable(GL_SCISSOR_TEST);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, height / 2, width, height - height / 2);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor((frames * 4) % width, 0, 16, height);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SwapBuffers(dc);
        frames++;
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR && errors++ < 4) wprintf(L"frame %d: GL error 0x%04x\n", frames, error);
    }
    QueryPerformanceCounter(&now);
    const double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / frequency.QuadPart;
    wprintf(L"swaps %d in %.2f s: %.1f a second\n", frames, elapsed, frames / elapsed);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(window, dc);
    DestroyWindow(window);
    return 0;
}
