#ifndef AV_CLICD_MAPPED_REGION_HPP
#define AV_CLICD_MAPPED_REGION_HPP

#include "icd/common/staged_mapping.hpp"
#include "icd/opencl/cl_image_region.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class MappedRegion : public icd::StagedMapping {
public:
    enum class Memory : uint8_t { shared, staged, application };

    MappedRegion();
    MappedRegion(uint64_t buffer, uint64_t host, size_t offset, size_t bytes, cl_map_flags flags, Memory memory);
    MappedRegion(uint64_t image, const size_t *origin, const size_t *region, size_t element_bytes, size_t bytes,
                 cl_map_flags flags);

    uint64_t      host() const;
    bool          image() const;
    ClImageRegion image_layout() const;
    bool          writes_back() const;

    bool invariants_hold() const;

private:
    uint64_t host_;
    size_t   origin_[3];
    size_t   region_[3];
    size_t   element_bytes_;
    Memory   memory_;
};

}

#endif
