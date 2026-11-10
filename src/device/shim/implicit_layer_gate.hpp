#ifndef AV_IMPLICIT_LAYER_GATE_HPP
#define AV_IMPLICIT_LAYER_GATE_HPP

#include "core/support/vulkan_layer_filter.hpp"

namespace av {

class ProcessEnvironment final : public LayerEnvironment {
public:
    const char *value(const char *name) const override;
};

class ImplicitLayerGate {
public:
    ImplicitLayerGate();

    void close_implicit_layers();

private:
    void read_manifests(VulkanLayerFilter &filter) const;
    void read_directory(VulkanLayerFilter &filter, const char *directory, uint32_t length) const;
    void read_file(VulkanLayerFilter &filter, const char *path) const;
    void read_list(VulkanLayerFilter &filter, const char *list, const char *suffix) const;
};

}

#endif
