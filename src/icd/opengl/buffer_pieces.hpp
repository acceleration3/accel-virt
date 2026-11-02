#ifndef AV_GLICD_BUFFER_PIECES_HPP
#define AV_GLICD_BUFFER_PIECES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class BufferPieces {
public:
    static constexpr size_t max_bytes = size_t{1} << 30u;

    explicit BufferPieces(ClientTransport &wire);
    BufferPieces(const BufferPieces &) = delete;
    BufferPieces &operator=(const BufferPieces &) = delete;

    bool cuts_write(intptr_t size, const void *data);
    bool cuts_read(intptr_t size, const void *data) const;
    bool lends_whole(size_t bytes);

    void write(uint32_t target, intptr_t offset, size_t bytes, const uint8_t *from);
    void write_named(uint32_t buffer, intptr_t offset, size_t bytes, const uint8_t *from);
    void write_named_ext(uint32_t buffer, intptr_t offset, size_t bytes, const uint8_t *from);
    void read(uint32_t target, intptr_t offset, size_t bytes, uint8_t *into);
    void read_named(uint32_t buffer, intptr_t offset, size_t bytes, uint8_t *into);
    void read_named_ext(uint32_t buffer, intptr_t offset, size_t bytes, uint8_t *into);

    uint32_t commands() const;

private:
    ClientTransport &wire_;
    uint32_t         commands_;
};

}

#endif
