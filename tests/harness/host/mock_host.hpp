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
    static constexpr std::uint32_t interrupt_lines = max_doorbells + 2u;
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
    Status   list_directory(const char *path, char *buffer, std::size_t capacity, std::size_t &length) override;
    void    *allocate(std::size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    void    *map_guest(std::uint64_t address, std::uint64_t &length, bool &direct) override;
    void     unmap_guest(void *pointer, std::uint64_t length) override;
    Status   reserve_range(std::uint64_t bytes, void *&base) override;
    Status   alias_guest_run(void *run, std::uint64_t bytes, void *at) override;
    Status   overlay_guest_run(void *run, std::uint64_t bytes, int fd, std::uint64_t offset) override;
    Status   restore_guest_run(void *run, std::uint64_t bytes) override;
    void     release_range(void *base, std::uint64_t bytes) override;
    std::uint64_t hostmem_size() override;
    Status   place_in_hostmem(int fd, std::uint64_t size, std::uint64_t offset) override;
    void     remove_from_hostmem(std::uint64_t offset, std::uint64_t size) override;
    std::uint64_t monotonic_nanoseconds() override;
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
    Status   receive_export(int connection, std::uint8_t *buffer, std::size_t capacity, std::size_t &length) override;
    Status   send_export(int connection, const std::uint8_t *bytes, std::size_t length, int descriptor) override;
    void     shutdown_export(int connection) override;
    void     close_export(int descriptor) override;
    int      duplicate_export(int descriptor) override;

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
        const std::uint8_t *blob;
        Plane         plane;
        std::uint32_t shows;
        std::uint32_t updates;
        std::uint32_t hides;
    };

    [[nodiscard]] Status show_scanout_plane(std::uint32_t head, const std::uint8_t *blob, const Plane &plane);

    void add_gpu(const std::uint8_t *uuid);
    void add_gpu(const GpuIdentity &identity);
    struct PciBus {
        std::uint32_t domain;
        std::uint32_t bus;
        std::uint32_t device;
        std::uint32_t function;
    };
    void place_gpu_on_pci(std::uint32_t index, const PciBus &bus);
    bool gpu_bus(std::uint32_t index, PciBus &bus);
    void add_file(const char *path, const char *text, std::size_t length);
    void fail_thread_start_at(std::uint32_t nth);
    void fail_event_at(std::uint32_t nth);
    void fail_mutex_at(std::uint32_t nth);
    void hold_mutex_elsewhere(void *mutex, bool held);
    void fail_allocation_at(std::uint32_t nth);
    void leave_listing_unterminated();
    void fail_placement_at(std::uint32_t nth);
    void fail_bind_gpu_at(std::uint32_t nth);
    void          hold_store(void *at, const void *bytes, std::size_t size);
    void          watch(const void *word);
    std::uint32_t store_fences();
    std::uint64_t watched_when_landed();
    void advance_clock_per_read(std::uint64_t nanoseconds);
    void set_clock(std::uint64_t now);
    bool          next_deadline(std::uint64_t &deadline);
    bool          wait_for_deadline_after(std::uint64_t after, std::uint32_t milliseconds);
    std::uint32_t    images_shown(std::uint32_t head) const;
    AvExportedImage last_image(std::uint32_t head) const;
    void empty_mapping_at(std::uint32_t nth);
    void fail_recording_create_at(std::uint32_t nth);
    void fail_recording_append_at(std::uint32_t nth);
    void fail_recording_truncate_at(std::uint32_t nth);
    const std::uint8_t *recording_bytes(const char *path);
    std::size_t         recording_length(const char *path);

    void fail_reserve_at(std::uint32_t nth);
    void fail_alias_at(std::uint32_t nth);
    void fail_overlay_at(std::uint32_t nth);
    void fail_restore_at(std::uint32_t nth);
    std::uint64_t overlaid_bytes();
    std::uint32_t live_descriptors() const;
    int           open_descriptor();
    bool          descriptor_open(int fd) const;

    int           add_guest_region(std::uint32_t region, void *pointer, std::uint64_t size);
    void         *guest_region_mapping(int descriptor, std::uint64_t size);
    void          fail_event_descriptor_at(std::uint32_t nth);
    void          forward_requests();
    void          connect_renderer(RendererPeer &peer);
    void          fail_renderer_start_at(std::uint32_t nth);
    void          renderer_gone();
    bool          renderer_running();
    std::uint32_t renderer_starts();
    std::uint32_t renderer_kills();
    void          fail_export_send_at(std::uint32_t nth);
    std::uint32_t export_sends();
    std::uint32_t exports_closed();
    const char   *started_renderer();
    std::uint32_t renderer_datagrams();
    std::uint32_t last_receive_timeout();

    void          add_display_heads(std::uint32_t heads);
    void          refuse_scanout_at(std::uint32_t nth);
    Scanout       head(std::uint32_t head) const;
    std::uint32_t live_scanouts() const;
    void          last_update(std::uint32_t head, std::uint32_t &x, std::uint32_t &y, std::uint32_t &width,
                              std::uint32_t &height) const;

    using GuestTranslator = void *(*)(void *context, std::uint64_t address, std::uint64_t length);
    void          translate_guest_with(GuestTranslator translate, void *context);
    void          add_guest_ram(std::uint64_t base, std::size_t size);
    void          split_guest_memory_at(std::uint64_t address);
    void          add_bounce_region(std::uint64_t base, std::uint64_t size);
    std::uint8_t *guest_ram(std::uint64_t address);
    void          set_hostmem_size(std::uint64_t size);
    void          add_resource(std::uint32_t id);
    void          fail_fence_connect_at(std::uint32_t nth);
    bool          fence_interrupts_connected();
    std::uint32_t raised(std::uint32_t line);
    bool          ring_doorbell(std::uint32_t index);
    void         *placed_at(std::uint64_t offset);
    struct Placement {
        int           fd;
        std::uint64_t size;
        std::uint64_t offset;
    };
    std::uint32_t placements() const;
    Placement     last_placement() const;

    bool          wait_for_sleepers(std::uint32_t threads, std::uint32_t milliseconds);
    bool          wait_for_main_loop_work(std::uint32_t work, std::uint32_t milliseconds);
    std::uint32_t run_main_loop();

    bool          holds_main_loop() const;
    bool          claim(HostViolation kind);
    std::uint32_t unclaimed_violations();
    std::uint32_t logged();
    const char   *last_log();
    std::uint32_t live_threads();
    std::uint32_t live_events();
    std::uint32_t live_mutexes();
    std::uint64_t mutex_locks() const;
    std::uint32_t live_allocations();
    std::uint32_t live_guest_mappings();
    std::uint32_t bound_gpu();
    std::uint32_t live_placements();
    std::uint32_t live_doorbells();
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
        int             descriptor;
        std::uint32_t   sleepers;
        std::uint64_t   deadline;
        bool            timed;
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
    static constexpr std::size_t   recording_capacity = 4u << 20;
    static constexpr std::uint64_t page = 4096u;

    static constexpr std::uint32_t max_renderer_regions = 16u;
    static constexpr std::uint32_t max_renderer_descriptors = 64u;

    struct GuestRegion {
        std::uint64_t base;
        std::uint64_t size;
        std::uint8_t *bytes;
        int           fd;
        std::uint64_t mapped;
    };
    struct GuestMapping {
        void         *pointer;
        std::uint64_t length;
        bool          bounce;
    };
    struct LivePlacement {
        void         *mapping;
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
    GuestRegion *region_holding(void *pointer, std::uint64_t bytes);
    Range       *range_holding(void *at, std::uint64_t bytes);
    bool         holds_range(void *at, std::uint64_t bytes);
    bool    fail_now(std::uint32_t &countdown);
    Thread *thread_of(void *thread);
    Event  *event_of(void *event);
    std::uint32_t doorbell_of(void *event);
    Mutex  *mutex_of(void *mutex);
    RecordingFile *recording_of(void *file);
    RecordingFile *recording_at(const char *path);
    void            count_descriptor(int descriptor);
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
    bool            unterminated_listing_ = false;
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
    std::atomic<std::uint64_t> mutex_locks_{0u};
    std::uint64_t   clock_step_;
    std::uint32_t   fail_bind_gpu_;
    std::uint32_t   bound_gpu_;
    Range           ranges_[max_ranges];
    std::uint32_t   fail_reserve_;
    std::uint32_t   fail_alias_;
    std::uint32_t   fail_overlay_ = 0u;
    std::uint32_t   fail_restore_ = 0u;
    std::uint64_t   overlaid_ = 0u;
    int             counted_[max_counted_descriptors];
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

}

#endif
