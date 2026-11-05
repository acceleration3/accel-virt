#ifndef AV_SCRIPTED_SLICE_IMPORTER_HPP
#define AV_SCRIPTED_SLICE_IMPORTER_HPP

#include "executor/shim/slice_importer.hpp"

#include <cstdint>

namespace av {

class ScriptedSliceImporter final : public SliceImporter {
public:
    ScriptedSliceImporter();

    bool import_to_draw(OwnedFd &descriptor, std::uint64_t bytes, VulkanBuffer &buffer) override;

    void          refuse(bool refusing);
    std::uint32_t imports() const;
    std::uint64_t last_bytes() const;
    bool          last_descriptor_held() const;

private:
    bool          refusing_;
    std::uint32_t imports_;
    std::uint64_t last_bytes_;
    bool          last_descriptor_held_;
};

}

#endif
