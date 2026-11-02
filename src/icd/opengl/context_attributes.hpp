#ifndef AV_GLICD_CONTEXT_ATTRIBUTES_HPP
#define AV_GLICD_CONTEXT_ATTRIBUTES_HPP

#include "proto/context/gl_context_options.hpp"

#include <stdint.h>

namespace av::glicd {

struct ContextAttributesProbe;

class ContextAttributes {
public:
    static constexpr uint32_t max_pairs = 64u;

    static constexpr uint32_t error_invalid_parameter = 87u;
    static constexpr uint32_t error_invalid_version = 0x2095u;
    static constexpr uint32_t error_invalid_profile = 0x2096u;

    static constexpr int32_t major_version = 0x2091;
    static constexpr int32_t minor_version = 0x2092;
    static constexpr int32_t layer_plane = 0x2093;
    static constexpr int32_t context_flags = 0x2094;
    static constexpr int32_t profile_mask = 0x9126;
    static constexpr int32_t reset_strategy = 0x8256;

    static constexpr int32_t flag_debug = 0x1;
    static constexpr int32_t flag_forward_compatible = 0x2;
    static constexpr int32_t flag_robust_access = 0x4;
    static constexpr int32_t flag_reset_isolation = 0x8;
    static constexpr int32_t profile_core = 0x1;
    static constexpr int32_t profile_compatibility = 0x2;
    static constexpr int32_t no_reset_notification = 0x8261;
    static constexpr int32_t lose_context_on_reset = 0x8252;

    explicit ContextAttributes(const int32_t *list);

    uint32_t         decode();
    GlContextOptions options() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t names = 6u;

    uint32_t read_pairs();
    uint32_t judge() const;

    const int32_t *list_;
    uint32_t       seen_;
    int32_t        values_[names];
    bool           accepted_;

    friend struct ContextAttributesProbe;
};

}

#endif
