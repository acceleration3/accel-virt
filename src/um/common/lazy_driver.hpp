/* A user-mode driver's root and everything it is built over - its Seams -
 * made on the first call into the DLL rather than at attach, where the loader
 * lock forbids opening a file or an event, and by exactly one thread however
 * many arrive at once. The Vulkan loader and opengl32 serialise their first
 * call; the Khronos OpenCL loader and the Direct3D runtimes promise nothing
 * (research 052 §3, gap 2), so the first call runs under RunOnce for all four.
 *
 * Seams has `log` (the driver's FileLog), `root` (what tracing reports) and
 * `trace` (its ProcessTrace), and `initialize()`, the fallible half of its
 * construction: a process it fails in is refused for good, and the DLL
 * answers every call as a driver that does not serve here. */
#ifndef AV_UM_LAZY_DRIVER_HPP
#define AV_UM_LAZY_DRIVER_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/driver_log.hpp"
#include "um/common/once_body.hpp"
#include "um/common/run_once.hpp"
#include "um/trace/process_trace.hpp"

namespace av {

template <typename Seams>
class LazyDriver final : public OnceBody {
public:
    /* `prefix` starts the driver's lines ("avvk: "), `module` names its
     * trace ("avvk"), and `crashed` is the exports' unhandled-exception
     * filter, which forwards to crashed(). Constexpr: the driver is the
     * exports' constinit storage. */
    constexpr LazyDriver(const char *prefix, const char *module, ProcessTrace::Filter crashed);
    LazyDriver(const LazyDriver &) = delete;
    LazyDriver &operator=(const LazyDriver &) = delete;

    /* The seams, made on the first call; null in a refused process. */
    Seams *seams();
    /* The process is crashing: the crash file, if tracing started. */
    long   crashed(void *pointers);
    void   run_once() override;

private:
    ObjectStorage<Seams> seams_;
    RunOnce              once_;
    const char          *prefix_;
    const char          *module_;
    ProcessTrace::Filter crashed_;
};

template <typename Seams>
constexpr LazyDriver<Seams>::LazyDriver(const char *prefix, const char *module, ProcessTrace::Filter crashed)
    : seams_(), once_(), prefix_(prefix), module_(module), crashed_(crashed)
{}

template <typename Seams>
Seams *LazyDriver<Seams>::seams()
{
    AV_SCOPE("LazyDriver::seams");
    if (!once_.run(*this) || !seams_.constructed()) return nullptr;
    return &seams_.object();
}

template <typename Seams>
long LazyDriver<Seams>::crashed(void *pointers)
{
    AV_SCOPE("LazyDriver::crashed");
    return seams_.constructed() ? seams_.object().trace.crashed(pointers) : 0;
}

template <typename Seams>
void LazyDriver<Seams>::run_once()
{
    AV_SCOPE("LazyDriver::run_once");
    if (seams_.construct() != Status::ok) return;
    Seams         &seams = seams_.object();
    DriverLog lines(seams.log, prefix_);
    if (seams.initialize() != Status::ok) {
        lines.write("no D3DKMT wait event; this process is refused");
        seams_.destroy();
        return;
    }
    if (seams.trace.start(module_, seams.root, crashed_) != Status::ok)
        lines.write("tracing is off: its section, pin or service failed");
}

}  // namespace av

#endif  // AV_UM_LAZY_DRIVER_HPP
