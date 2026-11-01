/* dxgkrnl's D3DKMT layer over the driver's registered DDI table, for one
 * process on the one adapter the full-stack rig started (docs/testing.md, L4).
 * Each call is the DDI sequence dxgkrnl makes for it: a device is
 * CreateDevice; an allocation is CreateAllocation with the descriptor as
 * private data, its D3DKMT handle from FakeHandleTable, OpenAllocation on the
 * creating device, and guest pages VidMm would back it with; residency in the
 * aperture segment is BuildPagingBuffer's MAP_APERTURE_SEGMENT over those
 * pages; a context is CreateContext with virtual addressing, its private data
 * copied, then SetRootPageTable; a submission gets node 0's next fence and is
 * SubmitCommandVirtual at PASSIVE with its private data copied, refused past
 * the size the context asked for. A wait delivers interrupts until the
 * driver has completed what the context submitted; a vertical
 * blank turns the driver's vsync on and runs the platform's timers to the
 * next one. Paging runs inside the call that asks for it, so every paging
 * fence is reached on return. VidMm's GPU page tables are not built: GPU
 * addresses are handed out and never translated. */
#ifndef AV_DDI_KMT_HPP
#define AV_DDI_KMT_HPP

#include "miniport/driver/ddi_slot.hpp"
#include "guest/kmt/kmt.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "harness/fullstack/doorbell_fabric.hpp"
#include "harness/fullstack/node_fences.hpp"

#include <cstdint>

namespace av {

class DdiTable;
class FakeDxgkrnlAdapter;
class FakeDxgkrnlFdo;
class FakeVirtioGpuFunction;
class InterruptPump;
class MockHost;
class MockPlatform;

class DdiKmt final : public Kmt {
public:
    static constexpr std::uint32_t adapter_handle = 0x00000100u;
    static constexpr std::uint64_t rig_luid = 0x0000000100000100ull;
    static constexpr std::uint32_t max_devices = 8u;
    static constexpr std::uint32_t max_allocations = 32u;
    static constexpr std::uint32_t max_contexts = NodeFences::max_contexts;
    static constexpr std::uint32_t max_queues = 8u;
    /* One hardware queue and one doorbell per context a process runs. */
    static constexpr std::uint32_t max_hardware_queues = DoorbellFabric::max_pages;
    /* A host-visible chunk of 64 MiB (feature 045). */
    static constexpr std::uint32_t max_pages = 16384u;
    static constexpr std::uint32_t max_private_data = 256u;
    /* How long a wait on a context lasts before it is a timeout, as LoopbackDevice's. */
    static constexpr std::uint32_t patience_ms = 5000u;
    /* STATUS_IO_TIMEOUT, what GdiKmt reports for a wait that expired. */
    static constexpr std::uint32_t timed_out = 0xC00000B5u;

    /* The driver's own pages live in file-backed guest RAM of `host` at this
     * address, far from the platform's own physical addresses. */
    static constexpr std::uint64_t chunk_ram_address = 0x6000000000ull;
    static constexpr std::uint64_t chunk_ram_bytes = std::uint64_t{256} << 20u;

    DdiKmt(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl,
           FakeVirtioGpuFunction &function, InterruptPump &pump, MockHost &host);
    ~DdiKmt();
    DdiKmt(const DdiKmt &) = delete;
    /* What the platform lends the driver as its own pages: the next run of
     * chunk RAM, at `physical`, or null when it is spent. */
    void *lend_pages(std::uint64_t bytes, std::uint64_t &physical);
    DdiKmt &operator=(const DdiKmt &) = delete;

    Status   enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status   query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status   query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status   query_adapter_location(std::uint32_t adapter, PciLocation &location) override;
    Status   query_adapter_name(std::uint32_t adapter, std::uint16_t *name, std::uint32_t capacity) override;
    Status   query_kmd_driver_version(std::uint32_t adapter, std::uint64_t &version) override;
    Status   query_segment_sizes(std::uint32_t adapter, std::uint64_t &dedicated_video, std::uint64_t &dedicated_system,
                                   std::uint64_t &shared_system) override;
    Status   query_native_gpu_fences(std::uint32_t adapter, bool &supported) override;
    Status   query_adapter_features(std::uint32_t adapter, UmdAdapterInfo &info) override;
    void     sleep_briefly() override;
    Status   close_adapter(std::uint32_t adapter) override;
    /* rig_luid, the one adapter's. */
    Status   adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
    Status   primary_display_luid(std::uint64_t &luid) override;
    /* The budget is the size of the segments the driver reports in the group,
     * asked through QUERYSEGMENT4 as VidMm asks: local those that are neither
     * an aperture nor populated from system memory, non-local the rest. The
     * usage is what this process holds there: every allocation lives in the
     * aperture or VidMm's system memory segment, so all of it is non-local. */
    Status   query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                std::uint64_t &usage) override;
    Status   create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status   destroy_device(std::uint32_t device) override;
    Status   create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                               std::uint32_t &allocation) override;
    Status   destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status   create_shared_allocation(std::uint32_t device, std::uint8_t *private_data, std::uint32_t size,
                                      std::uint32_t &resource) override;
    Status   share_object(std::uint32_t device, std::uint32_t resource, std::uint64_t &handle) override;
    Status   destroy_shared_allocation(std::uint32_t device, std::uint32_t resource) override;
    Status   open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &resource,
                                std::uint8_t *private_data, std::uint32_t capacity,
                                std::uint32_t &written) override;
    Status   close_shared_object(std::uint64_t handle) override;
    Status   lock(std::uint32_t device, std::uint32_t allocation, void *&view) override;
    Status   unlock(std::uint32_t device, std::uint32_t allocation) override;
    Status   create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object) override;
    Status   destroy_paging_queue(std::uint32_t queue) override;
    Status   make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence) override;
    Status   map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                     std::uint64_t &address, std::uint64_t &paging_fence) override;
    Status   free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages) override;
    Status   wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence) override;
    Status   create_context(std::uint32_t device, std::uint32_t &context) override;
    Status   create_hw_queue_context(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                     std::uint32_t &context) override;
    Status   create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                    std::uint32_t &context) override;
    Status   destroy_context(std::uint32_t context) override;
    Status   submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                    const std::uint8_t *private_data, std::uint32_t private_size) override;
    Status   wait_for_context(std::uint32_t device, std::uint32_t context) override;
    /* Feature 049: a hardware queue is DxgkDdiCreateHwQueue on the context,
     * with the progress fence dxgkrnl would allocate; a doorbell is
     * DxgkDdiCreateDoorbell on that queue, its status page and queued-fence
     * word this rig's memory as they are dxgkrnl's on a machine. Connecting
     * one asks the driver which device page it is and arms a page of real
     * memory that carries a store there (DoorbellFabric), because the driver's
     * own mapping is an address only MockPlatform can reach. */
    Status   create_hw_queue(std::uint32_t device, std::uint32_t context, GuestHwQueue &queue) override;
    Status   submit_to_hw_queue(std::uint32_t queue, std::uint64_t commands, std::uint64_t length,
                                const std::uint8_t *private_data, std::uint32_t private_size,
                                std::uint64_t progress_fence_id) override;
    Status   wait_for_hw_queue(std::uint32_t device, std::uint32_t progress_fence, std::uint64_t value) override;
    Status   destroy_hw_queue(std::uint32_t queue) override;
    Status   create_doorbell(std::uint32_t queue, std::uint32_t ring, std::uint32_t &doorbell, void *&page,
                             const std::uint32_t *&status, std::uint64_t *&queued) override;
    Status   connect_doorbell(std::uint32_t doorbell) override;
    Status   destroy_doorbell(std::uint32_t doorbell) override;
    Status   notify_work_submission(std::uint32_t doorbell) override;
    /* Hardware-accelerated GPU scheduling is the OS's decision and off unless
     * it is turned on (049 research.md §11), so a rig that wants the doorbell
     * path asks for it and every other rig keeps submitting through the
     * kernel. */
    void     allow_hardware_scheduling();
    /* TailPublishRecord's escape answered as an older driver answers it, not
     * handed over: every submission then goes through the kernel, and the
     * process never stores its own ring tail. */
    void     refuse_tail_publishing();
    /* A host window open answered as a device with no window answers it, not
     * handed over: a context then has no blob arena, and its bulk data
     * crosses inline (docs/host-memory.md). */
    void     refuse_host_windows();
    /* A process that watches its own memory for its answers, as the
     * transport does before it asks the kernel to wait: its clock is the
     * machine's, and what the device finished interrupts the driver before
     * its next submission, as both do on a machine. Off, the rig's clock is
     * the test's alone, which is what a test that times by it needs. */
    void     let_process_watch();
    /* dxgkrnl's side of a disconnect, which a process is never asked about:
     * the driver is told, and the status page the process reads says what to
     * do about it - retry for one that may come back, abort for one that will
     * not. */
    Status   disconnect_doorbell(std::uint32_t doorbell, DoorbellStatus says);
    /* The handle of the `index`th doorbell this process holds, 0 past the
     * end, and the page it stores to: how a test reaches the doorbell a
     * transport made for itself. */
    std::uint32_t   doorbell_handle(std::uint32_t index);
    void           *doorbell_page(std::uint32_t doorbell);
    /* Where the process writes the fence value it has queued: dxgkrnl's
     * memory, and a page a rogue process may write anything into. */
    std::uint64_t  *queued_fence(std::uint32_t doorbell);
    /* The stores a process made, carried to the device (nothing carries them
     * by itself, as nothing delivers an interrupt by itself). */
    DoorbellFabric &fabric();

    Status   wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    /* The miniport's DxgkDdiGetScanLine answers, as dxgkrnl has it do. */
    Status   scan_line(std::uint32_t adapter, std::uint32_t source, BeamPosition &position) override;
    /* Windows' current mode is dxgkrnl's to keep, which this harness is not. */
    Status   refresh_millihz(std::uint32_t adapter, std::uint32_t source, std::uint32_t &millihz) override;
    Status   escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                    std::uint32_t size) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    /* What dxgkrnl does for a process that exits holding objects: its contexts,
     * then its allocations, then its devices are destroyed. */
    void                end_process();
    const NodeFences   &fences() const;
    FakeHandleTable    &handles();
    /* Allocations, contexts and devices still open. */
    std::uint32_t       open_objects() const;

private:
    static constexpr std::uint64_t page_bytes = 4096u;
    static constexpr std::uint32_t first_handle = 0x00001000u;
    /* Where GPU virtual addresses start: inside the root a 512-entry
     * SetRootPageTable reaches, and never 0. */
    static constexpr std::uint64_t first_gpu_address = 0x100000000ull;

    struct Device {
        std::uint32_t handle;
        void         *driver;
    };
    struct Allocation {
        std::uint32_t handle;
        std::uint32_t device;
        void         *driver;
        void         *opened;
        std::uint8_t *block;
        std::uint64_t bytes;
        bool          aperture;
        bool          resident;
        std::uint64_t offset_in_pages;
        std::uint64_t gpu_address;
        std::uint64_t *frames;
        /* The resource the create made, for one asked for as a carrier; null
         * otherwise. D3DKMTShareObjects names this and not the allocation
         * (research.md §16). */
        void         *resource;
        std::uint8_t  private_data[max_private_data];
        /* How much of it the create carried, which is what an open must name:
         * the miniport reads 48 bytes or 88 and refuses every other length. */
        std::uint32_t  private_size;
        /* A slot that is a second device's *open* of somebody else's
         * allocation: it closes its open and lets the creator's allocation
         * alone (feature 053). */
        bool           opened_only;
    };
    /* One NT handle this rig minted. dxgkrnl owns the handle table, and this
     * class is dxgkrnl over the real miniport - so the token is modelled here
     * while every allocation call below stays the driver's own. */
    struct SharedHandle {
        std::uint64_t handle;
        std::uint32_t allocation;
    };
    struct Context {
        std::uint32_t handle;
        std::uint32_t device;
        void         *driver;
        std::uint32_t private_capacity;
    };
    struct Queue {
        std::uint32_t handle;
        std::uint32_t device;
        std::uint32_t fence_object;
    };
    struct HardwareQueue {
        std::uint32_t handle;
        std::uint32_t context;
        void         *driver;
        std::uint64_t progress;
    };
    struct Doorbell {
        std::uint32_t  handle;
        std::uint32_t  queue;
        void          *driver;
        void          *page;
        std::uint32_t  status;
        std::uint64_t  queued;
    };

    template <typename Routine>
    Routine routine(DdiSlot slot) const;
    /* Records a DDI's NTSTATUS as the last code and answers it as a Status. */
    Status  answer(std::int32_t code);
    /* The guest's clock since the rig began, in nanoseconds. */
    std::uint64_t guest_ns() const;
    /* The mock host's clock brought up to the guest's, and whatever the
     * device's clock thread made of that delivered. */
    void          catch_up();
    Status  refuse(Status status);

    Device     *find_device(std::uint32_t handle);
    Allocation   *find_allocation(std::uint32_t handle);
    SharedHandle *find_shared(std::uint64_t handle);
    Context    *find_context(std::uint32_t handle);
    Queue      *find_queue(std::uint32_t handle);
    HardwareQueue *find_hardware_queue(std::uint32_t handle);
    /* Whether `context`'s hardware queues have caught up with what user mode
     * queued on them. */
    Status         hardware_queue_idle(std::uint32_t context);
    bool           waiting_on(const Doorbell &doorbell, std::uint32_t context);
    Doorbell      *find_doorbell(std::uint32_t handle);
    std::uint32_t slot_of(const Context &context) const;

    /* The rest of create_allocation once the driver has answered: pages, a
     * handle and the open, or everything undone. */
    /* `flags` is the DXGK_CREATEALLOCATIONFLAGS Value: a carrier asks for
     * Resource, and every other create asks for nothing. */
    Status create_with(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                       std::uint32_t flags, std::uint32_t &allocation);
    /* The destroy both public ones do, naming `resource` where a carrier is
     * being taken and nothing where an ordinary allocation is. */
    Status destroy_slot(std::uint32_t device, std::uint32_t allocation, void *resource);
    Status admit(Allocation &slot, std::uint32_t device, void *driver_device);
    Status back(Allocation &slot);

    Status open_on(Allocation &slot, void *driver_device);
    Status page(Allocation &slot, std::int32_t operation);
    Status create_context_on(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                             bool hardware_queue,
                             std::uint32_t &context);
    void   release(Allocation &slot);

    MockPlatform                &platform_;
    const DdiTable              &table_;
    FakeDxgkrnlFdo              &fdo_;
    FakeDxgkrnlAdapter          &dxgkrnl_;
    FakeVirtioGpuFunction       &function_;
    InterruptPump               &pump_;
    MockHost                    &host_;
    FakeHandleTable              handles_;
    NodeFences                   fences_;
    Device                       devices_[max_devices];
    static constexpr std::uint32_t max_shared = 8u;
    static constexpr std::uint64_t first_shared_handle = 0x4000000000000004ull;
    SharedHandle                 shared_[max_shared];
    std::uint64_t                next_shared_handle_;
    Allocation                   allocations_[max_allocations];
    Context                      contexts_[max_contexts];
    Queue                        queues_[max_queues];
    HardwareQueue                hardware_queues_[max_hardware_queues];
    Doorbell                     doorbells_[max_hardware_queues];
    DoorbellFabric               fabric_;
    bool                         hardware_scheduling_;
    bool                         refuse_tail_publishing_;
    bool                         refuse_host_windows_;
    bool                         watching_;
    std::uint8_t                 kernel_copy_[max_private_data];
    std::uint32_t                next_handle_;
    std::uint64_t                next_aperture_page_;
    std::uint64_t                next_gpu_address_;
    std::uint64_t                paging_fence_;
    std::uint64_t                next_chunk_address_;
    std::uint32_t                last_code_;
    /* When the rig began, on the real clock the guest's counter runs on, moved
     * on by the real time spent simulating blank waits. */
    std::uint64_t                started_ns_;
    /* The guest's counter when the rig began: the mock host's clock is the
     * guest's since then. */
    std::uint64_t                guest_origin_;
};

}  // namespace av

#endif  // AV_DDI_KMT_HPP
