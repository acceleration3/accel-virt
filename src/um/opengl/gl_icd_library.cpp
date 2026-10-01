#include "um/opengl/gl_icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_icd_binding.hpp"

namespace av {

GlIcdLibrary::Seams::Seams()
    : heap(), lock(), send_lock(), log(L"rvkgl"), kmt(), surface(), slot(), opengl32(), chains_lock(),
      chains(kmt, chains_lock, log), present_lock(), present_kmt(), present_thread(), present_handed(),
      present_shown(),
      presents(present_thread, present_handed, present_shown, present_lock, present_kmt, surface, chains, log),
      root(heap, kmt, lock, send_lock, log, surface, slot, opengl32, presents, glicd::gl_icd_binding), trace()
{
    AV_SCOPE("GlIcdLibrary::Seams::Seams");
    log.open();
}

bool GlIcdLibrary::ready()
{
    AV_SCOPE("GlIcdLibrary::ready");
    if (!seams_.constructed() && !refused_ && seams_.construct() == Status::ok &&
        seams_.object().kmt.initialize() != Status::ok) {
        seams_.object().log.write("rvkgl: no D3DKMT wait event, refusing opengl32");
        seams_.destroy();
        refused_ = true;
        return false;
    }
    if (seams_.constructed() && !started_presents_) {
        started_presents_ = true;
        Seams &seams = seams_.object();
        if (seams.present_kmt.initialize() != Status::ok || seams.present_handed.initialize() != Status::ok ||
            seams.present_shown.initialize() != Status::ok) {
            seams.log.write("rvkgl: no present thread: its D3DKMT event or wake events could not be made");
            seams.presents.refuse();
        }
    }
    if (seams_.constructed() && !started_trace_) {
        started_trace_ = true;
        Seams &seams = seams_.object();
        const Status traced = seams.trace.start("rvkgl", seams.root, crashed_);
        if (traced != Status::ok) seams.log.write("rvkgl: tracing is off: its section, pin or service failed");
    }
    return seams_.constructed();
}

int32_t GlIcdLibrary::validate_version(uint32_t version)
{
    AV_SCOPE("GlIcdLibrary::validate_version");
    return ready() ? glicd::entry::DrvValidateVersion(version) : 0;
}

void GlIcdLibrary::set_callback_procs(int32_t count, void *const *procs)
{
    AV_SCOPE("GlIcdLibrary::set_callback_procs");
    if (ready()) glicd::entry::DrvSetCallbackProcs(count, procs);
}

int32_t GlIcdLibrary::describe_pixel_format(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor)
{
    AV_SCOPE("GlIcdLibrary::describe_pixel_format");
    return ready() ? glicd::entry::DrvDescribePixelFormat(dc, index, bytes, descriptor) : 0;
}

int32_t GlIcdLibrary::set_pixel_format(void *dc, int32_t index)
{
    AV_SCOPE("GlIcdLibrary::set_pixel_format");
    return ready() ? glicd::entry::DrvSetPixelFormat(dc, index) : 0;
}

uintptr_t GlIcdLibrary::create_context(void *dc)
{
    AV_SCOPE("GlIcdLibrary::create_context");
    return ready() ? glicd::entry::DrvCreateContext(dc) : 0u;
}

uintptr_t GlIcdLibrary::create_layer_context(void *dc, int32_t layer)
{
    AV_SCOPE("GlIcdLibrary::create_layer_context");
    return ready() ? glicd::entry::DrvCreateLayerContext(dc, layer) : 0u;
}

int32_t GlIcdLibrary::delete_context(uintptr_t context)
{
    AV_SCOPE("GlIcdLibrary::delete_context");
    return ready() ? glicd::entry::DrvDeleteContext(context) : 0;
}

void *GlIcdLibrary::set_context(void *dc, uintptr_t context, void *set_proc_table)
{
    AV_SCOPE("GlIcdLibrary::set_context");
    return ready() ? glicd::entry::DrvSetContext(dc, context, set_proc_table) : nullptr;
}

int32_t GlIcdLibrary::release_context(uintptr_t context)
{
    AV_SCOPE("GlIcdLibrary::release_context");
    return ready() ? glicd::entry::DrvReleaseContext(context) : 0;
}

int32_t GlIcdLibrary::copy_context(uintptr_t source, uintptr_t destination, uint32_t mask)
{
    AV_SCOPE("GlIcdLibrary::copy_context");
    return ready() ? glicd::entry::DrvCopyContext(source, destination, mask) : 0;
}

int32_t GlIcdLibrary::share_lists(uintptr_t first, uintptr_t second)
{
    AV_SCOPE("GlIcdLibrary::share_lists");
    return ready() ? glicd::entry::DrvShareLists(first, second) : 0;
}

void *GlIcdLibrary::proc_address(const char *name)
{
    AV_SCOPE("GlIcdLibrary::proc_address");
    return ready() ? glicd::entry::DrvGetProcAddress(name) : nullptr;
}

int32_t GlIcdLibrary::describe_layer_plane(void *dc, int32_t format, int32_t plane, uint32_t bytes, void *descriptor)
{
    AV_SCOPE("GlIcdLibrary::describe_layer_plane");
    return ready() ? glicd::entry::DrvDescribeLayerPlane(dc, format, plane, bytes, descriptor) : 0;
}

int32_t GlIcdLibrary::swap_layer_buffers(void *dc, uint32_t planes)
{
    AV_SCOPE("GlIcdLibrary::swap_layer_buffers");
    return ready() ? glicd::entry::DrvSwapLayerBuffers(dc, planes) : 0;
}

int32_t GlIcdLibrary::swap_buffers(void *dc)
{
    AV_SCOPE("GlIcdLibrary::swap_buffers");
    return ready() ? glicd::entry::DrvSwapBuffers(dc) : 0;
}

long GlIcdLibrary::crashed(void *pointers)
{
    AV_SCOPE("GlIcdLibrary::crashed");
    return seams_.constructed() ? seams_.object().trace.crashed(pointers) : 0;
}

}  // namespace av
