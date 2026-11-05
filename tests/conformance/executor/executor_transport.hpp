#ifndef AV_EXECUTOR_TRANSPORT_HPP
#define AV_EXECUTOR_TRANSPORT_HPP

#include "device/abi/av_executor.h"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ExecutorTransport final : public ClientTransport {
public:
    ExecutorTransport(const AvExecutorOps &ops, AvExecutor *executor);
    ExecutorTransport(const ExecutorTransport &) = delete;
    ExecutorTransport &operator=(const ExecutorTransport &) = delete;
    ~ExecutorTransport() = default;

    WireWriter                    request_writer() override;
    WireWriter                    request_writer_with_room(const std::uint64_t *sizes, std::uint32_t count) override;
    [[nodiscard]] TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override;
    [[nodiscard]] TransportResult send_sync(std::uint32_t command_id, WireWriter &writer,
                                            WireReader &reply) override;
    void                          end_call() override;
    WireArena                    &decode_arena() override;
    ClientBlobArena              &blobs() override;
    [[nodiscard]] bool            make_room_for_blobs(const std::uint64_t *sizes, std::uint32_t count) override;
    ClientBlobLease               open_region(std::uint64_t bytes) override;
    void                          close_region(const ClientBlobLease &region) override;
    const TransferRoom           &room() const override;
    void                          note(ChannelCounter counter, std::uint64_t amount) override;

    bool ran() const;

private:
    TransportResult deliver(std::uint32_t command_id, WireWriter &writer, std::size_t &answered);

    const AvExecutorOps &ops_;
    AvExecutor          *executor_;
    std::uint8_t         request_[4096];
    std::uint8_t         reply_[4096];
    std::uint8_t         block_[4096];
    WireArena            arena_;
    ClientBlobArena      blobs_;
    TransferRoom         room_;
    bool                 ran_;
};

}

#endif
