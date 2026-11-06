#ifndef AV_TRANSFER_ROOM_HPP
#define AV_TRANSFER_ROOM_HPP

#include "proto/wire/guest_bytes.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClientTransport;

class TransferRoom {
public:
    static constexpr size_t command_bytes = 4096u;
    static constexpr size_t lent_bytes = static_cast<size_t>(GuestBytes::slot_bytes / 2u);
    static constexpr size_t floor_bytes = size_t{4} << 20u;
    static constexpr size_t lent_read_bytes = size_t{4} << 20u;

    TransferRoom(size_t staging_bytes, size_t reply_bytes);

    size_t inline_write() const;
    size_t inline_read() const;
    bool   inline_both(size_t bytes) const;
    size_t lent_piece(ClientTransport &wire, uint64_t left) const;
    size_t upload_most(ClientTransport &wire, uint64_t total) const;
    size_t write_piece(ClientTransport &wire, uint64_t left) const;
    size_t read_piece(ClientTransport &wire) const;

private:
    size_t staging_bytes_;
    size_t reply_bytes_;
};

}

#endif
