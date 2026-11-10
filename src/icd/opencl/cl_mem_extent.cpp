#include "icd/opencl/cl_mem_extent.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClMemExtent::ClMemExtent()
    : shape_(Shape::unknown), type_(0u), base_(0u), bytes_(0u), width_(0u), height_(0u), depth_(0u), array_(0u),
      format_{}, row_pitch_(0u), slice_pitch_(0u)
{
    AV_SCOPE("ClMemExtent::ClMemExtent");
}

void ClMemExtent::as_buffer(uint64_t base, uint64_t bytes)
{
    AV_SCOPE("ClMemExtent::as_buffer");
    shape_ = Shape::buffer;
    type_ = CL_MEM_OBJECT_BUFFER;
    base_ = base;
    bytes_ = bytes;
}

void ClMemExtent::as_image(cl_mem_object_type type, size_t width, size_t height, size_t depth, size_t array,
                           const cl_image_format &format)
{
    AV_SCOPE("ClMemExtent::as_image");
    shape_ = Shape::image;
    type_ = type;
    width_ = width;
    height_ = height;
    depth_ = depth;
    array_ = array;
    format_ = format;
}

void ClMemExtent::with_pitches(size_t row, size_t slice)
{
    AV_SCOPE("ClMemExtent::with_pitches");
    row_pitch_ = row;
    slice_pitch_ = slice;
}

size_t ClMemExtent::row_pitch() const
{
    AV_SCOPE("ClMemExtent::row_pitch");
    return row_pitch_ != 0u ? row_pitch_ : width_ * element_bytes();
}

size_t ClMemExtent::slice_pitch() const
{
    AV_SCOPE("ClMemExtent::slice_pitch");
    if (slice_pitch_ != 0u) return slice_pitch_;
    return type_ == CL_MEM_OBJECT_IMAGE1D_BUFFER || type_ == CL_MEM_OBJECT_IMAGE1D || type_ == CL_MEM_OBJECT_IMAGE2D
               ? 0u
               : row_pitch() * (type_ == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1u : height_);
}

cl_mem_object_type ClMemExtent::type() const
{
    AV_SCOPE("ClMemExtent::type");
    return type_;
}

ClMemExtent::Shape ClMemExtent::shape() const
{
    AV_SCOPE("ClMemExtent::shape");
    return shape_;
}

uint64_t ClMemExtent::base() const
{
    AV_SCOPE("ClMemExtent::base");
    return base_;
}

uint64_t ClMemExtent::bytes() const
{
    AV_SCOPE("ClMemExtent::bytes");
    return bytes_;
}

bool ClMemExtent::holds_range(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("ClMemExtent::holds_range");
    if (shape_ != Shape::buffer) return true;
    return bytes != 0u && offset <= bytes_ && bytes <= bytes_ - offset;
}

bool ClMemExtent::same_format(const ClMemExtent &other) const
{
    AV_SCOPE("ClMemExtent::same_format");
    if (shape_ != Shape::image || other.shape_ != Shape::image) return true;
    return format_.image_channel_order == other.format_.image_channel_order &&
           format_.image_channel_data_type == other.format_.image_channel_data_type;
}

uint32_t ClMemExtent::element_bytes() const
{
    AV_SCOPE("ClMemExtent::element_bytes");
    if (shape_ != Shape::image) return 0u;
    uint32_t channels = 0u;
    switch (format_.image_channel_order) {
    case CL_R: case CL_A: case CL_INTENSITY: case CL_LUMINANCE: case CL_DEPTH: channels = 1u; break;
    case CL_RG: case CL_RA: case CL_Rx: channels = 2u; break;
    case CL_RGB: case CL_RGx: channels = 3u; break;
    case CL_RGBA: case CL_BGRA: case CL_ARGB: case CL_ABGR: case CL_RGBx: case CL_sRGB: case CL_sRGBx: case CL_sRGBA:
    case CL_sBGRA:
        channels = 4u;
        break;
    default: return 0u;
    }
    switch (format_.image_channel_data_type) {
    case CL_SNORM_INT8: case CL_UNORM_INT8: case CL_SIGNED_INT8: case CL_UNSIGNED_INT8: return channels;
    case CL_SNORM_INT16: case CL_UNORM_INT16: case CL_SIGNED_INT16: case CL_UNSIGNED_INT16: case CL_HALF_FLOAT:
        return channels * 2u;
    case CL_SIGNED_INT32: case CL_UNSIGNED_INT32: case CL_FLOAT: return channels * 4u;
    case CL_UNORM_SHORT_565: case CL_UNORM_SHORT_555: return 2u;
    case CL_UNORM_INT_101010: return 4u;
    default: return 0u;
    }
}

bool ClMemExtent::same_geometry(const ClMemExtent &other) const
{
    AV_SCOPE("ClMemExtent::same_geometry");
    return type_ == other.type_ && width_ == other.width_ && height_ == other.height_ && depth_ == other.depth_ &&
           array_ == other.array_;
}

cl_int ClMemExtent::judge_region(const size_t *origin, const size_t *region) const
{
    AV_SCOPE("ClMemExtent::judge_region");
    if (shape_ != Shape::image) return CL_SUCCESS;
    if (origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    if (region[0] == 0u || region[1] == 0u || region[2] == 0u) return CL_INVALID_VALUE;
    size_t limits[3] = {width_, 1u, 1u};
    switch (type_) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        limits[1] = array_;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        limits[1] = height_;
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        limits[1] = height_;
        limits[2] = array_;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        limits[1] = height_;
        limits[2] = depth_;
        break;
    default:
        return CL_SUCCESS;
    }
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        if (origin[axis] > limits[axis] || region[axis] > limits[axis] - origin[axis]) return CL_INVALID_VALUE;
    }
    return CL_SUCCESS;
}

bool ClMemExtent::regions_overlap(const size_t *origin, const ClMemExtent &other, const size_t *other_origin,
                                  const size_t *region) const
{
    AV_SCOPE("ClMemExtent::regions_overlap");
    static_cast<void>(other);
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        const size_t low = origin[axis] < other_origin[axis] ? other_origin[axis] : origin[axis];
        const size_t high_a = origin[axis] + region[axis];
        const size_t high_b = other_origin[axis] + region[axis];
        const size_t high = high_a < high_b ? high_a : high_b;
        if (low >= high) return false;
    }
    return true;
}

}
