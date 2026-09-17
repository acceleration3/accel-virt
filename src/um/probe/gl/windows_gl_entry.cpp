#include "um/probe/gl/windows_gl_entry.hpp"

#include "core/trace/function_scope.hpp"
#include "um/probe/probe_window.hpp"

#include <windows.h>

/* GL 1.1 as opengl32.dll exports it; the SDK subset carries no GL/gl.h. */
extern "C" {
const unsigned char *WINAPI glGetString(unsigned int name);
unsigned int WINAPI         glGetError();
void WINAPI                 glViewport(int x, int y, int width, int height);
void WINAPI                 glClearColor(float red, float green, float blue, float alpha);
void WINAPI                 glClear(unsigned int mask);
void WINAPI                 glDrawArrays(unsigned int mode, int first, int count);
void WINAPI                 glFinish();
void WINAPI glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void *pixels);
void WINAPI glGenTextures(int count, unsigned int *textures);
void WINAPI glBindTexture(unsigned int target, unsigned int texture);
void WINAPI glTexImage2D(unsigned int target, int level, int internal_format, int width, int height, int border,
                         unsigned int format, unsigned int type, const void *pixels);
void WINAPI glGetTexImage(unsigned int target, int level, unsigned int format, unsigned int type, void *pixels);
void WINAPI glDeleteTextures(int count, const unsigned int *textures);
}

namespace av {

namespace {

constexpr unsigned int gl_texture_2d = 0x0DE1u, gl_rgba = 0x1908u, gl_rgba8 = 0x8058u, gl_unsigned_byte = 0x1401u;
constexpr BYTE         colour_bits = 32u, depth_bits = 24u, stencil_bits = 8u;
constexpr DWORD        elsewhere_timeout_ms = 10000u;

/* A wglMakeCurrent from a second thread: what it was asked and answered. */
struct Elsewhere {
    HDC   dc;
    HGLRC context;
    DWORD error;
};

DWORD WINAPI elsewhere_thread(LPVOID argument)
{
    AV_SCOPE("elsewhere_thread");
    auto *const request = static_cast<Elsewhere *>(argument);
    SetLastError(0u);
    const BOOL made = wglMakeCurrent(request->dc, request->context);
    request->error = made ? 0u : GetLastError();
    if (made) static_cast<void>(wglMakeCurrent(nullptr, nullptr));
    return 0u;
}

}  // namespace

WindowsGlEntry::WindowsGlEntry(ProbeWindow &window) : window_(window)
{
    AV_SCOPE("WindowsGlEntry::WindowsGlEntry");
}

template <typename Function>
Function WindowsGlEntry::named(const char *name) const
{
    AV_SCOPE("WindowsGlEntry::named");
    return reinterpret_cast<Function>(wglGetProcAddress(name));
}

uint32_t WindowsGlEntry::width() const
{
    AV_SCOPE("WindowsGlEntry::width");
    return ProbeWindow::side;
}

uint32_t WindowsGlEntry::height() const
{
    AV_SCOPE("WindowsGlEntry::height");
    return ProbeWindow::side;
}

int32_t WindowsGlEntry::choose_pixel_format()
{
    AV_SCOPE("WindowsGlEntry::choose_pixel_format");
    PIXELFORMATDESCRIPTOR wanted = {};
    wanted.nSize = sizeof wanted;
    wanted.nVersion = 1u;
    wanted.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    wanted.iPixelType = PFD_TYPE_RGBA;
    wanted.cColorBits = colour_bits;
    wanted.cDepthBits = depth_bits;
    wanted.cStencilBits = stencil_bits;
    return ChoosePixelFormat(static_cast<HDC>(window_.dc()), &wanted);
}

bool WindowsGlEntry::set_pixel_format(int32_t format)
{
    AV_SCOPE("WindowsGlEntry::set_pixel_format");
    PIXELFORMATDESCRIPTOR described = {};
    static_cast<void>(DescribePixelFormat(static_cast<HDC>(window_.dc()), format, sizeof described, &described));
    SetLastError(0u);
    return SetPixelFormat(static_cast<HDC>(window_.dc()), format, &described) != FALSE;
}

void *WindowsGlEntry::create_context()
{
    AV_SCOPE("WindowsGlEntry::create_context");
    SetLastError(0u);
    return wglCreateContext(static_cast<HDC>(window_.dc()));
}

/* wglCreateContextAttribsARB is an extension function: opengl32 answers it
 * only while some context of the ICD is current, as for any application. */
void *WindowsGlEntry::create_context_attribs(const int32_t *attributes)
{
    AV_SCOPE("WindowsGlEntry::create_context_attribs");
    using Create = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
    const Create create = named<Create>("wglCreateContextAttribsARB");
    if (create == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return nullptr;
    }
    SetLastError(0u);
    return create(static_cast<HDC>(window_.dc()), nullptr, attributes);
}

bool WindowsGlEntry::make_current(void *context)
{
    AV_SCOPE("WindowsGlEntry::make_current");
    SetLastError(0u);
    return wglMakeCurrent(context != nullptr ? static_cast<HDC>(window_.dc()) : nullptr,
                          static_cast<HGLRC>(context)) != FALSE;
}

bool WindowsGlEntry::delete_context(void *context)
{
    AV_SCOPE("WindowsGlEntry::delete_context");
    SetLastError(0u);
    return wglDeleteContext(static_cast<HGLRC>(context)) != FALSE;
}

bool WindowsGlEntry::share_lists(void *first, void *second)
{
    AV_SCOPE("WindowsGlEntry::share_lists");
    SetLastError(0u);
    return wglShareLists(static_cast<HGLRC>(first), static_cast<HGLRC>(second)) != FALSE;
}

bool WindowsGlEntry::swap_buffers()
{
    AV_SCOPE("WindowsGlEntry::swap_buffers");
    SetLastError(0u);
    return SwapBuffers(static_cast<HDC>(window_.dc())) != FALSE;
}

uint32_t WindowsGlEntry::last_error() const
{
    AV_SCOPE("WindowsGlEntry::last_error");
    return GetLastError();
}

uint32_t WindowsGlEntry::make_current_elsewhere(void *context)
{
    AV_SCOPE("WindowsGlEntry::make_current_elsewhere");
    Elsewhere request{static_cast<HDC>(window_.dc()), static_cast<HGLRC>(context), ERROR_TIMEOUT};
    const HANDLE thread = CreateThread(nullptr, 0u, &elsewhere_thread, &request, 0u, nullptr);
    if (thread == nullptr) return GetLastError();
    const DWORD waited = WaitForSingleObject(thread, elsewhere_timeout_ms);
    static_cast<void>(CloseHandle(thread));
    return waited == WAIT_OBJECT_0 ? request.error : ERROR_TIMEOUT;
}

int32_t WindowsGlEntry::force_fault()
{
    AV_SCOPE("WindowsGlEntry::force_fault");
    using Force = int(WINAPI *)();
    const Force hook = named<Force>("rvkForceContextFault");
    return hook != nullptr ? hook() : -1;
}

uint32_t WindowsGlEntry::reset_status()
{
    AV_SCOPE("WindowsGlEntry::reset_status");
    using Query = unsigned int(WINAPI *)();
    const Query query = named<Query>("glGetGraphicsResetStatus");
    return query != nullptr ? query() : 0u;
}

const char *WindowsGlEntry::string(uint32_t name)
{
    AV_SCOPE("WindowsGlEntry::string");
    return reinterpret_cast<const char *>(glGetString(name));
}

uint32_t WindowsGlEntry::error()
{
    AV_SCOPE("WindowsGlEntry::error");
    return glGetError();
}

void WindowsGlEntry::viewport(int32_t x, int32_t y, int32_t width, int32_t height)
{
    AV_SCOPE("WindowsGlEntry::viewport");
    glViewport(x, y, width, height);
}

void WindowsGlEntry::clear_colour(float red, float green, float blue, float alpha)
{
    AV_SCOPE("WindowsGlEntry::clear_colour");
    glClearColor(red, green, blue, alpha);
}

void WindowsGlEntry::clear(uint32_t mask)
{
    AV_SCOPE("WindowsGlEntry::clear");
    glClear(mask);
}

void WindowsGlEntry::draw_arrays(uint32_t mode, int32_t first, int32_t count)
{
    AV_SCOPE("WindowsGlEntry::draw_arrays");
    glDrawArrays(mode, first, count);
}

void WindowsGlEntry::finish()
{
    AV_SCOPE("WindowsGlEntry::finish");
    glFinish();
}

void WindowsGlEntry::read_pixels(int32_t width, int32_t height, uint8_t *rgba)
{
    AV_SCOPE("WindowsGlEntry::read_pixels");
    glReadPixels(0, 0, width, height, gl_rgba, gl_unsigned_byte, rgba);
}

uint32_t WindowsGlEntry::create_shader(uint32_t type)
{
    AV_SCOPE("WindowsGlEntry::create_shader");
    using Create = unsigned int(WINAPI *)(unsigned int);
    const Create create = named<Create>("glCreateShader");
    return create != nullptr ? create(type) : 0u;
}

void WindowsGlEntry::shader_source(uint32_t shader, const char *source)
{
    AV_SCOPE("WindowsGlEntry::shader_source");
    using Source = void(WINAPI *)(unsigned int, int, const char *const *, const int *);
    const Source give = named<Source>("glShaderSource");
    const char *const sources[] = {source};
    if (give != nullptr) give(shader, 1, sources, nullptr);
}

void WindowsGlEntry::compile_shader(uint32_t shader)
{
    AV_SCOPE("WindowsGlEntry::compile_shader");
    using Compile = void(WINAPI *)(unsigned int);
    const Compile compile = named<Compile>("glCompileShader");
    if (compile != nullptr) compile(shader);
}

int32_t WindowsGlEntry::shader_parameter(uint32_t shader, uint32_t pname)
{
    AV_SCOPE("WindowsGlEntry::shader_parameter");
    using Query = void(WINAPI *)(unsigned int, unsigned int, int *);
    const Query query = named<Query>("glGetShaderiv");
    int         value = 0;
    if (query != nullptr) query(shader, pname, &value);
    return value;
}

void WindowsGlEntry::delete_shader(uint32_t shader)
{
    AV_SCOPE("WindowsGlEntry::delete_shader");
    using Delete = void(WINAPI *)(unsigned int);
    const Delete remove = named<Delete>("glDeleteShader");
    if (remove != nullptr) remove(shader);
}

uint32_t WindowsGlEntry::create_program()
{
    AV_SCOPE("WindowsGlEntry::create_program");
    using Create = unsigned int(WINAPI *)();
    const Create create = named<Create>("glCreateProgram");
    return create != nullptr ? create() : 0u;
}

void WindowsGlEntry::attach_shader(uint32_t program, uint32_t shader)
{
    AV_SCOPE("WindowsGlEntry::attach_shader");
    using Attach = void(WINAPI *)(unsigned int, unsigned int);
    const Attach attach = named<Attach>("glAttachShader");
    if (attach != nullptr) attach(program, shader);
}

void WindowsGlEntry::link_program(uint32_t program)
{
    AV_SCOPE("WindowsGlEntry::link_program");
    using Link = void(WINAPI *)(unsigned int);
    const Link link = named<Link>("glLinkProgram");
    if (link != nullptr) link(program);
}

int32_t WindowsGlEntry::program_parameter(uint32_t program, uint32_t pname)
{
    AV_SCOPE("WindowsGlEntry::program_parameter");
    using Query = void(WINAPI *)(unsigned int, unsigned int, int *);
    const Query query = named<Query>("glGetProgramiv");
    int         value = 0;
    if (query != nullptr) query(program, pname, &value);
    return value;
}

void WindowsGlEntry::use_program(uint32_t program)
{
    AV_SCOPE("WindowsGlEntry::use_program");
    using Use = void(WINAPI *)(unsigned int);
    const Use use = named<Use>("glUseProgram");
    if (use != nullptr) use(program);
}

void WindowsGlEntry::delete_program(uint32_t program)
{
    AV_SCOPE("WindowsGlEntry::delete_program");
    using Delete = void(WINAPI *)(unsigned int);
    const Delete remove = named<Delete>("glDeleteProgram");
    if (remove != nullptr) remove(program);
}

uint32_t WindowsGlEntry::gen_vertex_array()
{
    AV_SCOPE("WindowsGlEntry::gen_vertex_array");
    using Gen = void(WINAPI *)(int, unsigned int *);
    const Gen    gen = named<Gen>("glGenVertexArrays");
    unsigned int array = 0u;
    if (gen != nullptr) gen(1, &array);
    return array;
}

void WindowsGlEntry::bind_vertex_array(uint32_t array)
{
    AV_SCOPE("WindowsGlEntry::bind_vertex_array");
    using Bind = void(WINAPI *)(unsigned int);
    const Bind bind = named<Bind>("glBindVertexArray");
    if (bind != nullptr) bind(array);
}

void WindowsGlEntry::delete_vertex_array(uint32_t array)
{
    AV_SCOPE("WindowsGlEntry::delete_vertex_array");
    using Delete = void(WINAPI *)(int, const unsigned int *);
    const Delete remove = named<Delete>("glDeleteVertexArrays");
    if (remove != nullptr) remove(1, &array);
}

uint32_t WindowsGlEntry::gen_texture()
{
    AV_SCOPE("WindowsGlEntry::gen_texture");
    unsigned int texture = 0u;
    glGenTextures(1, &texture);
    return texture;
}

void WindowsGlEntry::bind_texture(uint32_t texture)
{
    AV_SCOPE("WindowsGlEntry::bind_texture");
    glBindTexture(gl_texture_2d, texture);
}

void WindowsGlEntry::texture_image(int32_t width, int32_t height, const uint8_t *rgba)
{
    AV_SCOPE("WindowsGlEntry::texture_image");
    glTexImage2D(gl_texture_2d, 0, static_cast<int>(gl_rgba8), width, height, 0, gl_rgba, gl_unsigned_byte, rgba);
}

void WindowsGlEntry::read_texture(uint8_t *rgba)
{
    AV_SCOPE("WindowsGlEntry::read_texture");
    glGetTexImage(gl_texture_2d, 0, gl_rgba, gl_unsigned_byte, rgba);
}

void WindowsGlEntry::delete_texture(uint32_t texture)
{
    AV_SCOPE("WindowsGlEntry::delete_texture");
    glDeleteTextures(1, &texture);
}

}  // namespace av
