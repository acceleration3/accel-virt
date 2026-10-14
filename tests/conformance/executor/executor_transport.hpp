/* The generated Vulkan client's commands, delivered through an executor
 * library's own execute: what a guest's ICD sends, with no guest and no
 * device between, so the conformance suite drives a library by the same
 * encoding a guest does rather than by bytes it wrote by hand. */
#ifndef AV_EXECUTOR_TRANSPORT_HPP
#define AV_EXECUTOR_TRANSPORT_HPP

#include "device/abi/av_executor.h"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"

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
    [[nodiscard]] TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override;
    [[nodiscard]] TransportResult send_sync(std::uint32_t command_id, WireWriter &writer,
                                            WireReader &reply) override;
    void                          end_call() override;
    WireArena                    &decode_arena() override;
    ClientBlobArena              &blobs() override;

    /* Whether the library ran the last command sent, which is all a command
     * with no answer says. */
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
    bool                 ran_;
};

}  // namespace av

#endif  // AV_EXECUTOR_TRANSPORT_HPP
