/* The adversarial host. Threads are real pthreads, so the worker tests run the
 * code that will run in QEMU under ASan and UBSan; the thread that constructed
 * the mock is the main loop. It records every contract violation, fails any
 * create call on request, and fails the test when it is destroyed with an
 * unclaimed violation, a live thread, event or mutex, or main-loop work never
 * run. */
#ifndef AV_MOCK_HOST_HPP
#define AV_MOCK_HOST_HPP

#include "device/host/host.hpp"
#include "harness/host/host_violation.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstddef>
#include <cstdint>
#include <pthread.h>

namespace av {

class MockHost final : public Host {
public:
    static constexpr std::uint32_t no_gpu = 0xFFFFFFFFu;
    static constexpr std::uint32_t max_threads = 128u;
    static constexpr std::uint32_t max_events = 256u;
    static constexpr std::uint32_t max_mutexes = 256u;
    static constexpr std::uint32_t max_gpus = 8u;
    static constexpr std::uint32_t max_main_loop_work = 1024u;
    static constexpr std::uint32_t max_doorbells = 64u;

    MockHost();
    ~MockHost();
    MockHost(const MockHost &) = delete;
    MockHost &operator=(const MockHost &) = delete;

    void     log(const char *message) override;
    void     require_main_loop(const char *function) override;
    void     require_worker(const char *function) override;
    Status   start_thread(HostRoutine entry, void *context, void *&thread) override;
    void     join_thread(void *thread) override;
    Status   create_event(void *&event) override;
    void     destroy_event(void *event) override;
    void     signal_event(void *event) override;
    void     wait_event(void *event) override;
    Status   create_mutex(void *&mutex) override;
    void     destroy_mutex(void *mutex) override;
    void     lock_mutex(void *mutex) override;
    void     unlock_mutex(void *mutex) override;
    bool     try_lock_mutex(void *mutex) override;
    void     run_on_main_loop(HostRoutine routine, void *context) override;
    std::uint32_t gpu_count() override;
    GpuIdentity gpu_identity(std::uint32_t index) override;
    void    *allocate(std::size_t bytes) override;
    void     release(void *block) override;
    void    *map_guest(std::uint64_t address, std::uint64_t &length, bool &direct) override;
    void     unmap_guest(void *pointer, std::uint64_t length) override;
    Status   reserve_range(std::uint64_t bytes, void *&base) override;
    Status   alias_guest_run(void *run, std::uint64_t bytes, void *at) override;
    void     release_range(void *base, std::uint64_t bytes) override;
    Status   dmabuf_over_guest(void *const *runs, const std::uint64_t *sizes, std::uint32_t count,
                               int &fd) override;
    void     close_dmabuf(int fd) override;
    void    *create_host_memory(std::uint64_t size, void *&pointer) override;
    void     destroy_host_memory(void *memory) override;
    std::uint64_t hostmem_size() override;
    Status   place_in_hostmem(void *memory, std::uint64_t offset) override;
    void     remove_from_hostmem(std::uint64_t offset, std::uint64_t size) override;
    std::uint64_t monotonic_nanoseconds() override;
    Status   read_hostmem(std::uint64_t offset, std::uint8_t *buffer, std::uint64_t size) override;
    Status   bind_gpu(std::uint32_t index) override;
    void    *export_gpu_memory(std::uint64_t size, void *&pointer, std::int32_t &host_error) override;
    void     release_gpu_memory(void *memory) override;
    void    *import_guest_memory(void *pointer, std::uint64_t size, std::int32_t &host_error) override;
    void     release_guest_memory(void *memory) override;
    Status   read_imported_memory(void *memory, std::uint64_t offset, std::uint8_t *buffer,
                                  std::uint64_t size) override;
    void     connect_doorbell(void *event, std::uint32_t index) override;
    void     disconnect_doorbell(void *event, std::uint32_t index) override;
    Status   connect_fence_interrupts() override;
    void     disconnect_fence_interrupts() override;
    void     raise_fence_interrupt(std::uint32_t line) override;
    std::size_t read_request(void *request, std::size_t offset, std::uint8_t *buffer, std::size_t size) override;
    void     respond(void *request, const std::uint8_t *bytes, std::size_t size) override;
    bool     resource_exists(std::uint32_t id) override;

    /* A discrete GPU named "mock gpu" with this UUID. */
    void add_gpu(const std::uint8_t *uuid);
    void add_gpu(const GpuIdentity &identity);
    /* Fail the Nth call from now (1-based); 0 disables. */
    void fail_thread_start_at(std::uint32_t nth);
    void fail_event_at(std::uint32_t nth);
    void fail_mutex_at(std::uint32_t nth);
    /* try_lock_mutex refuses `mutex` as if another thread held it. */
    void hold_mutex_elsewhere(void *mutex, bool held);
    void fail_allocation_at(std::uint32_t nth);
    void fail_host_memory_at(std::uint32_t nth);
    void fail_placement_at(std::uint32_t nth);
    /* The Nth export or import from now is refused, carrying `host_error` -
     * a VkResult, an errno, or 0 for a host with no number to give. */
    void fail_bind_gpu_at(std::uint32_t nth);
    /* The Nth read of the hostmem region from now answers bytes nothing wrote:
     * a region that maps something other than the block placed in it. */
    void corrupt_hostmem_read_at(std::uint32_t nth);
    /* The Nth export from now answers a handle with no mapping behind it, and
     * the Nth read of the region refuses outright: a host that says yes and
     * means no. */
    void empty_export_at(std::uint32_t nth);
    /* The Nth host memory block from now is a handle with no memory behind it. */
    void empty_host_memory_at(std::uint32_t nth);
    void refuse_hostmem_read_at(std::uint32_t nth);
    /* The Nth read through an import from now is refused outright. */
    void refuse_import_read_at(std::uint32_t nth);
    /* The scripted clock moves this many nanoseconds per reading, so a test
     * decides how long the work it measures took. */
    void advance_clock_per_read(std::uint64_t nanoseconds);
    void refuse_export_at(std::uint32_t nth, std::int32_t host_error);
    void refuse_import_at(std::uint32_t nth, std::int32_t host_error);
    /* The Nth read through an import from now answers bytes the device never
     * wrote: a host that imported something else. */
    void corrupt_import_read_at(std::uint32_t nth);
    /* The Nth guest mapping from now answers a pointer with nothing mapped, a
     * host that says yes and means no. */
    void empty_mapping_at(std::uint32_t nth);
    /* The Nth range reservation, or alias of a guest run, from now is refused. */
    void fail_reserve_at(std::uint32_t nth);
    void fail_alias_at(std::uint32_t nth);
    /* The nth dma-buf is refused, as a host with no udmabuf refuses them all. */
    void fail_dmabuf_at(std::uint32_t nth);
    /* Every dma-buf refused from here on, as a kernel with no udmabuf refuses
     * them: the caller then imports a host pointer, which is the path that has
     * to keep working on such a host. */
    void refuse_dmabufs();
    /* Dma-bufs made and not yet closed, so a test can say none leaked. */
    std::uint32_t live_dmabufs() const;
    /* A real descriptor of this process's, counted as a dma-buf, for a test
     * whose subject owns a descriptor it did not make itself - a shared
     * object's, which is exported by an executor on one context and closed by
     * the device when the last context lets it go (feature 053). -1 where the
     * kernel gives none. */
    int           open_descriptor();
    /* Whether `fd` is still open in this process, which is how a test says
     * that a descriptor was closed exactly once rather than counted down. */
    bool          descriptor_open(int fd) const;

    /* Guest memory another harness owns (the guest's MockPlatform in the
     * loopback suite): `translate` answers the host pointer of `length` bytes
     * at `address`, or null, and is asked before any guest RAM region. */
    using GuestTranslator = void *(*)(void *context, std::uint64_t address, std::uint64_t length);
    void          translate_guest_with(GuestTranslator translate, void *context);
    /* Guest RAM at [base, base + size), backed by the mock with a file, as a
     * shared memory backend is, so a run of it can be aliased; a page nothing
     * may map follows it, so no other region is ever adjacent. A mapping stops
     * at a region's end and at every split, as QEMU's does at a region boundary. */
    void          add_guest_ram(std::uint64_t base, std::size_t size);
    void          split_guest_memory_at(std::uint64_t address);
    /* Guest addresses QEMU would only reach through a bounce buffer. */
    void          add_bounce_region(std::uint64_t base, std::uint64_t size);
    std::uint8_t *guest_ram(std::uint64_t address);
    void          set_hostmem_size(std::uint64_t size);
    void          add_resource(std::uint32_t id);
    void          fail_fence_connect_at(std::uint32_t nth);
    bool          fence_interrupts_connected();
    /* Interrupts raised on fence queue `line`. */
    std::uint32_t raised(std::uint32_t line);
    /* A guest store to doorbell page `index`: whether an event was connected to it. */
    bool          ring_doorbell(std::uint32_t index);
    /* The device view of the block placed at `offset`, or null. */
    void         *placed_at(std::uint64_t offset);

    /* Waits up to `milliseconds` for at least `work` routines to be queued for
     * the main loop; false on timeout. */
    bool          wait_for_main_loop_work(std::uint32_t work, std::uint32_t milliseconds);
    /* Runs everything queued for the main loop, including work queued while it
     * runs, on the calling thread; answers how many routines ran. */
    std::uint32_t run_main_loop();

    /* Whether the calling thread is the one that constructed the mock. */
    bool          holds_main_loop() const;
    bool          claim(HostViolation kind);
    std::uint32_t unclaimed_violations();
    std::uint32_t logged();
    /* The last message logged, empty before the first, cut at its capacity. */
    const char   *last_log();
    std::uint32_t live_threads();
    std::uint32_t live_events();
    std::uint32_t live_mutexes();
    std::uint32_t live_allocations();
    std::uint32_t live_guest_mappings();
    std::uint32_t live_host_memory();
    /* The host GPU this device was bound to, or no_gpu. */
    std::uint32_t bound_gpu();
    /* Blocks exported from the host GPU, and windows imported into it. */
    std::uint32_t live_gpu_memory();
    std::uint32_t live_imports();
    std::uint32_t live_placements();
    std::uint32_t live_doorbells();
    /* Ranges reserved and not released, and the runs aliased into them. */
    std::uint32_t live_ranges();
    std::uint32_t live_aliases();

private:
    static constexpr std::uint32_t violation_kinds = static_cast<std::uint32_t>(HostViolation::count);

    struct Thread {
        pthread_t   handle;
        HostRoutine entry;
        void       *context;
        bool        live;
    };
    struct Event {
        pthread_mutex_t lock;
        pthread_cond_t  signalled;
        bool            set;
        bool            live;
    };
    struct Mutex {
        pthread_mutex_t lock;
        bool            locked;
        bool            elsewhere;
        bool            live;
    };
    struct MainLoopWork {
        HostRoutine routine;
        void       *context;
    };

    static constexpr std::uint32_t max_allocations = 2048u;
    static constexpr std::uint32_t max_guest_mappings = 20000u;
    static constexpr std::uint32_t max_regions = 8u;
    static constexpr std::uint32_t max_host_memory = 64u;
    static constexpr std::uint32_t max_imports = 16u;
    static constexpr std::uint32_t max_resources = 64u;
    static constexpr std::uint32_t max_ranges = 128u;
    static constexpr std::uint64_t page = 4096u;

    /* Runs one dma-buf may cover. A chunk scattered page by page is bounded
     * by GuestPageAlias::max_runs; this is the harness's own smaller bound,
     * since no test aliases more. */
    static constexpr std::uint32_t max_dmabuf_runs = 256u;

    struct GuestRegion {
        std::uint64_t base;
        std::uint64_t size;
        std::uint8_t *bytes;   /* null for a bounce region */
        int           fd;
        std::uint64_t mapped;  /* the file's size, whole pages */
    };
    struct GuestMapping {
        void         *pointer;
        std::uint64_t length;
        bool          bounce;
    };
    struct HostBlock {
        void         *pointer;
        std::uint64_t size;
        bool          live;
        bool          placed;
        std::uint64_t offset;
        bool          exported;   /* from the host GPU, released by release_gpu_memory */
    };
    struct Range {
        void         *base;
        std::uint64_t bytes;
        std::uint32_t aliases;
    };
    struct Import {
        std::uint8_t *pointer;
        std::uint64_t size;
        bool          live;
    };

    void    record(HostViolation kind, const char *function);
    GuestRegion *region_of(std::uint64_t address);
    /* The file-backed region holding all of [pointer, pointer + bytes), or null. */
    GuestRegion *region_holding(void *pointer, std::uint64_t bytes);
    Range       *range_holding(void *at, std::uint64_t bytes);
    bool         holds_range(void *at, std::uint64_t bytes);
    HostBlock   *block_of(void *memory);
    bool    fail_now(std::uint32_t &countdown);
    Thread *thread_of(void *thread);
    Event  *event_of(void *event);
    /* The page `event` is connected to, or max_doorbells. */
    std::uint32_t doorbell_of(void *event);
    Mutex  *mutex_of(void *mutex);

    pthread_t       main_loop_;
    pthread_mutex_t state_;
    pthread_cond_t  work_queued_;
    std::uint32_t   violations_[violation_kinds];
    std::uint32_t   logged_;
    char            last_log_[192];
    Thread          threads_[max_threads];
    Event           events_[max_events];
    Mutex           mutexes_[max_mutexes];
    GpuIdentity     gpus_[max_gpus];
    std::uint32_t   gpu_count_;
    MainLoopWork    work_[max_main_loop_work];
    std::uint32_t   work_count_;
    std::uint32_t   fail_thread_;
    std::uint32_t   fail_event_;
    std::uint32_t   fail_mutex_;
    std::uint32_t   fail_allocation_;
    std::uint32_t   fail_host_memory_;
    std::uint32_t   fail_placement_;
    std::uint32_t   empty_mapping_;
    void           *allocations_[max_allocations];
    GuestRegion     regions_[max_regions];
    std::uint32_t   region_count_;
    std::uint64_t   splits_[max_regions];
    std::uint32_t   split_count_;
    GuestMapping   *guest_mappings_;
    HostBlock       host_blocks_[max_host_memory];
    std::uint64_t   hostmem_size_;
    std::uint32_t   resources_[max_resources];
    std::uint32_t   resource_count_;
    void           *doorbells_[max_doorbells];
    GuestTranslator translate_;
    void           *translate_context_;
    bool            fence_connected_;
    std::uint32_t   fail_fence_connect_;
    std::uint32_t   raised_[max_doorbells];
    std::uint32_t   refuse_import_read_;
    std::uint32_t   empty_export_;
    std::uint32_t   empty_host_memory_;
    std::uint32_t   refuse_hostmem_read_;
    std::uint64_t   clock_;
    std::uint64_t   clock_step_;
    std::uint32_t   corrupt_hostmem_read_;
    std::uint32_t   fail_bind_gpu_;
    std::uint32_t   bound_gpu_;
    std::uint32_t   refuse_export_;
    std::int32_t    export_error_;
    std::uint32_t   refuse_import_;
    std::int32_t    import_error_;
    std::uint32_t   corrupt_import_read_;
    Import          imports_[max_imports];
    Range           ranges_[max_ranges];
    std::uint32_t   fail_reserve_;
    std::uint32_t   fail_alias_;
    std::uint32_t   fail_dmabuf_;
    bool            no_dmabufs_;
    std::uint32_t   dmabufs_;
};

}  // namespace av

#endif  // AV_MOCK_HOST_HPP
