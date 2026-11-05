#ifndef AV_SLICE_STORAGE_HPP
#define AV_SLICE_STORAGE_HPP

#include "executor/shim/vulkan_buffer.hpp"

#include <stdint.h>

namespace av {

class SliceImporter;
class SliceProgress;
class SourceMemory;

class SliceStorage {
public:
    SliceStorage(SourceMemory &store, SliceImporter &importer, const SliceProgress &progress);
    ~SliceStorage();
    SliceStorage(const SliceStorage &) = delete;
    SliceStorage &operator=(const SliceStorage &) = delete;

    [[nodiscard]] bool import();
    void               stamp();
    void               retire();

    bool                holds(const SourceMemory &store, const SliceImporter &importer) const;
    bool                holds_memory(const SourceMemory &store) const;
    bool                imported_by(const SliceImporter &importer) const;
    bool                imported() const;
    bool                refused() const;
    bool                retired() const;
    bool                done() const;
    const VulkanBuffer &buffer() const;

    bool invariants_hold() const;

private:
    SourceMemory  *store_;
    SliceImporter *importer_;
    const SliceProgress *progress_;
    VulkanBuffer  buffer_;
    uint64_t      mark_;
    bool          refused_;
    bool          retired_;
};

}

#endif
