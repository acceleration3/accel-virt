#include "harness/icd/gl_transfer_host.hpp"

#include "proto/generated/opengl/gl_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdlib>
#include <cstring>

namespace av {

namespace {

constexpr GLenum pixel_pack_buffer = 0x88EBu, pixel_unpack_buffer = 0x88ECu;
constexpr GLenum pack_binding = 0x88EDu, unpack_binding = 0x88EFu;
constexpr GLenum pack_store[6] = {0x0D05u, 0x0D02u, 0x0D03u, 0x806Bu, 0x0D04u, 0x806Cu};
constexpr GLenum unpack_store[6] = {0x0CF5u, 0x0CF2u, 0x0CF3u, 0x806Du, 0x0CF4u, 0x806Eu};
constexpr GLenum gl_rgba = 0x1908u, gl_unsigned_byte = 0x1401u;
constexpr std::size_t pixel_bytes = 4u;

GlTransferHost *live = nullptr;

void entry_gen_buffers(GLsizei n, GLuint *names) { live->gen_buffers(n, names); }
void entry_delete_buffers(GLsizei n, const GLuint *names) { live->delete_buffers(n, names); }
void entry_bind_buffer(GLenum target, GLuint name) { live->bind_buffer(target, name); }
void entry_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum) { live->buffer_data(target, size, data); }
void entry_buffer_storage(GLenum target, GLsizeiptr size, const void *data, GLbitfield)
{
    live->buffer_data(target, size, data);
}
void entry_named_buffer_data(GLuint name, GLsizeiptr size, const void *data, GLenum)
{
    live->named_buffer_data(name, size, data);
}
void entry_named_buffer_storage(GLuint name, GLsizeiptr size, const void *data, GLbitfield)
{
    live->named_buffer_data(name, size, data);
}
void entry_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    live->buffer_sub_data(target, offset, size, data);
}
void entry_named_buffer_sub_data(GLuint name, GLintptr offset, GLsizeiptr size, const void *data)
{
    live->named_buffer_sub_data(name, offset, size, data);
}
void entry_get_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
    live->get_buffer_sub_data(target, offset, size, data);
}
void entry_get_named_buffer_sub_data(GLuint name, GLintptr offset, GLsizeiptr size, void *data)
{
    live->get_named_buffer_sub_data(name, offset, size, data);
}
void entry_integer(GLenum pname, GLint *value) { live->integer(pname, value); }
void entry_pixel_store(GLenum pname, GLint value) { live->pixel_store(pname, value); }
void entry_read_pixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void *pixels)
{
    live->read_pixels(x, y, w, h, format, type, pixels);
}
void entry_readn_pixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLsizei, void *pixels)
{
    live->read_pixels(x, y, w, h, format, type, pixels);
}
void entry_tex_sub_image(GLenum, GLint, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type,
                         const void *pixels)
{
    live->tex_sub_image(x, y, w, h, format, type, pixels);
}

std::uint32_t pattern(GLsizei x, GLsizei y)
{
    return static_cast<std::uint32_t>(x) | static_cast<std::uint32_t>(y) << 12u | 0xA5000000u;
}

}

GlTransferHost::GlTransferHost()
    : RoomedTransport(request_bytes, reply_bytes), request_(static_cast<std::uint8_t *>(std::calloc(1u, request_bytes))),
      reply_(static_cast<std::uint8_t *>(std::calloc(1u, reply_bytes))),
      block_(static_cast<std::uint8_t *>(std::calloc(1u, arena_bytes))),
      framebuffer_(static_cast<std::uint8_t *>(std::calloc(1u, image_bytes))),
      texture_(static_cast<std::uint8_t *>(std::calloc(1u, image_bytes))), buffers_{}, pack_binding_(0u),
      unpack_binding_(0u), other_binding_(0u), pack_{{4, 0, 0, 0, 0, 0}}, unpack_{{4, 0, 0, 0, 0, 0}},
      name_nothing_(false), generated_(0u), commands_(0u), ids_{}, id_counts_{}, id_kinds_(0u), refusals_(0u),
      largest_(0u), answered_(0u), lent_(nullptr), lent_bytes_(0u), host_arena_(block_, arena_bytes),
      guest_arena_(nullptr, 0u), blobs_(), guest_(), table_()
{
    live = this;
    for (GLsizei y = 0; y < height; y++)
        for (GLsizei x = 0; x < width; x++) {
            const std::uint32_t value = pattern(x, y);
            std::memcpy(framebuffer_ + (std::size_t(y) * width + std::size_t(x)) * pixel_bytes, &value, pixel_bytes);
        }
    table_.glGenBuffers = entry_gen_buffers;
    table_.glDeleteBuffers = entry_delete_buffers;
    table_.glBindBuffer = entry_bind_buffer;
    table_.glBufferData = entry_buffer_data;
    table_.glBufferStorage = entry_buffer_storage;
    table_.glNamedBufferData = entry_named_buffer_data;
    table_.glNamedBufferDataEXT = entry_named_buffer_data;
    table_.glNamedBufferStorage = entry_named_buffer_storage;
    table_.glNamedBufferStorageEXT = entry_named_buffer_storage;
    table_.glBufferSubData = entry_buffer_sub_data;
    table_.glNamedBufferSubData = entry_named_buffer_sub_data;
    table_.glNamedBufferSubDataEXT = entry_named_buffer_sub_data;
    table_.glGetBufferSubData = entry_get_buffer_sub_data;
    table_.glGetNamedBufferSubData = entry_get_named_buffer_sub_data;
    table_.glGetNamedBufferSubDataEXT = entry_get_named_buffer_sub_data;
    table_.glGetIntegerv = entry_integer;
    table_.glPixelStorei = entry_pixel_store;
    table_.glReadPixels = entry_read_pixels;
    table_.glReadnPixels = entry_readn_pixels;
    table_.glTexSubImage2D = entry_tex_sub_image;
}

GlTransferHost::~GlTransferHost()
{
    if (live == this) live = nullptr;
    for (Buffer &buffer : buffers_) std::free(buffer.bytes);
    std::free(request_);
    std::free(reply_);
    std::free(block_);
    std::free(framebuffer_);
    std::free(texture_);
}

void GlTransferHost::lend(std::uint8_t *block, std::size_t bytes)
{
    lent_ = block;
    lent_bytes_ = bytes;
    blobs_.lend(block, bytes);
    guest_.unbind();
    static_cast<void>(guest_.bind(block, bytes));
    table_.av_guest = &guest_;
}

void GlTransferHost::name_nothing() { name_nothing_ = true; }

std::uint8_t *GlTransferHost::lent(std::uint64_t at, std::uint64_t bytes)
{
    if (lent_ == nullptr || at > lent_bytes_ || bytes > lent_bytes_ - at) return nullptr;
    return lent_ + at;
}

std::uint32_t GlTransferHost::framebuffer_pixel(GLsizei x, GLsizei y) const
{
    std::uint32_t value = 0u;
    std::memcpy(&value, framebuffer_ + (std::size_t(y) * width + std::size_t(x)) * pixel_bytes, pixel_bytes);
    return value;
}

std::uint32_t GlTransferHost::texture_pixel(GLsizei x, GLsizei y) const
{
    std::uint32_t value = 0u;
    std::memcpy(&value, texture_ + (std::size_t(y) * width + std::size_t(x)) * pixel_bytes, pixel_bytes);
    return value;
}

const std::uint8_t *GlTransferHost::buffer(GLuint name) const
{
    return name != 0u && name <= max_buffers && buffers_[name - 1u].live ? buffers_[name - 1u].bytes : nullptr;
}

std::size_t GlTransferHost::buffer_size(GLuint name) const
{
    return name != 0u && name <= max_buffers && buffers_[name - 1u].live ? buffers_[name - 1u].size : 0u;
}

GLuint GlTransferHost::bound(GLenum target) const
{
    if (target == pixel_pack_buffer) return pack_binding_;
    if (target == pixel_unpack_buffer) return unpack_binding_;
    return other_binding_;
}

std::uint32_t GlTransferHost::live_buffers() const
{
    std::uint32_t count = 0u;
    for (const Buffer &buffer : buffers_) count += buffer.live ? 1u : 0u;
    return count;
}

std::uint32_t GlTransferHost::generated() const { return generated_; }
std::uint32_t GlTransferHost::commands() const { return commands_; }
std::uint32_t GlTransferHost::refusals() const { return refusals_; }
std::size_t   GlTransferHost::largest_request() const { return largest_; }

std::uint32_t GlTransferHost::count_of(std::uint32_t command_id) const
{
    for (std::uint32_t kind = 0u; kind < id_kinds_; kind++)
        if (ids_[kind] == command_id) return id_counts_[kind];
    return 0u;
}

GlTransferHost::Buffer *GlTransferHost::named(GLuint name)
{
    return name != 0u && name <= max_buffers && buffers_[name - 1u].live ? &buffers_[name - 1u] : nullptr;
}

GlTransferHost::Buffer *GlTransferHost::at_target(GLenum target) { return named(bound(target)); }

void GlTransferHost::gen_buffers(GLsizei count, GLuint *names)
{
    for (GLsizei index = 0; index < count; index++) {
        names[index] = 0u;
        if (name_nothing_) continue;
        for (GLuint name = 1u; name <= max_buffers; name++) {
            if (buffers_[name - 1u].live) continue;
            buffers_[name - 1u].live = true;
            names[index] = name;
            generated_++;
            break;
        }
    }
}

void GlTransferHost::delete_buffers(GLsizei count, const GLuint *names)
{
    for (GLsizei index = 0; index < count; index++) {
        Buffer *const buffer = named(names[index]);
        if (buffer == nullptr) continue;
        std::free(buffer->bytes);
        *buffer = Buffer{};
        if (pack_binding_ == names[index]) pack_binding_ = 0u;
        if (unpack_binding_ == names[index]) unpack_binding_ = 0u;
        if (other_binding_ == names[index]) other_binding_ = 0u;
    }
}

void GlTransferHost::bind_buffer(GLenum target, GLuint name)
{
    if (target == pixel_pack_buffer)
        pack_binding_ = name;
    else if (target == pixel_unpack_buffer)
        unpack_binding_ = name;
    else
        other_binding_ = name;
}

void GlTransferHost::named_buffer_data(GLuint name, GLsizeiptr size, const void *data)
{
    Buffer *const buffer = named(name);
    if (buffer == nullptr || size < 0 || static_cast<std::size_t>(size) > max_buffer_bytes) return;
    std::free(buffer->bytes);
    buffer->size = static_cast<std::size_t>(size);
    buffer->bytes = static_cast<std::uint8_t *>(std::calloc(1u, buffer->size + 1u));
    if (data != nullptr) std::memcpy(buffer->bytes, data, buffer->size);
}

void GlTransferHost::buffer_data(GLenum target, GLsizeiptr size, const void *data)
{
    named_buffer_data(bound(target), size, data);
}

void GlTransferHost::named_buffer_sub_data(GLuint name, GLintptr offset, GLsizeiptr size, const void *data)
{
    Buffer *const buffer = named(name);
    if (buffer == nullptr || offset < 0 || size < 0 || std::size_t(offset) + std::size_t(size) > buffer->size) return;
    std::memcpy(buffer->bytes + offset, data, static_cast<std::size_t>(size));
}

void GlTransferHost::buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    named_buffer_sub_data(bound(target), offset, size, data);
}

void GlTransferHost::get_named_buffer_sub_data(GLuint name, GLintptr offset, GLsizeiptr size, void *data)
{
    Buffer *const buffer = named(name);
    if (buffer == nullptr || offset < 0 || size < 0 || std::size_t(offset) + std::size_t(size) > buffer->size) return;
    std::memcpy(data, buffer->bytes + offset, static_cast<std::size_t>(size));
}

void GlTransferHost::get_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
    get_named_buffer_sub_data(bound(target), offset, size, data);
}

void GlTransferHost::integer(GLenum pname, GLint *value) const
{
    *value = 0;
    if (pname == pack_binding) *value = static_cast<GLint>(pack_binding_);
    if (pname == unpack_binding) *value = static_cast<GLint>(unpack_binding_);
    for (std::uint32_t index = 0u; index < 6u; index++) {
        if (pname == pack_store[index]) *value = pack_.values[index];
        if (pname == unpack_store[index]) *value = unpack_.values[index];
    }
}

void GlTransferHost::pixel_store(GLenum pname, GLint value)
{
    for (std::uint32_t index = 0u; index < 6u; index++) {
        if (pname == pack_store[index]) pack_.values[index] = value;
        if (pname == unpack_store[index]) unpack_.values[index] = value;
    }
}

std::size_t GlTransferHost::layout_offset(const Store &store, GLsizei columns, GLint column, GLint row) const
{
    const std::size_t alignment = static_cast<std::size_t>(store.values[0]);
    const std::size_t length = store.values[1] != 0 ? std::size_t(store.values[1]) : std::size_t(columns);
    std::size_t       stride = length * pixel_bytes + alignment - 1u;
    stride -= stride % alignment;
    return (std::size_t(store.values[2]) + std::size_t(row)) * stride +
           (std::size_t(store.values[4]) + std::size_t(column)) * pixel_bytes;
}

void GlTransferHost::read_pixels(GLint x, GLint y, GLsizei columns, GLsizei rows, GLenum format, GLenum type,
                                 void *pixels)
{
    if (format != gl_rgba || type != gl_unsigned_byte || x < 0 || y < 0 || x + columns > width || y + rows > height)
        return;
    Buffer *const  pack = at_target(pixel_pack_buffer);
    std::uint8_t  *base = static_cast<std::uint8_t *>(pixels);
    std::size_t    room = ~std::size_t{0};
    if (pack != nullptr) {
        const std::size_t offset = reinterpret_cast<std::uintptr_t>(pixels);
        if (offset > pack->size) return;
        base = pack->bytes + offset;
        room = pack->size - offset;
    }
    if (rows > 0 && columns > 0 && layout_offset(pack_, columns, columns - 1, rows - 1) + pixel_bytes > room) return;
    for (GLint row = 0; row < rows; row++)
        for (GLint column = 0; column < columns; column++) {
            const std::uint32_t value = framebuffer_pixel(x + column, y + row);
            std::memcpy(base + layout_offset(pack_, columns, column, row), &value, pixel_bytes);
        }
}

void GlTransferHost::tex_sub_image(GLint x, GLint y, GLsizei columns, GLsizei rows, GLenum format, GLenum type,
                                   const void *pixels)
{
    if (format != gl_rgba || type != gl_unsigned_byte || x < 0 || y < 0 || x + columns > width || y + rows > height)
        return;
    Buffer *const       unpack = at_target(pixel_unpack_buffer);
    const std::uint8_t *base = static_cast<const std::uint8_t *>(pixels);
    std::size_t         room = ~std::size_t{0};
    if (unpack != nullptr) {
        const std::size_t offset = reinterpret_cast<std::uintptr_t>(pixels);
        if (offset > unpack->size) return;
        base = unpack->bytes + offset;
        room = unpack->size - offset;
    }
    if (rows > 0 && columns > 0 && layout_offset(unpack_, columns, columns - 1, rows - 1) + pixel_bytes > room)
        return;
    for (GLint row = 0; row < rows; row++)
        for (GLint column = 0; column < columns; column++)
            std::memcpy(texture_ + (std::size_t(y + row) * width + std::size_t(x + column)) * pixel_bytes,
                        base + layout_offset(unpack_, columns, column, row), pixel_bytes);
}

WireWriter GlTransferHost::request_writer() { return WireWriter(request_, request_bytes); }

bool GlTransferHost::dispatch(std::uint32_t command_id, WireWriter &writer, WireWriter &answer)
{
    commands_++;
    std::uint32_t kind = 0u;
    while (kind < id_kinds_ && ids_[kind] != command_id) kind++;
    if (kind == id_kinds_ && id_kinds_ < max_counted) ids_[id_kinds_++] = command_id;
    if (kind < max_counted) id_counts_[kind]++;
    if (writer.status() != Status::ok) {
        refusals_++;
        return false;
    }
    largest_ = writer.offset() > largest_ ? writer.offset() : largest_;
    host_arena_.reset();
    WireReader payload(request_, writer.offset());
    if (!glproto::dispatch_command(table_, command_id, payload, answer, host_arena_) ||
        answer.status() != Status::ok) {
        refusals_++;
        return false;
    }
    return true;
}

TransportResult GlTransferHost::send_async(std::uint32_t command_id, WireWriter &writer)
{
    std::uint8_t discard[64];
    WireWriter   answer(discard, sizeof discard);
    return dispatch(command_id, writer, answer) ? TransportResult::ok : TransportResult::failed;
}

TransportResult GlTransferHost::send_sync(std::uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    WireWriter answer(reply_, reply_bytes);
    reply = WireReader(nullptr, 0u);
    if (!dispatch(command_id, writer, answer)) return TransportResult::failed;
    answered_ = answer.offset();
    reply = WireReader(reply_, answered_);
    return TransportResult::ok;
}

void GlTransferHost::end_call()
{
    blobs_.seal();
    blobs_.mark(commands_);
    blobs_.retire_through(commands_);
}
WireArena       &GlTransferHost::decode_arena() { return guest_arena_; }
ClientBlobArena &GlTransferHost::blobs() { return blobs_; }

}
