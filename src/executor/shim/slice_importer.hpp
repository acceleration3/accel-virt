#ifndef AV_SLICE_IMPORTER_HPP
#define AV_SLICE_IMPORTER_HPP

#include <stdint.h>

namespace av {

class OwnedFd;
class VulkanBuffer;

class SliceImporter {
public:
    SliceImporter(const SliceImporter &) = delete;
    SliceImporter &operator=(const SliceImporter &) = delete;

    [[nodiscard]] virtual bool import_to_draw(OwnedFd &descriptor, uint64_t bytes, VulkanBuffer &buffer) = 0;

protected:
    SliceImporter() = default;
    ~SliceImporter() = default;
};

}

#endif
