#include "executor/executor_root.hpp"

#include "executor/context_refusals.hpp"
#include "executor/executor_log.hpp"
#include "core/log/log_line.hpp"

#include "executor/shim/d3d11_host_device.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/host_apis.hpp"
#include "executor/host_gpu.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/host_ram_ledger.hpp"
#include "executor/vram_ledger.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/teardown_budget.hpp"
#include "executor/watcher_exit.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

namespace {

alignas(16) uint8_t decode_blocks[ExecutorRoot::sessions][ContextSession::arena_bytes];

void *run_wedged_watch(void *root)
{
    AV_SCOPE("run_wedged_watch");
    static_cast<ExecutorRoot *>(root)->watch_wedged();
    return nullptr;
}

void *run_closing_watch(void *root)
{
    AV_SCOPE("run_closing_watch");
    static_cast<ExecutorRoot *>(root)->watch_closing();
    return nullptr;
}

}

ExecutorRoot::ExecutorRoot(ExecutorLog &log)
    : gpus_(), domains_(), memory_groups_{}, group_of_{}, group_refs_{}, sessions_{}, vulkan_{}, opencl_{}, opengl_{}, d3d_{}, shares_{}, apis_of_{}, gpus_of_{},
      wedged_{}, reaping_{}, closing_{}, wedged_since_{}, lock_(), wake_(lock_), watcher_{}, watching_(false), helpers_{}, helpers_running_(0u), prepared_{}, exhausted_(0u), exhaustion_logged_(false), stopping_(false), clock_(), apis_(nullptr), log_(log), lost_leak_after_(lost_leak_nanoseconds), refusals_(log.sessions())
{
    AV_SCOPE("ExecutorRoot::ExecutorRoot");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) group_of_[slot] = max_sessions;
    for (uint32_t slot = 0u; slot < warm_sessions; slot++) prepare_slot(slot);
}

void ExecutorRoot::prepare_slot(uint32_t slot)
{
    AV_SCOPE("ExecutorRoot::prepare_slot");
    if (prepared_[slot]) return;
    static_cast<void>(sessions_[slot].construct(log_.sessions()));
    static_cast<void>(vulkan_[slot].construct(log_.vulkan()));
    static_cast<void>(opencl_[slot].construct(log_.opencl()));
    static_cast<void>(opengl_[slot].construct(log_.opengl()));
    static_cast<void>(d3d_[slot].construct(log_));
    prepared_[slot] = true;
}

ExecutorRoot::~ExecutorRoot()
{
    AV_SCOPE("ExecutorRoot::~ExecutorRoot");
    stop_watching();
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        d3d_[slot].destroy();
        opengl_[slot].destroy();
        opencl_[slot].destroy();
        vulkan_[slot].destroy();
        sessions_[slot].destroy();
    }
}

void ExecutorRoot::stop_watching()
{
    AV_SCOPE("ExecutorRoot::stop_watching");
    WatcherExit::process().disarm(*this);
    halt_watcher();
}

void ExecutorRoot::halt_watcher()
{
    AV_SCOPE("ExecutorRoot::halt_watcher");
    lock_.acquire();
    const bool joined = watching_;
    const uint32_t helpers = helpers_running_;
    watching_ = false;
    helpers_running_ = 0u;
    stopping_ = true;
    wake_.broadcast();
    lock_.release();
    if (joined) pthread_join(watcher_, nullptr);
    for (uint32_t helper = 0u; helper < helpers; helper++) pthread_join(helpers_[helper], nullptr);
}

Status ExecutorRoot::initialize(HostApis &apis)
{
    AV_SCOPE("ExecutorRoot::initialize");
    if (apis_ != nullptr) return Status::invalid_state;
    gpus_.initialize(apis);
    apis_ = &apis;
    return Status::ok;
}

uint32_t ExecutorRoot::sub_apis() const
{
    AV_SCOPE("ExecutorRoot::sub_apis");
    return apis_ != nullptr ? gpus_.sub_apis() : 0u;
}

uint32_t ExecutorRoot::open_domain(const uint8_t *gpu_uuid, uint32_t vram_reserve_mib, uint64_t host_ram_bytes,
                                   uint64_t host_ram_floor_bytes)
{
    AV_SCOPE("ExecutorRoot::open_domain");
    const HostGpu *const gpu = apis_ != nullptr ? gpus_.find(gpu_uuid) : nullptr;
    if (gpu == nullptr) return 0u;
    HostMemorySource *const source = apis_->memory_source(gpu->index());
    HostRamLedger *const host_ram = apis_->host_ram_ledger();
    if (host_ram != nullptr) {
        host_ram->budget(host_ram_bytes);
        host_ram->floor(host_ram_floor_bytes);
    }
    if (source != nullptr) {
        source->reserve_vram(vram_reserve_mib);
        source->use_host_ram(host_ram);
    }
    VramLedger *const ledger = apis_->vram_ledger(gpu->index());
    if (ledger != nullptr) ledger->reserve_vram(vram_reserve_mib);
    if (source != nullptr) source->use_vram_ledger(ledger);
    return domains_.open(gpu->index());
}

void ExecutorRoot::close_domain(uint32_t domain)
{
    AV_SCOPE("ExecutorRoot::close_domain");
    release_shared(domain, 0u);
    HostMemorySource *const source = source_of(domain);
    if (source != nullptr) source->free_domain(domain);
    uint32_t gpu = 0u;
    static_cast<void>(domains_.close(domain, gpu));
}

HostMemorySource *ExecutorRoot::source_of(uint32_t domain) const
{
    AV_SCOPE("ExecutorRoot::source_of");
    const HostGpu *const gpu = gpu_of(domain);
    return gpu != nullptr ? apis_->memory_source(gpu->index()) : nullptr;
}

Status ExecutorRoot::allocate_memory(uint32_t domain, uint64_t key, uint64_t size, uint32_t request,
                                     AvHostMemory &memory)
{
    AV_SCOPE("ExecutorRoot::allocate_memory");
    HostMemorySource *const source = source_of(domain);
    if (source == nullptr) return Status::no_memory;
    return source->allocate(domain, key, size, HostMemoryRequest(request), memory);
}

Status ExecutorRoot::alias_memory(uint32_t domain, uint64_t key, uint32_t from_domain, uint64_t from_key,
                                  AvHostMemory &memory)
{
    AV_SCOPE("ExecutorRoot::alias_memory");
    HostMemorySource *const source = source_of(domain);
    HostMemorySource *const lender = source_of(from_domain);
    if (source == nullptr || lender == nullptr) return Status::no_memory;
    return source->alias(domain, key, *lender, from_domain, from_key, memory);
}

void ExecutorRoot::free_memory(uint32_t domain, uint64_t key)
{
    AV_SCOPE("ExecutorRoot::free_memory");
    HostMemorySource *const source = source_of(domain);
    if (source != nullptr) source->free(domain, key);
}

bool ExecutorRoot::export_image(uint32_t domain, uint64_t id, AvExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::export_image");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->export_image(domain, id, image);
}

bool ExecutorRoot::compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                  uint32_t height, AvExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::compose_planes");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->compose_planes(domain, planes, count, width, height, image);
}

void ExecutorRoot::release_shared(uint32_t domain, uint64_t id)
{
    AV_SCOPE("ExecutorRoot::release_shared");
    const HostGpu *const gpu = gpu_of(domain);
    if (gpu == nullptr) return;
    apis_->shares(gpu->index())->release(domain, id);
    apis_->surfaces(gpu->index())->release(id);
    retire(gpu->index());
}

bool ExecutorRoot::read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width,
                              uint32_t height)
{
    AV_SCOPE("ExecutorRoot::read_image");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->read_image(domain, id, pixels, stride, width, height);
}

bool ExecutorRoot::map_frame(uint32_t domain, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view)
{
    AV_SCOPE("ExecutorRoot::map_frame");
    const HostGpu *const  gpu = gpu_of(domain);
    SharedSurfaces *const surfaces = gpu != nullptr ? apis_->surfaces(gpu->index()) : nullptr;
    return surfaces != nullptr && surfaces->map(frame, wait_ms, view);
}

void ExecutorRoot::unmap_frame(uint32_t domain, const SurfacePoint &frame)
{
    AV_SCOPE("ExecutorRoot::unmap_frame");
    const HostGpu *const  gpu = gpu_of(domain);
    SharedSurfaces *const surfaces = gpu != nullptr ? apis_->surfaces(gpu->index()) : nullptr;
    if (surfaces != nullptr) surfaces->unmap(frame);
}

bool ExecutorRoot::export_surface(uint32_t domain, uint64_t surface, uint32_t flags, AvExportedImage &image)
{
    AV_SCOPE("ExecutorRoot::export_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->export_surface(domain, surface, flags, image);
}

bool ExecutorRoot::copy_surface(uint32_t domain, const SurfacePair &surfaces, const AvCopyRegion *regions,
                                uint32_t count, uint32_t wait_ms)
{
    AV_SCOPE("ExecutorRoot::copy_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->copy_surface(domain, surfaces.source(), surfaces.destination(), regions, count, wait_ms);
}

bool ExecutorRoot::map_surface(uint32_t domain, uint64_t surface, uint32_t wait_ms, AvFrameView &view)
{
    AV_SCOPE("ExecutorRoot::map_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->map_surface(domain, surface, wait_ms, view);
}

void ExecutorRoot::unmap_surface(uint32_t domain, uint64_t surface)
{
    AV_SCOPE("ExecutorRoot::unmap_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    if (device != nullptr) device->unmap_surface(surface);
}

bool ExecutorRoot::sync_surface(uint32_t domain, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd)
{
    AV_SCOPE("ExecutorRoot::sync_surface");
    const HostGpu *const   gpu = gpu_of(domain);
    D3d11HostDevice *const device = gpu != nullptr ? apis_->d3d11_device(gpu->index()) : nullptr;
    return device != nullptr && device->sync_surface(domain, surface, flags, writes, fd);
}

ContextSession *ExecutorRoot::open(uint32_t sub_api, const AvContextOptions &options, ContextSession *share)
{
    AV_SCOPE("ExecutorRoot::open");
    const char *const api = api_name(sub_api);
    const HostGpu *const gpu = gpu_of(options.domain);
    if (gpu == nullptr) {
        refusals_.refuse(ContextRefusals::Reason::domain, api, options.domain);
        return nullptr;
    }
    const bool options_asked = options.major != 0u || options.minor != 0u || options.profile != 0u || options.flags != 0u;
    if (options_asked && sub_api != AV_EXECUTOR_SUB_API_OPENGL) {
        refusals_.refuse(ContextRefusals::Reason::options, api, options.flags);
        return nullptr;
    }
    if (share != nullptr && sub_api != AV_EXECUTOR_SUB_API_OPENGL && sub_api != AV_EXECUTOR_SUB_API_OPENCL) {
        refusals_.refuse(ContextRefusals::Reason::share_api, api, sub_api);
        return nullptr;
    }
    if ((gpu->sub_apis() & sub_api) == 0u) {
        refusals_.refuse(ContextRefusals::Reason::sub_api_mask, api, gpu->sub_apis());
        return nullptr;
    }
    if (backend_in(0u, sub_api) == nullptr) {
        refusals_.refuse(ContextRefusals::Reason::no_backend, api, sub_api);
        return nullptr;
    }
    reap_wedged();

    lock_.acquire();
    const OpenGlBackend *const shared = share != nullptr ? sharing(share, *gpu) : nullptr;
    if (share != nullptr && shared == nullptr) {
        lock_.release();
        refusals_.refuse(ContextRefusals::Reason::share_gone, api, 0u);
        return nullptr;
    }
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        prepare_slot(slot);
        SessionBackend &backend = *backend_in(slot, sub_api);
        if (sessions_[slot].object().initialize(backend, decode_blocks[slot], ContextSession::arena_bytes) != Status::ok)
            continue;
        GlMemoryGroup *const group = sub_api == AV_EXECUTOR_SUB_API_OPENGL ? reserve_group(share, slot) : nullptr;
        lock_.release();
        shares_[slot].bind(*apis_->shares(gpu->index()), options.domain, backend);
        if (!bind(slot, sub_api, *gpu, options, shared, group)) {
            shares_[slot].end();
            TeardownBudget budget(clock_, teardown_nanoseconds);
            const bool released = sessions_[slot].object().release(budget);
            lock_.acquire();
            wedged_[slot] = !released;
            if (released) {
                release_group(slot);
                sessions_[slot].object().vacate();
            }
            lock_.release();
            if (!released) {
                sessions_[slot].object().let_go_of_thread();
                static_cast<void>(note_wedged());
            }
            return nullptr;
        }
        lock_.acquire();
        apis_of_[slot] = sub_api;
        gpus_of_[slot] = gpu->index();
        lock_.release();
        HostMemorySource *const source = apis_->memory_source(gpu->index());
        if (source != nullptr) sessions_[slot].object().draw_from(*source, options.domain);
        if (sub_api != AV_EXECUTOR_SUB_API_OPENCL && clocks_[gpu->index()].open(apis_->render_minor(gpu->index())))
            sessions_[slot].object().hold_clocks(clocks_[gpu->index()]);
        return &sessions_[slot].object();
    }
    lock_.release();
    note_exhausted(sub_api);
    return nullptr;
}

void ExecutorRoot::note_exhausted(uint32_t sub_api)
{
    AV_SCOPE("ExecutorRoot::note_exhausted");
    lock_.acquire();
    exhausted_++;
    const bool first = !exhaustion_logged_;
    exhaustion_logged_ = true;
    const uint32_t leaving = closing_here();
    const uint32_t stuck = wedged_here();
    lock_.release();
    if (!first) return;
    LogLine(log_.sessions())
        .text("every one of the ")
        .number(max_sessions)
        .text(" sessions is taken, so a ")
        .text(api_name(sub_api))
        .text(" context is refused: ")
        .number(live())
        .text(" live, ")
        .number(leaving)
        .text(" closing, ")
        .number(stuck)
        .text(" wedged; the guest is told it is out of resources")
        .write();
}

uint32_t ExecutorRoot::exhausted() const
{
    AV_SCOPE("ExecutorRoot::exhausted");
    const ThreadLockHold held(lock_);
    return exhausted_;
}

SessionBackend *ExecutorRoot::backend_in(uint32_t slot, uint32_t sub_api)
{
    AV_SCOPE("ExecutorRoot::backend_in");
    switch (sub_api) {
    case AV_EXECUTOR_SUB_API_VULKAN: return &vulkan_[slot].object();
    case AV_EXECUTOR_SUB_API_OPENCL: return &opencl_[slot].object();
    case AV_EXECUTOR_SUB_API_OPENGL: return &opengl_[slot].object();
    case AV_EXECUTOR_SUB_API_D3D: return &d3d_[slot].object();
    default: return nullptr;
    }
}

const OpenGlBackend *ExecutorRoot::sharing(const ContextSession *share, const HostGpu &gpu) const
{
    AV_SCOPE("ExecutorRoot::sharing");
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (&sessions_[slot].object() != share) continue;
        const bool fits = share->live() && apis_of_[slot] == AV_EXECUTOR_SUB_API_OPENGL &&
                          gpus_of_[slot] == gpu.index();
        return fits ? &opengl_[slot].object() : nullptr;
    }
    return nullptr;
}

GlMemoryGroup *ExecutorRoot::reserve_group(const ContextSession *share, uint32_t slot)
{
    AV_SCOPE("ExecutorRoot::reserve_group");
    const uint32_t sharer = share != nullptr ? slot_of(share) : max_sessions;
    uint32_t       chosen = sharer != max_sessions ? group_of_[sharer] : max_sessions;
    for (uint32_t group = 0u; group < max_sessions && chosen == max_sessions && sharer == max_sessions; group++)
        if (group_refs_[group] == 0u && memory_groups_[group].members() == 0u) chosen = group;
    if (chosen == max_sessions) return nullptr;
    group_refs_[chosen]++;
    group_of_[slot] = chosen;
    return &memory_groups_[chosen];
}

void ExecutorRoot::release_group(uint32_t slot)
{
    AV_SCOPE("ExecutorRoot::release_group");
    if (group_of_[slot] == max_sessions) return;
    group_refs_[group_of_[slot]]--;
    group_of_[slot] = max_sessions;
}

bool ExecutorRoot::bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu, const AvContextOptions &options,
                        const OpenGlBackend *share, GlMemoryGroup *group)
{
    AV_SCOPE("ExecutorRoot::bind");
    sessions_[slot].object().use_host_ram(apis_->host_ram_ledger(), session_host_bytes);
    backend_in(slot, sub_api)->use_host_ram(apis_->host_ram_ledger(), session_host_bytes);
    bool started = false;
    switch (sub_api) {
    case AV_EXECUTOR_SUB_API_OPENCL:
        opencl_[slot].object().use_vram(apis_->vram_ledger(gpu.index()), apis_->host_ram_ledger());
        opencl_[slot].object().use_freed(apis_->cl_freed(gpu.index()));
        started = opencl_[slot].object().initialize(apis_->opencl_device(gpu.index()), apis_->gl_display(gpu.index()),
                                                    share, sessions_[slot].object().imports());
        break;
    case AV_EXECUTOR_SUB_API_OPENGL:
        if (group != nullptr) {
            opengl_[slot].object().use_vram(apis_->vram_ledger(gpu.index()), apis_->host_ram_ledger());
            started = opengl_[slot].object().initialize(apis_->gl_display(gpu.index()), &options, share, group);
        }
        break;
    case AV_EXECUTOR_SUB_API_D3D: {
        constexpr uint32_t renew_wait_ms = 2000u;
        D3d11HostDevice *const device11 = apis_->d3d11_device(gpu.index());
        if (device11 == nullptr || !device11->renew(renew_wait_ms)) {
            refusals_.refuse(ContextRefusals::Reason::d3d11_device, api_name(sub_api), device11 != nullptr ? 1u : 0u);
            return false;
        }
        started = d3d_[slot].object().initialize(shares_[slot], sessions_[slot].object().imports(), device11,
                                                 apis_->d3d12_device(gpu.index()), apis_->surfaces(gpu.index()),
                                                 apis_->d3d9_device(gpu.index()));
        break;
    }
    default:
        vulkan_[slot].object().use_vram(apis_->vram_ledger(gpu.index()), apis_->host_ram_ledger());
        vulkan_[slot].object().initialize(gpu, apis_->memory_source(gpu.index()), sessions_[slot].object().imports());
        started = true;
        break;
    }
    if (!started) {
        const HostRamLedger *const ram = apis_->host_ram_ledger();
        refusals_.refuse(ContextRefusals::Reason::bind, api_name(sub_api), ram != nullptr ? ram->tight_refusals() : 0u);
    }
    return started;
}

void ExecutorRoot::close(ContextSession *session)
{
    AV_SCOPE("ExecutorRoot::close");
    if (!owns(session)) return;
    const uint32_t slot = slot_of(session);
    session->bind_signal(nullptr, nullptr);
    uint64_t marked = 0u;
    uint64_t reached = 0u;
    session->gpu_progress(marked, reached);
    lock_.acquire();
    closing_[slot] = true;
    reaping_[slot] = marked == reached;
    lock_.release();
    if (marked == reached) {
        TeardownBudget idle(clock_, 0u);
        const bool done = finish(slot, idle);
        lock_.acquire();
        reaping_[slot] = false;
        lock_.release();
        if (done) return;
    }
    if (note_wedged()) return;
    TeardownBudget budget(clock_, teardown_nanoseconds);
    finish_slot(slot, budget);
}

void ExecutorRoot::finish_slot(uint32_t slot, TeardownBudget &budget)
{
    AV_SCOPE("ExecutorRoot::finish_slot");
    if (finish(slot, budget)) return;
    sessions_[slot].object().let_go_of_thread();
    lock_.acquire();
    wedged_[slot] = true;
    wedged_since_[slot] = clock_.now_nanoseconds();
    closing_[slot] = false;
    apis_of_[slot] = 0u;
    lock_.release();
    static_cast<void>(note_wedged());
}

bool ExecutorRoot::finish(uint32_t slot, TeardownBudget &budget)
{
    AV_SCOPE("ExecutorRoot::finish");
    ContextSession &session = sessions_[slot].object();
    if (!session.drain(budget)) return false;
    shares_[slot].end();
    if (!session.release(budget)) return false;
    lock_.acquire();
    const uint32_t gpu = gpus_of_[slot];
    wedged_[slot] = false;
    closing_[slot] = false;
    apis_of_[slot] = 0u;
    gpus_of_[slot] = 0u;
    release_group(slot);
    session.vacate();
    exhaustion_logged_ = false;
    lock_.release();
    retire(gpu);
    return true;
}

void ExecutorRoot::reap_wedged()
{
    AV_SCOPE("ExecutorRoot::reap_wedged");
    lock_.acquire();
    const bool over = stopping_;
    lock_.release();
    if (over) return;
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        lock_.acquire();
        const bool claimed = wedged_[slot] && !reaping_[slot];
        if (claimed) reaping_[slot] = true;
        lock_.release();
        if (!claimed) continue;
        TeardownBudget budget(clock_, 0u);
        if (!finish(slot, budget)) {
            sessions_[slot].object().let_go_of_thread();
            if (leakable(slot)) leak_slot(slot);
        }
        const ThreadLockHold held(lock_);
        reaping_[slot] = false;
    }
}

bool ExecutorRoot::leakable(uint32_t slot)
{
    AV_SCOPE("ExecutorRoot::leakable");
    lock_.acquire();
    const uint64_t age = clock_.now_nanoseconds() - wedged_since_[slot];
    lock_.release();
    return (age >= lost_leak_after_ && sessions_[slot].object().lost()) || age >= stuck_leak_nanoseconds;
}

void ExecutorRoot::leak_slot(uint32_t slot)
{
    AV_SCOPE("ExecutorRoot::leak_slot");
    log_.sessions().write("a session whose host device is lost never drained; its host objects are leaked and its slot reused");
    shares_[slot].leak();
    vulkan_[slot].forget();
    opencl_[slot].forget();
    opengl_[slot].forget();
    d3d_[slot].forget();
    static_cast<void>(vulkan_[slot].construct(log_.vulkan()));
    static_cast<void>(opencl_[slot].construct(log_.opencl()));
    static_cast<void>(opengl_[slot].construct(log_.opengl()));
    static_cast<void>(d3d_[slot].construct(log_));
    lock_.acquire();
    wedged_[slot] = false;
    closing_[slot] = false;
    apis_of_[slot] = 0u;
    gpus_of_[slot] = 0u;
    release_group(slot);
    sessions_[slot].forget();
    static_cast<void>(sessions_[slot].construct(log_.sessions()));
    lock_.release();
}

void ExecutorRoot::reap_closing()
{
    AV_SCOPE("ExecutorRoot::reap_closing");
    for (;;) {
        lock_.acquire();
        uint32_t claimed = max_sessions;
        for (uint32_t slot = 0u; slot < max_sessions && claimed == max_sessions && !stopping_; slot++)
            if (closing_[slot] && !reaping_[slot]) claimed = slot;
        if (claimed != max_sessions) reaping_[claimed] = true;
        lock_.release();
        if (claimed == max_sessions) return;
        TeardownBudget budget(clock_, teardown_nanoseconds);
        finish_slot(claimed, budget);
        const ThreadLockHold held(lock_);
        reaping_[claimed] = false;
    }
}

uint32_t ExecutorRoot::closing_here() const
{
    AV_SCOPE("ExecutorRoot::closing_here");
    uint32_t leaving = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++)
        if (closing_[slot]) leaving++;
    return leaving;
}

uint32_t ExecutorRoot::unclaimed_closing_here() const
{
    AV_SCOPE("ExecutorRoot::unclaimed_closing_here");
    uint32_t waiting = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++)
        if (closing_[slot] && !reaping_[slot]) waiting++;
    return waiting;
}

uint32_t ExecutorRoot::closing() const
{
    AV_SCOPE("ExecutorRoot::closing");
    const ThreadLockHold held(lock_);
    return closing_here();
}

void ExecutorRoot::retry_wedged()
{
    AV_SCOPE("ExecutorRoot::retry_wedged");
    reap_wedged();
}

uint32_t ExecutorRoot::wedged() const
{
    AV_SCOPE("ExecutorRoot::wedged");
    const ThreadLockHold held(lock_);
    return wedged_here();
}

uint32_t ExecutorRoot::wedged_here() const
{
    AV_SCOPE("ExecutorRoot::wedged_here");
    uint32_t stuck = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++)
        if (wedged_[slot]) stuck++;
    return stuck;
}

bool ExecutorRoot::note_wedged()
{
    AV_SCOPE("ExecutorRoot::note_wedged");
    const ThreadLockHold held(lock_);
    if (stopping_) return false;
    if (!watching_ && pthread_create(&watcher_, nullptr, &run_wedged_watch, this) == 0) {
        watching_ = true;
        WatcherExit::process().arm(*this);
    }
    if (!watching_) return false;
    while (helpers_running_ < teardown_workers && helpers_running_ + 1u < closing_here() &&
           pthread_create(&helpers_[helpers_running_], nullptr, &run_closing_watch, this) == 0)
        helpers_running_++;
    wake_.broadcast();
    return true;
}

void ExecutorRoot::watch_wedged()
{
    AV_SCOPE("ExecutorRoot::watch_wedged");
    lock_.acquire();
    while (!stopping_) {
        if (unclaimed_closing_here() != 0u) {
            lock_.release();
            reap_closing();
            lock_.acquire();
            continue;
        }
        if (wedged_here() == 0u) {
            wake_.wait();
            continue;
        }
        const timespec until = clock_.ahead(retry_wedged_nanoseconds);
        static_cast<void>(wake_.wait_until(until));
        if (stopping_) break;
        lock_.release();
        reap_wedged();
        lock_.acquire();
    }
    lock_.release();
}

void ExecutorRoot::watch_closing()
{
    AV_SCOPE("ExecutorRoot::watch_closing");
    lock_.acquire();
    while (!stopping_) {
        if (unclaimed_closing_here() == 0u) {
            wake_.wait();
            continue;
        }
        lock_.release();
        reap_closing();
        lock_.acquire();
    }
    lock_.release();
}

uint64_t ExecutorRoot::imported_handle(ContextSession *session, uint64_t blob_id)
{
    AV_SCOPE("ExecutorRoot::imported_handle");
    return owns(session) ? session->imported_handle(blob_id) : 0u;
}

uint32_t ExecutorRoot::share_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                                    uint64_t wire_handle)
{
    AV_SCOPE("ExecutorRoot::share_object");
    const uint32_t slot = slot_of(session);
    if (slot == max_sessions || session->faulted()) return AV_SHARED_REFUSED;
    return shared_answer(shares_[slot].publish(object_id, kind, size, wire_handle));
}

uint32_t ExecutorRoot::open_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                                   uint64_t into, uint64_t &wire_handle)
{
    AV_SCOPE("ExecutorRoot::open_object");
    wire_handle = 0u;
    const uint32_t slot = slot_of(session);
    if (slot == max_sessions || session->faulted()) return AV_SHARED_REFUSED;
    return shared_answer(shares_[slot].open(object_id, kind, size, into, wire_handle));
}

uint32_t ExecutorRoot::shared_answer(Status answered) const
{
    AV_SCOPE("ExecutorRoot::shared_answer");
    switch (answered) {
    case Status::ok: return AV_SHARED_DONE;
    case Status::not_found: return AV_SHARED_UNKNOWN;
    case Status::invalid_parameter: return AV_SHARED_MISMATCHED;
    case Status::conflict: return AV_SHARED_CONFLICT;
    default: return AV_SHARED_REFUSED;
    }
}

void ExecutorRoot::close_object(ContextSession *session, uint64_t object_id, uint64_t wire_handle)
{
    AV_SCOPE("ExecutorRoot::close_object");
    const uint32_t slot = slot_of(session);
    if (slot != max_sessions) shares_[slot].close(object_id, wire_handle);
}

void ExecutorRoot::set_signal(ContextSession *session, void (*signal)(void *context), void *context)
{
    AV_SCOPE("ExecutorRoot::set_signal");
    if (owns(session)) session->bind_signal(signal, context);
}

uint32_t ExecutorRoot::lose_device(ContextSession *session)
{
    AV_SCOPE("ExecutorRoot::lose_device");
    return owns(session) ? session->lose_device() : AV_UNKNOWN;
}

void ExecutorRoot::report(StateWriter &writer)
{
    AV_SCOPE("ExecutorRoot::report");
    writer.open("executor", 0u);
    writer.number("sessions", max_sessions);
    writer.number("live", live());
    writer.number("gpus", gpus_.count());
    writer.flag("overflowed", gpus_.overflowed());
    HostRamLedger *const host_ram = apis_ != nullptr ? apis_->host_ram_ledger() : nullptr;
    if (host_ram != nullptr) {
        writer.number("host_ram_cap_bytes", host_ram->cap());
        writer.number("host_ram_used_bytes", host_ram->used());
        writer.number("host_ram_refusals", host_ram->refusals());
        writer.number("host_ram_floor_bytes", host_ram->floor());
        writer.number("host_ram_tight_refusals", host_ram->tight_refusals());
    }
    gpus_.report(writer);
    domains_.report(writer);
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (!prepared_[slot] || !sessions_[slot].object().live()) continue;
        writer.open("session", slot);
        writer.text("api", api_name(apis_of_[slot]));
        writer.number("gpu", gpus_of_[slot]);
        writer.number("domain", shares_[slot].domain());
        writer.flag("wedged", wedged_[slot]);
        writer.flag("closing", closing_[slot]);
        sessions_[slot].object().report_channel(writer);
        writer.close();
    }
    for (uint32_t index = 0u; index < gpus_.count(); index++) {
        writer.open("gpu_shares", index);
        D3d11HostDevice *const  device = apis_->d3d11_device(index);
        HostMemorySource *const source = apis_->memory_source(index);
        apis_->shares(index)->report(writer);
        if (device != nullptr) device->report(writer);
        if (source != nullptr) source->report(writer);
        writer.close();
    }
    writer.close();
}

const char *ExecutorRoot::api_name(uint32_t sub_api) const
{
    AV_SCOPE("ExecutorRoot::api_name");
    switch (sub_api) {
    case AV_EXECUTOR_SUB_API_VULKAN: return "vulkan";
    case AV_EXECUTOR_SUB_API_OPENCL: return "opencl";
    case AV_EXECUTOR_SUB_API_OPENGL: return "opengl";
    case AV_EXECUTOR_SUB_API_D3D: return "d3d";
    default: return "unknown";
    }
}

uint32_t ExecutorRoot::live() const
{
    AV_SCOPE("ExecutorRoot::live");
    uint32_t running = 0u;
    for (uint32_t slot = 0u; slot < max_sessions; slot++) {
        if (prepared_[slot] && sessions_[slot].object().live()) running++;
    }
    return running;
}

bool ExecutorRoot::owns(const ContextSession *session) const
{
    AV_SCOPE("ExecutorRoot::owns");
    return slot_of(session) != max_sessions;
}

uint32_t ExecutorRoot::slot_of(const ContextSession *session) const
{
    AV_SCOPE("ExecutorRoot::slot_of");
    for (uint32_t slot = 0u; slot < max_sessions; slot++)
        if (&sessions_[slot].object() == session) return slot;
    return max_sessions;
}

const HostGpu *ExecutorRoot::gpu_of(uint32_t domain) const
{
    AV_SCOPE("ExecutorRoot::gpu_of");
    uint32_t index = 0u;
    return apis_ != nullptr && domains_.gpu_of(domain, index) ? gpus_.at(index) : nullptr;
}

void ExecutorRoot::retire(uint32_t gpu)
{
    AV_SCOPE("ExecutorRoot::retire");
    D3d11HostDevice *const device = apis_->d3d11_device(gpu);
    if (device != nullptr) device->retire_shares();
}

bool ExecutorRoot::invariants_hold() const
{
    AV_SCOPE("ExecutorRoot::invariants_hold");
    return gpus_.invariants_hold() && (apis_ != nullptr || gpus_.count() == 0u);
}

}
