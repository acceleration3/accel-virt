/* A buffer's bytes moved between the application's memory and the host in
 * commands that each fit what one command carries inline (TransferRoom): the
 * room a request has going and the reply slot coming back, less room for the
 * command's own fields.
 *
 * One command larger than either is not a short transfer but a lost one: its
 * writer overflows and it is dropped, its answer does not fit the reply slot,
 * or the host's decode arena has no room for it and the context faults. So
 * without a blob arena, or with one that cannot lend the bytes whole, a
 * transfer of any size goes as many commands as it takes
 * (docs/host-memory.md). Each read is answered before the next is sent, and
 * the writes are queued in order, so the pieces land as one transfer would. */
#ifndef AV_GLICD_BUFFER_PIECES_HPP
#define AV_GLICD_BUFFER_PIECES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::glicd {

class BufferPieces {
public:
    /* The most a transfer is cut for. Past it a size is not a transfer but a
     * negative GLsizeiptr read as unsigned, or more than any buffer holds, and
     * goes to the host whole to be refused there. */
    static constexpr size_t max_bytes = size_t{1} << 30u;

    explicit BufferPieces(ClientTransport &wire);
    BufferPieces(const BufferPieces &) = delete;
    BufferPieces &operator=(const BufferPieces &) = delete;

    /* Whether `size` bytes at `data` going to the host are cut: more than a
     * command carries inline, and more than the blob arena can lend whole
     * now. Nothing to send, or a size below zero, goes whole - for the host
     * to treat as no data, or to refuse. */
    bool cuts_write(intptr_t size, const void *data);
    /* Whether `size` bytes into `data` coming back are cut: more than one
     * reply holds. */
    bool cuts_read(intptr_t size, const void *data) const;
    /* Whether the blob arena can lend `bytes` whole now: a pixel read lent
     * them is answered in the arena rather than in the reply. */
    bool lends_whole(size_t bytes);

    /* glBufferSubData at `target`, glNamedBufferSubData and
     * glNamedBufferSubDataEXT, in pieces. */
    void write(uint32_t target, intptr_t offset, size_t bytes, const uint8_t *from);
    void write_named(uint32_t buffer, intptr_t offset, size_t bytes, const uint8_t *from);
    void write_named_ext(uint32_t buffer, intptr_t offset, size_t bytes, const uint8_t *from);
    /* glGetBufferSubData and its named spellings, in pieces. */
    void read(uint32_t target, intptr_t offset, size_t bytes, uint8_t *into);
    void read_named(uint32_t buffer, intptr_t offset, size_t bytes, uint8_t *into);
    void read_named_ext(uint32_t buffer, intptr_t offset, size_t bytes, uint8_t *into);

    /* Commands sent, which is what a test counts a transfer's pieces by. */
    uint32_t commands() const;

private:
    ClientTransport &wire_;
    uint32_t         commands_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_PIECES_HPP
