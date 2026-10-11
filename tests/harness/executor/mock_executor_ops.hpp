/* The executor C table for tests: every command runs and echoes its payload as
 * its answer, except the ids a test scripts to be unknown, undecodable, lose
 * the device, answer more than the capacity, or answer a value no library may.
 * It records every executor created and destroyed, every command executed, and
 * calls that leave the thread their executor was created on. */
#ifndef AV_MOCK_EXECUTOR_OPS_HPP
#define AV_MOCK_EXECUTOR_OPS_HPP

#include "device/abi/av_executor.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <pthread.h>

namespace av {

class MockExecutorOps {
public:
    static constexpr std::uint32_t unknown_id = 0x1FFF0u;
    static constexpr std::uint32_t undecodable_id = 0x1FFF1u;
    static constexpr std::uint32_t lost_id = 0x1FFF2u;
    static constexpr std::uint32_t oversized_id = 0x1FFF3u;
    static constexpr std::uint32_t garbage_id = 0x1FFF4u;
    static constexpr std::uint32_t max_executors = 8u;
    static constexpr std::uint32_t journal_capacity = 256u;

    explicit MockExecutorOps(std::uint32_t sub_apis);
    ~MockExecutorOps();
    MockExecutorOps(const MockExecutorOps &) = delete;
    MockExecutorOps &operator=(const MockExecutorOps &) = delete;

    const AvExecutorOps &table() const;
    AvExecutorOps       &editable();

    /* While held, every command waits at the start of execute, so a test can
     * keep a worker busy; release lets them all go. */
    void          hold();
    void          release();
    /* Commands waiting behind a hold. */
    std::uint32_t waiting();
    /* While held, every create_with_options waits before it makes anything,
     * so a test can keep a context's creation open; release lets them go. */
    void              hold_creates();
    void              release_creates();
    std::uint32_t     creates_waiting();
    /* Creates, what the last one was asked - its domain included - and
     * whether its share was an executor of this table still alive when that
     * create returned. */
    std::uint32_t     option_creates();
    /* What the device gave this library to call when a host thread finishes
     * work, and calling it: the one thing in the table that runs
     * device-ward, so a test fires it rather than waits. */
    std::uint32_t     signals_set();
    bool              signal_bound() const;
    /* Calls it, as a host API's own thread would. False when nothing is
     * bound. */
    bool              fire_signal();
    AvContextOptions last_options();
    AvExecutor      *last_share();
    bool              last_share_live();
    /* The Nth create from now answers NULL. */
    void          fail_create_at(std::uint32_t nth);
    std::uint32_t created();
    std::uint32_t destroyed();
    std::uint32_t live();
    std::uint32_t executed();
    /* Commands executed, in order, across executors. */
    std::uint32_t id_at(std::uint32_t index);
    std::uint32_t size_at(std::uint32_t index);
    /* The sub-API of the last create. */
    std::uint32_t last_sub_api();
    /* Domains: the Nth open_domain from now answers 0, and what was opened,
     * closed, and on which GPU (its UUID's first byte) the last was opened. */
    void          refuse_domain_at(std::uint32_t nth);
    std::uint32_t domains_opened();
    std::uint32_t domains_closed();
    std::uint32_t last_closed_domain();
    std::uint8_t  last_domain_gpu();
    /* Calls on an executor this table never made or already destroyed, and
     * calls from another thread than the one that created it. */
    std::uint32_t misuses();
    /* What import_kinds answers, for every sub-API and GPU. */
    void          set_import_kinds(std::uint32_t kinds);
    /* Blobs imported and released, so a test can prove destroy released what
     * the guest never did. */
    std::uint32_t imported();
    std::uint32_t released();
    /* The Nth import_blob from now answers AV_UNDECODABLE and records nothing. */
    void          refuse_import_at(std::uint32_t nth);
    /* The Nth imported_handle from now answers 0, a library that imported and
     * cannot say what it made. */
    void          withhold_handle_at(std::uint32_t nth);
    /* Every imported_handle answers `handle` instead of its own - one handle
     * for two imports is a library breaking the ABI; 0 goes back to its own. */
    void          answer_handle(std::uint64_t handle);
    /* What a live import was handed, by the id the device chose: false when
     * there is none. */
    bool          import_of(std::uint64_t blob_id, AvBlobHandle &handle);
    std::uint32_t live_imports();
    /* The handle imported_handle answers for a live import of `blob_id`, unless
     * a test scripted another. */
    static constexpr std::uint64_t handle_bit = 0x4000000000000000u;

    /* Shared objects (feature 053), kept as the library keeps them: an id
     * shared in a domain by one executor, and opened by others, each holding
     * its own handle, until it closes or is destroyed. */
    /* The abi_version the table reports, so a test can present a library of
     * another version. Back to AV_EXECUTOR_ABI_VERSION with 0. */
    void          set_abi_version(std::uint32_t version);
    /* The Nth share, or open, from now is refused (AV_SHARED_REFUSED). */
    void          refuse_share_at(std::uint32_t nth);
    void          refuse_open_at(std::uint32_t nth);
    /* The Nth open from now answers done naming nothing, a library that
     * opened and cannot say what it made. */
    void          withhold_open_handle_at(std::uint32_t nth);
    std::uint32_t shared_objects();
    std::uint32_t opened_objects();
    std::uint32_t closed_objects();
    std::uint32_t live_opens();
    /* Whether `executor` holds `object_id` open, and as what. */
    bool          opened_as(AvExecutor *executor, std::uint64_t object_id, std::uint64_t &wire_handle);
    /* map_frame answers `view` for `surface` at `point` until it is unmapped. */
    void                 serve_frame(std::uint64_t surface, std::uint32_t point, const AvFrameView &view);
    std::uint32_t        frame_unmaps() const;
    /* export_surface answers `image` for `surface`, and sync_surface `writes`
     * and `fence` - neither descriptor looked at here; every other surface is
     * AV_UNKNOWN. */
    void                 serve_surface(std::uint64_t surface, const AvExportedImage &image, std::uint64_t writes,
                                       int fence);
    /* The domain the last frame, surface, image or release call named. */
    std::uint32_t        last_domain() const;
    /* The guest's shared releases the library was told of: how many, and the
     * last one's id. */
    std::uint32_t        shared_releases();
    std::uint64_t        last_shared_release();
    /* The handle an object open answers, above every blob handle. */
    static constexpr std::uint64_t object_bit = 0x2000000000000000u;

private:
    struct Executor {
        pthread_t thread;
        bool      live;
    };
    struct Import {
        std::uint64_t blob_id;
        AvBlobHandle handle;
        bool          live;
    };
    struct Share {
        std::uint64_t object_id;
        std::uint32_t domain;
        std::uint32_t kind;
        std::uint64_t size;
        AvExecutor   *publisher;
        bool          live;
    };
    struct Open {
        std::uint64_t object_id;
        AvExecutor   *executor;
        std::uint64_t wire_handle;
        bool          live;
    };
    static constexpr std::uint32_t max_imports = 512u;
    /* More than any per-context table the device kept, so a test can show
     * there is none now. */
    static constexpr std::uint32_t max_objects = 256u;
    static constexpr std::uint32_t domain_bit = 0x100u;

    Executor *executor_of(AvExecutor *executor);
    bool      alive(AvExecutor *executor);

    AvExecutorOps             table_;
    std::uint32_t              sub_apis_;
    std::uint32_t              import_kinds_;
    std::uint32_t              imported_;
    std::uint32_t              released_blobs_;
    pthread_mutex_t            lock_;
    pthread_cond_t             released_;
    bool                       held_;
    std::uint32_t              waiting_;
    Executor                   executors_[max_executors];
    std::uint32_t              fail_create_;
    std::uint32_t              created_;
    std::uint32_t              destroyed_;
    std::uint32_t              executed_;
    std::uint32_t              ids_[journal_capacity];
    std::uint32_t              sizes_[journal_capacity];
    std::uint32_t              last_sub_api_;
    std::atomic<std::uint32_t> misuses_;
    Import                     imports_[max_imports];
    std::uint32_t              refuse_import_;
    std::uint32_t              withhold_handle_;
    std::uint64_t              answered_handle_;
    std::uint32_t              option_creates_;
    std::uint32_t              signals_set_;
    void                     (*signal_)(void *context);
    void                      *signal_context_;
    AvContextOptions          last_options_;
    AvExecutor               *last_share_;
    bool                       last_share_live_;
    bool                       creates_held_;
    std::uint32_t              creates_waiting_;
    std::uint32_t              refuse_domain_ = 0u;
    std::uint32_t              domains_opened_ = 0u;
    std::uint32_t              domains_closed_ = 0u;
    std::uint32_t              last_closed_domain_ = 0u;
    std::uint8_t               last_domain_gpu_ = 0u;
    std::uint32_t              refuse_share_ = 0u;
    std::uint32_t              refuse_open_ = 0u;
    std::uint32_t              withhold_open_handle_ = 0u;
    std::uint32_t              shared_objects_ = 0u;
    std::uint32_t              opened_objects_ = 0u;
    std::uint32_t              closed_objects_ = 0u;
    std::uint32_t              shared_releases_ = 0u;
    std::uint64_t              last_shared_release_ = 0u;
    std::uint32_t              last_domain_ = 0u;
    std::uint64_t              frame_surface_ = 0u;
    std::uint32_t              frame_point_ = 0u;
    AvFrameView               frame_ = {};
    std::uint32_t              frame_unmaps_ = 0u;
    std::uint64_t              surface_ = 0u;
    AvExportedImage            surface_image_ = {};
    std::uint64_t              surface_writes_ = 0u;
    int                        surface_fence_ = -1;
    std::uint64_t              next_object_ = 0u;
    Share                      shares_[max_objects] = {};
    Open                       opens_[max_objects] = {};
};

}  // namespace av

#endif  // AV_MOCK_EXECUTOR_OPS_HPP
