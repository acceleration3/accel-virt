#ifndef AV_CLICD_CL_MEM_EXTENT_HPP
#define AV_CLICD_CL_MEM_EXTENT_HPP

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class ClMemExtent {
public:
    enum class Shape : uint8_t { unknown, buffer, image };

    ClMemExtent();

    void as_buffer(uint64_t base, uint64_t bytes);
    void as_image(cl_mem_object_type type, size_t width, size_t height, size_t depth, size_t array,
                  const cl_image_format &format);

    Shape    shape() const;
    uint64_t base() const;
    uint64_t bytes() const;

    bool holds_range(uint64_t offset, uint64_t bytes) const;
    bool same_format(const ClMemExtent &other) const;
    uint32_t element_bytes() const;
    bool same_geometry(const ClMemExtent &other) const;

    [[nodiscard]] cl_int judge_region(const size_t *origin, const size_t *region) const;
    [[nodiscard]] bool   regions_overlap(const size_t *origin, const ClMemExtent &other, const size_t *other_origin,
                                         const size_t *region) const;

private:
    Shape              shape_;
    cl_mem_object_type type_;
    uint64_t           base_;
    uint64_t           bytes_;
    size_t             width_;
    size_t             height_;
    size_t             depth_;
    size_t             array_;
    cl_image_format    format_;
};

}

#endif
