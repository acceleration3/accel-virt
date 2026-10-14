/* A ClientTransport that is an OpenGL host's buffers, framebuffer and one
 * texture, for the transfers the ICD cuts into pieces or stages in a pixel
 * buffer (BufferPieces, PixelStagingBuffer).
 *
 * What the generated client sends is decoded by the generated server into a
 * fake driver of its own, so a test states what the host holds after a
 * transfer, not which commands were sent - and the commands are counted too.
 * Every room is the size it is between a guest and the executor: the request
 * slot is a context's staging, the reply slot its reply slot, and the decode
 * arena the executor's. A command too large for any of them is refused as
 * the executor refuses it, which faults a real context; `refusals` counts
 * those. Memory it is lent is a context's blob arena. */
#ifndef AV_GL_TRANSFER_HOST_HPP
#define AV_GL_TRANSFER_HOST_HPP

#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/wire/wire_arena.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class GlTransferHost final : public ClientTransport {
public:
    static constexpr std::uint32_t max_buffers = 8u;
    /* The framebuffer and the texture: RGBA8, rows longer than a reply. */
    static constexpr GLsizei       width = 1024;
    static constexpr GLsizei       height = 512;
    static constexpr std::size_t   image_bytes = std::size_t{width} * height * 4u;
    static constexpr std::size_t   max_buffer_bytes = std::size_t{8} << 20u;

    GlTransferHost();
    ~GlTransferHost();
    GlTransferHost(const GlTransferHost &) = delete;
    GlTransferHost &operator=(const GlTransferHost &) = delete;

    /* `bytes` at `block` are the blob arena from now on. */
    void lend(std::uint8_t *block, std::size_t bytes);
    /* glGenBuffers answers no name from now on, as a host out of them. */
    void name_nothing();

    /* The framebuffer's pixel (x, y), as four bytes packed little-endian. */
    std::uint32_t framebuffer_pixel(GLsizei x, GLsizei y) const;
    std::uint32_t texture_pixel(GLsizei x, GLsizei y) const;
    /* Buffer `name`'s storage and its size; null for none. */
    const std::uint8_t *buffer(GLuint name) const;
    std::size_t         buffer_size(GLuint name) const;
    GLuint              bound(GLenum target) const;
    /* Names made and not deleted. */
    std::uint32_t       live_buffers() const;
    std::uint32_t       generated() const;

    std::uint32_t commands() const;
    std::uint32_t count_of(std::uint32_t command_id) const;
    /* Commands the host could not decode or answer: each faults a context. */
    std::uint32_t refusals() const;
    /* The most bytes one request carried. */
    std::size_t   largest_request() const;

    WireWriter      request_writer() override;
    [[nodiscard]] TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override;
    [[nodiscard]] TransportResult send_sync(std::uint32_t command_id, WireWriter &writer,
                                            WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    /* The driver's entry points, as the table calls them. */
    void gen_buffers(GLsizei count, GLuint *names);
    void delete_buffers(GLsizei count, const GLuint *names);
    void bind_buffer(GLenum target, GLuint name);
    void buffer_data(GLenum target, GLsizeiptr size, const void *data);
    void named_buffer_data(GLuint name, GLsizeiptr size, const void *data);
    void buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
    void named_buffer_sub_data(GLuint name, GLintptr offset, GLsizeiptr size, const void *data);
    void get_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, void *data);
    void get_named_buffer_sub_data(GLuint name, GLintptr offset, GLsizeiptr size, void *data);
    void integer(GLenum pname, GLint *value) const;
    void pixel_store(GLenum pname, GLint value);
    void read_pixels(GLint x, GLint y, GLsizei columns, GLsizei rows, GLenum format, GLenum type, void *pixels);
    void tex_sub_image(GLint x, GLint y, GLsizei columns, GLsizei rows, GLenum format, GLenum type,
                       const void *pixels);
    std::uint8_t *lent(std::uint64_t at, std::uint64_t bytes);

private:
    static constexpr std::size_t request_bytes = 256u * 1024u;
    static constexpr std::size_t reply_bytes = 64u * 1024u;
    static constexpr std::size_t arena_bytes = std::size_t{1} << 20u;
    static constexpr std::uint32_t max_counted = 64u;

    struct Buffer {
        std::uint8_t *bytes;
        std::size_t   size;
        bool          live;
    };
    /* The six values of a direction's pixel store: alignment, row length,
     * skip rows, skip images, skip pixels, image height. */
    struct Store {
        GLint values[6];
    };

    bool    dispatch(std::uint32_t command_id, WireWriter &writer, WireWriter &answer);
    Buffer *named(GLuint name);
    Buffer *at_target(GLenum target);
    /* Where pixel (column, row) of a transfer lies in client memory laid out
     * by `store`, or the buffer bound at `target` when one is. */
    std::size_t layout_offset(const Store &store, GLsizei columns, GLint column, GLint row) const;

    std::uint8_t   *request_;
    std::uint8_t   *reply_;
    std::uint8_t   *block_;
    std::uint8_t   *framebuffer_;
    std::uint8_t   *texture_;
    Buffer          buffers_[max_buffers];
    GLuint          pack_binding_;
    GLuint          unpack_binding_;
    GLuint          other_binding_;
    Store           pack_;
    Store           unpack_;
    bool            name_nothing_;
    std::uint32_t   generated_;
    std::uint32_t   commands_;
    std::uint32_t   ids_[max_counted];
    std::uint32_t   id_counts_[max_counted];
    std::uint32_t   id_kinds_;
    std::uint32_t   refusals_;
    std::size_t     largest_;
    std::size_t     answered_;
    std::uint8_t   *lent_;
    std::size_t     lent_bytes_;
    WireArena       host_arena_;
    WireArena       guest_arena_;
    ClientBlobArena blobs_;
    glproto::RealGl table_;
};

}  // namespace av

#endif  // AV_GL_TRANSFER_HOST_HPP
