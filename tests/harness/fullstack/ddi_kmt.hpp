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
    static constexpr std::uint32_t max_hardware_queues = DoorbellFabric::max_pages;
    static constexpr std::uint32_t max_pages = 16384u;
    static constexpr std::uint32_t max_private_data = 256u;
    static constexpr std::uint32_t patience_ms = 5000u;
    static constexpr std::uint32_t timed_out = 0xC00000B5u;

    static constexpr std::uint64_t chunk_ram_address = 0x6000000000ull;
    static constexpr std::uint64_t chunk_ram_bytes = std::uint64_t{256} << 20u;

    DdiKmt(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlFdo &fdo, FakeDxgkrnlAdapter &dxgkrnl,
           FakeVirtioGpuFunction &function, InterruptPump &pump, MockHost &host);
    ~DdiKmt();
    DdiKmt(const DdiKmt &) = delete;
    void *lend_pages(std::uint64_t bytes, std::uint64_t &physical);
    DdiKmt &operator=(const DdiKmt &) = delete;

    Status   enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status   query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status   query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status   query_adapter_location(std::uint32_t adapter, PciLocation &location) override;
    Status   query_adapter_name(std::uint32_t adapter, std::uint16_t *name, std::uint32_t capacity) override;
    Status   query_umd_driver_version(std::uint32_t adapter, std::uint64_t &version) override;
    Status   query_segment_sizes(std::uint32_t adapter, std::uint64_t &dedicated_video, std::uint64_t &dedicated_system,
                                   std::uint64_t &shared_system) override;
    Status   query_native_gpu_fences(std::uint32_t adapter, bool &supported) override;
    Status   query_adapter_features(std::uint32_t adapter, UmdAdapterInfo &info) override;
    void     sleep_briefly() override;
    Status   close_adapter(std::uint32_t adapter) override;
    Status   adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
    Status   primary_display_luid(std::uint64_t &luid) override;
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
    void     allow_hardware_scheduling();
    void     refuse_tail_publishing();
    void     refuse_host_windows();
    void     let_process_watch();
    Status   disconnect_doorbell(std::uint32_t doorbell, DoorbellStatus says);
    std::uint32_t   doorbell_handle(std::uint32_t index);
    void           *doorbell_page(std::uint32_t doorbell);
    std::uint64_t  *queued_fence(std::uint32_t doorbell);
    DoorbellFabric &fabric();

    Status   wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    Status   scan_line(std::uint32_t adapter, std::uint32_t source, BeamPosition &position) override;
    Status   refresh_millihz(std::uint32_t adapter, std::uint32_t source, std::uint32_t &millihz) override;
    Status   escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                    std::uint32_t size) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    void                end_process();
    const NodeFences   &fences() const;
    FakeHandleTable    &handles();
    std::uint32_t       open_objects() const;

private:
    static constexpr std::uint64_t page_bytes = 4096u;
    static constexpr std::uint32_t first_handle = 0x00001000u;
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
        void         *resource;
        std::uint8_t  private_data[max_private_data];
        std::uint32_t  private_size;
        bool           opened_only;
    };
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
    Status  answer(std::int32_t code);
    std::uint64_t guest_ns() const;
    void          catch_up();
    Status  refuse(Status status);

    Device     *find_device(std::uint32_t handle);
    Allocation   *find_allocation(std::uint32_t handle);
    SharedHandle *find_shared(std::uint64_t handle);
    Context    *find_context(std::uint32_t handle);
    Queue      *find_queue(std::uint32_t handle);
    HardwareQueue *find_hardware_queue(std::uint32_t handle);
    Status         hardware_queue_idle(std::uint32_t context);
    bool           waiting_on(const Doorbell &doorbell, std::uint32_t context);
    Doorbell      *find_doorbell(std::uint32_t handle);
    std::uint32_t slot_of(const Context &context) const;

    Status create_with(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                       std::uint32_t flags, std::uint32_t &allocation);
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
    std::uint64_t                started_ns_;
    std::uint64_t                guest_origin_;
};

}

#endif
