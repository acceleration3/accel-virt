/* What lies between a process's store and the device's doorbell page in the
 * full-stack rig: on a machine, dxgkrnl maps a page into the process and
 * points it at the device's doorbell page while the doorbell is connected, and
 * at a page of its own while it is not. In this rig the driver's mapping is a
 * synthetic address only MockPlatform can reach, so a process is given a page
 * of real memory and this carries what lands in it to where the driver's own
 * store would have gone - or nowhere, while the doorbell is disconnected.
 *
 * Nothing carries it by itself: like the rig's interrupts, a store arrives on
 * the test thread between the calls a process makes, which is what makes a
 * test of "the store arrived late" or "not at all" writable.
 */
#ifndef AV_DOORBELL_FABRIC_HPP
#define AV_DOORBELL_FABRIC_HPP

#include <cstdint>

namespace av {

class MockPlatform;

class DoorbellFabric {
public:
    /* More than the rig's contexts: one page per doorbell a process holds. */
    static constexpr std::uint32_t max_pages = 16u;
    static constexpr std::uint32_t page_bytes = 4096u;

    explicit DoorbellFabric(MockPlatform &platform);
    DoorbellFabric(const DoorbellFabric &) = delete;
    DoorbellFabric &operator=(const DoorbellFabric &) = delete;

    /* A page for a process to store to, carrying nowhere until it is aimed;
     * null when every page is taken. */
    void         *reserve();
    /* Where this page's stores go from now on: the device page the driver
     * mapped at `kernel`. */
    void          aim(void *page, void *kernel);
    /* Stores to this page reach nothing, as they do while dxgkrnl points a
     * disconnected doorbell at a page of its own. */
    void          blind(void *page);
    void          release(void *page);

    /* Every store made since the last pass, carried where the driver's own
     * store would have gone: how many were carried. */
    std::uint32_t deliver();
    /* Stores carried since the rig started. */
    std::uint32_t delivered() const;
    /* Stores made to a page aimed at nothing: what a process storing to a
     * doorbell the kernel took back writes into, and what the device must
     * never see. */
    std::uint32_t dropped() const;

    /* No two pages carry to the same device page: a doorbell is one context's,
     * and no store may reach another's. */
    bool invariants_hold() const;

private:
    std::uint32_t index_of(const void *page) const;

    MockPlatform &platform_;
    /* Page-aligned so a doorbell page is what a process is given on a machine:
     * whole, and its own. */
    alignas(page_bytes) std::uint8_t bytes_[max_pages][page_bytes];
    bool          taken_[max_pages];
    /* Where the driver mapped the device page this one carries to, null while
     * the doorbell is disconnected. */
    void         *kernel_[max_pages];
    std::uint32_t delivered_;
    std::uint32_t dropped_;
};

}  // namespace av

#endif  // AV_DOORBELL_FABRIC_HPP
