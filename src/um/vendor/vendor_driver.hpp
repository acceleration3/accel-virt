#ifndef AV_UM_VENDOR_DRIVER_HPP
#define AV_UM_VENDOR_DRIVER_HPP

#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "um/common/once_body.hpp"
#include "um/common/run_once.hpp"
#include "um/common/srw_lock.hpp"
#include "um/kmt/gdi_kmt.hpp"

namespace av {

template <typename Root>
class VendorDriver final : public OnceBody {
public:
    constexpr VendorDriver();
    VendorDriver(const VendorDriver &) = delete;
    VendorDriver &operator=(const VendorDriver &) = delete;

    Root *root();
    void  run_once() override;

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;

        SrwLock lock;
        GdiKmt  kmt;
        Root    root;
    };

    ObjectStorage<Seams> seams_;
    RunOnce              once_;
};

template <typename Root>
constexpr VendorDriver<Root>::VendorDriver() : seams_(), once_()
{}

template <typename Root>
VendorDriver<Root>::Seams::Seams() : lock(), kmt(), root(kmt, lock)
{
    AV_SCOPE("VendorDriver::Seams::Seams");
}

template <typename Root>
Root *VendorDriver<Root>::root()
{
    AV_SCOPE("VendorDriver::root");
    if (!once_.run(*this) || !seams_.constructed()) return nullptr;
    return &seams_.object().root;
}

template <typename Root>
void VendorDriver<Root>::run_once()
{
    AV_SCOPE("VendorDriver::run_once");
    static_cast<void>(seams_.construct());
}

}

#endif
