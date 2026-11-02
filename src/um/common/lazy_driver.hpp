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
    constexpr LazyDriver(const char *module, ProcessTrace::Filter crashed);
    LazyDriver(const LazyDriver &) = delete;
    LazyDriver &operator=(const LazyDriver &) = delete;

    Seams *seams();
    auto   root() -> decltype(Seams::root) *;
    long   crashed(void *pointers);
    void   run_once() override;

private:
    ObjectStorage<Seams> seams_;
    RunOnce              once_;
    const char          *module_;
    ProcessTrace::Filter crashed_;
};

template <typename Seams>
constexpr LazyDriver<Seams>::LazyDriver(const char *module, ProcessTrace::Filter crashed)
    : seams_(), once_(), module_(module), crashed_(crashed)
{}

template <typename Seams>
Seams *LazyDriver<Seams>::seams()
{
    AV_SCOPE("LazyDriver::seams");
    if (!once_.run(*this) || !seams_.constructed()) return nullptr;
    return &seams_.object();
}

template <typename Seams>
auto LazyDriver<Seams>::root() -> decltype(Seams::root) *
{
    AV_SCOPE("LazyDriver::root");
    Seams *const made = seams();
    return made == nullptr ? nullptr : &made->root;
}

template <typename Seams>
long LazyDriver<Seams>::crashed(void *pointers)
{
    AV_SCOPE("LazyDriver::crashed");
    return seams_.constructed() ? seams_.object().process.trace().crashed(pointers) : 0;
}

template <typename Seams>
void LazyDriver<Seams>::run_once()
{
    AV_SCOPE("LazyDriver::run_once");
    if (seams_.construct() != Status::ok) return;
    Seams    &seams = seams_.object();
    DriverLog &lines = seams.process.log();
    if (seams.process.initialize() != Status::ok) {
        lines.write("no D3DKMT wait event; this process is refused");
        seams_.destroy();
        return;
    }
    if (seams.process.trace().start(module_, seams.root, crashed_) != Status::ok)
        lines.write("tracing is off: its section, pin or service failed");
}

}

#endif
