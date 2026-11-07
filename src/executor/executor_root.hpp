#ifndef AV_EXECUTOR_ROOT_HPP
#define AV_EXECUTOR_ROOT_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_reporter.hpp"
#include "device/shim/posix_condition.hpp"
#include "device/shim/posix_lock.hpp"
#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/executor_domains.hpp"
#include "executor/gpu_clock_hold.hpp"
#include "executor/host_gpu_set.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/opengl/gl_memory_group.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/vulkan/vulkan_backend.hpp"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLog;
class SurfacePoint;
class TeardownBudget;

class HostApis;
class HostMemorySource;
class HostGpu;
class SessionBackend;

class ExecutorRoot final : public StateReporter {
public:
    static constexpr uint32_t sessions = 64u;
    static constexpr uint64_t teardown_nanoseconds = 1000000000u;
    static constexpr uint64_t retry_wedged_nanoseconds = 250000000u;

    explicit ExecutorRoot(ExecutorLog &log);
    ~ExecutorRoot();
    ExecutorRoot(const ExecutorRoot &) = delete;
    ExecutorRoot &operator=(const ExecutorRoot &) = delete;

    [[nodiscard]] Status initialize(HostApis &apis);

    uint32_t sub_apis() const;
    uint32_t open_domain(const uint8_t *gpu_uuid);
    void     close_domain(uint32_t domain);
    [[nodiscard]] Status allocate_memory(uint32_t domain, uint64_t key, uint64_t size, uint32_t request,
                                         AvHostMemory &memory);
    void                 free_memory(uint32_t domain, uint64_t key);
    [[nodiscard]] Status alias_memory(uint32_t domain, uint64_t key, uint32_t from_domain, uint64_t from_key,
                                      AvHostMemory &memory);
    bool     export_image(uint32_t domain, uint64_t id, AvExportedImage &image);
    bool     compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                            uint32_t height, AvExportedImage &image);
    void     release_shared(uint32_t domain, uint64_t id);
    bool     read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width, uint32_t height);
    bool     map_frame(uint32_t domain, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view);
    void     unmap_frame(uint32_t domain, const SurfacePoint &frame);
    bool     export_surface(uint32_t domain, uint64_t surface, uint32_t flags, AvExportedImage &image);
    bool     sync_surface(uint32_t domain, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd);

    ContextSession *open(uint32_t sub_api, const AvContextOptions &options, ContextSession *share = nullptr);
    void            close(ContextSession *session);
    uint64_t        imported_handle(ContextSession *session, uint64_t blob_id);
    void            set_signal(ContextSession *session, void (*signal)(void *context), void *context);
    uint32_t        lose_device(ContextSession *session);
    uint32_t share_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size,
                          uint64_t wire_handle);
    uint32_t open_object(ContextSession *session, uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                         uint64_t &wire_handle);
    void                 close_object(ContextSession *session, uint64_t object_id, uint64_t wire_handle);
    uint32_t        live() const;
    uint32_t        wedged() const;
    void            retry_wedged();
    void            watch_wedged();
    void            stop_watching();
    void            report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    friend struct ExecutorRootProbe;
    friend class WatcherExit;

    static constexpr uint32_t max_sessions = sessions;

    SessionBackend *backend_in(uint32_t slot, uint32_t sub_api);
    [[nodiscard]] bool bind(uint32_t slot, uint32_t sub_api, const HostGpu &gpu, const AvContextOptions &options,
                            const OpenGlBackend *share, GlMemoryGroup *group);
    GlMemoryGroup *reserve_group(const ContextSession *share, uint32_t slot);
    void           release_group(uint32_t slot);
    void           note_wedged();
    void           halt_watcher();
    const HostGpu *gpu_of(uint32_t domain) const;
    HostMemorySource *source_of(uint32_t domain) const;
    uint32_t       slot_of(const ContextSession *session) const;
    bool           finish(uint32_t slot, TeardownBudget &budget);
    void           reap_wedged();
    uint32_t       wedged_here() const;
    uint32_t       shared_answer(Status answered) const;
    void           retire(uint32_t gpu);
    const OpenGlBackend *sharing(const ContextSession *share, const HostGpu &gpu) const;
    bool owns(const ContextSession *session) const;
    const char *api_name(uint32_t sub_api) const;

    HostGpuSet      gpus_;
    ExecutorDomains domains_;
    GpuClockHold   clocks_[HostGpuSet::max_gpus];
    GlMemoryGroup  memory_groups_[max_sessions];
    uint32_t       group_of_[max_sessions];
    uint32_t       group_refs_[max_sessions];
    ObjectStorage<ContextSession> sessions_[max_sessions];
    ObjectStorage<VulkanBackend>  vulkan_[max_sessions];
    ObjectStorage<OpenClBackend>  opencl_[max_sessions];
    ObjectStorage<OpenGlBackend>  opengl_[max_sessions];
    ObjectStorage<D3dBackend>     d3d_[max_sessions];
    SessionShares  shares_[max_sessions];
    uint32_t       apis_of_[max_sessions];
    uint32_t       gpus_of_[max_sessions];
    bool           wedged_[max_sessions];
    bool           reaping_[max_sessions];
    mutable PosixLock lock_;
    PosixCondition wake_;
    pthread_t      watcher_;
    bool           watching_;
    bool           stopping_;
    MonotonicClock clock_;
    HostApis      *apis_;
};

}

#endif
