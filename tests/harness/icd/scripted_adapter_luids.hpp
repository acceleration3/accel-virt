/* The adapter LUID a test decides, answered for whichever adapter is asked
 * about - the one handle MockKmt gave the search - or refused. Stands in for
 * Kmt::adapter_luid until T021's Kmt arrives (integration point). */
#ifndef AV_SCRIPTED_ADAPTER_LUIDS_HPP
#define AV_SCRIPTED_ADAPTER_LUIDS_HPP

#include "icd/vulkan/adapter_luids.hpp"

#include <cstdint>

namespace av {

class ScriptedAdapterLuids final : public icd::AdapterLuids {
public:
    /* Both halves non-zero and different, so a half dropped or swapped shows. */
    static constexpr std::uint64_t default_luid = 0x0000000200001234ull;

    ScriptedAdapterLuids();

    [[nodiscard]] Status adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;

    void          answer(std::uint64_t luid);
    /* Every later question is refused with not_found. */
    void          refuse();
    /* The adapter last asked about, 0 before the first question. */
    std::uint32_t asked() const;

private:
    std::uint64_t luid_;
    bool          refusing_;
    std::uint32_t asked_;
};

}  // namespace av

#endif  // AV_SCRIPTED_ADAPTER_LUIDS_HPP
