/* A D3DKMT that answers what a test scripts: adapters with their identity and
 * driver version, handles handed out in sequence, any call failed with an
 * NTSTATUS or answered with a zero handle, and a count of every object still
 * open and every call made with a handle it never gave. Allocations decode
 * their descriptor as the driver does; submissions must land inside a
 * mapping of the submitting context's device; a lock hands out the one view
 * this mock owns, which keeps what is written through it; and a wait on a
 * transport's context can find a scripted answer in its reply slot - one for
 * every wait, or a queue of them, one per wait - with the ring consumed up to
 * the last submission, as a device that finished it would. Every command a
 * transport's submission carried is journalled by id and size. A context made
 * with an AvContextRecord runs, as the driver's does, on a ring and a reply
 * slot this mock maps - an escape tells the process where - and an escape opens
 * and closes chunks in a host window a test gives the device, refused where
 * the window cannot hold one, as the driver refuses it. A test can run its own
 * callback at a chosen wait to play the host's work on the views. */
#ifndef AV_MOCK_KMT_HPP
#define AV_MOCK_KMT_HPP

#include "guest/kmt/beam_position.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_location.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>

namespace av {

class HostWindowRecord;

/* The driver's side of an adapter-only escape, as a test plays it. */
class AdapterEscapes {
public:
    AdapterEscapes() = default;
    AdapterEscapes(const AdapterEscapes &) = delete;
    AdapterEscapes &operator=(const AdapterEscapes &) = delete;
    /* The NTSTATUS the escape answers; `data` may be written. */
    virtual std::uint32_t answer(std::uint8_t *data, std::uint32_t size) = 0;

protected:
    ~AdapterEscapes() = default;
};

/* What a test does each time the code under test reads the clock. */
class ClockReader {
public:
    ClockReader(const ClockReader &) = delete;
    ClockReader &operator=(const ClockReader &) = delete;
    virtual void clock_read() = 0;

protected:
    ClockReader() = default;
    ~ClockReader() = default;
};

class MockKmt final : public Kmt {
public:
    enum class Call : unsigned {
        enumerate_adapters,
        query_identity,
        query_driver_version,
        close_adapter,
        create_device,
        destroy_device,
        create_allocation,
        create_shared_allocation,
        destroy_shared_allocation,
        destroy_allocation,
        share_object,
        open_shared_object,
        close_shared_object,
        lock,
        unlock,
        create_paging_queue,
        destroy_paging_queue,
        make_resident,
        map_gpu_virtual_address,
        free_gpu_virtual_address,
        wait_for_fence,
        create_context,
        create_context_virtual,
        create_hw_queue_context,
        destroy_context,
        submit,
        wait_for_context,
        create_hw_queue,
        submit_to_hw_queue,
        wait_for_hw_queue,
        destroy_hw_queue,
        create_doorbell,
        connect_doorbell,
        destroy_doorbell,
        notify_work_submission,
        wait_for_vertical_blank,
        scan_line,
        escape,
        adapter_luid,
        query_video_memory,
        query_adapter_location,
        query_adapter_name,
        query_kmd_driver_version,
        query_segment_sizes,
        query_adapter_features,
        count
    };
    static constexpr std::uint32_t max_objects = 32u;
    static constexpr std::uint32_t invalid_parameter = 0xC000000Du;
    static constexpr std::uint32_t pending = 0x00000103u;
    /* What a wait that ran out its bound ends in, and a wait on a fence no one
     * will reach. */
    static constexpr std::uint32_t io_timeout = 0xC00000B5u;
    static constexpr std::uint64_t page_bytes = 4096u;
    /* The performance counter this answers, in the 100ns units Windows uses. */
    static constexpr std::uint64_t ticks_per_second = 10000000u;
    /* The beam scan_line reports: a 1080-line mode with 1125 lines in all,
     * its blank beginning at line 1080 as the driver numbers it. */
    static constexpr std::uint32_t beam_height = 1080u;
    static constexpr std::uint32_t beam_total_lines = 1125u;
    /* The memory each lock of an allocation up to this size addresses; several
     * may be held at once. A larger allocation's lock addresses all of it. */
    static constexpr std::uint32_t view_capacity = 8192u;
    static constexpr std::uint32_t max_locks = 16u;
    /* The largest context or escape private data this records for a test to
     * read back. */
    static constexpr std::uint32_t max_private = 64u;
    /* The largest allocation private driver data this keeps, which is a
     * descriptor and a shared object's record with room over (feature 053). */
    static constexpr std::uint32_t max_allocation_private = 128u;
    /* NT handles this hands out at once: one more than a pool block of
     * icd::SharedHandles holds, so a test can see that table grow past it
     * (feature 053). Every rig keeps one of these, some on the stack. */
    static constexpr std::uint32_t max_shared = 65u;
    /* The first NT handle it mints, which is nothing a D3DKMT_HANDLE could be
     * so a test that confuses the two fails rather than passing by luck. */
    static constexpr std::uint64_t first_shared_handle = 0x8000000000000004ull;
    /* The adapter number this mock stamps into every record it mints, standing
     * for the one SharedObjectTable makes from the performance counter, and
     * the first object id - both non-zero, which is what "minted" means. */
    static constexpr std::uint64_t shared_adapter = 0x5A5A0001u;
    static constexpr std::uint64_t first_object_id = 1u;
    /* The largest answer a test scripts for a transport's context: room for a
     * Vulkan device's memory properties and their framing. */
    static constexpr std::uint32_t max_answer = 1024u;
    static constexpr std::uint32_t not_supported = 0xC00000BBu;
    /* Chunks held at once - the driver's free chunk slots - and the resource
     * the first one answers: a host blob id, as the driver's are. */
    static constexpr std::uint32_t max_window_chunks = 16u;
    static constexpr std::uint32_t first_window_resource = HostBlobId::first;
    /* The window a rig's device offers unless a test says otherwise: as much
     * as any test opens at once, and free until written, since the mock maps
     * it unreserved. */
    static constexpr std::uint64_t rig_window_bytes = std::uint64_t{2} << 30u;
    /* STATUS_NOT_FOUND: what a handle this mock never minted answers. */
    static constexpr std::uint32_t not_found = 0xC0000225u;
    /* The LUID the Nth adapter added answers until a test sets another:
     * first_luid + N. */
    static constexpr std::uint64_t first_luid = 0x0000000100000000ull;
    static constexpr std::uint64_t default_local_budget = std::uint64_t{256} << 20u;
    static constexpr std::uint64_t default_non_local_budget = std::uint64_t{1} << 30u;
    /* Answers a test can queue, and commands the journal keeps. */
    static constexpr std::uint32_t max_queued = 320u;
    static constexpr std::uint32_t max_journal = 128u;
    /* The leading bytes of each journalled command a test can read back. */
    static constexpr std::uint32_t journal_bytes = 128u;

    MockKmt();
    ~MockKmt();
    MockKmt(const MockKmt &) = delete;
    MockKmt &operator=(const MockKmt &) = delete;

    Status        enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status        query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status        query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status        query_native_gpu_fences(std::uint32_t adapter, bool &supported) override;
    /* What describe_adapter set for the adapter, or nothing; set_adapter_features likewise. */
    Status        query_adapter_features(std::uint32_t adapter, UmdAdapterInfo &info) override;
    Status        query_adapter_location(std::uint32_t adapter, PciLocation &location) override;
    Status        query_adapter_name(std::uint32_t adapter, std::uint16_t *name, std::uint32_t capacity) override;
    Status        query_kmd_driver_version(std::uint32_t adapter, std::uint64_t &version) override;
    Status        query_segment_sizes(std::uint32_t adapter, std::uint64_t &dedicated_video,
                                      std::uint64_t &dedicated_system, std::uint64_t &shared_system) override;
    void          sleep_briefly() override;
    Status        close_adapter(std::uint32_t adapter) override;
    Status        adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
    Status        primary_display_luid(std::uint64_t &luid) override;
    /* The adapter the primary display is on; 0, the default, answers none. */
    void          set_primary_display_luid(std::uint64_t luid);
    Status        query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                     std::uint64_t &usage) override;
    Status        create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status        destroy_device(std::uint32_t device) override;
    Status        create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                    std::uint32_t &allocation) override;
    Status        destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status        create_shared_allocation(std::uint32_t device, std::uint8_t *private_data,
                                            std::uint32_t size, std::uint32_t &resource) override;
    Status        share_object(std::uint32_t device, std::uint32_t resource, std::uint64_t &handle) override;
    Status        destroy_shared_allocation(std::uint32_t device, std::uint32_t resource) override;
    Status        open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &resource,
                                     std::uint8_t *private_data, std::uint32_t capacity,
                                     std::uint32_t &written) override;
    Status        close_shared_object(std::uint64_t handle) override;
    Status        lock(std::uint32_t device, std::uint32_t allocation, void *&view) override;
    Status        unlock(std::uint32_t device, std::uint32_t allocation) override;
    Status        create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object) override;
    Status        destroy_paging_queue(std::uint32_t queue) override;
    Status        make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence) override;
    Status        map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                          std::uint64_t &address, std::uint64_t &paging_fence) override;
    Status        free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages) override;
    Status        wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence) override;
    Status        create_context(std::uint32_t device, std::uint32_t &context) override;
    Status        create_hw_queue_context(std::uint32_t device, const std::uint8_t *private_data,
                                          std::uint32_t size, std::uint32_t &context) override;
    Status        create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                        std::uint32_t &context) override;
    Status        destroy_context(std::uint32_t context) override;
    Status        submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                         const std::uint8_t *private_data, std::uint32_t private_size) override;
    Status        wait_for_context(std::uint32_t device, std::uint32_t context) override;
    /* Feature 049. A hardware queue is answered only while
     * `hardware_scheduling_` is on, which a test turns off to play an OS that
     * will not give this adapter one; a doorbell answers three pages of this
     * mock's own memory, which a test reads to see what the transport stored.
     */
    Status        create_hw_queue(std::uint32_t device, std::uint32_t context, GuestHwQueue &queue) override;
    Status        submit_to_hw_queue(std::uint32_t queue, std::uint64_t commands, std::uint64_t length,
                                     const std::uint8_t *private_data, std::uint32_t private_size,
                                     std::uint64_t progress_fence_id) override;
    Status        wait_for_hw_queue(std::uint32_t device, std::uint32_t progress_fence,
                                    std::uint64_t value) override;
    Status        destroy_hw_queue(std::uint32_t queue) override;
    Status        create_doorbell(std::uint32_t queue, std::uint32_t ring, std::uint32_t &doorbell, void *&page,
                                  const std::uint32_t *&status, std::uint64_t *&queued) override;
    Status        connect_doorbell(std::uint32_t doorbell) override;
    Status        destroy_doorbell(std::uint32_t doorbell) override;
    Status        notify_work_submission(std::uint32_t doorbell) override;

    /* What a test plays with: whether the OS gives this adapter a hardware
     * queue at all, what a process reads out of a doorbell's status page, and
     * the stores it made to the doorbell. */
    /* Feature 049: whether this OS gives the adapter hardware queues at all.
     * Off by default, as Windows is until hardware-accelerated GPU scheduling
     * is turned on (research.md §11), so a test that wants the doorbell path
     * asks for it. */
    void          allow_hardware_scheduling();
    void          refuse_hardware_scheduling();
    void          set_doorbell_status(std::uint32_t doorbell, std::uint32_t status);
    /* A driver that answers a doorbell and leaves out one of the three
     * addresses it is supposed to map - the page, the status or the queued
     * fence, numbered from 1 - which is what a process must not believe
     * (feature 049). */
    void          answer_doorbell_without_pages(std::uint32_t which);
    /* What is in the doorbell's page now: a process's store is a four-byte
     * write of a value that carries nothing, so what a test reads is whether
     * it was stored to since the page was last cleared. */
    std::uint32_t doorbell_page(std::uint32_t doorbell) const;
    void          clear_doorbell_page(std::uint32_t doorbell);
    std::uint64_t queued_fence(std::uint32_t doorbell) const;
    /* The handle of the `index`th doorbell ever made here, 0 past the end: how
     * a test reaches the doorbell a transport made for itself. */
    std::uint32_t doorbell_handle(std::uint32_t index) const;
    Status        wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    /* Where the beam is on the clock performance_counter answers: a blank
     * begins every blank period, the first at tick 0. */
    Status        scan_line(std::uint32_t adapter, std::uint32_t source, BeamPosition &position) override;
    /* As the driver answers them: a ContextViewsRecord with where the
     * context's ring and reply slot are, refused for a context made without a
     * record; a HostWindowRecord as open_host_window says; anything else
     * STATUS_NOT_SUPPORTED; any of them on a lost context refused. */
    Status        escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                         std::uint32_t size) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    /* An adapter enumeration opens; up to eight. */
    void          add_adapter(const PciIdentity &identity, std::uint32_t driver_version);
    /* The `index`th adapter presents `identity` from now on: avgpu presenting
     * its host GPU's vendor, device and revision. */
    void          set_identity(std::uint32_t index, const PciIdentity &identity);
    /* Where the adapter added `index`th (0-based) sits on the PCI bus, the
     * name its driver wrote (ASCII here, UTF-16 as answered) and its driver
     * package's version; past the adapters kept, nothing. */
    void          describe_adapter(std::uint32_t index, const PciLocation &location, const char *name,
                                   std::uint64_t kmd_version);
    /* The memory every adapter reports. */
    void          set_segment_sizes(std::uint64_t dedicated_video, std::uint64_t dedicated_system,
                                    std::uint64_t shared_system);
    /* The UmdAdapterInfo features that adapter's miniport answers. */
    void          set_adapter_features(std::uint32_t index, std::uint64_t features);
    /* The LUID of the adapter added `index`th (0-based); past the adapters
     * kept, nothing. */
    void          set_adapter_luid(std::uint32_t index, std::uint64_t luid);
    /* What a video memory query of `group` answers for every adapter; a group
     * D3DKMT does not define is not kept. */
    void          set_video_memory(MemorySegmentGroup group, std::uint64_t budget, std::uint64_t usage);
    /* The next enumeration claims this many adapters, whatever the capacity. */
    void          claim_adapters(std::uint32_t count);
    /* The Nth `call` from now (1-based) answers `code`; a success code answers success. */
    void          fail_at(Call call, std::uint32_t nth, std::uint32_t code);
    /* The first `count` calls fail and the ones after them do not: what a host
     * refusing an ask a driver then repeats differently looks like (feature
     * 051's profile ladder). */
    void          fail_first(Call call, std::uint32_t count, std::uint32_t code);
    /* The next `call` succeeds with a zero handle or address; a paging queue
     * comes without its fence object. */
    void          zero_next(Call call);
    /* The `nth` `call` from now (1 for the next) succeeds zeroed, as zero_next
     * does: for a call an earlier one of its kind comes before. */
    void          zero_at(Call call, std::uint32_t nth);
    /* The next shared create succeeds and leaves bytes where the record was
     * that no record decodes from: a driver that wrote nonsense back into the
     * creator's private data, which a caller reading the id must catch rather
     * than trust the status (feature 053). */
    void          scribble_the_next_record();
    /* The next open hands back fewer bytes than a record needs: what a driver
     * could do that leaves a caller reading an identity that is not there
     * (feature 053). */
    void          hand_back_a_short_record();
    /* The next share carries these bytes rather than the carrier's own: what
     * the *creating* process's kernel wrote is the only thing that crosses to
     * an opener, and an opener may not assume it was written by a driver of
     * this version, or by one at all (constitution V, feature 053). */
    void          carry_at_the_next_share(const std::uint8_t *bytes, std::uint32_t size);
    /* How far the clock moves at each vertical blank; the default is the
     * committed mode's 60 Hz. */
    void          set_blank_period(std::uint64_t ticks);
    /* A wait for a context costs this many ticks, as waiting for real work
     * does; 0 (the default) leaves the clock where it was. */
    void          spend_on_wait(std::uint64_t ticks);
    /* The next `count` waits for a context take what was submitted, as a host
     * that is working does, and still run out their bound: STATUS_IO_TIMEOUT. */
    void          time_out_while_consuming(std::uint32_t count);
    /* The next `count` waits for a context take nothing and still run out
     * their bound while the fence page's heartbeat moves: a batch the host GPU
     * is still running, slower than the wait. */
    void          time_out_while_beating(std::uint32_t count);
    /* The next wait for a context takes what was submitted, faults the fence
     * page (device lost) and runs out its bound: a device that gave up on the
     * context while the process waited. */
    void          time_out_faulted();
    /* The context's fence page faulted now, device lost, with no wait: a
     * host that lost the device an idle context runs on. False for a context
     * that is not one, or has no page. */
    bool          fault_page_of(std::uint32_t context);
    /* The next `count` waits for a context take nothing, move nothing and run
     * out their bound: a host that has stopped, once the counts above are
     * spent. */
    void          time_out_silently(std::uint32_t count);
    /* The next `count` waits for a context take nothing, move neither head
     * nor progress, and run out their bound while the device's pulse beats:
     * one host call blocking for the whole of them. */
    void          time_out_while_pulsing(std::uint32_t count);
    /* The context's fence page as the device writes it: its pulse's beat one
     * further, or a completed fence, the rest of the page kept. What a test
     * reading the clock (reads_clock) plays the device with. */
    void          pulse(std::uint32_t context);
    void          complete_on_page(std::uint32_t context, std::uint64_t fence);
    /* A submission through the kernel costs this many ticks, as the transition
     * into dxgkrnl does; a store to a doorbell costs nothing here, because it
     * is a write in the process and this mock never sees it (feature 049). */
    void          spend_on_submit(std::uint64_t ticks);
    /* Every `nth` wait for a context costs `ticks` more, as one the host was
     * slow to answer does: what a latency's median and fastest are judged by. */
    void          slow_every(std::uint32_t nth, std::uint64_t ticks);
    /* The frequency the performance counter answers; ticks_per_second until
     * set, and 0 is a clock that cannot say how long a tick is. */
    void          set_frequency(std::uint64_t frequency);
    /* A clock that moves when it is read, by `step` ticks, and tells `reader`
     * each time. Nothing moves and nobody is told until this is called, so a
     * test whose arithmetic counts ticks is untouched by it.
     *
     * It is what a single-threaded test has instead of a second thread: the
     * code under test spinning on the clock is the only moment at which a fake
     * can play something happening elsewhere - an ICD's event thread running a
     * callback while the application waits for it. A null reader is a clock
     * that only moves. */
    void          reads_clock(ClockReader *reader, std::uint64_t step);
    /* Every `nth` blank takes `ticks` longer, as one the waiter missed does. */
    void          slip_every(std::uint32_t nth, std::uint64_t ticks);
    /* From now on a wait on a context created with an AvContextRecord finds
     * `answer` (up to max_answer bytes) in the record's reply slot, for the
     * fence the last submission named, while the process holds a view of the
     * slot: how a test plays the device behind a transport it cannot reach. */
    void          answer_waits(const std::uint8_t *answer, std::uint32_t size);
    /* The next wait on such a context that finds no earlier queued answer finds
     * this one (up to max_answer bytes), before any answer_waits answers: how a
     * test plays a device through a sequence of calls. */
    void          queue_answer(const std::uint8_t *answer, std::uint32_t size);
    /* The Nth wait for a context from now (1-based) succeeds, writes nothing
     * and takes no queued answer: how a context the host faulted looks to its
     * process, since the driver completes what it queued and never reports
     * DMA_FAULTED, which bugchecks the guest. The driver knows the context
     * lost from then on, and refuses a bind or unbind on it. */
    void          withhold_answer_at(std::uint32_t nth);
    /* The Nth wait for a context from now (1-based) succeeds with the ring's
     * head where it was, and the head moves at the next clock read: how the
     * device looks when its fence is seen before the head it published. */
    void          consume_late_at(std::uint32_t nth);
    /* The same, with the head moving only at the `reads`th clock read after
     * that wait: a device whose fence the kernel signalled long before the
     * ring it publishes caught up. */
    void          consume_late_at(std::uint32_t nth, std::uint32_t reads);
    /* From now on a context views escape newer than `version` is refused with
     * STATUS_INVALID_PARAMETER, as a driver from before it refuses it. */
    void          answer_views_up_to(std::uint16_t version);
    /* The context's fence page as the driver maps it, null for none. */
    const std::uint8_t *fence_page_of(std::uint32_t context);
    /* The Nth wait for a context from now (1-based) runs `play(context)` before
     * any answer is written: how a test plays work the host does between a
     * submission and its wait, such as a GPU copy between two locked views. */
    void          play_at_wait(std::uint32_t nth, void (*play)(void *context), void *context);
    /* The next escape that names its handles rightly succeeds with these bytes
     * (cut to the escape's size) written over its data, whatever it asked. */
    void          answer_next_escape(const std::uint8_t *answer, std::uint32_t size);
    /* From now on an escape naming only an adapter - no device, no context, as
     * avinspect.exe's trace escapes are - is answered by `driver`; without
     * one such an escape is refused as the driver refuses what it does not
     * know. */
    void          answer_adapter_escapes(AdapterEscapes *driver);
    /* From now on the device has a host window of `bytes` (whole pages),
     * backed by memory this mock maps so a view is written and read like the
     * real one: an open is placed first fit from the window's start, in whole
     * pages, and answers the lowest host blob id no chunk holds and the
     * address of its place; a close names a chunk the same context holds;
     * destroying a context closes what it holds, as the driver does. With no
     * window an open is STATUS_NOT_SUPPORTED; one the window has no room for,
     * or past max_window_chunks held, is STATUS_INVALID_PARAMETER, as the
     * driver refuses it: nothing falls back to guest RAM. */
    void                open_host_window(std::uint64_t bytes);
    /* The next window open answers `code`, whatever room there is. */
    void                refuse_next_window_open(std::uint32_t code);
    /* Window chunks held now, and opens ever granted in the window. */
    std::uint32_t       window_chunks() const;
    std::uint32_t       window_opens() const;
    /* The memory the last open asked for, and the chunk `resource` was
     * opened as - no request for one not held. */
    HostMemoryRequest   last_window_request() const;
    HostMemoryRequest   window_request(std::uint32_t resource) const;
    /* Where chunk `resource` lies in the window, null for one not held. */
    std::uint8_t       *window_chunk_memory(std::uint32_t resource) const;
    /* The next chunk opened answers `bytes` rather than what it holds, as a
     * driver that miscounted would. */
    void                answer_next_chunk_bytes(std::uint64_t bytes);
    /* The ring - header, then data - and the reply slot of a live context made
     * with a record, or null. */
    std::uint8_t       *ring_of(std::uint32_t context);
    std::uint8_t       *reply_of(std::uint32_t context);
    /* The ring of the live context whose record named it by allocation
     * `name`, or null. */
    std::uint8_t       *ring_named(std::uint32_t name);
    /* The window's first byte, how a test plays the host on a chunk's
     * memory; null without a window. */
    std::uint8_t       *window_base() const;
    /* The driver takes wake events (WakeEventRecord), as one on a device
     * that counts raises does; refused as not supported until then. */
    void                accept_wake_events();
    /* The handle the last wake event taken named, 0 for none, and how many
     * were taken. */
    std::uint64_t       wake_event() const;
    std::uint32_t       wake_events() const;
    /* The data of the last escape as it was sent, and its size. */
    const std::uint8_t *last_escape() const;
    std::uint32_t       last_escape_size() const;
    /* Queued answers no wait has taken yet. */
    std::uint32_t queued_answers() const;
    /* Commands submitted on transports' rings, in order, and the id and size
     * (header included) of the one at `index`, 0 past the journal. */
    std::uint32_t commands() const;
    std::uint32_t command_id(std::uint32_t index) const;
    std::uint32_t command_size(std::uint32_t index) const;
    /* Its first journal_bytes bytes, header first - past a shorter command,
     * whatever the ring held there; null past the journal. */
    const std::uint8_t *command_bytes(std::uint32_t index) const;

    std::uint32_t calls(Call call) const;
    /* Adapters, devices, allocations, queues, contexts and mappings still open. */
    std::uint32_t open_objects() const;
    /* Calls naming a handle this mock never gave or already closed. */
    std::uint32_t misuses() const;
    /* Bytes of the view this mock hands out that are not zero: what was
     * written through the pointer a lock answered, counting a large view's
     * as they stood when it was unlocked and let go. */
    std::uint32_t view_filled() const;
    /* The buffer a locked allocation's view points into, or null: how a test
     * plays the device, reading and writing the guest's mapped memory. */
    std::uint8_t *view_of(std::uint32_t allocation);
    /* The private data of the last create_context_virtual, and its size. */
    const std::uint8_t *last_private() const;
    std::uint32_t       last_private_size() const;
    /* The private data of the last submit, and its size. */
    const std::uint8_t *last_submission() const;
    std::uint32_t       last_submission_size() const;
    /* The AllocationKind a live allocation was created with, or 0. */
    std::uint16_t       allocation_kind(std::uint32_t allocation);
    /* How many pages it was created with, or 0. */
    std::uint64_t       allocation_pages(std::uint32_t allocation);

private:
    enum class Kind : unsigned { none, adapter, device, allocation, queue, context, mapping, hw_queue,
                                doorbell };
    struct WindowChunk;
    /* One doorbell this mock handed out: the pages a process was given, what
     * it stored, and what its status page says (feature 049). */
    struct DoorbellPages {
        std::uint32_t handle;
        std::uint32_t queue;
        std::uint32_t page;      /* a 4-byte store lands here */
        std::uint32_t status;
        std::uint64_t queued;
        std::uint32_t stores;
        bool          connected;
    };
    /* A context's `ring` is the allocation its record names it by, and `lost`
     * whether a wait on it was withheld - how the driver sees a context the
     * host faulted. */
    struct Object {
        Kind          kind;
        std::uint32_t handle;
        std::uint32_t owner;
        std::uint64_t address;
        std::uint64_t pages;
        std::uint16_t allocation_kind;
        std::uint32_t ring;
        bool          lost;
        /* A context created as one a hardware queue may run on: dxgkrnl
         * refuses D3DKMTSubmitCommand for it outright (feature 049). */
        bool          hardware_scheduled;
        /* An allocation keeps the private driver data it was created with, as
         * the runtime does: it is what dxgkrnl hands the opening process, and
         * so the only thing a shared object's name travels on (feature 053). */
        std::uint8_t  created_with[max_allocation_private];
        std::uint32_t created_size;
        /* Asked for as a resource, which only a carrier is: dxgkrnl names a
         * resource and the allocations in it by handles of different types and
         * refuses each in the other's place, so this mock's one number stands
         * for both and remembers which it is (feature 053, research.md §17). */
        bool          resource;
        /* A context's ring and reply slot, as the driver maps them. */
        std::uint8_t *ring_memory = nullptr;
        std::uint64_t ring_bytes = 0u;
        /* The fence page is the page mapped right after the reply slot, so
         * that every object of this mock does not grow by a pointer for it:
         * completed_fence is the last submission's fence once a wait has
         * consumed it, as the device writes it. */
        std::uint8_t *reply_memory = nullptr;
        std::uint64_t reply_bytes = 0u;
    };
    /* One NT handle this mock minted: what it names, and the creator's bytes
     * as they were when it was minted. */
    struct SharedHandle {
        std::uint64_t handle;
        std::uint32_t allocation;
        std::uint8_t  carried[max_allocation_private];
        std::uint32_t carried_size;
    };
    static constexpr std::uint32_t max_adapters = 8u;
    /* More than a transport holds: one per context it runs. */
    static constexpr std::uint32_t max_doorbells = 8u;
    static constexpr std::uint32_t segment_groups = 2u;
    static constexpr std::uint64_t first_address = 0x100000000ull;

    DoorbellPages *doorbell_of(std::uint32_t handle);

    bool          begin(Call call);
    Status        refuse(std::uint32_t code);
    Status        succeed();
    bool          zeroed(Call call);
    bool          withheld();
    std::uint32_t open(Kind kind, std::uint32_t owner, std::uint64_t address, std::uint64_t pages);
    Object       *find(Kind kind, std::uint32_t handle);
    Object       *find_mapping(std::uint64_t address);
    bool          is_locked(std::uint32_t allocation) const;
    void          write_answer(const Object &context);
    Status        carry(const Object &context, std::uint64_t commands, std::uint64_t length,
                        const std::uint8_t *private_data, std::uint32_t private_size);
    void          take_doorbell_store(const Object &context);
    void          write_bytes(const Object &context, const std::uint8_t *answer, std::uint32_t size);
    void          journal(const Object &context);
    void          consume(const Object &context) const;
    /* The fence page's heartbeat one further, or its fault set: what a device
     * writes while it decodes a batch, and when it gives a context up. */
    void          beat(const Object &context) const;
    void          fault_fence_page(const Object &context) const;
    /* The context's fence page, past its reply slot; null with no memory. */
    std::uint8_t *fence_memory(const Object &context) const;
    bool          played();
    Status        context_views(std::uint32_t context, std::uint8_t *data, std::uint32_t size);
    Status        wake_event(const std::uint8_t *data, std::uint32_t size);
    /* The ring and reply slot the record sizes, mapped for `context`, with the
     * ring's header written, as the driver makes them. */
    void          map_context_memory(Object &context, std::uint8_t order, std::uint32_t reply_bytes);
    void          unmap_context_memory(Object &context);
    Status        window_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size);
    Status        open_window_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size,
                                    const HostWindowRecord &request);
    /* The lowest host blob id no chunk holds. */
    std::uint32_t free_window_resource() const;
    /* The first page from the window's start where `pages` fit, or
     * window_pages_ when none does. */
    std::uint64_t window_fit(std::uint64_t pages) const;
    void          close_window_chunks_of(std::uint32_t context);
    /* A chunk slot freed. */
    void          clear_chunk(WindowChunk &chunk);
    /* What an open answers for `pages`: their bytes, or what a test said. */
    std::uint64_t answered_bytes(std::uint64_t pages);
    Status        close(Kind kind, std::uint32_t handle);
    SharedHandle *shared_row(std::uint64_t handle);
    Status        misuse();

    PciIdentity   identities_[max_adapters];
    std::uint32_t versions_[max_adapters];
    std::uint64_t luids_[max_adapters];
    PciLocation    locations_[max_adapters];
    char          names_[max_adapters][64] = {};
    std::uint64_t kmd_versions_[max_adapters] = {};
    std::uint64_t adapter_features_[max_adapters] = {};
    std::uint64_t segment_sizes_[3] = {};
    std::uint64_t budgets_[segment_groups];
    std::uint64_t usages_[segment_groups];
    std::uint32_t adapter_count_;
    std::uint32_t claimed_;
    bool          claim_;
    std::uint32_t failures_[static_cast<unsigned>(Call::count)];
    std::uint32_t countdown_[static_cast<unsigned>(Call::count)];
    std::uint32_t failing_run_[static_cast<unsigned>(Call::count)];
    bool          zeroing_[static_cast<unsigned>(Call::count)];
    std::uint32_t zero_countdown_[static_cast<unsigned>(Call::count)] = {};
    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    Object        objects_[max_objects];
    std::uint8_t  views_[max_locks][view_capacity];
    std::uint32_t locked_[max_locks];
    std::uint8_t *large_views_[max_locks];
    std::uint64_t large_bytes_[max_locks];
    std::uint8_t  escape_[max_private];
    std::uint32_t escape_size_;
    std::uint8_t  escape_answer_[max_private];
    std::uint32_t escape_answer_size_;
    bool          escape_scripted_;
    bool          accepts_wakes_ = false;
    std::uint64_t wake_event_ = 0u;
    std::uint32_t wake_events_ = 0u;
    AdapterEscapes *adapter_escapes_;
    /* One host window chunk held: the resource it answered, the context that
     * holds it, and where it lies in the window, in pages; 0 resource is a
     * free slot. */
    struct WindowChunk {
        std::uint32_t resource;
        std::uint32_t context;
        std::uint64_t first;
        std::uint64_t pages;
        std::uint32_t memory;   /* the HostMemoryRequest it was opened with */
    };
    std::uint8_t *window_;
    std::uint64_t window_pages_;
    WindowChunk   window_chunks_[max_window_chunks];
    std::uint32_t window_opens_;
    std::uint32_t last_window_request_ = 0u;
    std::uint64_t chunk_bytes_answer_ = 0u;
    std::uint32_t window_refusal_;
    std::uint32_t play_countdown_;
    void        (*play_)(void *context);
    void         *play_context_;
    std::uint8_t  private_[max_private];
    std::uint32_t private_size_;
    std::uint8_t  submission_[max_private];
    std::uint32_t submission_size_;
    SharedHandle  shared_[max_shared];
    std::uint64_t next_shared_handle_;
    std::uint64_t next_object_id_;
    bool          scribble_record_;
    bool          short_record_;
    std::uint8_t  carried_next_[max_allocation_private];
    std::uint32_t carried_next_size_;
    bool          carry_next_;
    std::uint32_t next_handle_;
    std::uint64_t next_address_;
    std::uint64_t next_fence_;
    std::uint32_t success_code_;
    std::uint32_t last_code_;
    std::uint32_t misuses_;
    std::uint64_t ticks_;
    /* What reading the clock has added to it, which only a read changes -
     * hence mutable, since reading a clock is const to its caller. */
    mutable std::uint64_t read_ticks_;
    /* A frozen clock read over and over with no call between the reads is
     * code spinning until time passes, which no real clock lets last forever:
     * after spin_reads such reads each one moves the clock a millisecond. */
    static constexpr std::uint32_t spin_reads = 64u;
    mutable std::uint32_t          idle_reads_;
    std::uint64_t                  released_filled_ = 0u;
    std::uint64_t clock_step_;
    ClockReader  *clock_reader_;
    std::uint64_t blank_period_;
    std::uint32_t slip_nth_;
    std::uint64_t slip_ticks_;
    std::uint64_t wait_cost_;
    std::uint32_t timeouts_while_consuming_;
    std::uint32_t timeouts_while_beating_;
    std::uint32_t timeouts_while_pulsing_;
    bool          timeout_faulted_;
    std::uint32_t timeouts_silent_;
    std::uint64_t submit_cost_;
    std::uint32_t slow_nth_;
    std::uint64_t slow_ticks_;
    std::uint64_t frequency_;
    std::uint8_t  answer_[max_answer];
    std::uint32_t answer_size_;
    bool          answering_;
    std::uint8_t  queued_[max_queued][max_answer];
    std::uint32_t queued_sizes_[max_queued];
    std::uint32_t queued_count_;
    std::uint32_t queued_taken_;
    std::uint32_t withheld_countdown_;
    std::uint32_t late_countdown_ = 0u;
    mutable std::uint32_t late_reads_ = 0u;
    std::uint16_t newest_views_ = ContextViewsRecord::with_fence;
    mutable const Object *late_context_ = nullptr;
    /* Feature 049: whether the OS gives this adapter hardware queues, and the
     * doorbells it handed out. */
    bool          hardware_scheduling_;
    std::uint32_t doorbell_page_missing_;
    DoorbellPages doorbells_[max_doorbells];
    std::uint32_t doorbell_count_;
    /* What a hardware queue has finished, as dxgkrnl maps it read-only into
     * the process that made the queue, and the monitored fence object it is
     * reported on: the handle of the queue itself, so a wait names what it
     * waits for. */
    std::uint64_t queue_progress_[max_doorbells];
    std::uint32_t queue_progress_taken_;
    std::uint32_t journal_ids_[max_journal];
    std::uint32_t journal_sizes_[max_journal];
    std::uint8_t  journal_bytes_[max_journal][journal_bytes];
    std::uint32_t journal_count_;
    std::uint64_t primary_display_luid_ = 0u;
};

}  // namespace av

#endif  // AV_MOCK_KMT_HPP
