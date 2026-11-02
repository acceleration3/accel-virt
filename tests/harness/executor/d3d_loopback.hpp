#ifndef AV_D3D_LOOPBACK_HPP
#define AV_D3D_LOOPBACK_HPP

#include "executor/imported_blob.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/wire/wire_arena.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class D3dBackend;
class HostMemorySource;
class SourceMemory;

class D3dLoopback final : public ClientTransport {
public:
    static constexpr std::size_t   command_bytes = 1u << 20u;
    static constexpr std::size_t   reply_bytes = 64u * 1024u;
    static constexpr std::size_t   arena_bytes = 4u << 20u;
    static constexpr std::uint32_t max_memories = 8u;
    static constexpr std::uint32_t domain = 1u;

    explicit D3dLoopback(D3dBackend &backend, HostMemorySource *source = nullptr, std::size_t region_bytes = 0u,
                         std::size_t lent_bytes = 0u);
    ~D3dLoopback();
    D3dLoopback(const D3dLoopback &) = delete;
    D3dLoopback &operator=(const D3dLoopback &) = delete;

    std::uint8_t *guest(std::size_t offset) const;
    std::uint64_t open_memory(HostMemoryRequest request, std::uint64_t bytes);
    std::uint8_t *memory(std::uint64_t handle, std::uint64_t offset = 0u) const;
    const SourceMemory *source_memory(std::uint64_t handle) const;
    void          close_memory(std::uint64_t handle);

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    uint32_t refused() const;

private:
    bool run(uint32_t command_id, WireWriter &writer, bool awaited, std::size_t &answered);
    bool open(HostMemoryRequest request, std::uint64_t bytes, ImportedBlob &blob);
    void close(ImportedBlob &blob);

    D3dBackend       &backend_;
    HostMemorySource *source_;
    uint8_t          *command_;
    uint8_t          *reply_;
    uint8_t          *arena_block_;
    WireArena         arena_;
    WireArena         decode_arena_;
    ClientBlobArena   blobs_;
    uint32_t          refused_;
    uint64_t          answers_;
    std::uint64_t     next_key_;
    ImportedBlob      region_;
    ImportedBlob      memories_[max_memories];
};

}

#endif
