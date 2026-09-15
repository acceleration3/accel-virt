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

#include "core/miniport/ddi_slot.hpp"
#include "guest/kmt/kmt.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
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
    /* A host-visible chunk of 64 MiB (feature 045). */
    static constexpr std::uint32_t max_pages = 16384u;
    static constexpr std::uint32_t max_private_data = 256u;
    /* How long a wait on a context lasts before it is a timeout, as LoopbackDevice's. */
    static constexpr std::uint32_t patience_ms = 5000u;
    /* STATUS_IO_TIMEOUT, what GdiKmt reports for a wait that expired. */
    static constexpr std::uint32_t timed_out = 0xC00000B5u;

    /* Host-visible chunks live in file-backed guest RAM of `host` at this
     * address, far from the platform's own physical addresses. */
    static constexpr std::uint64_t chunk_ram_address = 0x6000000000ull;
    static constexpr std::uint64_t chunk_ram_bytes = std::uint64_t{256} << 20u;

    DdiKmt(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl,
           FakeVirtioGpuFunction &function, InterruptPump &pump, MockHost &host);
    ~DdiKmt();
    DdiKmt(const DdiKmt &) = delete;
    DdiKmt &operator=(const DdiKmt &) = delete;

    Status   enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status   query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status   query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status   close_adapter(std::uint32_t adapter) override;
    /* rig_luid, the one adapter's. */
    Status   adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
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
    Status   create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                    std::uint32_t &context) override;
    Status   destroy_context(std::uint32_t context) override;
    Status   submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                    const std::uint8_t *private_data, std::uint32_t private_size) override;
    Status   wait_for_context(std::uint32_t device, std::uint32_t context) override;
    Status   wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
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
        /* The guest-RAM address of a chunk's pages; 0 for platform memory. */
        std::uint64_t  guest_ram;
        std::uint8_t  private_data[max_private_data];
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

    template <typename Routine>
    Routine routine(DdiSlot slot) const;
    /* Records a DDI's NTSTATUS as the last code and answers it as a Status. */
    Status  answer(std::int32_t code);
    Status  refuse(Status status);

    Device     *find_device(std::uint32_t handle);
    Allocation *find_allocation(std::uint32_t handle);
    Context    *find_context(std::uint32_t handle);
    Queue      *find_queue(std::uint32_t handle);
    std::uint32_t slot_of(const Context &context) const;

    /* The rest of create_allocation once the driver has answered: pages, a
     * handle and the open, or everything undone. */
    Status admit(Allocation &slot, std::uint32_t device, void *driver_device);
    Status back(Allocation &slot);
    Status open_on(Allocation &slot, void *driver_device);
    Status page(Allocation &slot, std::int32_t operation);
    Status create_context_on(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
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
    Allocation                   allocations_[max_allocations];
    Context                      contexts_[max_contexts];
    Queue                        queues_[max_queues];
    std::uint8_t                 kernel_copy_[max_private_data];
    std::uint32_t                next_handle_;
    std::uint64_t                next_aperture_page_;
    std::uint64_t                next_gpu_address_;
    std::uint64_t                paging_fence_;
    std::uint64_t                next_chunk_address_;
    std::uint32_t                last_code_;
};

}  // namespace av

#endif  // AV_DDI_KMT_HPP
