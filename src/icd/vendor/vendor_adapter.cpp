#include "icd/vendor/vendor_adapter.hpp"

#include "core/trace/function_scope.hpp"

namespace av::vendor {

VendorAdapter::VendorAdapter() : facts_(), primary_(false)
{
    AV_SCOPE("VendorAdapter::VendorAdapter");
}

VendorAdapter::VendorAdapter(const icd::AdapterFacts &facts, bool primary) : facts_(facts), primary_(primary)
{
    AV_SCOPE("VendorAdapter::VendorAdapter");
}

const icd::AdapterFacts &VendorAdapter::facts() const
{
    AV_SCOPE("VendorAdapter::facts");
    return facts_;
}

bool VendorAdapter::primary() const
{
    AV_SCOPE("VendorAdapter::primary");
    return primary_;
}

}
