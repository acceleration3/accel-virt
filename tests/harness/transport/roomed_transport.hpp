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
    [[nodiscard]] bool make_room_for_blobs(std::size_t bytes) override;
    const TransferRoom &room() const override;
    void                note(ChannelCounter counter, std::uint64_t amount) override;
    std::uint64_t       noted(ChannelCounter counter) const;

protected:
    RoomedTransport(std::size_t staging_bytes, std::size_t reply_bytes);
    ~RoomedTransport() = default;

private:
    TransferRoom  room_;
    NotedCounters noted_;
};

}

#endif
