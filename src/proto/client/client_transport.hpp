#ifndef AV_CLIENT_TRANSPORT_HPP
#define AV_CLIENT_TRANSPORT_HPP

#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClientBlobArena;
class TransferRoom;

enum class TransportResult : uint32_t {
    ok = 0u,
    failed
};

class ClientTransport {
public:
    ClientTransport(const ClientTransport &) = delete;
    ClientTransport &operator=(const ClientTransport &) = delete;

    virtual WireWriter request_writer() = 0;
    [[nodiscard]] virtual TransportResult send_async(uint32_t command_id, WireWriter &writer) = 0;
    [[nodiscard]] virtual TransportResult send_sync(uint32_t command_id, WireWriter &writer,
                                                    WireReader &reply) = 0;
    virtual void end_call() = 0;
    virtual WireArena &decode_arena() = 0;
    virtual ClientBlobArena &blobs() = 0;
    [[nodiscard]] virtual bool make_room_for_blobs(size_t bytes) = 0;
    virtual const TransferRoom &room() const = 0;

protected:
    ClientTransport() = default;
    ~ClientTransport() = default;
};

}

#endif
