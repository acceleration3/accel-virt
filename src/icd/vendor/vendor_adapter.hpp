/* One avgpu adapter as a GPU vendor's management library describes it - AMD's
 * Display Library, NVIDIA's NVAPI (docs/vendor-libraries.md): what Windows
 * says of it (icd::AdapterFacts), and whether the primary display is on it.
 * Every fact is one Windows already reports for the adapter, so the library
 * and Device Manager, SetupAPI and DXGI agree about the one device. */
#ifndef AV_VENDOR_VENDOR_ADAPTER_HPP
#define AV_VENDOR_VENDOR_ADAPTER_HPP

#include "icd/common/adapter_facts.hpp"

namespace av::vendor {

class VendorAdapter {
public:
    VendorAdapter();
    VendorAdapter(const icd::AdapterFacts &facts, bool primary);

    const icd::AdapterFacts &facts() const;
    /* The primary display is on this adapter. */
    bool                     primary() const;

private:
    icd::AdapterFacts facts_;
    bool              primary_;
};

}  // namespace av::vendor

#endif  // AV_VENDOR_VENDOR_ADAPTER_HPP
