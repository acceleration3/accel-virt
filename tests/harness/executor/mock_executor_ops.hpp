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

    void          hold();
    void          release();
    std::uint32_t waiting();
    void              hold_creates();
    void              release_creates();
    std::uint32_t     creates_waiting();
    std::uint32_t     option_creates();
    std::uint32_t     signals_set();
    bool              signal_bound() const;
    bool              fire_signal();
    void              lose_host_device();
    void              set_losable(bool losable);
    std::uint32_t     losses();
    AvContextOptions last_options();
    AvExecutor      *last_share();
    bool              last_share_live();
    void          fail_create_at(std::uint32_t nth);
    std::uint32_t created();
    std::uint32_t destroyed();
    std::uint32_t live();
    std::uint32_t executed();
    std::uint32_t id_at(std::uint32_t index);
    std::uint32_t size_at(std::uint32_t index);
    std::uint32_t last_sub_api();
    void          refuse_domain_at(std::uint32_t nth);
    std::uint32_t domains_opened();
    std::uint32_t domains_closed();
    std::uint32_t last_opened_domain();
    std::uint32_t last_closed_domain();
    std::uint8_t  last_domain_gpu();
    std::uint32_t last_vram_reserve_mib();
    std::uint32_t misuses();
    void          refuse_memory_at(std::uint32_t nth);
    void          withhold_memory_descriptor_at(std::uint32_t nth);
    void          shrink_memory_at(std::uint32_t nth);
    void          round_memory_to(std::uint64_t bytes);
    void          set_memory_caching(std::uint32_t caching);
    std::uint32_t memory_allocations();
    std::uint32_t memory_frees();
    std::uint32_t held_memory();
    std::uint32_t live_memory();
    struct MemoryRequest {
        std::uint32_t domain;
        std::uint64_t key;
        std::uint64_t size;
        std::uint32_t request;
    };
    MemoryRequest last_memory_request();
    std::uint8_t *memory_bytes(std::uint32_t domain, std::uint64_t key);
    int           answered_descriptor(std::uint32_t domain, std::uint64_t key);
    std::uint32_t imported();
    std::uint32_t last_role();
    std::uint64_t last_arena_base();
    std::uint32_t released();
    void          refuse_import_at(std::uint32_t nth);
    void          withhold_handle_at(std::uint32_t nth);
    void          answer_handle(std::uint64_t handle);
    bool          import_of(std::uint64_t blob_id, std::uint64_t &memory_key);
    std::uint32_t live_imports();
    static constexpr std::uint64_t handle_bit = 0x4000000000000000u;

    void          set_abi_version(std::uint32_t version);
    void          refuse_share_at(std::uint32_t nth);
    void          refuse_open_at(std::uint32_t nth);
    void          withhold_open_handle_at(std::uint32_t nth);
    std::uint32_t shared_objects();
    std::uint32_t opened_objects();
    std::uint32_t closed_objects();
    std::uint32_t live_opens();
    bool          opened_as(AvExecutor *executor, std::uint64_t object_id, std::uint64_t &wire_handle);
    void                 serve_frame(std::uint64_t surface, std::uint32_t point, const AvFrameView &view);
    std::uint32_t        frame_unmaps() const;
    void                 serve_surface(std::uint64_t surface, const AvExportedImage &image, std::uint64_t writes,
                                       int fence);
    std::uint32_t        last_domain() const;
    std::uint32_t        shared_releases();
    std::uint64_t        last_shared_release();
    static constexpr std::uint64_t object_bit = 0x2000000000000000u;

private:
    struct Executor {
        pthread_t     thread;
        std::uint32_t domain;
        bool          live;
    };
    struct Memory {
        std::uint32_t domain;
        std::uint64_t key;
        std::uint64_t size;
        int           fd;
        std::uint8_t *bytes;
        int           answered;
        bool          held;
        std::uint32_t imports;
    };
    struct Import {
        std::uint64_t blob_id;
        std::uint64_t memory_key;
        Memory       *memory;
        AvExecutor   *executor;
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
    static constexpr std::uint32_t max_imports = 8192u;
    static constexpr std::uint32_t max_memory = 256u;
    static constexpr std::uint32_t max_objects = 256u;
    static constexpr std::uint32_t domain_bit = 0x100u;

    Executor *executor_of(AvExecutor *executor);
    bool      alive(AvExecutor *executor);
    Memory   *held_memory_of(std::uint32_t domain, std::uint64_t key);
    void      settle(Memory &memory);
    bool      fires(std::uint32_t &countdown);
    std::uint32_t allocate(std::uint32_t domain, std::uint64_t key, std::uint64_t size, std::uint32_t request,
                           AvHostMemory &answer);
    void          release_import(Import &import);

    AvExecutorOps             table_;
    std::uint32_t              sub_apis_;
    std::uint32_t              imported_;
    std::uint32_t              last_role_;
    std::uint64_t              last_arena_base_;
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
    std::uint8_t              *kept_;
    std::size_t                kept_length_;
    std::uint32_t              ids_[journal_capacity];
    std::uint32_t              sizes_[journal_capacity];
    std::uint32_t              last_sub_api_;
    std::atomic<std::uint32_t> misuses_;
    Import                     imports_[max_imports];
    std::uint32_t              refuse_import_;
    std::uint32_t              withhold_handle_;
    std::uint64_t              answered_handle_;
    std::uint32_t              option_creates_;
    std::atomic<bool>          host_lost_{false};
    std::atomic<bool>          losable_{true};
    std::atomic<std::uint32_t> losses_{0u};
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
    std::uint32_t              last_vram_reserve_mib_ = 0u;
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
    Memory                     memory_[max_memory] = {};
    std::uint32_t              refuse_memory_ = 0u;
    std::uint32_t              withhold_memory_descriptor_ = 0u;
    std::uint32_t              shrink_memory_ = 0u;
    std::uint64_t              memory_rounding_ = 0u;
    std::uint32_t              memory_caching_ = AV_MEMORY_WRITE_BACK;
    std::uint32_t              memory_allocations_ = 0u;
    std::uint32_t              memory_frees_ = 0u;
    MemoryRequest              last_memory_request_ = {};
};

}

#endif
