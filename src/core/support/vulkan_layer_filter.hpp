#ifndef AV_VULKAN_LAYER_FILTER_HPP
#define AV_VULKAN_LAYER_FILTER_HPP

#include <stdint.h>

namespace av {

class LayerEnvironment {
public:
    virtual ~LayerEnvironment() = default;
    virtual const char *value(const char *name) const = 0;
};

class VulkanLayerFilter {
public:
    static constexpr uint32_t max_assignments = 64u;
    static constexpr uint32_t max_text = 96u;

    explicit VulkanLayerFilter(const LayerEnvironment &environment);

    void add_manifest(const char *json);
    void plan();

    uint32_t    count() const;
    const char *name(uint32_t index) const;
    const char *value(uint32_t index) const;

private:
    struct Assignment {
        char name[max_text];
        char value[max_text];
    };

    bool chosen(const char *name) const;
    void add(const char *name, const char *value);
    bool set_by_user(const char *name) const;

    const LayerEnvironment &environment_;
    uint32_t                manifest_count_;
    uint32_t                count_;
    Assignment              manifest_[max_assignments];
    Assignment              planned_[max_assignments];
};

}

#endif
