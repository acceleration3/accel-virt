#include "harness/executor/recording_gl.hpp"

#include "proto/wire/wire_arena.hpp"

namespace av {

namespace {

constexpr GLenum pack_store[6] = {0x0D05u, 0x0D04u, 0x0D03u, 0x806Bu, 0x0D02u, 0x806Cu};
constexpr GLenum unpack_store[6] = {0x0CF5u, 0x0CF4u, 0x0CF3u, 0x806Du, 0x0CF2u, 0x806Eu};
constexpr GLenum compressed_block_store[4] = {0x9127u, 0x9128u, 0x9129u, 0x912Au};
constexpr GLenum pixel_pack_buffer = 0x88EBu;
constexpr GLenum pixel_unpack_buffer = 0x88ECu;
constexpr GLenum array_buffer = 0x8892u;
constexpr GLenum element_array_buffer = 0x8893u;
constexpr GLenum draw_indirect_buffer = 0x8F3Fu;
constexpr GLenum parameter_buffer = 0x80EEu;
constexpr GLenum pixel_pack_buffer_binding = 0x88EDu;
constexpr GLenum pixel_unpack_buffer_binding = 0x88EFu;
constexpr GLenum array_buffer_binding = 0x8894u;
constexpr GLenum element_array_buffer_binding = 0x8895u;
constexpr GLenum draw_indirect_buffer_binding = 0x8F43u;
constexpr GLenum parameter_buffer_binding = 0x80EFu;
constexpr GLenum vertex_array_buffer_binding = 0x8896u;
constexpr GLenum texture_coord_array_buffer_binding = 0x889Au;
constexpr GLenum max_vertex_attribs = 0x8869u;
constexpr GLenum max_texture_coords = 0x8871u;
constexpr GLenum client_active_texture = 0x84E1u;
constexpr GLenum texture0 = 0x84C0u;
constexpr GLenum num_compressed_texture_formats = 0x86A2u;
constexpr GLenum compressed_texture_formats = 0x86A3u;
constexpr GLenum vertex_array = 0x8074u;
constexpr GLenum texture_coord_array = 0x8078u;
constexpr GLenum attrib_array_enabled = 0x8622u;
constexpr GLenum attrib_array_buffer_binding = 0x889Fu;
constexpr GLenum current_vertex_attrib = 0x8626u;
constexpr GLenum texture_width = 0x1000u;
constexpr GLenum texture_height = 0x1001u;
constexpr GLenum texture_depth = 0x8071u;
constexpr GLenum texture_border_color = 0x1004u;
constexpr GLenum uniform_block_active_uniforms = 0x8A42u;
constexpr GLenum uniform_block_active_uniform_indices = 0x8A43u;
constexpr GLenum map1_vertex_3 = 0x0D97u;
constexpr GLenum map_coefficients = 0x0A00u;
constexpr GLenum map_order_query = 0x0A01u;
constexpr GLint  default_alignment = 4;
constexpr GLint  stipple_side = 32;
constexpr std::uint64_t map_dimensions = 3u;

/* The live RecordingGl the table's entry points reach. See the constructor's
 * comment in the header. */
RecordingGl *live = nullptr;

void store(GLenum pname, GLint value) { live->pixel_store(pname, value); }
void bind(GLenum target, GLuint name) { live->bind_buffer(target, name); }
void entry_delete_buffers(GLsizei n, const GLuint *names) { live->delete_buffers(n, names); }
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
void gen_buffers(GLsizei n, GLuint *names) { live->generate(n, names); }
void shader_info_log(GLuint, GLsizei room, GLsizei *length, GLchar *log) { live->info_log(room, length, log); }
void uniform4fv(GLint, GLsizei count, const GLfloat *values) { live->uniform_values(count, values); }
void tex_parameteriv(GLenum, GLenum pname, const GLint *values) { live->texture_parameter(pname, values); }

void tex_image_2d(GLenum, GLint, GLint, GLsizei width, GLsizei height, GLint, GLenum format,
                  GLenum type, const void *pixels)
{
    live->upload(width, height, format, type, pixels, true);
}

void tex_sub_image_2d(GLenum, GLint, GLint, GLint, GLsizei width, GLsizei height, GLenum format,
                      GLenum type, const void *pixels)
{
    live->upload(width, height, format, type, pixels, false);
}

void compressed_tex_image_2d(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei size,
                             const void *data)
{
    live->compressed(size, data);
}

void clear_buffer_data(GLenum, GLenum, GLenum format, GLenum type, const void *data)
{
    live->clear_data(format, type, data);
}

void entry_buffer_data(GLenum, GLsizeiptr size, const void *data, GLenum) { live->buffer_data(size, data); }
void entry_call_lists(GLsizei n, GLenum type, const void *lists) { live->call_lists(n, type, lists); }

void entry_shader_source(GLuint, GLsizei count, const GLchar **strings, const GLint *lengths)
{
    live->shader_source(count, strings, lengths);
}

void debug_message_insert(GLenum, GLenum, GLuint, GLenum, GLsizei length, const GLchar *text)
{
    live->debug_text(length, text);
}

void draw_elements(GLenum, GLsizei, GLenum, const void *) { live->draw(true); }
void draw_arrays(GLenum, GLint, GLsizei) { live->draw(false); }

void multi_draw_elements(GLenum, const GLsizei *counts, GLenum, const void **indices, GLsizei drawcount)
{
    live->draw_many(drawcount, counts, indices);
}

void draw_arrays_indirect(GLenum, const void *) { live->draw_indirect(); }

void vertex_attrib_pointer(GLuint index, GLint, GLenum, GLboolean, GLsizei, const void *pointer)
{
    live->attribute_pointer(index, pointer);
}

void enable_attribute(GLuint index) { live->attribute_enabled(index, true); }
void disable_attribute(GLuint index) { live->attribute_enabled(index, false); }
void get_attribute(GLuint index, GLenum pname, GLint *value) { live->attribute_query(index, pname, value); }
GLboolean entry_is_enabled(GLenum capability) { return live->is_enabled(capability); }
void enable_client_state(GLenum capability) { live->client_state(capability, true); }
void disable_client_state(GLenum capability) { live->client_state(capability, false); }
void client_active_texture_entry(GLenum unit) { live->client_texture(unit); }
void vertex_pointer(GLint, GLenum, GLsizei, const void *pointer) { live->array_pointer(vertex_array, pointer); }

void tex_coord_pointer(GLint, GLenum, GLsizei, const void *pointer)
{
    live->array_pointer(texture_coord_array, pointer);
}

void entry_uniform_block(GLuint, GLuint, GLenum pname, GLint *values) { live->uniform_block(pname, values); }

void map1f(GLenum target, GLfloat, GLfloat, GLint stride, GLint order, const GLfloat *points)
{
    live->map(target, stride, order, points);
}

void get_mapfv(GLenum target, GLenum query, GLfloat *values) { live->map_values(target, query, values); }
void get_mapiv(GLenum target, GLenum query, GLint *values) { live->map_order(target, query, values); }
void select_buffer(GLsizei, GLuint *) { live->kept_pointer(); }
void get_polygon_stipple(GLubyte *mask) { live->stipple(mask); }

void bitmap_entry(GLsizei width, GLsizei height, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte *bits)
{
    live->bitmap(width, height, bits);
}

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

/* Rows padded to the store's alignment. */
std::uint64_t aligned(std::uint64_t bytes, GLint alignment)
{
    const std::uint64_t step = static_cast<std::uint64_t>(alignment);
    return (bytes + step - 1u) / step * step;
}

}  // namespace

RecordingGl::RecordingGl(const std::uint8_t *block, const WireArena &arena)
    : block_(block), arena_(arena), table_(), pack_{default_alignment, 0, 0, 0, 0, 0},
      unpack_{default_alignment, 0, 0, 0, 0, 0}, compressed_block_{}, pack_buffer_(0u),
      unpack_buffer_(0u), array_buffer_(0u), element_buffer_(0u), indirect_buffer_(0u),
      parameter_buffer_(0u), attribute_enabled_{}, attribute_buffer_{},
      vertex_array_enabled_(false), vertex_array_buffer_(0u), texture_array_enabled_{},
      texture_array_buffer_{}, client_unit_(0), map_target_(0u), map_order_(0), level_extent_{},
      reads_(0u), calls_(0u), strays_(0u)
{
    live = this;
    table_.glPixelStorei = store;
    table_.glBindBuffer = bind;
    table_.glDeleteBuffers = entry_delete_buffers;
    table_.glGetIntegerv = get_integer;
    table_.glGetTexLevelParameteriv = get_level;
    table_.glReadPixels = entry_read_pixels;
    table_.glReadnPixels = readn_pixels;
    table_.glGetTexImage = get_tex_image;
    table_.glGetCompressedTexImage = get_compressed;
    table_.glGetQueryObjectiv = get_query;
    table_.glGenBuffers = gen_buffers;
    table_.glGetShaderInfoLog = shader_info_log;
    table_.glUniform4fv = uniform4fv;
    table_.glTexParameteriv = tex_parameteriv;
    table_.glTexImage2D = tex_image_2d;
    table_.glTexSubImage2D = tex_sub_image_2d;
    table_.glCompressedTexImage2D = compressed_tex_image_2d;
    table_.glClearBufferData = clear_buffer_data;
    table_.glBufferData = entry_buffer_data;
    table_.glCallLists = entry_call_lists;
    table_.glShaderSource = entry_shader_source;
    table_.glDebugMessageInsert = debug_message_insert;
    table_.glDrawElements = draw_elements;
    table_.glDrawArrays = draw_arrays;
    table_.glMultiDrawElements = multi_draw_elements;
    table_.glDrawArraysIndirect = draw_arrays_indirect;
    table_.glVertexAttribPointer = vertex_attrib_pointer;
    table_.glEnableVertexAttribArray = enable_attribute;
    table_.glDisableVertexAttribArray = disable_attribute;
    table_.glGetVertexAttribiv = get_attribute;
    table_.glIsEnabled = entry_is_enabled;
    table_.glEnableClientState = enable_client_state;
    table_.glDisableClientState = disable_client_state;
    table_.glClientActiveTexture = client_active_texture_entry;
    table_.glVertexPointer = vertex_pointer;
    table_.glTexCoordPointer = tex_coord_pointer;
    table_.glGetActiveUniformBlockiv = entry_uniform_block;
    table_.glMap1f = map1f;
    table_.glGetMapfv = get_mapfv;
    table_.glGetMapiv = get_mapiv;
    table_.glSelectBuffer = select_buffer;
    table_.glGetPolygonStipple = get_polygon_stipple;
    table_.glBitmap = bitmap_entry;
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
std::uint32_t RecordingGl::calls() const { return calls_; }
std::uint32_t RecordingGl::strays() const { return strays_; }

void RecordingGl::pixel_store(GLenum pname, GLint value)
{
    /* GL refuses a negative value or an alignment other than 1, 2, 4 or 8,
     * and keeps what it had. */
    if (value < 0) return;
    const bool alignment = pname == pack_store[0] || pname == unpack_store[0];
    if (alignment && value != 1 && value != 2 && value != 4 && value != 8) return;
    for (std::size_t i = 0u; i < 6u; i++) {
        if (pname == pack_store[i]) pack_[i] = value;
        if (pname == unpack_store[i]) unpack_[i] = value;
    }
    for (std::size_t i = 0u; i < 4u; i++) {
        if (pname == compressed_block_store[i]) compressed_block_[i] = value;
    }
}

GLuint *RecordingGl::binding(GLenum target)
{
    switch (target) {
    case pixel_pack_buffer: return &pack_buffer_;
    case pixel_unpack_buffer: return &unpack_buffer_;
    case array_buffer: return &array_buffer_;
    case element_array_buffer: return &element_buffer_;
    case draw_indirect_buffer: return &indirect_buffer_;
    case parameter_buffer: return &parameter_buffer_;
    default: return nullptr;
    }
}

void RecordingGl::bind_buffer(GLenum target, GLuint name)
{
    GLuint *bound = binding(target);
    if (bound != nullptr) *bound = name;
}

void RecordingGl::delete_buffers(GLsizei n, const GLuint *names)
{
    calls_++;
    if (n <= 0) return;
    const std::uint32_t before = strays_;
    check_span(names, static_cast<std::uint64_t>(n) * sizeof *names);
    if (strays_ != before) return;
    /* A deleted buffer is unbound from everything in this context, the
     * arrays it was behind included: what is left is a client array. */
    for (GLsizei i = 0; i < n; i++) {
        const GLuint name = names[i];
        if (name == 0u) continue;
        GLuint *const bindings[] = {&pack_buffer_, &unpack_buffer_, &array_buffer_, &element_buffer_,
                                    &indirect_buffer_, &parameter_buffer_, &vertex_array_buffer_,
                                    &texture_array_buffer_[0], &texture_array_buffer_[1]};
        for (GLuint *bound : bindings) {
            if (*bound == name) *bound = 0u;
        }
        for (GLuint &bound : attribute_buffer_) {
            if (bound == name) bound = 0u;
        }
    }
}

void RecordingGl::integer(GLenum pname, GLint *value)
{
    for (std::size_t i = 0u; i < 6u; i++) {
        if (pname == pack_store[i]) *value = pack_[i];
        if (pname == unpack_store[i]) *value = unpack_[i];
    }
    for (std::size_t i = 0u; i < 4u; i++) {
        if (pname == compressed_block_store[i]) *value = compressed_block_[i];
    }
    switch (pname) {
    case pixel_pack_buffer_binding: *value = static_cast<GLint>(pack_buffer_); break;
    case pixel_unpack_buffer_binding: *value = static_cast<GLint>(unpack_buffer_); break;
    case array_buffer_binding: *value = static_cast<GLint>(array_buffer_); break;
    case element_array_buffer_binding: *value = static_cast<GLint>(element_buffer_); break;
    case draw_indirect_buffer_binding: *value = static_cast<GLint>(indirect_buffer_); break;
    case parameter_buffer_binding: *value = static_cast<GLint>(parameter_buffer_); break;
    case vertex_array_buffer_binding: *value = static_cast<GLint>(vertex_array_buffer_); break;
    case texture_coord_array_buffer_binding:
        *value = static_cast<GLint>(texture_array_buffer_[client_unit_]);
        break;
    case max_vertex_attribs: *value = vertex_attributes; break;
    case max_texture_coords: *value = texture_units; break;
    case client_active_texture: *value = static_cast<GLint>(texture0) + client_unit_; break;
    case num_compressed_texture_formats: *value = compressed_formats; break;
    case compressed_texture_formats: {
        const std::uint32_t before = strays_;
        check_span(value, static_cast<std::uint64_t>(compressed_formats) * sizeof *value);
        if (strays_ == before) {
            for (GLint i = 0; i < compressed_formats; i++) value[i] = 0x83F0 + i;
        }
        break;
    }
    default: break;
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
    if (!reach(pack_, width, height, depth, format, type, bytes)) return;
    check_span(pixels, bytes);
}

void RecordingGl::read_bounded(std::size_t bytes, void *pixels)
{
    reads_++;
    if (pack_buffer_ != 0u) return;
    check_span(pixels, bytes);
}

void RecordingGl::generate(GLsizei n, GLuint *names)
{
    calls_++;
    if (n > 0) check_span(names, static_cast<std::uint64_t>(n) * sizeof *names);
}

void RecordingGl::info_log(GLsizei room, GLsizei *length, GLchar *log)
{
    calls_++;
    if (length != nullptr) check_span(length, sizeof *length);
    if (room <= 0) return;
    if (log == nullptr) {
        strays_++;
        return;
    }
    check_span(log, static_cast<std::uint64_t>(room));
}

void RecordingGl::uniform_values(GLsizei count, const GLfloat *values)
{
    constexpr std::uint64_t components = 4u;
    calls_++;
    if (count > 0) check_span(values, static_cast<std::uint64_t>(count) * components * sizeof *values);
}

void RecordingGl::texture_parameter(GLenum pname, const GLint *values)
{
    constexpr std::uint64_t colour = 4u;
    calls_++;
    check_span(values, (pname == texture_border_color ? colour : 1u) * sizeof *values);
}

void RecordingGl::upload(GLsizei width, GLsizei height, GLenum format, GLenum type,
                         const void *pixels, bool specifies)
{
    calls_++;
    /* An empty extent reads nothing, whatever the format. */
    if (unpack_buffer_ != 0u || width <= 0 || height <= 0) return;
    if (pixels == nullptr) {
        if (!specifies) strays_++;
        return;
    }
    /* A pairing this fake does not size reads at least a byte a pixel, which
     * is still a bound a short upload falls under. */
    constexpr GLenum one_byte_format = 0x1903u, one_byte_type = 0x1401u;
    std::uint64_t bytes = 0u;
    if (!reach(unpack_, width, height, 1, format, type, bytes))
        (void)reach(unpack_, width, height, 1, one_byte_format, one_byte_type, bytes);
    check_span(pixels, bytes);
}

void RecordingGl::compressed(GLsizei size, const void *data)
{
    calls_++;
    if (unpack_buffer_ != 0u || data == nullptr || size <= 0) return;
    /* With a block size set, the skips move the start whole blocks in. */
    const bool skipped = unpack_[1] != 0 || unpack_[2] != 0 || unpack_[3] != 0;
    const bool blocked = compressed_block_[0] != 0 || compressed_block_[1] != 0 ||
                         compressed_block_[2] != 0 || compressed_block_[3] != 0;
    if (skipped && blocked) {
        strays_++;
        return;
    }
    check_span(data, static_cast<std::uint64_t>(size));
}

void RecordingGl::clear_data(GLenum format, GLenum type, const void *data)
{
    calls_++;
    if (data == nullptr) return;
    const std::uint64_t pixel = pixel_size(format, type);
    if (pixel == 0u) {
        strays_++;
        return;
    }
    check_span(data, pixel);
}

void RecordingGl::buffer_data(GLsizeiptr size, const void *data)
{
    calls_++;
    if (data != nullptr && size > 0) check_span(data, static_cast<std::uint64_t>(size));
}

void RecordingGl::call_lists(GLsizei n, GLenum type, const void *lists)
{
    constexpr GLenum unsigned_byte = 0x1401u, unsigned_short = 0x1403u;
    calls_++;
    if (n <= 0) return;
    const std::uint64_t width = type == unsigned_byte ? 1u : type == unsigned_short ? 2u : 4u;
    check_span(lists, static_cast<std::uint64_t>(n) * width);
}

void RecordingGl::shader_source(GLsizei count, const GLchar *const *strings, const GLint *lengths)
{
    calls_++;
    if (count <= 0) return;
    const std::uint32_t before = strays_;
    check_span(strings, static_cast<std::uint64_t>(count) * sizeof *strings);
    if (lengths != nullptr) check_span(lengths, static_cast<std::uint64_t>(count) * sizeof *lengths);
    if (strays_ != before) return;
    for (GLsizei i = 0; i < count; i++) {
        if (strings[i] == nullptr) {
            strays_++;
        } else if (lengths != nullptr && lengths[i] >= 0) {
            check_span(strings[i], static_cast<std::uint64_t>(lengths[i]));
        } else {
            check_terminated(strings[i]);
        }
    }
}

void RecordingGl::debug_text(GLsizei length, const GLchar *text)
{
    calls_++;
    if (text == nullptr) {
        strays_++;
    } else if (length >= 0) {
        check_span(text, static_cast<std::uint64_t>(length));
    } else {
        check_terminated(text);
    }
}

void RecordingGl::draw(bool indexed)
{
    calls_++;
    if (indexed && element_buffer_ == 0u) strays_++;
    if (client_array_enabled()) strays_++;
}

void RecordingGl::draw_many(GLsizei drawcount, const GLsizei *counts, const void *const *indices)
{
    /* No draws read no indices and no arrays. */
    if (drawcount <= 0) {
        calls_++;
        return;
    }
    check_span(counts, static_cast<std::uint64_t>(drawcount) * sizeof *counts);
    check_span(indices, static_cast<std::uint64_t>(drawcount) * sizeof *indices);
    draw(true);
}

void RecordingGl::draw_indirect()
{
    if (indirect_buffer_ == 0u) strays_++;
    draw(false);
}

void RecordingGl::attribute_pointer(GLuint index, const void *pointer)
{
    calls_++;
    if (index >= static_cast<GLuint>(vertex_attributes)) return;
    if (pointer != nullptr && array_buffer_ == 0u) strays_++;
    attribute_buffer_[index] = array_buffer_;
}

void RecordingGl::attribute_enabled(GLuint index, bool enabled)
{
    if (index < static_cast<GLuint>(vertex_attributes)) attribute_enabled_[index] = enabled;
}

void RecordingGl::attribute_query(GLuint index, GLenum pname, GLint *value)
{
    if (index >= static_cast<GLuint>(vertex_attributes)) return;
    switch (pname) {
    case attrib_array_enabled: *value = attribute_enabled_[index] ? 1 : 0; break;
    case attrib_array_buffer_binding: *value = static_cast<GLint>(attribute_buffer_[index]); break;
    case current_vertex_attrib:
        check_span(value, 4u * sizeof *value);
        break;
    default: *value = 0; break;
    }
}

GLboolean RecordingGl::is_enabled(GLenum capability) const
{
    if (capability == vertex_array) return vertex_array_enabled_ ? 1u : 0u;
    if (capability == texture_coord_array) return texture_array_enabled_[client_unit_] ? 1u : 0u;
    return 0u;
}

void RecordingGl::client_state(GLenum capability, bool enabled)
{
    calls_++;
    if (capability == vertex_array) vertex_array_enabled_ = enabled;
    if (capability == texture_coord_array) texture_array_enabled_[client_unit_] = enabled;
}

void RecordingGl::client_texture(GLenum unit)
{
    if (unit >= texture0 && unit < texture0 + static_cast<GLenum>(texture_units))
        client_unit_ = static_cast<GLint>(unit - texture0);
}

void RecordingGl::array_pointer(GLenum array, const void *pointer)
{
    calls_++;
    if (pointer != nullptr && array_buffer_ == 0u) strays_++;
    if (array == vertex_array) vertex_array_buffer_ = array_buffer_;
    if (array == texture_coord_array) texture_array_buffer_[client_unit_] = array_buffer_;
}

void RecordingGl::uniform_block(GLenum pname, GLint *values)
{
    switch (pname) {
    case uniform_block_active_uniforms: *values = block_uniforms; break;
    case uniform_block_active_uniform_indices:
        calls_++;
        check_span(values, static_cast<std::uint64_t>(block_uniforms) * sizeof *values);
        break;
    default:
        calls_++;
        check_span(values, sizeof *values);
        break;
    }
}

void RecordingGl::map(GLenum target, GLint stride, GLint order, const GLfloat *points)
{
    calls_++;
    if (target != map1_vertex_3 || order < 1 || stride < static_cast<GLint>(map_dimensions)) return;
    const std::uint64_t values = static_cast<std::uint64_t>(order - 1) * static_cast<std::uint64_t>(stride) +
                                 map_dimensions;
    check_span(points, values * sizeof *points);
    map_target_ = target;
    map_order_ = order;
}

void RecordingGl::map_values(GLenum target, GLenum query, GLfloat *values)
{
    calls_++;
    std::uint64_t count = 1u;
    if (query == map_coefficients && target == map_target_)
        count = static_cast<std::uint64_t>(map_order_) * map_dimensions;
    check_span(values, count * sizeof *values);
}

void RecordingGl::map_order(GLenum target, GLenum query, GLint *values) const
{
    *values = query == map_order_query && target == map_target_ ? map_order_ : 0;
}

void RecordingGl::stipple(GLubyte *mask)
{
    calls_++;
    if (pack_buffer_ != 0u) return;
    std::uint64_t bytes = 0u;
    if (bitmap_reach(pack_, stipple_side, stipple_side, bytes)) check_span(mask, bytes);
}

void RecordingGl::bitmap(GLsizei width, GLsizei height, const GLubyte *bits)
{
    calls_++;
    if (unpack_buffer_ != 0u || width <= 0 || height <= 0) return;
    if (bits == nullptr) {
        strays_++;
        return;
    }
    std::uint64_t bytes = 0u;
    if (bitmap_reach(unpack_, width, height, bytes)) check_span(bits, bytes);
}

void RecordingGl::kept_pointer() { strays_++; }

bool RecordingGl::reach(const Store &store, GLsizei width, GLsizei height, GLsizei depth,
                        GLenum format, GLenum type, std::uint64_t &bytes) const
{
    const std::uint64_t pixel = pixel_size(format, type);
    if (pixel == 0u) return false;
    bytes = 0u;
    if (width <= 0 || height <= 0 || depth <= 0) return true;
    /* The last pixel GL touches: after the skipped images, rows and pixels,
     * with every row padded to the alignment and every image image_height
     * rows tall. Anything that overflows is further than any arena. */
    const std::uint64_t columns = static_cast<std::uint64_t>(store[4] > 0 ? store[4] : width);
    const std::uint64_t rows = static_cast<std::uint64_t>(store[5] > 0 ? store[5] : height);
    const std::uint64_t align = static_cast<std::uint64_t>(store[0]);
    std::uint64_t stride = 0u, image = 0u, before_images = 0u, before_rows = 0u, through = 0u;
    const bool overflowed =
        __builtin_mul_overflow(columns, pixel, &stride) ||
        __builtin_add_overflow(stride, align - 1u, &stride) ||
        __builtin_mul_overflow(stride / align * align, rows, &image) ||
        __builtin_mul_overflow(static_cast<std::uint64_t>(store[3]) +
                                   static_cast<std::uint64_t>(depth - 1), image, &before_images) ||
        __builtin_mul_overflow(static_cast<std::uint64_t>(store[2]) +
                                   static_cast<std::uint64_t>(height - 1), stride / align * align,
                               &before_rows) ||
        __builtin_mul_overflow(static_cast<std::uint64_t>(store[1]) +
                                   static_cast<std::uint64_t>(width), pixel, &through) ||
        __builtin_add_overflow(before_images, before_rows, &bytes) ||
        __builtin_add_overflow(bytes, through, &bytes);
    if (overflowed) bytes = ~std::uint64_t{0};
    return true;
}

bool RecordingGl::bitmap_reach(const Store &store, GLsizei width, GLsizei height,
                               std::uint64_t &bytes) const
{
    constexpr std::uint64_t bits_per_byte = 8u;
    const std::uint64_t columns = static_cast<std::uint64_t>(store[4] > 0 ? store[4] : width);
    const std::uint64_t stride = aligned((columns + bits_per_byte - 1u) / bits_per_byte, store[0]);
    const std::uint64_t last = (static_cast<std::uint64_t>(store[1]) + static_cast<std::uint64_t>(width) +
                                bits_per_byte - 1u) / bits_per_byte;
    bytes = (static_cast<std::uint64_t>(store[2]) + static_cast<std::uint64_t>(height - 1)) * stride + last;
    return width > 0 && height > 0;
}

void RecordingGl::check_span(const void *bytes, std::uint64_t length)
{
    if (length == 0u) return;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(bytes);
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(block_);
    const std::uintptr_t end = first + arena_.used();
    if (start < first || start > end || length > end - start) strays_++;
}

void RecordingGl::check_terminated(const GLchar *text)
{
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(text);
    const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(block_);
    const std::uintptr_t end = first + arena_.used();
    for (std::uintptr_t at = start; at >= first && at < end; at++) {
        if (block_[at - first] == 0u) return;
    }
    strays_++;
}

bool RecordingGl::client_array_enabled() const
{
    for (GLint i = 0; i < vertex_attributes; i++) {
        if (attribute_enabled_[i] && attribute_buffer_[i] == 0u) return true;
    }
    for (GLint i = 0; i < texture_units; i++) {
        if (texture_array_enabled_[i] && texture_array_buffer_[i] == 0u) return true;
    }
    return vertex_array_enabled_ && vertex_array_buffer_ == 0u;
}

}  // namespace av
