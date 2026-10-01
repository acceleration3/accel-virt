/* av-renderer's half of the device, in the test's own process: the real
 * RendererService over a MockHost of its own - the renderer's host, whose main
 * loop is the test thread as QEMU's MockHost's is - with MockRendererResources
 * mapping each guest region to the pointer the QEMU side registered, and the
 * renderer's host resolving guest extents through the GuestMemoryTable those
 * regions fill, as av-renderer's does. Every datagram the device sends is
 * carried out at once and its answer queued for the device's next receive,
 * so a test drives both processes' code on one thread with nothing in
 * between but the bytes and descriptors. Each start after the first that
 * found a renderer already spoken to is a new process: the old one's
 * contexts, blobs and regions go, and a fresh service answers. */
#ifndef AV_LOOPBACK_RENDERER_PEER_HPP
#define AV_LOOPBACK_RENDERER_PEER_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/renderer/doorbell_offers.hpp"
#include "device/renderer/guest_memory_table.hpp"
#include "device/renderer/renderer_service.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_renderer_resources.hpp"
#include "harness/host/renderer_peer.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class LoopbackRendererPeer final : public RendererPeer {
public:
    static constexpr std::uint32_t max_answers = 16u;

    /* `qemu` is the device's host, whose guest regions this renderer maps. */
    LoopbackRendererPeer(MockHost &qemu, const AvExecutorOps &executor);
    ~LoopbackRendererPeer();

    void started() override;
    void deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                 std::uint32_t count) override;
    bool answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                std::uint32_t capacity_descriptors, std::uint32_t &count) override;

    /* This renderer answers nothing from here on, as one that hung; the next
     * process started answers again. */
    void                   fall_silent();
    /* Processes started so far, the first included. */
    std::uint32_t          generation() const;
    /* What the service made of the last datagram delivered. */
    Status                 last_outcome() const;
    std::uint32_t          delivered() const;
    std::uint32_t          queued() const;
    RendererService       &service();
    MockRendererResources &resources();
    GuestMemoryTable      &memory();
    /* The renderer process's own host. */
    MockHost              &host();

private:
    /* One av-renderer process's state. */
    struct Process {
        Process(MockHost &qemu, const AvExecutorOps &executor);
        ~Process();
        Process(const Process &) = delete;
        Process &operator=(const Process &) = delete;

        /* A region the service added, remembered so it can be unmapped and
         * closed when the process goes: the table does not enumerate. */
        void remember_region(const std::uint8_t *bytes, std::size_t length);

        MockHost              host;
        MockRendererResources resources;
        GuestMemoryTable      memory;
        DoorbellOffers        doorbells;
        RendererService       service;
        std::uint32_t         regions[GuestMemoryTable::max_regions];
        std::uint32_t         region_count;
    };
    struct Answer {
        std::uint8_t bytes[RendererService::answer_capacity];
        std::size_t  length;
        /* Sent beside the answer, as SCM_RIGHTS would, or -1: still the
         * renderer's, so what crosses is a duplicate of it. */
        int          descriptor;
    };

    MockHost               &qemu_;
    const AvExecutorOps   &executor_;
    ObjectStorage<Process>  process_;
    Answer                  answers_[max_answers];
    std::uint32_t           first_;
    std::uint32_t           queued_;
    Status                  last_outcome_;
    std::uint32_t           delivered_;
    std::uint32_t           generation_;
    bool                    spoken_to_;
    bool                    silent_;
};

}  // namespace av

#endif  // AV_LOOPBACK_RENDERER_PEER_HPP
