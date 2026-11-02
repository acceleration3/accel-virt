#ifndef AV_VENDOR_VENDOR_ADAPTER_HPP
#define AV_VENDOR_VENDOR_ADAPTER_HPP

#include "icd/common/adapter_facts.hpp"

namespace av::vendor {

class VendorAdapter {
public:
    VendorAdapter();
    VendorAdapter(const icd::AdapterFacts &facts, bool primary);

    const icd::AdapterFacts &facts() const;
    bool                     primary() const;

private:
    icd::AdapterFacts facts_;
    bool              primary_;
};

}

#endif
