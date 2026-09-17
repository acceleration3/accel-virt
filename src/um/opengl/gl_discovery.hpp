/* rvkgl.dll's discovery build (feature 050, T007): the ICD opengl32 loads, with
 * nothing behind it. Each entry opengl32 calls is logged - its name and first
 * argument - to C:\ProgramData\rvk\rvkgl-<pid>.log and answered so the load
 * can proceed as far as a pixel format: version 2 is valid, one RGBA8
 * D24S8 double-buffered format is described, a pixel format is taken, and
 * nothing else is done. What the live run shows decides whether the ICD can be
 * built on this path (research.md §9). Replaced by the ICD itself in T030. One
 * per process, in the exports' storage; the log and tracing start on the first
 * call, not at DLL attach, where the loader lock forbids opening files. */
#ifndef AV_UM_GL_DISCOVERY_HPP
#define AV_UM_GL_DISCOVERY_HPP

#include "core/support/object_storage.hpp"
#include "core/trace/state_reporter.hpp"
#include "um/icd/file_log.hpp"
#include "um/trace/process_trace.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class GlDiscovery final : public StateReporter {
public:
    /* Every Drv* entry the contract names, in its order. */
    enum class Entry : uint32_t {
        validate_version, set_callback_procs, describe_pixel_format, set_pixel_format, create_context,
        create_layer_context, delete_context, set_context, release_context, copy_context, share_lists,
        get_proc_address, describe_layer_plane, swap_layer_buffers, swap_buffers, count,
    };
    /* The only interface version opengl32 is answered for. */
    static constexpr uint32_t driver_version = 2u;
    static constexpr int32_t  formats = 1;

    constexpr explicit GlDiscovery(ProcessTrace::Filter crashed);
    GlDiscovery(const GlDiscovery &) = delete;
    GlDiscovery &operator=(const GlDiscovery &) = delete;

    /* Each logs the entry with its first argument, counts it, and answers:
     * TRUE (1) for version 2 and for any pixel format, FALSE (0) otherwise. */
    int32_t validate(uint32_t version);
    int32_t answer(Entry entry, uint64_t argument);
    /* The same for an entry answering a pointer: always null. */
    void   *decline(Entry entry, uint64_t argument);
    /* A by-name lookup, logged with the name: nothing is offered. */
    void   *look_up(const char *name);
    /* The format count; the descriptor filled for format 1 and a size that holds it. */
    int32_t describe(uint64_t dc, int32_t format, uint32_t bytes, void *descriptor);
    long    crashed(void *pointers);

    void report(StateWriter &writer) override;

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;

        FileLog      log;
        ProcessTrace trace;
    };

    Seams *seams();
    void   called(Entry entry, uint64_t argument);

    ObjectStorage<Seams> seams_;
    ProcessTrace::Filter crashed_;
    uint32_t             calls_[static_cast<uint32_t>(Entry::count)];
};

constexpr GlDiscovery::GlDiscovery(ProcessTrace::Filter crashed) : seams_(), crashed_(crashed), calls_{} {}

}  // namespace av

#endif  // AV_UM_GL_DISCOVERY_HPP
