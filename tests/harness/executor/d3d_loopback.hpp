/* The generated Direct3D client, run straight into a D3dBackend in the same
 * process: every command the client encodes is decoded by the generated
 * server and run on the host's Direct3D as it is sent, and a synchronous
 * command's answer is read back the way the guest reads its reply slot. What
 * the test proves is the whole host half - the encoding, the decoding, and
 * what DXVK drew - without a guest.
 *
 * The memory the guest shares with the host is its GPU's memory source's, as
 * the guest's is (docs/host-memory.md): the session's region one allocation,
 * and each Direct3D 12 heap or slice store one more, each imported into the
 * session as the device imports a blob, and read and written here through the
 * source's own mapping where the guest would use its view. */
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
    /* Allocations open at once besides the region. */
    static constexpr std::uint32_t max_memories = 8u;
    /* The domain the loopback's guest device allocates in. */
    static constexpr std::uint32_t domain = 1u;

    /* With `source` and `region_bytes`, the session's region is that many
     * bytes of the source's readback memory, imported as a session's first
     * import is: the first `lent_bytes` of it lend bulk data, and the rest is
     * the test's to address by offset. A region the source refuses is none. */
    explicit D3dLoopback(D3dBackend &backend, HostMemorySource *source = nullptr, std::size_t region_bytes = 0u,
                         std::size_t lent_bytes = 0u);
    ~D3dLoopback();
    D3dLoopback(const D3dLoopback &) = delete;
    D3dLoopback &operator=(const D3dLoopback &) = delete;

    /* The region at `offset`; null with no region, or past its end. */
    std::uint8_t *guest(std::size_t offset) const;
    /* `bytes` of `request`'s memory allocated by the source and imported into
     * the session, as the guest opens a Direct3D 12 heap's or a slice
     * store's: the handle commands name it by. 0 when there is no source, no
     * room here, or the source or the session refuses it. */
    std::uint64_t open_memory(HostMemoryRequest request, std::uint64_t bytes);
    /* What the memory `handle` names holds at `offset`; null for a handle
     * naming nothing open, and past its end. */
    std::uint8_t *memory(std::uint64_t handle, std::uint64_t offset = 0u) const;
    /* The source's memory `handle` names, for a test to import as the
     * session would; null for a handle naming nothing open. */
    const SourceMemory *source_memory(std::uint64_t handle) const;
    /* The memory `handle` names let go of by the session and the source, as a
     * guest closing it does; nothing for a handle naming nothing open. */
    void          close_memory(std::uint64_t handle);

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    /* Commands the backend refused - a frame it did not decode, or one whose
     * values it judged wrong. */
    uint32_t refused() const;

private:
    /* `awaited`: the guest waits on the answer, so it is lent room. */
    bool run(uint32_t command_id, WireWriter &writer, bool awaited, std::size_t &answered);
    /* An allocation under a key of its own, imported into `blob`. */
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

}  // namespace av

#endif  // AV_D3D_LOOPBACK_HPP
