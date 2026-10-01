/* The generated Direct3D client, run straight into a D3dBackend in the same
 * process: every command the client encodes is decoded by the generated
 * server and run on the host's Direct3D as it is sent, and a synchronous
 * command's answer is read back the way the guest reads its reply slot. What
 * the test proves is the whole host half - the encoding, the decoding, and
 * what DXVK drew - without a guest. */
#ifndef AV_D3D_LOOPBACK_HPP
#define AV_D3D_LOOPBACK_HPP

#include "device/executor/imported_blob.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"
#include "proto/wire/wire_arena.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class D3dBackend;

class D3dLoopback final : public ClientTransport {
public:
    static constexpr std::size_t command_bytes = 1u << 20u;
    static constexpr std::size_t reply_bytes = 64u * 1024u;
    static constexpr std::size_t arena_bytes = 4u << 20u;

    /* With `guest_bytes`, a session blob arena of that many bytes of this
     * process's memory stands for the guest's: the backend adopts it as it
     * would a blob the guest imported, the first `lent_bytes` of it lend bulk
     * data, and the rest is the test's to place heaps in by offset. */
    explicit D3dLoopback(D3dBackend &backend, std::size_t guest_bytes = 0u, std::size_t lent_bytes = 0u);
    ~D3dLoopback();

    /* The guest memory at `offset` of the arena; null with no arena. */
    uint8_t *guest(std::size_t offset) const;

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

    D3dBackend     &backend_;
    uint8_t        *command_;
    uint8_t        *reply_;
    uint8_t        *arena_block_;
    WireArena       arena_;
    WireArena       decode_arena_;
    ClientBlobArena blobs_;
    uint32_t        refused_;
    uint8_t        *guest_;
    std::size_t     guest_bytes_;
    ImportedBlob    blob_;
};

}  // namespace av

#endif  // AV_D3D_LOOPBACK_HPP
