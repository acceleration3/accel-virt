/* A D3DKMT that answers what a test scripts: adapters with their identity and
 * driver version, handles handed out in sequence, any call failed with an
 * NTSTATUS or answered with a zero handle, and a count of every object still
 * open and every call made with a handle it never gave. Allocations decode
 * their descriptor as the driver does; submissions must land inside a
 * mapping of the submitting context's device. */
#ifndef AV_MOCK_KMT_HPP
#define AV_MOCK_KMT_HPP

#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"

#include <cstdint>

namespace av {

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
        destroy_allocation,
        create_paging_queue,
        destroy_paging_queue,
        make_resident,
        map_gpu_virtual_address,
        free_gpu_virtual_address,
        wait_for_fence,
        create_context,
        destroy_context,
        submit,
        wait_for_context,
        wait_for_vertical_blank,
        count
    };
    static constexpr std::uint32_t max_objects = 32u;
    static constexpr std::uint32_t invalid_parameter = 0xC000000Du;
    static constexpr std::uint32_t pending = 0x00000103u;
    static constexpr std::uint64_t page_bytes = 4096u;
    /* The performance counter this answers, in the 100ns units Windows uses. */
    static constexpr std::uint64_t ticks_per_second = 10000000u;

    MockKmt();

    Status        enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status        query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status        query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status        close_adapter(std::uint32_t adapter) override;
    Status        create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status        destroy_device(std::uint32_t device) override;
    Status        create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                    std::uint32_t &allocation) override;
    Status        destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status        create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object) override;
    Status        destroy_paging_queue(std::uint32_t queue) override;
    Status        make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence) override;
    Status        map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                          std::uint64_t &address, std::uint64_t &paging_fence) override;
    Status        free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages) override;
    Status        wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence) override;
    Status        create_context(std::uint32_t device, std::uint32_t &context) override;
    Status        destroy_context(std::uint32_t context) override;
    Status        submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length) override;
    Status        wait_for_context(std::uint32_t device, std::uint32_t context) override;
    Status        wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    /* An adapter enumeration opens; up to eight. */
    void          add_adapter(const PciIdentity &identity, std::uint32_t driver_version);
    /* The next enumeration claims this many adapters, whatever the capacity. */
    void          claim_adapters(std::uint32_t count);
    /* The Nth `call` from now (1-based) answers `code`; a success code answers success. */
    void          fail_at(Call call, std::uint32_t nth, std::uint32_t code);
    /* The next `call` succeeds with a zero handle or address; a paging queue
     * comes without its fence object. */
    void          zero_next(Call call);
    /* How far the clock moves at each vertical blank; the default is the
     * committed mode's 60 Hz. */
    void          set_blank_period(std::uint64_t ticks);
    /* A wait for a context costs this many ticks, as waiting for real work
     * does; 0 (the default) leaves the clock where it was. */
    void          spend_on_wait(std::uint64_t ticks);
    /* Every `nth` blank takes `ticks` longer, as one the waiter missed does. */
    void          slip_every(std::uint32_t nth, std::uint64_t ticks);

    std::uint32_t calls(Call call) const;
    /* Adapters, devices, allocations, queues, contexts and mappings still open. */
    std::uint32_t open_objects() const;
    /* Calls naming a handle this mock never gave or already closed. */
    std::uint32_t misuses() const;

private:
    enum class Kind : unsigned { none, adapter, device, allocation, queue, context, mapping };
    struct Object {
        Kind          kind;
        std::uint32_t handle;
        std::uint32_t owner;
        std::uint64_t address;
        std::uint64_t pages;
    };
    static constexpr std::uint32_t max_adapters = 8u;
    static constexpr std::uint64_t first_address = 0x100000000ull;

    bool          begin(Call call);
    Status        refuse(std::uint32_t code);
    Status        succeed();
    bool          zeroed(Call call);
    std::uint32_t open(Kind kind, std::uint32_t owner, std::uint64_t address, std::uint64_t pages);
    Object       *find(Kind kind, std::uint32_t handle);
    Object       *find_mapping(std::uint64_t address);
    Status        close(Kind kind, std::uint32_t handle);
    Status        misuse();

    PciIdentity   identities_[max_adapters];
    std::uint32_t versions_[max_adapters];
    std::uint32_t adapter_count_;
    std::uint32_t claimed_;
    bool          claim_;
    std::uint32_t failures_[static_cast<unsigned>(Call::count)];
    std::uint32_t countdown_[static_cast<unsigned>(Call::count)];
    bool          zeroing_[static_cast<unsigned>(Call::count)];
    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    Object        objects_[max_objects];
    std::uint32_t next_handle_;
    std::uint64_t next_address_;
    std::uint64_t next_fence_;
    std::uint32_t success_code_;
    std::uint32_t last_code_;
    std::uint32_t misuses_;
    std::uint64_t ticks_;
    std::uint64_t blank_period_;
    std::uint32_t slip_nth_;
    std::uint64_t slip_ticks_;
    std::uint64_t wait_cost_;
};

}  // namespace av

#endif  // AV_MOCK_KMT_HPP
