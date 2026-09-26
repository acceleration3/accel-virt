/* The executor C table for tests: every command runs and echoes its payload as
 * its answer, except the ids a test scripts to be unknown, undecodable, lose
 * the device, answer more than the capacity, or answer a value no library may.
 * It records every executor created and destroyed, every command executed, and
 * calls that leave the thread their executor was created on. */
#ifndef AV_MOCK_EXECUTOR_OPS_HPP
#define AV_MOCK_EXECUTOR_OPS_HPP

#include "device/abi/rvk_executor.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <pthread.h>

namespace av {

class MockHost;

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

    const RvkExecutorOps &table() const;
    RvkExecutorOps       &editable();

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
    /* Version 4's creates, what the last one was asked, and whether its share
     * was an executor of this table still alive when that create returned. */
    std::uint32_t     option_creates();
    /* What the device gave this library to call when a host thread finishes
     * work (executor ABI version 5), and calling it: the one thing in the
     * table that runs device-ward, so a test fires it rather than waits. */
    std::uint32_t     signals_set();
    bool              signal_bound() const;
    /* Calls it, as a host API's own thread would. False when nothing is
     * bound. */
    bool              fire_signal();
    RvkContextOptions last_options();
    RvkExecutor      *last_share();
    bool              last_share_live();
    bool              is_live(RvkExecutor *executor);
    /* The Nth create from now answers NULL (either spelling). */
    void          fail_create_at(std::uint32_t nth);
    std::uint32_t created();
    std::uint32_t destroyed();
    std::uint32_t live();
    std::uint32_t executed();
    /* Commands executed, in order, across executors. */
    std::uint32_t id_at(std::uint32_t index);
    std::uint32_t size_at(std::uint32_t index);
    /* The sub-API and GPU UUID's first byte of the last create. */
    std::uint32_t last_sub_api();
    std::uint8_t  last_uuid_byte();
    /* Calls on an executor this table never made or already destroyed, and
     * calls from another thread than the one that created it. */
    std::uint32_t misuses();
    /* What version 2's import_kinds answers, for every sub-API and GPU. */
    void          set_import_kinds(std::uint32_t kinds);
    /* Blobs imported and released, so a test can prove destroy released what
     * the guest never did. */
    std::uint32_t imported();
    std::uint32_t released();
    /* The Nth import_blob from now answers RVK_UNDECODABLE and records nothing. */
    void          refuse_import_at(std::uint32_t nth);
    /* The Nth imported_handle from now answers 0, a library that imported and
     * cannot say what it made. */
    void          withhold_handle_at(std::uint32_t nth);
    /* Every imported_handle answers `handle` instead of its own - one handle
     * for two imports is a library breaking the ABI; 0 goes back to its own. */
    void          answer_handle(std::uint64_t handle);
    /* What a live import was handed, by the id the device chose: false when
     * there is none. */
    bool          import_of(std::uint64_t blob_id, RvkBlobHandle &handle);
    std::uint32_t live_imports();
    /* The handle imported_handle answers for a live import of `blob_id`, unless
     * a test scripted another. */
    static constexpr std::uint64_t handle_bit = 0x4000000000000000u;

    /* Version 6's shared objects (feature 053). A descriptor this table
     * exports is a real one, taken from `host` so the device that closes it is
     * the one counting it; without a host, every export is refused, which is
     * what a library that exports nothing looks like. */
    void          descriptors_from(MockHost &host);
    /* The abi_version the table reports, so a test can present a library from
     * before a version it needs. Back to RVK_EXECUTOR_ABI_VERSION with 0. */
    void          set_abi_version(std::uint32_t version);
    /* The RVK_BLOB_* kind an export says its descriptor is. */
    void          export_handle_kind(std::uint32_t kind);
    /* The Nth export from now answers success with nothing behind it - no
     * descriptor and no kind - which is a library the device must not
     * believe. */
    void          lie_on_export_at(std::uint32_t nth);
    /* The Nth export, or import, of an object from now is refused. */
    void          refuse_export_at(std::uint32_t nth);
    void          refuse_object_import_at(std::uint32_t nth);
    /* The Nth object import from now answers RVK_EXECUTED naming nothing, a
     * library that imported and cannot say what it made. */
    void          withhold_object_handle_at(std::uint32_t nth);
    std::uint32_t exported_objects();
    std::uint32_t imported_objects();
    std::uint32_t released_objects();
    std::uint32_t live_objects();
    /* The guest's shared releases the library was told of (ABI 12): how many,
     * and the last one's id and GPU. */
    std::uint32_t        shared_releases();
    std::uint64_t        last_shared_release();
    const std::uint8_t *last_shared_gpu();
    /* What a live imported object was made of: false when there is none. */
    bool          object_of(std::uint64_t wire_handle, std::uint32_t &kind, std::uint32_t &handle_kind,
                            std::uint64_t &size, int &fd);
    /* The handle an object import answers, above every blob handle. */
    static constexpr std::uint64_t object_bit = 0x2000000000000000u;

private:
    struct Executor {
        pthread_t thread;
        bool      live;
    };
    struct Import {
        std::uint64_t blob_id;
        RvkBlobHandle handle;
        bool          live;
    };
    struct SharedObject {
        std::uint64_t wire_handle;
        std::uint32_t kind;
        std::uint32_t handle_kind;
        std::uint64_t size;
        int           fd;
        bool          live;
    };
    static constexpr std::uint32_t max_imports = 512u;
    static constexpr std::uint32_t max_objects = 64u;

    Executor *executor_of(RvkExecutor *executor);
    bool      alive(RvkExecutor *executor);

    RvkExecutorOps             table_;
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
    std::uint8_t               last_uuid_byte_;
    std::atomic<std::uint32_t> misuses_;
    Import                     imports_[max_imports];
    std::uint32_t              refuse_import_;
    std::uint32_t              withhold_handle_;
    std::uint64_t              answered_handle_;
    std::uint32_t              option_creates_;
    std::uint32_t              signals_set_;
    void                     (*signal_)(void *context);
    void                      *signal_context_;
    RvkContextOptions          last_options_;
    RvkExecutor               *last_share_;
    bool                       last_share_live_;
    bool                       creates_held_;
    std::uint32_t              creates_waiting_;
    MockHost                  *descriptors_;
    std::uint32_t              export_handle_kind_;
    std::uint32_t              refuse_export_;
    std::uint32_t              lie_on_export_;
    std::uint32_t              refuse_object_import_;
    std::uint32_t              withhold_object_handle_;
    std::uint32_t              exported_objects_;
    std::uint32_t              imported_objects_;
    std::uint32_t              released_objects_;
    std::uint32_t              shared_releases_ = 0u;
    std::uint64_t              last_shared_release_ = 0u;
    const std::uint8_t        *last_shared_gpu_ = nullptr;
    std::uint64_t              next_object_;
    SharedObject               objects_[max_objects];
};

}  // namespace av

#endif  // AV_MOCK_EXECUTOR_OPS_HPP
