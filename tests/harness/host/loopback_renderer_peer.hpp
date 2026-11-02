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

    LoopbackRendererPeer(MockHost &qemu, const AvExecutorOps &executor);
    ~LoopbackRendererPeer();

    void started() override;
    void deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                 std::uint32_t count) override;
    bool answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                std::uint32_t capacity_descriptors, std::uint32_t &count) override;

    void                   fall_silent();
    std::uint32_t          generation() const;
    Status                 last_outcome() const;
    std::uint32_t          delivered() const;
    std::uint32_t          queued() const;
    RendererService       &service();
    MockRendererResources &resources();
    GuestMemoryTable      &memory();
    MockHost              &host();

private:
    struct Process {
        Process(MockHost &qemu, const AvExecutorOps &executor);
        ~Process();
        Process(const Process &) = delete;
        Process &operator=(const Process &) = delete;

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

}

#endif
