/* One of dxgkrnl's mode sets - source, target or monitor - as a fixed array of
 * mode records: CreateNewModeInfo hands out a scratch record with a fresh id,
 * AddMode copies it in (a duplicate answers
 * STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET), PinMode pins by id. Every mode info
 * acquired or created must be released; a set destroyed with one outstanding
 * fails the test. */
#ifndef AV_FAKE_MODE_SET_HPP
#define AV_FAKE_MODE_SET_HPP

#include "core/ddi/d3dkmdt_monitor_source_mode_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/ddi/d3dkmdt_vidpn_target_mode_record.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

namespace av {

/* What AddMode treats as the same mode: a source by surface size and format, a
 * target or monitor mode by active size and refresh rate. */
inline bool same_mode(const D3dkmdtVidPnSourceModeRecord &a, const D3dkmdtVidPnSourceModeRecord &b)
{
    return a.PrimSurfSizeCx == b.PrimSurfSizeCx && a.PrimSurfSizeCy == b.PrimSurfSizeCy &&
           a.PixelFormat == b.PixelFormat;
}

inline bool same_signal(const D3dkmdtVideoSignalInfoRecord &a, const D3dkmdtVideoSignalInfoRecord &b)
{
    return a.ActiveSizeCx == b.ActiveSizeCx && a.ActiveSizeCy == b.ActiveSizeCy &&
           a.VSyncFreqNumerator == b.VSyncFreqNumerator && a.VSyncFreqDenominator == b.VSyncFreqDenominator;
}

inline bool same_mode(const D3dkmdtVidPnTargetModeRecord &a, const D3dkmdtVidPnTargetModeRecord &b)
{
    return same_signal(a.VideoSignalInfo, b.VideoSignalInfo);
}

inline bool same_mode(const D3dkmdtMonitorSourceModeRecord &a, const D3dkmdtMonitorSourceModeRecord &b)
{
    return same_signal(a.VideoSignalInfo, b.VideoSignalInfo);
}

/* What dxgkrnl stamps into a mode info it hands out, and checks on the way back
 * in. DXGK_VIDPNTARGETMODESET_INTERFACE_V1_IMPL::AddModeImpl (dxgkrnl.sys
 * @1403b38ec, the call at @1403b40a1) bugchecks with VIDEO_DXGKRNL_FATAL_ERROR
 * (0x113, subcode 7) - not a returned status - when a target mode comes back
 * with MinimumVSyncFreq other than the all-ones CreateNewModeInfo wrote. Source
 * and monitor modes carry no such field. */
constexpr std::uint32_t unspecified_vsync = 0xFFFFFFFFu;

inline void stamp_created(D3dkmdtVidPnSourceModeRecord &) {}
inline void stamp_created(D3dkmdtMonitorSourceModeRecord &) {}
inline void stamp_created(D3dkmdtVidPnTargetModeRecord &mode)
{
    mode.MinimumVSyncFreqNumerator = unspecified_vsync;
    mode.MinimumVSyncFreqDenominator = unspecified_vsync;
}

inline bool stamp_intact(const D3dkmdtVidPnSourceModeRecord &) { return true; }
inline bool stamp_intact(const D3dkmdtMonitorSourceModeRecord &) { return true; }
inline bool stamp_intact(const D3dkmdtVidPnTargetModeRecord &mode)
{
    return mode.MinimumVSyncFreqNumerator == unspecified_vsync &&
           mode.MinimumVSyncFreqDenominator == unspecified_vsync;
}

template <typename Mode>
class FakeModeSet {
public:
    static constexpr std::uint32_t capacity = 32u;
    static constexpr std::int32_t  mode_already_in_mode_set = static_cast<std::int32_t>(0xC01E0314u);
    static constexpr std::int32_t  mode_not_in_mode_set = static_cast<std::int32_t>(0xC01E034Au);
    static constexpr std::int32_t  dataset_is_empty = 0x401E034B;
    static constexpr std::int32_t  no_more_elements = 0x401E034C;
    static constexpr std::int32_t  invalid_parameter = static_cast<std::int32_t>(0xC000000Du);

    FakeModeSet() : modes_{}, scratch_{}, count_(0u), pinned_(capacity), next_id_(1u), outstanding_(0u) {}

    ~FakeModeSet()
    {
        if (outstanding_ != 0u)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "%u mode info(s) never released", outstanding_);
    }

    FakeModeSet(const FakeModeSet &) = delete;
    FakeModeSet &operator=(const FakeModeSet &) = delete;

    void clear()
    {
        count_ = 0u;
        pinned_ = capacity;
    }

    /* A test builds a set directly: the mode is added, and pinned if asked. */
    void seed(const Mode &mode, bool pin)
    {
        if (count_ == capacity) return;
        modes_[count_] = mode;
        modes_[count_].Id = next_id_++;
        if (pin) pinned_ = count_;
        count_++;
    }

    std::int32_t count(std::uint64_t *modes) const
    {
        *modes = count_;
        return 0;
    }

    std::int32_t acquire_first(const Mode **mode)
    {
        *mode = count_ == 0u ? nullptr : &modes_[0];
        if (*mode == nullptr) return dataset_is_empty;
        outstanding_++;
        return 0;
    }

    std::int32_t acquire_next(const Mode *mode, const Mode **next)
    {
        const std::uint32_t index = index_of(mode) + 1u;
        *next = index < count_ ? &modes_[index] : nullptr;
        if (*next == nullptr) return no_more_elements;
        outstanding_++;
        return 0;
    }

    std::int32_t acquire_pinned(const Mode **mode)
    {
        *mode = pinned_ < count_ ? &modes_[pinned_] : nullptr;
        if (*mode != nullptr) outstanding_++;
        return 0;
    }

    std::int32_t release(const Mode *mode)
    {
        if (mode == nullptr || outstanding_ == 0u) {
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "released a mode info that was not held");
            return invalid_parameter;
        }
        outstanding_--;
        return 0;
    }

    std::int32_t create_new(Mode **mode)
    {
        std::memset(static_cast<void *>(&scratch_), 0, sizeof scratch_);
        scratch_.Id = next_id_++;
        stamp_created(scratch_);
        *mode = &scratch_;
        outstanding_++;
        return 0;
    }

    /* On success the new mode info is released, as dxgkrnl documents. */
    std::int32_t add(const Mode *mode)
    {
        /* dxgkrnl bugchecks here rather than refusing, so the harness fails the
         * test rather than answering a status the driver could handle. */
        if (!stamp_intact(*mode))
            TestRegistry::instance().record_failure(
                __FILE__, __LINE__,
                "AddMode: the mode info's MinimumVSyncFreq was overwritten; dxgkrnl bugchecks 0x113 subcode 7");
        for (std::uint32_t index = 0u; index < count_; index++)
            if (same_mode(modes_[index], *mode)) return mode_already_in_mode_set;
        if (count_ == capacity) return invalid_parameter;
        modes_[count_++] = *mode;
        outstanding_--;
        return 0;
    }

    std::int32_t pin(std::uint32_t id)
    {
        for (std::uint32_t index = 0u; index < count_; index++) {
            if (modes_[index].Id != id) continue;
            pinned_ = index;
            return 0;
        }
        return mode_not_in_mode_set;
    }

    std::uint32_t modes() const { return count_; }
    const Mode   &mode(std::uint32_t index) const { return modes_[index < capacity ? index : 0u]; }
    bool          pinned() const { return pinned_ < count_; }
    std::uint32_t outstanding() const { return outstanding_; }

private:
    std::uint32_t index_of(const Mode *mode) const
    {
        for (std::uint32_t index = 0u; index < count_; index++)
            if (&modes_[index] == mode) return index;
        return capacity;
    }

    Mode          modes_[capacity];
    Mode          scratch_;
    std::uint32_t count_;
    std::uint32_t pinned_;
    std::uint32_t next_id_;
    std::uint32_t outstanding_;
};

}  // namespace av

#endif  // AV_FAKE_MODE_SET_HPP
