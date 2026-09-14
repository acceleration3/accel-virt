#include "harness/executor/recording_gl.hpp"

#include "proto/wire/wire_arena.hpp"

namespace av {

namespace {

constexpr GLenum pack_alignment = 0x0D05u;
constexpr GLenum pack_skip_pixels = 0x0D04u;
constexpr GLenum pack_skip_rows = 0x0D03u;
constexpr GLenum pack_skip_images = 0x806Bu;
constexpr GLenum pack_row_length = 0x0D02u;
constexpr GLenum pack_image_height = 0x806Cu;
constexpr GLenum pixel_pack_buffer = 0x88EBu;
constexpr GLenum pixel_pack_buffer_binding = 0x88EDu;
constexpr GLenum texture_width = 0x1000u;
constexpr GLenum texture_height = 0x1001u;
constexpr GLenum texture_depth = 0x8071u;
constexpr GLint  default_alignment = 4;

/* The live RecordingGl the table's entry points reach. See the constructor's
 * comment in the header. */
RecordingGl *live = nullptr;

void store(GLenum pname, GLint value) { live->pixel_store(pname, value); }
void bind(GLenum target, GLuint name) { live->bind_buffer(target, name); }
void get_integer(GLenum pname, GLint *value) { live->integer(pname, value); }
void get_level(GLenum, GLint, GLenum pname, GLint *value) { live->level_parameter(pname, value); }

void entry_read_pixels(GLint, GLint, GLsizei width, GLsizei height, GLenum format, GLenum type,
                 void *pixels)
{
    live->read_pixels(width, height, 1, format, type, pixels);
}

void readn_pixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLsizei size, void *pixels)
{
    live->read_bounded(size < 0 ? 0u : static_cast<std::size_t>(size), pixels);
}

void get_tex_image(GLenum, GLint, GLenum format, GLenum type, void *pixels)
{
    GLint extent[3] = {};
    live->level_parameter(texture_width, &extent[0]);
    live->level_parameter(texture_height, &extent[1]);
    live->level_parameter(texture_depth, &extent[2]);
    live->read_pixels(extent[0], extent[1], extent[2], format, type, pixels);
}

void get_compressed(GLenum, GLint, void *pixels) { live->read_bounded(1u, pixels); }

void get_query(GLuint, GLenum, GLint *value) { live->read_bounded(sizeof *value, value); }

/* Bytes per pixel for the pairings the tests and seeds use. */
std::uint64_t pixel_size(GLenum format, GLenum type)
{
    constexpr GLenum rgba = 0x1908u, rgb = 0x1907u, red = 0x1903u, depth = 0x1902u;
    constexpr GLenum unsigned_byte = 0x1401u, floating = 0x1406u;
    if (type == unsigned_byte && format == rgba) return 4u;
    if (type == unsigned_byte && format == rgb) return 3u;
    if (type == unsigned_byte && format == red) return 1u;
    if (type == floating && format == rgba) return 16u;
    if (type == floating && format == depth) return 4u;
    return 0u;
}

}  // namespace

RecordingGl::RecordingGl(const std::uint8_t *block, const WireArena &arena)
    : block_(block), arena_(arena), table_(), alignment_(default_alignment), skip_pixels_(0),
      skip_rows_(0), skip_images_(0), row_length_(0), image_height_(0), pack_buffer_(0u),
      level_extent_{}, reads_(0u), strays_(0u)
{
    live = this;
    table_.glPixelStorei = store;
    table_.glBindBuffer = bind;
    table_.glGetIntegerv = get_integer;
    table_.glGetTexLevelParameteriv = get_level;
    table_.glReadPixels = entry_read_pixels;
    table_.glReadnPixels = readn_pixels;
    table_.glGetTexImage = get_tex_image;
    table_.glGetCompressedTexImage = get_compressed;
    table_.glGetQueryObjectiv = get_query;
}

RecordingGl::~RecordingGl()
{
    if (live == this) live = nullptr;
}

const glproto::RealGl &RecordingGl::table() const { return table_; }

void RecordingGl::set_level_extent(GLint width, GLint height, GLint depth)
{
    level_extent_[0] = width;
    level_extent_[1] = height;
    level_extent_[2] = depth;
}

void RecordingGl::forget_state_queries() { table_.glGetIntegerv = nullptr; }

std::uint32_t RecordingGl::reads() const { return reads_; }
std::uint32_t RecordingGl::strays() const { return strays_; }

void RecordingGl::pixel_store(GLenum pname, GLint value)
{
    /* GL refuses a negative value or an alignment other than 1, 2, 4 or 8,
     * and keeps what it had. */
    if (value < 0) return;
    if (pname == pack_alignment && value != 1 && value != 2 && value != 4 && value != 8) return;
    switch (pname) {
    case pack_alignment: alignment_ = value; break;
    case pack_skip_pixels: skip_pixels_ = value; break;
    case pack_skip_rows: skip_rows_ = value; break;
    case pack_skip_images: skip_images_ = value; break;
    case pack_row_length: row_length_ = value; break;
    case pack_image_height: image_height_ = value; break;
    default: break;
    }
}

void RecordingGl::bind_buffer(GLenum target, GLuint name)
{
    if (target == pixel_pack_buffer) pack_buffer_ = name;
}

void RecordingGl::integer(GLenum pname, GLint *value) const
{
    switch (pname) {
    case pack_alignment: *value = alignment_; break;
    case pack_skip_pixels: *value = skip_pixels_; break;
    case pack_skip_rows: *value = skip_rows_; break;
    case pack_skip_images: *value = skip_images_; break;
    case pack_row_length: *value = row_length_; break;
    case pack_image_height: *value = image_height_; break;
    case pixel_pack_buffer_binding: *value = static_cast<GLint>(pack_buffer_); break;
    default: *value = 0; break;
    }
}

void RecordingGl::level_parameter(GLenum pname, GLint *value) const
{
    switch (pname) {
    case texture_width: *value = level_extent_[0]; break;
    case texture_height: *value = level_extent_[1]; break;
    case texture_depth: *value = level_extent_[2]; break;
    default: *value = 0; break;
    }
}

void RecordingGl::read_pixels(GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                              GLenum type, void *pixels)
{
    reads_++;
    /* Behind a bound pack buffer the pointer is an offset GL bounds itself. */
    if (pack_buffer_ != 0u) return;
    std::uint64_t bytes = 0u;
    if (!reach(width, height, depth, format, type, bytes)) return;
    check_span(pixels, bytes);
}

void RecordingGl::read_bounded(std::size_t bytes, void *pixels)
{
    reads_++;
    if (pack_buffer_ != 0u) return;
    check_span(pixels, bytes);
}

bool RecordingGl::reach(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                        std::uint64_t &bytes) const
{
    const std::uint64_t pixel = pixel_size(format, type);
    if (pixel == 0u) return false;
    bytes = 0u;
    if (width <= 0 || height <= 0 || depth <= 0) return true;
    /* The last pixel GL writes: after the skipped images, rows and pixels,
     * with every row padded to the alignment and every image image_height
     * rows tall. Anything that overflows is further than any arena. */
    const std::uint64_t columns = static_cast<std::uint64_t>(row_length_ > 0 ? row_length_ : width);
    const std::uint64_t rows = static_cast<std::uint64_t>(image_height_ > 0 ? image_height_ : height);
    const std::uint64_t align = static_cast<std::uint64_t>(alignment_);
    std::uint64_t stride = 0u, image = 0u, before_images = 0u, before_rows = 0u, through = 0u;
    const bool overflowed =
        __builtin_mul_overflow(columns, pixel, &stride) ||
        __builtin_add_overflow(stride, align - 1u, &stride) ||
        __builtin_mul_overflow(stride / align * align, rows, &image) ||
        __builtin_mul_overflow(static_cast<std::uint64_t>(skip_images_) +
                                   static_cast<std::uint64_t>(depth - 1), image, &before_images) ||
        __builtin_mul_overflow(static_cast<std::uint64_t>(skip_rows_) +
                                   static_cast<std::uint64_t>(height - 1), stride / align * align,
                               &before_rows) ||
        __builtin_mul_overflow(static_cast<std::uint64_t>(skip_pixels_) +
                                   static_cast<std::uint64_t>(width), pixel, &through) ||
        __builtin_add_overflow(before_images, before_rows, &bytes) ||
        __builtin_add_overflow(bytes, through, &bytes);
    if (overflowed) bytes = ~std::uint64_t{0};
    return true;
}

void RecordingGl::check_span(const void *bytes, std::uint64_t length)
{
    if (length == 0u) return;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bytes);
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(block_);
    const std::uintptr_t end = first + arena_.used();
    if (start < first || start > end || length > end - start) strays_++;
}

}  // namespace av
