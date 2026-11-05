#ifndef AV_ROOMED_TRANSPORT_HPP
#define AV_ROOMED_TRANSPORT_HPP

#include "harness/transport/noted_counters.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class RoomedTransport : public ClientTransport {
public:
    [[nodiscard]] bool make_room_for_blobs(const std::uint64_t *sizes, std::uint32_t count) override;
    ClientBlobLease     open_region(std::uint64_t bytes) override;
    void                close_region(const ClientBlobLease &region) override;
    const TransferRoom &room() const override;
    void                note(ChannelCounter counter, std::uint64_t amount) override;
    std::uint64_t       noted(ChannelCounter counter) const;
    std::uint32_t       room_requests() const;
    std::uint32_t       last_room_blobs() const;
    std::uint64_t       last_room_bytes() const;
    std::uint32_t       room_asked_at() const;

protected:
    std::uint32_t       tick();

    RoomedTransport(std::size_t staging_bytes, std::size_t reply_bytes);
    ~RoomedTransport() = default;

private:
    TransferRoom  room_;
    NotedCounters noted_;
    std::uint32_t events_;
    std::uint32_t room_requests_;
    std::uint32_t last_room_blobs_;
    std::uint64_t last_room_bytes_;
    std::uint32_t room_asked_at_;
};

}

#endif
