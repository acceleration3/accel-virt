#ifndef AV_SLICE_STORAGE_HPP
#define AV_SLICE_STORAGE_HPP

#include "executor/shim/vulkan_buffer.hpp"

#include <stdint.h>

namespace av {

class D3d11HostDevice;
class SourceMemory;

class SliceStorage {
public:
    explicit SliceStorage(SourceMemory &store);
    ~SliceStorage();
    SliceStorage(const SliceStorage &) = delete;
    SliceStorage &operator=(const SliceStorage &) = delete;

    [[nodiscard]] bool import(D3d11HostDevice &device);
    void               used_at(uint64_t mark);
    void               retire();

    bool                holds(const SourceMemory &store) const;
    bool                imported() const;
    bool                refused() const;
    bool                retired() const;
    bool                done(uint64_t reached) const;
    const VulkanBuffer &buffer() const;

private:
    SourceMemory *store_;
    VulkanBuffer  buffer_;
    uint64_t      mark_;
    bool          refused_;
    bool          retired_;
};

}

#endif
