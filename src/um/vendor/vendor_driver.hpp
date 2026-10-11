/* A vendor library's root and what it is built over - D3DKMT through gdi32
 * and a lock - made on the first call into the DLL rather than at attach,
 * where the loader lock forbids work, and by exactly one thread however many
 * arrive at once (RunOnce).
 *
 * Lighter than the graphics drivers' LazyDriver on purpose: atiadlxx.dll and
 * nvapi64.dll are loaded by any application that looks for an AMD or NVIDIA
 * GPU - games, launchers, monitoring tools - so they install no
 * unhandled-exception filter, start no trace service and open no log in
 * someone else's process. What they answer is a handful of facts Windows
 * already reports, and the unit tests are where it is judged. */
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
    /* Constexpr: the driver is the exports' constinit storage. */
    constexpr VendorDriver();
    VendorDriver(const VendorDriver &) = delete;
    VendorDriver &operator=(const VendorDriver &) = delete;

    /* The root, made on the first call; null only where the kernel could not
     * run the first call's body. */
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

}  // namespace av

#endif  // AV_UM_VENDOR_DRIVER_HPP
