/* The D3DKMT calls the in-guest suite makes, in core's types: handles are the
 * kernel's D3DKMT_HANDLE values, results are Status, and the NTSTATUS behind
 * the last call is kept for the report. GdiKmt (src/um) calls gdi32; MockKmt
 * (tests/harness/guest) answers what a test scripts. A handle or address of 0
 * is never valid. Every wait is bounded by the implementation. */
#ifndef AV_KMT_HPP
#define AV_KMT_HPP

#include "core/support/status.hpp"
#include "guest/kmt/guest_hw_queue.hpp"
#include "guest/kmt/memory_segment_group.hpp"

#include <stdint.h>

namespace av {

class UmdAdapterInfo;

class BeamPosition;
class PciLocation;
class PciIdentity;

class Kmt {
public:
    /* Opens every adapter: at most `capacity` handles, how many in `count`. */
    virtual Status   enumerate_adapters(uint32_t *adapters, uint32_t capacity, uint32_t &count) = 0;
    virtual Status   query_identity(uint32_t adapter, PciIdentity &identity) = 0;
    /* KMT_DRIVERVERSION: 3200 for WDDM 3.2. */
    virtual Status   query_driver_version(uint32_t adapter, uint32_t &version) = 0;
    /* KMTQAITYPE_ADAPTERADDRESS: where the adapter sits on the PCI bus. */
    virtual Status   query_adapter_location(uint32_t adapter, PciLocation &location) = 0;
    /* KMTQAITYPE_ADAPTERREGISTRYINFO's AdapterString: the adapter's name as its
     * driver wrote it (HardwareInformation.AdapterString), UTF-16 and ended
     * within `capacity` units - an empty string where there is none. */
    virtual Status   query_adapter_name(uint32_t adapter, uint16_t *name, uint32_t capacity) = 0;
    /* KMTQAITYPE_KMD_DRIVER_VERSION: the driver package's version, the INF's
     * DriverVer as four 16-bit parts, the most significant first. */
    virtual Status   query_kmd_driver_version(uint32_t adapter, uint64_t &version) = 0;
    /* KMTQAITYPE_GETSEGMENTSIZE: the memory the adapter reports - dedicated
     * video, dedicated system and shared system bytes - which is what DXGI
     * describes it with. */
    virtual Status   query_segment_sizes(uint32_t adapter, uint64_t &dedicated_video, uint64_t &dedicated_system,
                                         uint64_t &shared_system) = 0;
    /* KMTQAITYPE_WDDM_3_1_CAPS: whether the OS let the adapter report native
     * GPU fences, without which no hardware queue is given a doorbell. */
    virtual Status   query_native_gpu_fences(uint32_t adapter, bool &supported) = 0;
    /* UMDRIVERPRIVATE: what avgpu.sys says the device under the adapter
     * offers a user-mode driver (UmdAdapterInfo); none from a miniport that
     * does not say. */
    virtual Status   query_adapter_features(uint32_t adapter, UmdAdapterInfo &info) = 0;
    virtual Status   close_adapter(uint32_t adapter) = 0;
    /* The adapter's LUID as enumeration reports it, LowPart in the low 32 bits
     * and HighPart in the high: what the Vulkan loader matches an ICD's
     * physical device against. */
    virtual Status   adapter_luid(uint32_t adapter, uint64_t &luid) = 0;
    /* The LUID of the adapter the primary display is on - the one DXGI lists
     * first - packed as adapter_luid packs it. */
    virtual Status   primary_display_luid(uint64_t &luid) = 0;
    /* D3DKMTQueryVideoMemoryInfo for this process: how much of `group` it may
     * use, and how much its devices use now. */
    virtual Status   query_video_memory(uint32_t adapter, MemorySegmentGroup group, uint64_t &budget,
                                        uint64_t &usage) = 0;
    virtual Status   create_device(uint32_t adapter, uint32_t &device) = 0;
    virtual Status   destroy_device(uint32_t device) = 0;
    virtual Status   create_allocation(uint32_t device, const uint8_t *descriptor, uint32_t size,
                                       uint32_t &allocation) = 0;
    virtual Status   destroy_allocation(uint32_t device, uint32_t allocation) = 0;
    /* An allocation created asking the miniport for a shared object's identity
     * (feature 053). `private_data` goes in as an AllocationDescriptor
     * followed by an unassigned SharedObjectRecord, and comes back with the id
     * and the adapter number the miniport minted into it - which is the only
     * thing an opening process is ever given, so the caller keeps exactly
     * these bytes and shares the resource by them.
     *
     * What comes back is the *resource*, not the allocation in it:
     * D3DKMTShareObjects mints an NT handle over a resource and refuses
     * anything else, with no message at all (research.md §16), so the carrier
     * is created as one and the allocation inside it lives and dies with it.
     *
     * Separate from create_allocation because the buffer is two-way. Every
     * other create hands the driver bytes and reads none back, and widening
     * that one to a mutable buffer would put an in/out parameter on the path
     * every allocation in the guest takes. invalid_parameter for a length that
     * is not a descriptor and a record; nothing is created unless ok. */
    virtual Status   create_shared_allocation(uint32_t device, uint8_t *private_data, uint32_t size,
                                              uint32_t &resource) = 0;
    /* D3DKMTShareObjects over one resource of `device`: an NT handle this
     * process may duplicate into another, which opens it with
     * open_shared_object. The handle is this process's and must be closed with
     * close_shared_object. 0 unless ok. invalid_parameter for an allocation of
     * 0; not_supported where the OS will not share what this names - an
     * allocation, rather than the resource holding it, is one (feature 053). */
    virtual Status   share_object(uint32_t device, uint32_t resource, uint64_t &handle) = 0;
    /* The resource a create_shared_allocation made, and the allocation in it.
     * Destroying by resource is how dxgkrnl takes both, and the only way to
     * take the resource at all. invalid_parameter for a resource of 0. */
    virtual Status   destroy_shared_allocation(uint32_t device, uint32_t resource) = 0;
    /* D3DKMTOpenResourceFromNtHandle: what `handle` names, opened on `device`
     * as a resource of this process, with the *creating* process's allocation
     * private driver data written into `private_data` and its length in
     * `written`.
     *
     * The private data is what the call is for. dxgkrnl carries the creator's
     * bytes to the opener and reads none of them (docs/kb/04-memory.md), which
     * is the only channel a shared object's name crosses a process boundary
     * on: the opening ICD reads the id out of them and names it on the wire.
     * What the open answers is the resource it made, because that is what the
     * opener must give back: the allocation inside it goes with it, and an
     * allocation handle is not what a destroy of a resource takes
     * (research.md §17). invalid_parameter for a handle of 0 or a buffer the
     * private data does not fit in; not_found for a handle this process may
     * not open. Nothing is opened unless ok. */
    virtual Status   open_shared_object(uint32_t device, uint64_t handle, uint32_t &resource,
                                        uint8_t *private_data, uint32_t capacity, uint32_t &written) = 0;
    /* CloseHandle on what share_object or open_shared_object was given. The
     * resource an open made is destroyed with destroy_shared_allocation: the
     * handle and what it named have separate lifetimes.
     * invalid_parameter for a handle of 0. */
    virtual Status   close_shared_object(uint64_t handle) = 0;
    /* The process's CPU view of an allocation, in `view`: for one in a
     * CPU-visible segment, the host's own memory through the hostmem BAR. It
     * stays addressable until unlock. */
    virtual Status   lock(uint32_t device, uint32_t allocation, void *&view) = 0;
    virtual Status   unlock(uint32_t device, uint32_t allocation) = 0;
    /* A paging queue and the synchronization object its fences are waited on. */
    virtual Status   create_paging_queue(uint32_t device, uint32_t &queue, uint32_t &fence_object) = 0;
    virtual Status   destroy_paging_queue(uint32_t queue) = 0;
    /* The paging fence that reaching makes it resident. */
    virtual Status   make_resident(uint32_t queue, uint32_t allocation, uint64_t &paging_fence) = 0;
    virtual Status   map_gpu_virtual_address(uint32_t queue, uint32_t allocation, uint64_t pages, uint64_t &address,
                                             uint64_t &paging_fence) = 0;
    virtual Status   free_gpu_virtual_address(uint32_t adapter, uint64_t address, uint64_t pages) = 0;
    virtual Status   wait_for_fence(uint32_t device, uint32_t fence_object, uint64_t fence) = 0;
    virtual Status   create_context(uint32_t device, uint32_t &context) = 0;
    /* A virtual context carrying private driver data - the AvContextRecord an
     * av transport asks its context with; the miniport decodes it. */
    virtual Status   create_context_virtual(uint32_t device, const uint8_t *private_data, uint32_t size,
                                            uint32_t &context) = 0;
    /* The same context, asked for as one a hardware queue may run on
     * (`D3DDDI_CREATECONTEXTFLAGS.HwQueueSupported`): what a process must ask
     * for before dxgkrnl will give it a queue, and so a doorbell (feature 049,
     * research.md §13). A hardware-scheduled context cannot be submitted to
     * with `submit` at all - `submit_to_hw_queue` is its path - so a process
     * that is refused one asks for a plain context instead.
     * not_supported where the OS hands out no hardware queues. */
    virtual Status   create_hw_queue_context(uint32_t device, const uint8_t *private_data, uint32_t size,
                                             uint32_t &context) = 0;
    virtual Status   destroy_context(uint32_t context) = 0;
    /* On a context made with create_context_virtual, dxgkrnl routes this to the
     * DxgkDdiSubmitCommandVirtual DDI; `commands` is the DMA buffer's GPU
     * virtual address. `private_data` is copied by dxgkrnl and handed to the
     * driver beside it - where an av transport puts its submission
     * descriptor, since the driver cannot read a GPU address (feature 043).
     * Null and 0 for none. */
    virtual Status   submit(uint32_t context, uint64_t commands, uint64_t length, const uint8_t *private_data,
                            uint32_t private_size) = 0;
    /* Until everything submitted on the context so far has completed. */
    virtual Status   wait_for_context(uint32_t device, uint32_t context) = 0;
    /* A hardware queue on `context`, which is what a doorbell is created for
     * (feature 049, research.md §1). `progress` is answered: where the driver
     * writes the queue's progress, which the process reads and never writes.
     * not_supported when the OS
     * will not give this adapter one - hardware scheduling is its decision,
     * not the driver's (research.md §11) - and the transport then submits the
     * way it always has. */
    virtual Status   create_hw_queue(uint32_t device, uint32_t context, GuestHwQueue &queue) = 0;
    /* What a hardware-scheduled context submits through, since
     * `D3DKMTSubmitCommand` refuses one outright (research.md §13): the same
     * batch and the same private data the kernel path carries, with the
     * progress fence value the queue reaches when the device has run it. */
    virtual Status   submit_to_hw_queue(uint32_t queue, uint64_t commands, uint64_t length,
                                        const uint8_t *private_data, uint32_t private_size,
                                        uint64_t progress_fence_id) = 0;
    /* Until the queue's progress fence reaches `value`: the wait a process
     * makes when what it submitted never went through a context's packets. */
    virtual Status   wait_for_hw_queue(uint32_t device, uint32_t progress_fence, uint64_t value) = 0;
    virtual Status   destroy_hw_queue(uint32_t queue) = 0;
    /* D3DKMTCreateDoorbell on `queue` over the process's `ring` allocation: the
     * three addresses it answers are the doorbell itself, the status page a
     * process reads before every store, and where it writes the fence value it
     * has queued. A doorbell created is not connected. */
    virtual Status   create_doorbell(uint32_t queue, uint32_t ring, uint32_t &doorbell, void *&page,
                                     const uint32_t *&status, uint64_t *&queued) = 0;
    /* Connected, the process may store; dxgkrnl disconnects it for suspend,
     * reset and migration without telling the process, which learns it from
     * the status page (contracts, research.md §6). */
    virtual Status   connect_doorbell(uint32_t doorbell) = 0;
    virtual Status   destroy_doorbell(uint32_t doorbell) = 0;
    /* What a process calls after a store when the status asked it to. This
     * device's never does, so nothing calls it in the steady state. */
    virtual Status   notify_work_submission(uint32_t doorbell) = 0;
    /* Until the next vertical blank of `source` on that device: what DWM's
     * frame pacing waits on, and the only way in to see the rate the driver
     * reports. */
    virtual Status   wait_for_vertical_blank(uint32_t adapter, uint32_t device, uint32_t source) = 0;
    /* Where the scan-out of `source` is now, as the driver's clock places it. */
    virtual Status   scan_line(uint32_t adapter, uint32_t source, BeamPosition &position) = 0;
    /* D3DKMTEscape with driver-private data on `context` of `device`: the
     * miniport reads the `size` bytes and may write its answer back into them,
     * which the caller finds in `data` on return. The status is the driver's. */
    virtual Status   escape(uint32_t adapter, uint32_t device, uint32_t context, uint8_t *data, uint32_t size) = 0;
    /* The system's performance counter and its frequency: a vertical blank
     * carries no timestamp, so a rate is measured against this. */
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    /* Gives the processor up for about a millisecond, for a wait that has
     * outlasted spinning and has no kernel object to sleep on. */
    virtual void     sleep_briefly() = 0;
    virtual uint32_t last_code() const = 0;

protected:
    ~Kmt() = default;
};

}  // namespace av

#endif  // AV_KMT_HPP
