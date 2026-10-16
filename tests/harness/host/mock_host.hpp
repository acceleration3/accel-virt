/* The adversarial host. Threads are real pthreads, so the worker tests run the
 * code that will run in QEMU under ASan and UBSan; the thread that constructed
 * the mock is the main loop. It records every contract violation, fails any
 * create call on request, and fails the test when it is destroyed with an
 * unclaimed violation, a live thread, event or mutex, or main-loop work never
 * run. */
#ifndef AV_MOCK_HOST_HPP
#define AV_MOCK_HOST_HPP

#include "device/abi/av_executor.h"
#include "device/host/host.hpp"
#include "harness/host/host_violation.hpp"
#include "proto/context/gpu_identity.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <pthread.h>

namespace av {

class RendererPeer;

class MockHost final : public Host {
public:
    static constexpr std::uint32_t no_gpu = 0xFFFFFFFFu;
    static constexpr std::uint32_t max_threads = 128u;
    static constexpr std::uint32_t max_events = 256u;
    static constexpr std::uint32_t max_mutexes = 256u;
    static constexpr std::uint32_t max_gpus = 8u;
    static constexpr std::uint32_t max_main_loop_work = 1024u;
    static constexpr std::uint32_t max_doorbells = 64u;
    /* The fence queues' interrupt lines, then the display's and the present
     * engine's. */
    static constexpr std::uint32_t interrupt_lines = max_doorbells + 2u;
    /* VIRTIO_GPU_MAX_SCANOUTS. */
    static constexpr std::uint32_t max_scanout_heads = 16u;

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
    /* Waits until signalled or until the scripted clock - moved by set_clock,
     * or by reads under advance_clock_per_read - reaches `deadline`. */
    bool     wait_event_until(void *event, std::uint64_t deadline) override;
    Status   create_mutex(void *&mutex) override;
    void     destroy_mutex(void *mutex) override;
    void     lock_mutex(void *mutex) override;
    void     unlock_mutex(void *mutex) override;
    bool     try_lock_mutex(void *mutex) override;
    void     run_on_main_loop(HostRoutine routine, void *context) override;
    std::uint32_t gpu_count() override;
    GpuIdentity gpu_identity(std::uint32_t index) override;
    Status   gpu_pci_address(std::uint32_t index, PciAddress &address) override;
    Status   read_file(const char *path, char *buffer, std::size_t capacity, std::size_t &length) override;
    /* The names directly under `path` of the files add_file was given. */
    Status   list_directory(const char *path, char *buffer, std::size_t capacity, std::size_t &length) override;
    void    *allocate(std::size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    void    *map_guest(std::uint64_t address, std::uint64_t &length, bool &direct) override;
    void     unmap_guest(void *pointer, std::uint64_t length) override;
    Status   reserve_range(std::uint64_t bytes, void *&base) override;
    Status   alias_guest_run(void *run, std::uint64_t bytes, void *at) override;
    /* As QEMU does: the run's bytes copied into the descriptor's pages, which
     * are then mapped over the run for real; a run outside every guest
     * region, or one not whole pages, is refused. Counted until restored. */
    Status   overlay_guest_run(void *run, std::uint64_t bytes, int fd, std::uint64_t offset) override;
    Status   restore_guest_run(void *run, std::uint64_t bytes) override;
    void     release_range(void *base, std::uint64_t bytes) override;
    std::uint64_t hostmem_size() override;
    /* Maps the descriptor's pages for real, as QEMU does with MAP_FIXED, so
     * what the guest reads at the offset (placed_at) is what the executor
     * wrote into its memory. */
    Status   place_in_hostmem(int fd, std::uint64_t size, std::uint64_t offset) override;
    void     remove_from_hostmem(std::uint64_t offset, std::uint64_t size) override;
    std::uint64_t monotonic_nanoseconds() override;
    /* Lets every store hold_store kept reach its memory, in the order held,
     * and reads the watched word as they land. */
    void     fence_stores() override;
    Status   bind_gpu(std::uint32_t index) override;
    void     connect_doorbell(void *event, std::uint32_t index) override;
    void     disconnect_doorbell(void *event, std::uint32_t index) override;
    Status   connect_fence_interrupts() override;
    void     disconnect_fence_interrupts() override;
    void     raise_fence_interrupt(std::uint32_t line) override;
    std::size_t read_request(void *request, std::size_t offset, std::uint8_t *buffer, std::size_t size) override;
    void     respond(void *request, const std::uint8_t *bytes, std::size_t size) override;
    bool     resource_exists(std::uint32_t id) override;
    Status   create_recording(const char *path, void *&file) override;
    Status   append_recording(void *file, const std::uint8_t *bytes, std::size_t size) override;
    Status   truncate_recording(void *file) override;
    void     close_recording(void *file) override;
    bool     display_takes_dmabuf(std::uint32_t head) override;
    Status   show_scanout(std::uint32_t head, const std::uint8_t *blob, const ScanoutFramebuffer &frame) override;
    void     update_scanout(std::uint32_t head, std::uint32_t x, std::uint32_t y, std::uint32_t width,
                            std::uint32_t height) override;
    void     hide_scanout(std::uint32_t head) override;
    /* Heads under add_display_heads show it; the rest answer not_supported. */
    Status   show_image(std::uint32_t head, const AvExportedImage &image) override;
    int      event_descriptor(void *event) override;
    int      fence_interrupt_descriptor(std::uint32_t line) override;
    std::uint32_t guest_region_count() override;
    Status   guest_region(std::uint32_t index, std::uint32_t &region, int &descriptor, std::uint64_t &size) override;
    Status   locate_guest(const void *pointer, std::uint64_t length, std::uint32_t &region,
                          std::uint64_t &offset) override;
    void     close_descriptor(int descriptor) override;
    Status   start_renderer(const char *path) override;
    Status   send_renderer(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                           std::uint32_t count) override;
    Status   receive_renderer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                              std::uint32_t capacity_descriptors, std::uint32_t &count,
                              std::uint32_t timeout_ms) override;
    void     kill_renderer() override;
    /* The export socket's connections, for real: a test hands the device one
     * end of a SOCK_SEQPACKET socketpair and is the consumer on the other. */
    Status   receive_export(int connection, std::uint8_t *buffer, std::size_t capacity, std::size_t &length) override;
    Status   send_export(int connection, const std::uint8_t *bytes, std::size_t length, int descriptor) override;
    void     shutdown_export(int connection) override;
    void     close_export(int descriptor) override;
    int      duplicate_export(int descriptor) override;

    /* The plane a head was told to show, field for field as it crossed the
     * boundary, so a test asserts the picture rather than that something was
     * shown. */
    struct Plane {
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t backing_width;
        std::uint32_t backing_height;
        std::uint32_t stride;
        std::uint32_t offset;
        std::uint32_t fourcc;
    };
    struct Scanout {
        bool                shown;
        const std::uint8_t *blob;   /* the device's mapping the head reads */
        Plane         plane;
        std::uint32_t shows;     /* times this head was pointed at something */
        std::uint32_t updates;
        std::uint32_t hides;
    };

    /* What the C table's show_scanout lands on, so the AvScanoutFrame the ABI
     * carries is asserted as it arrived rather than rebuilt. */
    [[nodiscard]] Status show_scanout_plane(std::uint32_t head, const std::uint8_t *blob, const Plane &plane);

    /* A discrete GPU named "mock gpu" with this UUID. */
    void add_gpu(const std::uint8_t *uuid);
    void add_gpu(const GpuIdentity &identity);
    /* What VK_EXT_pci_bus_info reports for a GPU. */
    struct PciBus {
        std::uint32_t domain;
        std::uint32_t bus;
        std::uint32_t device;
        std::uint32_t function;
    };
    /* GPU `index` reports VK_EXT_pci_bus_info at `bus`. */
    void place_gpu_on_pci(std::uint32_t index, const PciBus &bus);
    /* What the C table's gpu_properties carries: false for a GPU not on PCI. */
    bool gpu_bus(std::uint32_t index, PciBus &bus);
    /* A file read_file answers with `length` bytes of `text`, as sysfs would. */
    void add_file(const char *path, const char *text, std::size_t length);
    /* Fail the Nth call from now (1-based); 0 disables. */
    void fail_thread_start_at(std::uint32_t nth);
    void fail_event_at(std::uint32_t nth);
    void fail_mutex_at(std::uint32_t nth);
    /* try_lock_mutex refuses `mutex` as if another thread held it. */
    void hold_mutex_elsewhere(void *mutex, bool held);
    void fail_allocation_at(std::uint32_t nth);
    /* The Nth placement in the hostmem region from now is refused, as QEMU
     * refuses a dma-buf it cannot map. */
    void fail_placement_at(std::uint32_t nth);
    void fail_bind_gpu_at(std::uint32_t nth);
    /* Write-combining, as a CPU does it: the `size` bytes at `bytes` reach
     * `at` only at the next fence_stores, so a test sees what another
     * processor - the guest - could see before then. At most max_held_stores
     * of at most max_held_bytes each; one past either fails the test. */
    void          hold_store(void *at, const void *bytes, std::size_t size);
    /* The eight bytes at `word` are read whenever held stores land: what the
     * guest could already see when they did. */
    void          watch(const void *word);
    std::uint32_t store_fences();
    /* The watched word as the last held stores landed; 0 before any have. */
    std::uint64_t watched_when_landed();
    /* The scripted clock moves this many nanoseconds per reading, so a test
     * decides how long the work it measures took. */
    void advance_clock_per_read(std::uint64_t nanoseconds);
    /* The scripted clock reads `now` from here on, and every thread in
     * wait_event_until whose deadline that reaches wakes. */
    void set_clock(std::uint64_t now);
    /* The earliest deadline a thread in wait_event_until sleeps toward:
     * false when none does. */
    bool          next_deadline(std::uint64_t &deadline);
    /* Waits up to `milliseconds` for a thread to sleep in wait_event_until
     * toward a deadline past `after`: one woken at `after` that has done its
     * work and gone back to sleep. */
    bool          wait_for_deadline_after(std::uint64_t after, std::uint32_t milliseconds);
    /* Images show_image put on head `head`, and the last one. */
    std::uint32_t    images_shown(std::uint32_t head) const;
    AvExportedImage last_image(std::uint32_t head) const;
    /* The Nth guest mapping from now answers a pointer with nothing mapped, a
     * host that says yes and means no. */
    void empty_mapping_at(std::uint32_t nth);
    /* The Nth recording call of its kind from now is refused. */
    void fail_recording_create_at(std::uint32_t nth);
    void fail_recording_append_at(std::uint32_t nth);
    void fail_recording_truncate_at(std::uint32_t nth);
    /* What the recording at `path` holds, closed or not: null when nothing
     * was ever recorded there. */
    const std::uint8_t *recording_bytes(const char *path);
    std::size_t         recording_length(const char *path);

    /* The Nth range reservation, or alias of a guest run, from now is refused. */
    void fail_reserve_at(std::uint32_t nth);
    void fail_alias_at(std::uint32_t nth);
    /* The Nth overlay, or restore, of a guest run from now is refused. */
    void fail_overlay_at(std::uint32_t nth);
    void fail_restore_at(std::uint32_t nth);
    /* Guest pages a dma-buf lies over now. */
    std::uint64_t overlaid_bytes();
    /* Descriptors the mock handed the device - open_descriptor's, a guest
     * region's duplicate, a renderer's - not yet closed, so a test can say
     * none leaked. A descriptor the device got elsewhere (an executor's
     * dma-buf) is closed through close_descriptor too, and not counted. */
    std::uint32_t live_descriptors() const;
    /* A real descriptor of this process's, counted, for a test
     * whose subject owns a descriptor it did not make itself - a shared
     * object's, which is exported by an executor on one context and closed by
     * the device when the last context lets it go (feature 053). -1 where the
     * kernel gives none. */
    int           open_descriptor();
    /* Whether `fd` is still open in this process, which is how a test says
     * that a descriptor was closed exactly once rather than counted down. */
    bool          descriptor_open(int fd) const;

    /* Guest RAM as QEMU hands it to a renderer: region `region`, `size` bytes
     * this process sees at `pointer`, behind a descriptor the mock owns and
     * answers here. guest_region hands out a counted duplicate of it. A null
     * pointer is RAM with no shared file behind it, which guest_region
     * refuses. */
    int           add_guest_region(std::uint32_t region, void *pointer, std::uint64_t size);
    /* Where a renderer mapping `size` bytes of `descriptor` - any duplicate of
     * a region's - sees them: the region's pointer, or null for a descriptor
     * that is no region's or a size past the region. */
    void         *guest_region_mapping(int descriptor, std::uint64_t size);
    /* The Nth event_descriptor from now answers -1, as an event with no
     * eventfd behind it would. */
    void          fail_event_descriptor_at(std::uint32_t nth);
    /* This is a renderer's host: the requests it reads are the ForwardedRequest
     * its service makes of QEMU's bytes, not a MockRequest. */
    void          forward_requests();
    /* The renderer socket's far end; the mock keeps no ownership. */
    void          connect_renderer(RendererPeer &peer);
    /* The Nth start_renderer from now fails, as a path that does not spawn. */
    void          fail_renderer_start_at(std::uint32_t nth);
    /* The renderer died: every send and receive answers not_found until the
     * next start. */
    void          renderer_gone();
    bool          renderer_running();
    std::uint32_t renderer_starts();
    std::uint32_t renderer_kills();
    /* The Nth export send from now is refused, as a consumer that reads
     * nothing would refuse it. */
    void          fail_export_send_at(std::uint32_t nth);
    /* Datagrams sent on export connections, and descriptors close_export
     * closed. */
    std::uint32_t export_sends();
    std::uint32_t exports_closed();
    /* The path the last start named, empty before the first. */
    const char   *started_renderer();
    /* Datagrams sent while a renderer ran, delivered to a peer or not. */
    std::uint32_t renderer_datagrams();
    /* The milliseconds the last receive_renderer was allowed, 0 before the
     * first. */
    std::uint32_t last_receive_timeout();

    /* Display heads whose console can show a dma-buf. Zero - the default - is
     * a host with no GL display, which is what `-display none` gives and what
     * the fallback has to keep working on. */
    void          add_display_heads(std::uint32_t heads);
    /* The nth show from now is refused, as a display that cannot import the
     * descriptor refuses it. */
    void          refuse_scanout_at(std::uint32_t nth);
    Scanout       head(std::uint32_t head) const;
    /* Heads showing something, so a test can say none was left behind. */
    std::uint32_t live_scanouts() const;
    /* The rectangle of the last update of `head`. */
    void          last_update(std::uint32_t head, std::uint32_t &x, std::uint32_t &y, std::uint32_t &width,
                              std::uint32_t &height) const;

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
    /* What the guest sees at `offset` of the hostmem region: the mock's own
     * mapping of the placement starting there, or null. */
    void         *placed_at(std::uint64_t offset);
    /* One placement as place_in_hostmem was asked for it. */
    struct Placement {
        int           fd;
        std::uint64_t size;
        std::uint64_t offset;
    };
    /* Placements made since construction, live or not, and the last one. */
    std::uint32_t placements() const;
    Placement     last_placement() const;

    /* Waits up to `milliseconds` for at least `threads` to be blocked in
     * wait_event, each past the point it looked at anything; false on timeout. */
    bool          wait_for_sleepers(std::uint32_t threads, std::uint32_t milliseconds);
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
    /* The host GPU this device was bound to, or no_gpu. */
    std::uint32_t bound_gpu();
    std::uint32_t live_placements();
    std::uint32_t live_doorbells();
    /* Ranges reserved and not released, and the runs aliased into them. */
    std::uint32_t live_ranges();
    std::uint32_t live_aliases();
    std::uint32_t live_recordings();

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
        int             descriptor;   /* made on the first event_descriptor, -1 before */
        std::uint32_t   sleepers;     /* threads blocked in wait_event on it */
        std::uint64_t   deadline;     /* what a thread in wait_event_until sleeps toward */
        bool            timed;        /* whether one does */
    };
    struct Mutex {
        pthread_mutex_t lock;
        bool            locked;
        bool            elsewhere;
        bool            live;
    };
    struct RendererRegion {
        std::uint32_t region;
        void         *pointer;
        std::uint64_t size;
        int           descriptor;
        std::uint64_t device;
        std::uint64_t inode;
    };
    struct MainLoopWork {
        HostRoutine routine;
        void       *context;
    };

    static constexpr std::uint32_t max_allocations = 2048u;
    static constexpr std::uint32_t max_guest_mappings = 20000u;
    static constexpr std::uint32_t max_regions = 8u;
    static constexpr std::uint32_t max_placements = 256u;
    static constexpr std::uint32_t max_counted_descriptors = 256u;
    static constexpr std::uint32_t max_resources = 64u;
    static constexpr std::uint32_t max_ranges = 128u;
    static constexpr std::uint32_t max_recordings = 4u;
    static constexpr std::size_t   max_path = 128u;
    static constexpr std::uint32_t max_files = 24u;
    static constexpr std::size_t   max_file_bytes = 512u;
    /* What one recording may hold in the mock: larger than any test writes,
     * so a test that overruns it is a test that meant something else. */
    static constexpr std::size_t   recording_capacity = 4u << 20;
    static constexpr std::uint64_t page = 4096u;

    static constexpr std::uint32_t max_renderer_regions = 16u;
    static constexpr std::uint32_t max_renderer_descriptors = 64u;

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
    struct LivePlacement {
        void         *mapping;   /* null for a free slot */
        Placement     asked;
    };
    struct Range {
        void         *base;
        std::uint64_t bytes;
        std::uint32_t aliases;
    };
    struct HostFile {
        char        path[max_path];
        char        text[max_file_bytes];
        std::size_t length;
    };
    struct RecordingFile {
        char          path[max_path];
        std::uint8_t *bytes;
        std::size_t   length;
        bool          live;
    };

    void    record(HostViolation kind, const char *function);
    GuestRegion *region_of(std::uint64_t address);
    /* The file-backed region holding all of [pointer, pointer + bytes), or null. */
    GuestRegion *region_holding(void *pointer, std::uint64_t bytes);
    Range       *range_holding(void *at, std::uint64_t bytes);
    bool         holds_range(void *at, std::uint64_t bytes);
    bool    fail_now(std::uint32_t &countdown);
    Thread *thread_of(void *thread);
    Event  *event_of(void *event);
    /* The page `event` is connected to, or max_doorbells. */
    std::uint32_t doorbell_of(void *event);
    Mutex  *mutex_of(void *mutex);
    RecordingFile *recording_of(void *file);
    RecordingFile *recording_at(const char *path);
    /* `descriptor` counted toward live_descriptors. */
    void            count_descriptor(int descriptor);
    /* A pipe's read end, uncounted: -1 where the kernel gives none. */
    int             fresh_descriptor();

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
    PciBus          gpu_buses_[max_gpus];
    bool            gpu_on_pci_[max_gpus];
    HostFile        files_[max_files];
    std::uint32_t   file_count_;
    MainLoopWork    work_[max_main_loop_work];
    std::uint32_t   work_count_;
    std::uint32_t   fail_thread_;
    std::uint32_t   fail_event_;
    std::uint32_t   fail_mutex_;
    std::uint32_t   fail_allocation_;
    std::uint32_t   fail_placement_;
    std::uint32_t   empty_mapping_;
    void           *allocations_[max_allocations];
    GuestRegion     regions_[max_regions];
    std::uint32_t   region_count_;
    std::uint64_t   splits_[max_regions];
    std::uint32_t   split_count_;
    GuestMapping   *guest_mappings_;
    LivePlacement   placements_[max_placements];
    std::uint32_t   placements_made_;
    Placement       last_placement_;
    std::uint64_t   hostmem_size_;
    std::uint32_t   resources_[max_resources];
    std::uint32_t   resource_count_;
    void           *doorbells_[max_doorbells];
    GuestTranslator translate_;
    void           *translate_context_;
    bool            fence_connected_;
    std::uint32_t   fail_fence_connect_;
    std::uint32_t   raised_[interrupt_lines];
    std::atomic<std::uint64_t> clock_;
    std::uint64_t   clock_step_;
    std::uint32_t   fail_bind_gpu_;
    std::uint32_t   bound_gpu_;
    Range           ranges_[max_ranges];
    std::uint32_t   fail_reserve_;
    std::uint32_t   fail_alias_;
    std::uint32_t   fail_overlay_ = 0u;
    std::uint32_t   fail_restore_ = 0u;
    std::uint64_t   overlaid_ = 0u;
    int             counted_[max_counted_descriptors];   /* -1 for a free slot */
    RecordingFile   recordings_[max_recordings];
    std::uint32_t   fail_recording_create_;
    std::uint32_t   fail_recording_append_;
    std::uint32_t   fail_recording_truncate_;
    std::uint32_t   display_heads_;
    std::uint32_t   refuse_scanout_;
    Scanout         scanouts_[max_scanout_heads];
    std::uint32_t   images_shown_[max_scanout_heads];
    AvExportedImage last_images_[max_scanout_heads];
    std::uint32_t   update_rects_[max_scanout_heads][4];
    RendererRegion  renderer_regions_[max_renderer_regions];
    std::uint32_t   renderer_region_count_;
    RendererPeer   *renderer_peer_;
    bool            renderer_running_;
    std::uint32_t   fail_renderer_start_;
    std::uint32_t   renderer_starts_;
    std::uint32_t   renderer_kills_;
    std::uint32_t   renderer_datagrams_;
    std::uint32_t   receive_timeout_;
    bool            forward_requests_;
    std::uint32_t   fail_event_descriptor_;
    char            started_renderer_[max_path];
    std::uint32_t   fail_export_send_;
    std::uint32_t   export_sends_;
    std::uint32_t   exports_closed_;
    struct HeldStore {
        void         *at;
        std::size_t   size;
        std::uint8_t  bytes[256];
    };
    static constexpr std::uint32_t max_held_stores = 16u;
    static constexpr std::size_t   max_held_bytes = sizeof(HeldStore::bytes);
    HeldStore       held_stores_[max_held_stores] = {};
    std::uint32_t   held_count_ = 0u;
    const void     *watched_ = nullptr;
    std::uint64_t   watched_when_landed_ = 0u;
    std::uint32_t   store_fences_ = 0u;
};

}  // namespace av

#endif  // AV_MOCK_HOST_HPP
