#include "icd/common/adapter_facts.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/escape/umd_adapter_info.hpp"

namespace av::icd {

namespace {

constexpr uint32_t every_fact = (AdapterFacts::features_fact << 1u) - 1u;

}

AdapterFacts::AdapterFacts()
    : known_(0u), luid_(0u), identity_(), location_(), name_(), driver_version_(0u), dedicated_video_(0u),
      dedicated_system_(0u), shared_system_(0u), integrated_(false)
{
    AV_SCOPE("AdapterFacts::AdapterFacts");
}

void AdapterFacts::learn(Kmt &kmt, uint32_t adapter)
{
    AV_SCOPE("AdapterFacts::learn");
    forget();
    if (kmt.adapter_luid(adapter, luid_) == Status::ok && luid_ != 0u) known_ |= luid_fact;
    else luid_ = 0u;
    if (kmt.query_identity(adapter, identity_) == Status::ok) known_ |= identity_fact;
    else identity_ = PciIdentity();
    if (kmt.query_adapter_location(adapter, location_) == Status::ok) known_ |= location_fact;
    else location_ = PciLocation();
    uint16_t wide[AdapterString::bytes] = {};
    if (kmt.query_adapter_name(adapter, wide, AdapterString::bytes) == Status::ok) {
        name_.take(wide);
        known_ |= name_fact;
    }
    if (kmt.query_umd_driver_version(adapter, driver_version_) == Status::ok) known_ |= driver_version_fact;
    else driver_version_ = 0u;
    if (kmt.query_segment_sizes(adapter, dedicated_video_, dedicated_system_, shared_system_) == Status::ok) {
        known_ |= segments_fact;
    } else {
        dedicated_video_ = 0u;
        dedicated_system_ = 0u;
        shared_system_ = 0u;
    }
    UmdAdapterInfo info;
    if (kmt.query_adapter_features(adapter, info) != Status::ok) return;
    known_ |= features_fact;
    integrated_ = info.offers(UmdAdapterInfo::feature_integrated_gpu);
}

void AdapterFacts::forget()
{
    AV_SCOPE("AdapterFacts::forget");
    *this = AdapterFacts();
}

bool AdapterFacts::knows(uint32_t facts) const
{
    AV_SCOPE("AdapterFacts::knows");
    return (known_ & facts) == facts;
}

uint64_t AdapterFacts::luid() const
{
    AV_SCOPE("AdapterFacts::luid");
    return luid_;
}

const PciIdentity &AdapterFacts::identity() const
{
    AV_SCOPE("AdapterFacts::identity");
    return identity_;
}

const PciLocation &AdapterFacts::location() const
{
    AV_SCOPE("AdapterFacts::location");
    return location_;
}

const char *AdapterFacts::name() const
{
    AV_SCOPE("AdapterFacts::name");
    return name_.text();
}

uint64_t AdapterFacts::driver_version() const
{
    AV_SCOPE("AdapterFacts::driver_version");
    return driver_version_;
}

void AdapterFacts::append_driver_version(TextBuffer &text) const
{
    AV_SCOPE("AdapterFacts::append_driver_version");
    constexpr uint32_t parts = 4u;
    for (uint32_t part = 0u; part < parts; ++part) {
        if (part != 0u) text.append(".");
        text.append_decimal((driver_version_ >> (16u * (parts - 1u - part))) & 0xFFFFu);
    }
}

uint64_t AdapterFacts::dedicated_video_bytes() const
{
    AV_SCOPE("AdapterFacts::dedicated_video_bytes");
    return dedicated_video_;
}

uint64_t AdapterFacts::dedicated_system_bytes() const
{
    AV_SCOPE("AdapterFacts::dedicated_system_bytes");
    return dedicated_system_;
}

uint64_t AdapterFacts::shared_system_bytes() const
{
    AV_SCOPE("AdapterFacts::shared_system_bytes");
    return shared_system_;
}

bool AdapterFacts::integrated() const
{
    AV_SCOPE("AdapterFacts::integrated");
    return integrated_;
}

bool AdapterFacts::invariants_hold() const
{
    AV_SCOPE("AdapterFacts::invariants_hold");
    if ((known_ & ~every_fact) != 0u) return false;
    if (!knows(luid_fact) && luid_ != 0u) return false;
    if (!knows(name_fact) && name_.text()[0] != '\0') return false;
    if (!knows(driver_version_fact) && driver_version_ != 0u) return false;
    if (!knows(segments_fact) && (dedicated_video_ | dedicated_system_ | shared_system_) != 0u) return false;
    return knows(features_fact) || !integrated_;
}

}
