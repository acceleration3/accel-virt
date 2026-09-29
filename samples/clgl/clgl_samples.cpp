// The OpenCL-OpenGL sharing samples (cl_khr_gl_sharing): each shares one kind
// of object between an OpenGL context and an OpenCL context made from it,
// moves values through it one way or both, and checks every one. A sample
// passes only on exact values (or within one step of an 8-bit channel), so a
// shared object that silently became a copy, a stale view or a different
// object fails rather than passes.
//
// Guest only: Windows, no C runtime, OpenCL through the Khronos loader beside
// the program (OpenCL.dll) and OpenGL through opengl32 and the adapter's ICD.
// Prints one line per sample - "ok <name>" or "FAIL <name>: <why>" - and
// exits with the number that failed.
#include <windows.h>

#define CL_TARGET_OPENCL_VERSION 300
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#include <CL/cl_gl.h>

extern "C" {
[[gnu::used]] int _fltused = 0;
int __cdecl _purecall() { ExitProcess(3); }
#pragma function(memset, memcpy, memcmp)
void *__cdecl memset(void *to, int value, size_t count)
{
    unsigned char *p = static_cast<unsigned char *>(to);
    while (count--) *p++ = static_cast<unsigned char>(value);
    return to;
}
void *__cdecl memcpy(void *to, const void *from, size_t count)
{
    unsigned char       *d = static_cast<unsigned char *>(to);
    const unsigned char *s = static_cast<const unsigned char *>(from);
    while (count--) *d++ = *s++;
    return to;
}
int __cdecl memcmp(const void *a, const void *b, size_t count)
{
    const unsigned char *x = static_cast<const unsigned char *>(a), *y = static_cast<const unsigned char *>(b);
    for (; count--; x++, y++)
        if (*x != *y) return *x < *y ? -1 : 1;
    return 0;
}
}

namespace {

// ---- output, with no C runtime to format it

void write_text(const char *text)
{
    DWORD length = 0;
    while (text[length] != '\0') length++;
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, length, &wrote, nullptr);
}

void write_signed(long long value)
{
    char  digits[24];
    int   at = 23;
    bool  negative = value < 0;
    unsigned long long left = negative ? 0ull - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    digits[at] = '\0';
    do {
        digits[--at] = static_cast<char>('0' + left % 10ull);
        left /= 10ull;
    } while (left != 0ull && at > 1);
    if (negative) digits[--at] = '-';
    write_text(&digits[at]);
}

bool contains(const char *text, const char *word)
{
    for (const char *start = text; *start != '\0'; start++) {
        const char *a = start;
        const char *b = word;
        while (*b != '\0' && *a == *b) {
            a++;
            b++;
        }
        if (*b == '\0' && (*a == ' ' || *a == '\0') && (start == text || start[-1] == ' ')) return true;
    }
    return false;
}

void *allocate(size_t bytes) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes != 0 ? bytes : 1); }
void  release(void *memory)
{
    if (memory != nullptr) HeapFree(GetProcessHeap(), 0, memory);
}

// ---- OpenGL, resolved at run time

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef float         GLfloat;
typedef unsigned int  GLbitfield;
typedef long long     GLsizeiptr;
typedef long long     GLintptr;
typedef unsigned char GLubyte;

constexpr GLenum gl_array_buffer = 0x8892u;
constexpr GLenum gl_dynamic_draw = 0x88E8u;
constexpr GLenum gl_texture_2d = 0x0DE1u;
constexpr GLenum gl_rgba = 0x1908u;
constexpr GLenum gl_rgba8 = 0x8058u;
constexpr GLenum gl_rgba32f = 0x8814u;
constexpr GLenum gl_unsigned_byte = 0x1401u;
constexpr GLenum gl_float = 0x1406u;
constexpr GLenum gl_texture_min_filter = 0x2801u;
constexpr GLenum gl_texture_mag_filter = 0x2800u;
constexpr GLint  gl_nearest = 0x2600;
constexpr GLenum gl_framebuffer = 0x8D40u;
constexpr GLenum gl_renderbuffer = 0x8D41u;
constexpr GLenum gl_color_attachment0 = 0x8CE0u;
constexpr GLenum gl_framebuffer_complete = 0x8CD5u;
constexpr GLbitfield gl_color_buffer_bit = 0x4000u;
constexpr GLenum gl_pack_alignment = 0x0D05u;

struct Gl {
#define CLGL_GL_ENTRIES(X)                                                                                          \
    X(GenBuffers, void, GLsizei, GLuint *)                                                                          \
    X(BindBuffer, void, GLenum, GLuint)                                                                             \
    X(BufferData, void, GLenum, GLsizeiptr, const void *, GLenum)                                                   \
    X(BufferSubData, void, GLenum, GLintptr, GLsizeiptr, const void *)                                              \
    X(GetBufferSubData, void, GLenum, GLintptr, GLsizeiptr, void *)                                                 \
    X(DeleteBuffers, void, GLsizei, const GLuint *)                                                                 \
    X(GenTextures, void, GLsizei, GLuint *)                                                                         \
    X(BindTexture, void, GLenum, GLuint)                                                                            \
    X(TexImage2D, void, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)                \
    X(TexParameteri, void, GLenum, GLenum, GLint)                                                                   \
    X(GetTexImage, void, GLenum, GLint, GLenum, GLenum, void *)                                                     \
    X(DeleteTextures, void, GLsizei, const GLuint *)                                                                \
    X(GenFramebuffers, void, GLsizei, GLuint *)                                                                     \
    X(BindFramebuffer, void, GLenum, GLuint)                                                                        \
    X(FramebufferTexture2D, void, GLenum, GLenum, GLenum, GLuint, GLint)                                            \
    X(FramebufferRenderbuffer, void, GLenum, GLenum, GLenum, GLuint)                                                \
    X(CheckFramebufferStatus, GLenum, GLenum)                                                                       \
    X(DeleteFramebuffers, void, GLsizei, const GLuint *)                                                            \
    X(GenRenderbuffers, void, GLsizei, GLuint *)                                                                    \
    X(BindRenderbuffer, void, GLenum, GLuint)                                                                       \
    X(RenderbufferStorage, void, GLenum, GLenum, GLsizei, GLsizei)                                                  \
    X(DeleteRenderbuffers, void, GLsizei, const GLuint *)                                                           \
    X(ClearColor, void, GLfloat, GLfloat, GLfloat, GLfloat)                                                         \
    X(Clear, void, GLbitfield)                                                                                      \
    X(Viewport, void, GLint, GLint, GLsizei, GLsizei)                                                               \
    X(Finish, void)                                                                                                 \
    X(PixelStorei, void, GLenum, GLint)                                                                             \
    X(GetError, GLenum)
#define CLGL_GL_MEMBER(name, result, ...) result(WINAPI *name)(__VA_ARGS__);
    CLGL_GL_ENTRIES(CLGL_GL_MEMBER)
#undef CLGL_GL_MEMBER

    bool load()
    {
        HMODULE const opengl32 = GetModuleHandleW(L"opengl32.dll");
        bool          all = true;
#define CLGL_GL_LOAD(name, ...)                                                                                      \
    {                                                                                                                \
        PROC found = wglGetProcAddress("gl" #name);                                                                  \
        const long long value = reinterpret_cast<long long>(found);                                                  \
        if (value >= -1 && value <= 3) found = opengl32 != nullptr ? GetProcAddress(opengl32, "gl" #name) : nullptr; \
        name = reinterpret_cast<decltype(name)>(found);                                                              \
        if (name == nullptr) {                                                                                       \
            write_text("missing gl" #name "\n");                                                                     \
            all = false;                                                                                             \
        }                                                                                                            \
    }
        CLGL_GL_ENTRIES(CLGL_GL_LOAD)
#undef CLGL_GL_LOAD
        return all;
    }
};

// ---- OpenCL, from the loader beside the program

struct Cl {
#define CLGL_CL_ENTRIES(X)                                                                                          \
    X(GetPlatformIDs) X(GetPlatformInfo) X(GetDeviceIDs) X(GetDeviceInfo) X(CreateContext) X(ReleaseContext)        \
    X(CreateCommandQueueWithProperties) X(ReleaseCommandQueue) X(CreateProgramWithSource) X(BuildProgram)           \
    X(GetProgramBuildInfo) X(ReleaseProgram) X(CreateKernel) X(ReleaseKernel) X(SetKernelArg)                       \
    X(EnqueueNDRangeKernel) X(Finish) X(ReleaseMemObject) X(CreateBuffer) X(EnqueueReadBuffer) X(EnqueueReadImage)  \
    X(CreateFromGLBuffer) X(CreateFromGLTexture) X(CreateFromGLRenderbuffer) X(EnqueueAcquireGLObjects)             \
    X(EnqueueReleaseGLObjects) X(GetGLObjectInfo) X(GetGLTextureInfo) X(GetExtensionFunctionAddressForPlatform)     \
    X(CreateCommandQueue) X(EnqueueCopyBuffer) X(GetEventProfilingInfo) X(ReleaseEvent)
#define CLGL_CL_MEMBER(name) decltype(&::cl##name) name;
    CLGL_CL_ENTRIES(CLGL_CL_MEMBER)
#undef CLGL_CL_MEMBER
    clGetGLContextInfoKHR_fn GetGLContextInfo = nullptr;

    bool load()
    {
        HMODULE const loader = LoadLibraryA("OpenCL.dll");
        if (loader == nullptr) return false;
        bool all = true;
#define CLGL_CL_LOAD(name)                                                                                           \
    name = reinterpret_cast<decltype(name)>(GetProcAddress(loader, "cl" #name));                                     \
    if (name == nullptr) {                                                                                           \
        write_text("missing cl" #name "\n");                                                                         \
        all = false;                                                                                                 \
    }
        CLGL_CL_ENTRIES(CLGL_CL_LOAD)
#undef CLGL_CL_LOAD
        return all;
    }
};

const char kernels[] = R"(
__kernel void fill(__global float *out) { size_t i = get_global_id(0); out[i] = (float)i * 0.5f + 3.0f; }
__kernel void copy(__global const float *in, __global float *out) { size_t i = get_global_id(0); out[i] = in[i]; }
__kernel void increment(__global float *data) { size_t i = get_global_id(0); data[i] += 1.0f; }
__kernel void gradient(__write_only image2d_t image)
{
    int x = get_global_id(0), y = get_global_id(1);
    write_imagef(image, (int2)(x, y), (float4)(x / 63.0f, y / 63.0f, 0.25f, 1.0f));
}
__kernel void exact(__write_only image2d_t image)
{
    int x = get_global_id(0), y = get_global_id(1);
    write_imagef(image, (int2)(x, y), (float4)(x * 0.5f, y * 0.25f, 1.5f, -2.0f));
}
)";

constexpr int buffer_floats = 4096;
constexpr int side = 64;
constexpr int frames = 100;
// Photoshop's GPU sniffer: on a profiling queue of the context shared with its
// GL context, 64 copies of 16 MiB, each with an event; a copy reads and writes
// its bytes, and OpenCL is "available" only at 12 GB/s or more.
constexpr size_t copy_bytes = 16u << 20;
constexpr int    copies = 64;
constexpr double least_gigabytes_per_second = 12.0;

class Suite {
public:
    // A hidden window with an OpenGL context current on it, and an OpenCL
    // context made from that GL context. False, having said why, when any of
    // it cannot be had: nothing after it would mean anything.
    bool open()
    {
        if (!make_window() || !gl_.load()) return fail_setup("no OpenGL context");
        if (!cl_.load()) return fail_setup("no OpenCL loader or entry points");
        if (!find_device()) return fail_setup("no OpenCL device reports cl_khr_gl_sharing for this GL context");
        const cl_context_properties properties[] = {CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(gl_context_),
                                                    CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(dc_),
                                                    CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platform_), 0};
        cl_int error = CL_SUCCESS;
        context_ = cl_.CreateContext(properties, 1u, &device_, nullptr, nullptr, &error);
        if (context_ == nullptr) return fail_setup("clCreateContext from the GL context", error);
        queue_ = cl_.CreateCommandQueueWithProperties(context_, device_, nullptr, &error);
        if (queue_ == nullptr) return fail_setup("clCreateCommandQueueWithProperties", error);
        program_ = cl_.CreateProgramWithSource(context_, 1u, &kernel_text_, nullptr, &error);
        if (program_ == nullptr) return fail_setup("clCreateProgramWithSource", error);
        error = cl_.BuildProgram(program_, 1u, &device_, nullptr, nullptr, nullptr);
        if (error == CL_SUCCESS) return true;
        char log[4096] = {};
        cl_.GetProgramBuildInfo(program_, device_, CL_PROGRAM_BUILD_LOG, sizeof log - 1u, log, nullptr);
        write_text(log);
        return fail_setup("building the kernels", error);
    }

    int run()
    {
        sample("gl_sharing_reported", &Suite::gl_sharing_reported);
        sample("context_info_names_the_device", &Suite::context_info_names_the_device);
        sample("a_foreign_gl_context_is_refused", &Suite::a_foreign_gl_context_is_refused);
        sample("kernel_writes_a_gl_buffer", &Suite::kernel_writes_a_gl_buffer);
        sample("gl_writes_a_buffer_the_kernel_reads", &Suite::gl_writes_a_buffer_the_kernel_reads);
        sample("a_shared_buffer_every_frame", &Suite::a_shared_buffer_every_frame);
        sample("kernel_writes_a_gl_texture", &Suite::kernel_writes_a_gl_texture);
        sample("a_float_texture_round_trips_exactly", &Suite::a_float_texture_round_trips_exactly);
        sample("gl_renders_a_texture_the_kernel_reads", &Suite::gl_renders_a_texture_the_kernel_reads);
        sample("kernel_reads_a_renderbuffer", &Suite::kernel_reads_a_renderbuffer);
        sample("objects_say_what_they_share", &Suite::objects_say_what_they_share);
        sample("copies_are_timed_as_photoshop_times_them", &Suite::copies_are_timed_as_photoshop_times_them);
        write_signed(passed_);
        write_text(" passed, ");
        write_signed(failed_);
        write_text(" failed\n");
        return failed_;
    }

private:
    using Body = bool (Suite::*)();

    void sample(const char *name, Body body)
    {
        why_ = "";
        code_ = CL_SUCCESS;
        has_code_ = false;
        const bool ok = (this->*body)();
        if (ok) {
            passed_++;
            write_text("ok ");
            write_text(name);
            write_text("\n");
            return;
        }
        failed_++;
        write_text("FAIL ");
        write_text(name);
        write_text(": ");
        write_text(why_);
        if (has_code_) {
            write_text(" (");
            write_signed(code_);
            write_text(")");
        }
        write_text("\n");
    }

    bool no(const char *why)
    {
        why_ = why;
        return false;
    }
    bool no(const char *why, cl_int code)
    {
        why_ = why;
        code_ = code;
        has_code_ = true;
        return false;
    }
    bool fail_setup(const char *why, cl_int code = CL_SUCCESS)
    {
        write_text("FAIL setup: ");
        write_text(why);
        if (code != CL_SUCCESS) {
            write_text(" (");
            write_signed(code);
            write_text(")");
        }
        write_text("\n");
        return false;
    }

    bool make_window()
    {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW description = {};
        description.cbSize = sizeof description;
        description.style = CS_OWNDC;
        description.lpfnWndProc = &DefWindowProcW;
        description.hInstance = instance;
        description.lpszClassName = L"rvkclglsamples";
        if (RegisterClassExW(&description) == 0) return false;
        const HWND window = CreateWindowExW(0u, L"rvkclglsamples", L"rvkclglsamples", WS_POPUP, 0, 0, 64, 64, nullptr,
                                            nullptr, instance, nullptr);
        dc_ = window != nullptr ? GetDC(window) : nullptr;
        if (dc_ == nullptr) return false;
        PIXELFORMATDESCRIPTOR wanted = {};
        wanted.nSize = sizeof wanted;
        wanted.nVersion = 1u;
        wanted.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        wanted.iPixelType = PFD_TYPE_RGBA;
        wanted.cColorBits = 32u;
        const int format = ChoosePixelFormat(dc_, &wanted);
        PIXELFORMATDESCRIPTOR described = {};
        if (format == 0 || DescribePixelFormat(dc_, format, sizeof described, &described) == 0 ||
            !SetPixelFormat(dc_, format, &described))
            return false;
        gl_context_ = wglCreateContext(dc_);
        return gl_context_ != nullptr && wglMakeCurrent(dc_, gl_context_);
    }

    // The platform and device clGetGLContextInfoKHR names for the current GL
    // context, which is how an application is to find them.
    bool find_device()
    {
        cl_platform_id platforms[8];
        cl_uint        count = 0;
        if (cl_.GetPlatformIDs(8u, platforms, &count) != CL_SUCCESS) return false;
        for (cl_uint index = 0; index < count && index < 8u; index++) {
            auto info = reinterpret_cast<clGetGLContextInfoKHR_fn>(
                cl_.GetExtensionFunctionAddressForPlatform(platforms[index], "clGetGLContextInfoKHR"));
            if (info == nullptr) continue;
            const cl_context_properties properties[] = {
                CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(gl_context_), CL_WGL_HDC_KHR,
                reinterpret_cast<cl_context_properties>(dc_), CL_CONTEXT_PLATFORM,
                reinterpret_cast<cl_context_properties>(platforms[index]), 0};
            cl_device_id device = nullptr;
            if (info(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, nullptr) != CL_SUCCESS ||
                device == nullptr)
                continue;
            platform_ = platforms[index];
            device_ = device;
            cl_.GetGLContextInfo = info;
            return true;
        }
        return false;
    }

    cl_kernel kernel(const char *name)
    {
        cl_int error = CL_SUCCESS;
        return cl_.CreateKernel(program_, name, &error);
    }

    bool run_kernel(cl_kernel k, cl_mem object, size_t width, size_t height)
    {
        const size_t global[2] = {width, height};
        return cl_.SetKernelArg(k, 0u, sizeof object, &object) == CL_SUCCESS &&
               cl_.EnqueueNDRangeKernel(queue_, k, height > 1u ? 2u : 1u, nullptr, global, nullptr, 0u, nullptr,
                                        nullptr) == CL_SUCCESS;
    }

    GLuint gl_buffer(const float *initial)
    {
        GLuint name = 0;
        gl_.GenBuffers(1, &name);
        gl_.BindBuffer(gl_array_buffer, name);
        gl_.BufferData(gl_array_buffer, buffer_floats * static_cast<GLsizeiptr>(sizeof(float)), initial, gl_dynamic_draw);
        gl_.BindBuffer(gl_array_buffer, 0);
        return name;
    }

    GLuint gl_texture(GLenum internal, GLenum type)
    {
        GLuint name = 0;
        gl_.GenTextures(1, &name);
        gl_.BindTexture(gl_texture_2d, name);
        gl_.TexParameteri(gl_texture_2d, gl_texture_min_filter, gl_nearest);
        gl_.TexParameteri(gl_texture_2d, gl_texture_mag_filter, gl_nearest);
        gl_.TexImage2D(gl_texture_2d, 0, static_cast<GLint>(internal), side, side, 0, gl_rgba, type, nullptr);
        gl_.BindTexture(gl_texture_2d, 0);
        return name;
    }

    // ---- the samples

    bool gl_sharing_reported()
    {
        char extensions[4096] = {};
        const cl_int asked = cl_.GetDeviceInfo(device_, CL_DEVICE_EXTENSIONS, sizeof extensions - 1u, extensions, nullptr);
        if (asked != CL_SUCCESS) return no("CL_DEVICE_EXTENSIONS", asked);
        return contains(extensions, "cl_khr_gl_sharing") ? true : no("cl_khr_gl_sharing is not in CL_DEVICE_EXTENSIONS");
    }

    bool context_info_names_the_device()
    {
        const cl_context_properties properties[] = {
            CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(gl_context_), CL_WGL_HDC_KHR,
            reinterpret_cast<cl_context_properties>(dc_), CL_CONTEXT_PLATFORM,
            reinterpret_cast<cl_context_properties>(platform_), 0};
        cl_device_id devices[4] = {};
        size_t       bytes = 0;
        const cl_int asked = cl_.GetGLContextInfo(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof devices, devices, &bytes);
        if (asked != CL_SUCCESS) return no("CL_DEVICES_FOR_GL_CONTEXT_KHR", asked);
        if (bytes != sizeof(cl_device_id) || devices[0] != device_) return no("the devices are not the one current device");
        return true;
    }

    bool a_foreign_gl_context_is_refused()
    {
        const cl_context_properties properties[] = {
            CL_GL_CONTEXT_KHR, static_cast<cl_context_properties>(0x51E5), CL_WGL_HDC_KHR,
            reinterpret_cast<cl_context_properties>(dc_), CL_CONTEXT_PLATFORM,
            reinterpret_cast<cl_context_properties>(platform_), 0};
        cl_int           error = CL_SUCCESS;
        const cl_context made = cl_.CreateContext(properties, 1u, &device_, nullptr, nullptr, &error);
        if (made != nullptr) {
            cl_.ReleaseContext(made);
            return no("a context was made from a GL context nobody has");
        }
        return error == CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR ? true : no("the refusal is not the share-group one", error);
    }

    bool kernel_writes_a_gl_buffer()
    {
        float *const zero = static_cast<float *>(allocate(buffer_floats * sizeof(float)));
        const GLuint name = gl_buffer(zero);
        gl_.Finish();
        cl_int       error = CL_SUCCESS;
        const cl_mem shared = cl_.CreateFromGLBuffer(context_, CL_MEM_WRITE_ONLY, name, &error);
        const cl_kernel fill = kernel("fill");
        bool ok = shared != nullptr && fill != nullptr;
        if (!ok) no("clCreateFromGLBuffer or the kernel", error);
        ok = ok && cl_.EnqueueAcquireGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
             run_kernel(fill, shared, buffer_floats, 1u) &&
             cl_.EnqueueReleaseGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
             cl_.Finish(queue_) == CL_SUCCESS;
        if (ok) {
            gl_.BindBuffer(gl_array_buffer, name);
            gl_.GetBufferSubData(gl_array_buffer, 0, buffer_floats * static_cast<GLsizeiptr>(sizeof(float)), zero);
            gl_.BindBuffer(gl_array_buffer, 0);
            for (int i = 0; i < buffer_floats && ok; i++)
                if (zero[i] != static_cast<float>(i) * 0.5f + 3.0f) ok = no("GL read back a value the kernel did not write");
        } else if (why_[0] == '\0') {
            no("acquire, the kernel or release failed");
        }
        if (fill != nullptr) cl_.ReleaseKernel(fill);
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteBuffers(1, &name);
        release(zero);
        return ok;
    }

    bool gl_writes_a_buffer_the_kernel_reads()
    {
        float *const values = static_cast<float *>(allocate(buffer_floats * sizeof(float)));
        for (int i = 0; i < buffer_floats; i++) values[i] = static_cast<float>(i) * 7.0f - 11.0f;
        const GLuint name = gl_buffer(nullptr);
        gl_.BindBuffer(gl_array_buffer, name);
        gl_.BufferSubData(gl_array_buffer, 0, buffer_floats * static_cast<GLsizeiptr>(sizeof(float)), values);
        gl_.BindBuffer(gl_array_buffer, 0);
        gl_.Finish();
        cl_int       error = CL_SUCCESS;
        const cl_mem shared = cl_.CreateFromGLBuffer(context_, CL_MEM_READ_ONLY, name, &error);
        const cl_mem plain = cl_.CreateBuffer(context_, CL_MEM_READ_WRITE, buffer_floats * sizeof(float), nullptr, &error);
        const cl_kernel copy = kernel("copy");
        float *const got = static_cast<float *>(allocate(buffer_floats * sizeof(float)));
        const size_t global = buffer_floats;
        bool ok = shared != nullptr && plain != nullptr && copy != nullptr &&
                  cl_.EnqueueAcquireGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
                  cl_.SetKernelArg(copy, 0u, sizeof shared, &shared) == CL_SUCCESS &&
                  cl_.SetKernelArg(copy, 1u, sizeof plain, &plain) == CL_SUCCESS &&
                  cl_.EnqueueNDRangeKernel(queue_, copy, 1u, nullptr, &global, nullptr, 0u, nullptr, nullptr) == CL_SUCCESS &&
                  cl_.EnqueueReleaseGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
                  cl_.EnqueueReadBuffer(queue_, plain, CL_TRUE, 0u, buffer_floats * sizeof(float), got, 0u, nullptr,
                                        nullptr) == CL_SUCCESS;
        if (!ok) no("the shared buffer could not be copied out through a kernel", error);
        for (int i = 0; i < buffer_floats && ok; i++)
            if (got[i] != values[i]) ok = no("the kernel read a value GL did not write");
        if (copy != nullptr) cl_.ReleaseKernel(copy);
        if (plain != nullptr) cl_.ReleaseMemObject(plain);
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteBuffers(1, &name);
        release(got);
        release(values);
        return ok;
    }

    // What an application does every frame: the kernel works on the shared
    // buffer, then GL uses it, and the next frame starts from what GL left.
    bool a_shared_buffer_every_frame()
    {
        float *const values = static_cast<float *>(allocate(buffer_floats * sizeof(float)));
        const GLuint name = gl_buffer(values);
        gl_.Finish();
        cl_int          error = CL_SUCCESS;
        const cl_mem    shared = cl_.CreateFromGLBuffer(context_, CL_MEM_READ_WRITE, name, &error);
        const cl_kernel increment = kernel("increment");
        bool ok = shared != nullptr && increment != nullptr;
        if (!ok) no("clCreateFromGLBuffer or the kernel", error);
        for (int frame = 1; frame <= frames && ok; frame++) {
            ok = cl_.EnqueueAcquireGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
                 run_kernel(increment, shared, buffer_floats, 1u) &&
                 cl_.EnqueueReleaseGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
                 cl_.Finish(queue_) == CL_SUCCESS;
            if (!ok) {
                no("a frame's acquire, kernel or release failed");
                break;
            }
            if (frame % 10 != 0) continue;
            gl_.BindBuffer(gl_array_buffer, name);
            gl_.GetBufferSubData(gl_array_buffer, 0, buffer_floats * static_cast<GLsizeiptr>(sizeof(float)), values);
            gl_.BindBuffer(gl_array_buffer, 0);
            gl_.Finish();
            for (int i = 0; i < buffer_floats && ok; i++)
                if (values[i] != static_cast<float>(frame)) ok = no("GL saw a frame count the kernels did not reach");
        }
        if (increment != nullptr) cl_.ReleaseKernel(increment);
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteBuffers(1, &name);
        release(values);
        return ok;
    }

    bool kernel_writes_a_gl_texture()
    {
        const GLuint name = gl_texture(gl_rgba8, gl_unsigned_byte);
        gl_.Finish();
        cl_int          error = CL_SUCCESS;
        const cl_mem    shared = cl_.CreateFromGLTexture(context_, CL_MEM_WRITE_ONLY, gl_texture_2d, 0, name, &error);
        const cl_kernel gradient = kernel("gradient");
        bool ok = shared != nullptr && gradient != nullptr;
        if (!ok) no("clCreateFromGLTexture or the kernel", error);
        ok = ok && cl_.EnqueueAcquireGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
             run_kernel(gradient, shared, side, side) &&
             cl_.EnqueueReleaseGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
             cl_.Finish(queue_) == CL_SUCCESS;
        unsigned char *const pixels = static_cast<unsigned char *>(allocate(side * side * 4));
        if (ok) {
            gl_.PixelStorei(gl_pack_alignment, 1);
            gl_.BindTexture(gl_texture_2d, name);
            gl_.GetTexImage(gl_texture_2d, 0, gl_rgba, gl_unsigned_byte, pixels);
            gl_.BindTexture(gl_texture_2d, 0);
            for (int y = 0; y < side && ok; y++)
                for (int x = 0; x < side && ok; x++) {
                    const unsigned char *p = pixels + (y * side + x) * 4;
                    const int want[4] = {(x * 255 + 31) / 63, (y * 255 + 31) / 63, 64, 255};
                    for (int c = 0; c < 4 && ok; c++) {
                        const int diff = static_cast<int>(p[c]) - want[c];
                        if (diff < -1 || diff > 1) ok = no("a texel GL read is not what the kernel wrote");
                    }
                }
        } else if (why_[0] == '\0') {
            no("acquire, the kernel or release failed");
        }
        if (gradient != nullptr) cl_.ReleaseKernel(gradient);
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteTextures(1, &name);
        release(pixels);
        return ok;
    }

    bool a_float_texture_round_trips_exactly()
    {
        const GLuint name = gl_texture(gl_rgba32f, gl_float);
        gl_.Finish();
        cl_int          error = CL_SUCCESS;
        const cl_mem    shared = cl_.CreateFromGLTexture(context_, CL_MEM_WRITE_ONLY, gl_texture_2d, 0, name, &error);
        const cl_kernel exact = kernel("exact");
        bool ok = shared != nullptr && exact != nullptr;
        if (!ok) no("clCreateFromGLTexture of an RGBA32F texture or the kernel", error);
        ok = ok && cl_.EnqueueAcquireGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
             run_kernel(exact, shared, side, side) &&
             cl_.EnqueueReleaseGLObjects(queue_, 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS &&
             cl_.Finish(queue_) == CL_SUCCESS;
        float *const texels = static_cast<float *>(allocate(side * side * 4 * sizeof(float)));
        if (ok) {
            gl_.BindTexture(gl_texture_2d, name);
            gl_.GetTexImage(gl_texture_2d, 0, gl_rgba, gl_float, texels);
            gl_.BindTexture(gl_texture_2d, 0);
            for (int y = 0; y < side && ok; y++)
                for (int x = 0; x < side && ok; x++) {
                    const float *t = texels + (y * side + x) * 4;
                    if (t[0] != x * 0.5f || t[1] != y * 0.25f || t[2] != 1.5f || t[3] != -2.0f)
                        ok = no("a float texel is not exactly what the kernel wrote");
                }
        } else if (why_[0] == '\0') {
            no("acquire, the kernel or release failed");
        }
        if (exact != nullptr) cl_.ReleaseKernel(exact);
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteTextures(1, &name);
        release(texels);
        return ok;
    }

    // Reads `image` back through OpenCL and compares every pixel with `want`.
    bool pixels_are(cl_mem image, const unsigned char want[4], const char *why)
    {
        unsigned char *const pixels = static_cast<unsigned char *>(allocate(side * side * 4));
        const size_t origin[3] = {0, 0, 0};
        const size_t region[3] = {side, side, 1};
        bool ok = cl_.EnqueueAcquireGLObjects(queue_, 1u, &image, 0u, nullptr, nullptr) == CL_SUCCESS &&
                  cl_.EnqueueReadImage(queue_, image, CL_TRUE, origin, region, 0u, 0u, pixels, 0u, nullptr, nullptr) ==
                      CL_SUCCESS &&
                  cl_.EnqueueReleaseGLObjects(queue_, 1u, &image, 0u, nullptr, nullptr) == CL_SUCCESS &&
                  cl_.Finish(queue_) == CL_SUCCESS;
        if (!ok) no("acquire, the read or release failed");
        for (int i = 0; i < side * side && ok; i++)
            for (int c = 0; c < 4 && ok; c++) {
                const int diff = static_cast<int>(pixels[i * 4 + c]) - want[c];
                if (diff < -1 || diff > 1) ok = no(why);
            }
        release(pixels);
        return ok;
    }

    bool gl_renders_a_texture_the_kernel_reads()
    {
        const GLuint name = gl_texture(gl_rgba8, gl_unsigned_byte);
        GLuint       framebuffer = 0;
        gl_.GenFramebuffers(1, &framebuffer);
        gl_.BindFramebuffer(gl_framebuffer, framebuffer);
        gl_.FramebufferTexture2D(gl_framebuffer, gl_color_attachment0, gl_texture_2d, name, 0);
        const bool complete = gl_.CheckFramebufferStatus(gl_framebuffer) == gl_framebuffer_complete;
        gl_.Viewport(0, 0, side, side);
        gl_.ClearColor(0.2f, 0.4f, 0.6f, 1.0f);
        gl_.Clear(gl_color_buffer_bit);
        gl_.BindFramebuffer(gl_framebuffer, 0);
        gl_.Finish();
        cl_int       error = CL_SUCCESS;
        const cl_mem shared = cl_.CreateFromGLTexture(context_, CL_MEM_READ_ONLY, gl_texture_2d, 0, name, &error);
        bool ok = complete && shared != nullptr;
        if (!complete) no("the texture is no complete framebuffer");
        else if (shared == nullptr) no("clCreateFromGLTexture", error);
        const unsigned char want[4] = {51, 102, 153, 255};
        ok = ok && pixels_are(shared, want, "OpenCL read a texel GL did not render");
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteFramebuffers(1, &framebuffer);
        gl_.DeleteTextures(1, &name);
        return ok;
    }

    bool kernel_reads_a_renderbuffer()
    {
        GLuint renderbuffer = 0;
        GLuint framebuffer = 0;
        gl_.GenRenderbuffers(1, &renderbuffer);
        gl_.BindRenderbuffer(gl_renderbuffer, renderbuffer);
        gl_.RenderbufferStorage(gl_renderbuffer, gl_rgba8, side, side);
        gl_.GenFramebuffers(1, &framebuffer);
        gl_.BindFramebuffer(gl_framebuffer, framebuffer);
        gl_.FramebufferRenderbuffer(gl_framebuffer, gl_color_attachment0, gl_renderbuffer, renderbuffer);
        const bool complete = gl_.CheckFramebufferStatus(gl_framebuffer) == gl_framebuffer_complete;
        gl_.Viewport(0, 0, side, side);
        gl_.ClearColor(0.8f, 0.1f, 0.3f, 1.0f);
        gl_.Clear(gl_color_buffer_bit);
        gl_.BindFramebuffer(gl_framebuffer, 0);
        gl_.Finish();
        cl_int       error = CL_SUCCESS;
        const cl_mem shared = cl_.CreateFromGLRenderbuffer(context_, CL_MEM_READ_ONLY, renderbuffer, &error);
        bool ok = complete && shared != nullptr;
        if (!complete) no("the renderbuffer is no complete framebuffer");
        else if (shared == nullptr) no("clCreateFromGLRenderbuffer", error);
        const unsigned char want[4] = {204, 26, 77, 255};
        ok = ok && pixels_are(shared, want, "OpenCL read a pixel GL did not clear to");
        if (shared != nullptr) cl_.ReleaseMemObject(shared);
        gl_.DeleteFramebuffers(1, &framebuffer);
        gl_.DeleteRenderbuffers(1, &renderbuffer);
        return ok;
    }

    bool objects_say_what_they_share()
    {
        const GLuint buffer = gl_buffer(nullptr);
        const GLuint texture = gl_texture(gl_rgba8, gl_unsigned_byte);
        gl_.Finish();
        cl_int       error = CL_SUCCESS;
        const cl_mem from_buffer = cl_.CreateFromGLBuffer(context_, CL_MEM_READ_WRITE, buffer, &error);
        const cl_mem from_texture = cl_.CreateFromGLTexture(context_, CL_MEM_READ_ONLY, gl_texture_2d, 0, texture, &error);
        bool ok = from_buffer != nullptr && from_texture != nullptr;
        if (!ok) no("the objects could not be made", error);
        cl_gl_object_type type = 0;
        cl_GLuint         name = 0;
        if (ok && (cl_.GetGLObjectInfo(from_buffer, &type, &name) != CL_SUCCESS || type != CL_GL_OBJECT_BUFFER ||
                   name != buffer))
            ok = no("clGetGLObjectInfo does not name the GL buffer");
        if (ok && (cl_.GetGLObjectInfo(from_texture, &type, &name) != CL_SUCCESS || type != CL_GL_OBJECT_TEXTURE2D ||
                   name != texture))
            ok = no("clGetGLObjectInfo does not name the GL texture");
        GLenum target = 0;
        GLint  level = -1;
        if (ok && (cl_.GetGLTextureInfo(from_texture, CL_GL_TEXTURE_TARGET, sizeof target, &target, nullptr) != CL_SUCCESS ||
                   target != gl_texture_2d))
            ok = no("clGetGLTextureInfo does not say GL_TEXTURE_2D");
        if (ok && (cl_.GetGLTextureInfo(from_texture, CL_GL_MIPMAP_LEVEL, sizeof level, &level, nullptr) != CL_SUCCESS ||
                   level != 0))
            ok = no("clGetGLTextureInfo does not say mip level 0");
        if (from_texture != nullptr) cl_.ReleaseMemObject(from_texture);
        if (from_buffer != nullptr) cl_.ReleaseMemObject(from_buffer);
        gl_.DeleteTextures(1, &texture);
        gl_.DeleteBuffers(1, &buffer);
        return ok;
    }

    // What Photoshop CS6's sniffer_gpu.exe measures before it lets "Use
    // OpenCL" be ticked: the sum of END - START over the copies' events, which
    // are the host GPU's own timestamps.
    bool copies_are_timed_as_photoshop_times_them()
    {
        cl_int                 error = CL_SUCCESS;
        const cl_command_queue timed = cl_.CreateCommandQueue(context_, device_, CL_QUEUE_PROFILING_ENABLE, &error);
        if (timed == nullptr) return no("clCreateCommandQueue with CL_QUEUE_PROFILING_ENABLE", error);
        const cl_mem from = cl_.CreateBuffer(context_, CL_MEM_READ_ONLY, copy_bytes, nullptr, &error);
        const cl_mem to = cl_.CreateBuffer(context_, CL_MEM_WRITE_ONLY, copy_bytes, nullptr, &error);
        cl_event     events[copies] = {};
        bool         ok = from != nullptr && to != nullptr && cl_.Finish(timed) == CL_SUCCESS;
        if (!ok) no("the two buffers", error);
        for (int i = 0; i < copies && ok; i++) {
            error = cl_.EnqueueCopyBuffer(timed, from, to, 0u, 0u, copy_bytes, 0u, nullptr, &events[i]);
            if (error != CL_SUCCESS) ok = no("clEnqueueCopyBuffer", error);
        }
        if (ok && cl_.Finish(timed) != CL_SUCCESS) ok = no("clFinish");
        unsigned long long spent = 0u;
        for (int i = 0; i < copies && ok; i++) {
            cl_ulong start = 0u;
            cl_ulong end = 0u;
            error = cl_.GetEventProfilingInfo(events[i], CL_PROFILING_COMMAND_START, sizeof start, &start, nullptr);
            if (error == CL_SUCCESS)
                error = cl_.GetEventProfilingInfo(events[i], CL_PROFILING_COMMAND_END, sizeof end, &end, nullptr);
            if (error != CL_SUCCESS) ok = no("clGetEventProfilingInfo", error);
            else if (start == 0u || end <= start) ok = no("a copy that ended no later than it started");
            else spent += end - start;
        }
        for (int i = 0; i < copies; i++)
            if (events[i] != nullptr) cl_.ReleaseEvent(events[i]);
        if (to != nullptr) cl_.ReleaseMemObject(to);
        if (from != nullptr) cl_.ReleaseMemObject(from);
        cl_.ReleaseCommandQueue(timed);
        if (!ok) return false;
        const double gigabytes_per_second = static_cast<double>(copy_bytes) * 2.0 * copies / static_cast<double>(spent);
        write_text("  copies: ");
        write_signed(static_cast<long long>(gigabytes_per_second));
        write_text(" GB/s\n");
        return gigabytes_per_second >= least_gigabytes_per_second ? true : no("slower than the 12 GB/s Photoshop wants");
    }

    Gl               gl_ = {};
    Cl               cl_ = {};
    HDC              dc_ = nullptr;
    HGLRC            gl_context_ = nullptr;
    cl_platform_id   platform_ = nullptr;
    cl_device_id     device_ = nullptr;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_program       program_ = nullptr;
    const char      *kernel_text_ = kernels;
    const char      *why_ = "";
    cl_int           code_ = CL_SUCCESS;
    bool             has_code_ = false;
    int              passed_ = 0;
    int              failed_ = 0;
};

}  // namespace

extern "C" void rvkclglsamples_entry()
{
    Suite suite;
    ExitProcess(suite.open() ? static_cast<UINT>(suite.run()) : 100u);
}
